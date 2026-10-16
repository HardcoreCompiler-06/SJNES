/*
 * SJNES - Nintendo Entertainment System Emulator
 * Copyright (C) 2026 Chiến
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "SJNES.h"
#include <cstdint>
#include <functional>
#include <algorithm>

// ---- DEBUG TẠM: khai báo biến global từ EmuWorker.cpp, reset ngay sau khi
// nạp ROM mới xong (xem loadRomFile bên dưới), để ngân sách log PC luôn bắt
// đầu đúng lúc mở file .fds thay vì bị "ăn" hết bởi cycle chạy trước đó.
extern long g_debugPcLogCount;

#include <QMenu>
#include <QStringList>
#include <QFileDialog>
#include <QMessageBox>
#include <algorithm>
#include "Mapper.h"
#include <QSettings>
#include <QFileInfo>
#include "LogBuffer.h"
#include "NSFFile.h"
#include <QActionGroup>
#include "GpuScreenWidget.h"
#include "Mapper_024.h"
#include "Mapper_069.h"
#include <QAction>
#include <QShortcut>
#include <QKeySequence>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include "miniz.h"
#include <vector>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QTextEdit>
#include "Mapper_085.h"
#include "Mapper_005.h"
#include "Mapper_019.h"
#include "Mapper_020.h"
#include <QTextBrowser>
#include <QFileInfo>
#include <QElapsedTimer>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "EQWindow.h"
#include "BinaryIO.h"
#include "AviWriter.h"

// Nhớ kích thước cửa sổ NGAY TRƯỚC KHI vào NSF mode, để khi rời NSF (mở lại
// file .nes) trả cửa sổ về đúng size cũ thay vì giữ nguyên khung 16:9 của
// NSF (gây ra 2 mép trắng 2 bên vì gameScreen bên trong tự letterbox về 4:3).
static QSize g_preNsfWindowSize;

// Cac khoi oscilloscope phu (ban sao cua cua so chip) dung de xep kenh lien tuc khong chong nhau
// trong NSF nhieu chip. Xem SJNES::resizeEvent.
static QVector<AudioWaveWindow*> g_waveClones;
static QString g_waveClonesSig;

// Rate thật (Hz) mà audio_sink đang mở, để watchdog trong restartAudioSink()
// so sánh với rate live của thiết bị mỗi giây (phát hiện đổi rate ngay cả
// khi QAudioSink không tự báo lỗi gì).
static int g_lastKnownAudioDeviceRate = 0;

// Cửa sổ game riêng: top-level window chỉ chứa đúng ui.gameScreen (GpuScreenWidget),
// không menu, nền đen, tự giữ tỉ lệ 256:240. OBS Game Capture / RTSS hook vào
// cửa sổ này nên chỉ thấy khung NES (OSD FPS nằm đúng góc khung NES).
// Không dùng Q_OBJECT nên không cần moc; phím bấm được chuyển về SJNES qua callback.
class GameWindow : public QWidget
{
public:
    explicit GameWindow(QWidget* screen)
        : QWidget(nullptr, Qt::Window), m_screen(screen)
    {
        setWindowTitle("SJNES Game");
        QPalette pal = palette();
        pal.setColor(QPalette::Window, Qt::black);
        setPalette(pal);
        setAutoFillBackground(true);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_DeleteOnClose);
    }

    std::function<void(QKeyEvent*)> onKeyPress;
    std::function<void(QKeyEvent*)> onKeyRelease;
    std::function<void()> onClosing;

protected:
    void keyPressEvent(QKeyEvent* e) override { if (onKeyPress) onKeyPress(e); }
    void keyReleaseEvent(QKeyEvent* e) override { if (onKeyRelease) onKeyRelease(e); }

    void closeEvent(QCloseEvent* e) override
    {
        if (onClosing) onClosing();   // trả gameScreen về cửa sổ chính
        m_screen = nullptr;
        e->accept();
    }

    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);

        // Giữ cửa sổ đúng tỉ lệ 256:240 để không có thanh đen trong vùng capture
        // (bỏ qua khi fullscreen / maximize: lúc đó GpuScreenWidget tự letterbox).
        if (!m_adjusting && !isFullScreen() && !isMaximized() && width() > 0 && height() > 0)
        {
            int w = width();
            int h = height();
            const QSize old = e->oldSize();
            if (old.isValid() && old.height() == h && old.width() != w)
                h = w * 240 / 256;      // người dùng kéo cạnh ngang
            else
                w = h * 256 / 240;      // kéo cạnh dọc / góc

            if (w != width() || h != height())
            {
                m_adjusting = true;
                resize(w, h);
                m_adjusting = false;
            }
        }

        if (m_screen)
            m_screen->setGeometry(rect());
    }

private:
    QWidget* m_screen = nullptr;
    bool m_adjusting = false;
};

// ---- Overlay debug (F10 lần 2): CPU / bus / PPU đang ở đâu, đọc trực tiếp mỗi frame ----
// UI thread đọc trực tiếp trạng thái đang chạy trên emuThread (không lock) -> chỉ là
// "ảnh chụp" gần đúng để hiển thị, như debugger live; không ảnh hưởng tới emulation.
static QString HexN(uint32_t v, int digits)
{
    return QString("$%1").arg(v, digits, 16, QChar('0')).toUpper();
}

static const char* CpuRegionName(uint16_t a, bool isWrite)
{
    if (a < 0x2000) return "RAM";
    if (a < 0x4000) return "PPU REG";
    if (a < 0x4020) return "APU/IO";
    if (a < 0x6000) return "EXP";
    if (a < 0x8000) return "WRAM";
    return isWrite ? "MAPPER REG" : "PRG ROM";
}

static const char* PpuPhaseName(int scanline)
{
    if (scanline < 0)    return "PRE-RENDER";
    if (scanline < 240)  return "VISIBLE";
    if (scanline == 240) return "POST-RENDER";
    return "VBLANK";
}

static QString VramRegionName(uint16_t v)
{
    v &= 0x3FFF;
    if (v < 0x1000) return "PATTERN 0 (CHR)";
    if (v < 0x2000) return "PATTERN 1 (CHR)";
    if (v < 0x3F00) return QString("NAMETABLE %1").arg((v >> 10) & 3);
    return "PALETTE";
}

static QString BuildDebugOverlayText(EmuWorker* w)
{
    if (!w) return QString();

    const uint16_t pc = w->nes_cpu.pc;
    const uint16_t bus = w->nes_bus.last_bus_addr;
    const bool busWrite = w->nes_bus.last_bus_write;
    const int sl = w->nes_ppu.GetScanline();
    const int dot = w->nes_ppu.GetCycle();
    const uint16_t vram = w->nes_ppu.GetVramAddr();

    return QString("PC   %1 [%2]\nBUS  %3 %4 [%5]\nPPU  SL %6 DOT %7 [%8]\nVRAM %9 [%10]")
        .arg(HexN(pc, 4))
        .arg(CpuRegionName(pc, false))
        .arg(HexN(bus, 4))
        .arg(busWrite ? "W" : "R")
        .arg(CpuRegionName(bus, busWrite))
        .arg(sl)
        .arg(dot)
        .arg(PpuPhaseName(sl))
        .arg(HexN(vram & 0x3FFF, 4))
        .arg(VramRegionName(vram));
}

static void ShowQuickStartDialog(QWidget* parent, bool forceShow = false)
{
    QSettings settings("chienz", "NesEmulator");

    bool hideQuickStart = settings.value("hideQuickStart", false).toBool();

    if (hideQuickStart && !forceShow)
        return;

    QDialog dialog(parent);
    dialog.setWindowTitle("SJNES Quick Start");
    dialog.resize(560, 420);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLabel* title = new QLabel("SJNES - Quick Start");
    QFont titleFont = title->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    title->setFont(titleFont);

    QTextEdit* text = new QTextEdit();
    text->setReadOnly(true);

    text->setPlainText(
        "Chào mừng đến với SJNES!\n\n"
        "Đây là phần mềm giả lập máy chơi game Nintendo Entertament System\n"
        "phần mềm được code bởi Nguyễn chiến "
        "nếu thấy ok vui lòng cho mình 1 sao ở github nhé=)\n"
        "1. Mở ROM:\n"
        "   Vào File -> Open ROM để chọn file .nes hoặc .zip, (file NSF).\n\n"
        "2. Điều khiển Player 1:\n"
        "   K = A\n"
        "   J = B\n"
        "   U = Select\n"
        "   Enter = Start\n"
        "   W A S D = Di chuyển\n\n"
        "3. Điều khiển Player 2:\n"
        "   Phím mũi tên = Di chuyển\n"
        "   Numpad 6 = A\n"
        "   Numpad 5 = B\n"
        "   Numpad 7 = Select\n"
        "   Numpad 9 = Start\n\n"
        "4. Phím tắt:\n"
        "   ctrl + O chọn ROM game\n"
        "   ctrl + F chuyển chế độ 30 hoặc 60FPS\n"
        "   ctrl + M chế độ âm thanh mono\n"
        "   ctrl + shift + M chế độ âm thanh stereo\n"
        "   Tab = Tua nhanh\n"
        "   F1 = Save State Mapper 0\n"
        "   F2 = Load State Mapper 0\n"
        "   F11 = Fullscreen\n"
        "   ctrl + W = xem sóng nes\n"
        "   alt + 5 = xem sóng sunsoft 5B\n"
        "   alt + 6 = xem sóng vrc6\n"
        "   alt + 7 = xem sóng vrc7\n"
        "   alt + 8 = xem sóng mmc5\n"
        "   alt + 9 = xem sóng n163\n"
        "   F10 = bật tắt hiện fps\n"
        "   ctrl + 1 hoặc 2,3,4,5,6 = tắt kênh âm thanh của nes (có thể tắt các kênh khác trong setting)\n\n"
        "5. Debug:\n"
        "   Menu Debug có oscilloscope, Mapper Viewer, Sprite Viewer, palette viewer.\n\n"
        "6. NSF Player:\n"
        "   F3 = previous track\n"
        "   F4 = next track\n"
        "   SPACE = play/pause\n\n"
        "Lưu ý: SJNES là phần mềm giả lập NES đang trong quá trình phát triển, được thực hiện với mục đích học tập và nghiên cứu.\n"
        "Phần mềm không bao gồm, không lưu trữ, không phân phối và không cung cấp liên kết tải ROM, game có bản quyền của Nintendo hoặc bất kỳ bên thứ ba nào."
        "Người dùng chỉ nên sử dụng các ROM homebrew, public domain hoặc các bản sao được tạo hợp pháp từ băng game mà mình sở hữu, tùy theo quy định pháp luật tại nơi sinh sống."
        "SJNES là dự án độc lập, không liên kết, không được tài trợ và không được xác nhận bởi Nintendo."

    );

    QCheckBox* dontShowAgain = new QCheckBox("Không hiện lại lần sau");

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton* okButton = new QPushButton("OK");
    buttonLayout->addWidget(okButton);

    layout->addWidget(title);
    layout->addWidget(text);
    layout->addWidget(dontShowAgain);
    layout->addLayout(buttonLayout);

    QObject::connect(okButton, &QPushButton::clicked, &dialog, [&]() {
        if (dontShowAgain->isChecked())
            settings.setValue("hideQuickStart", true);

        dialog.accept();
        });

    dialog.exec();
}
SJNES::SJNES(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    statusBar()->hide();

    connect(ui.actionrecord, &QAction::triggered, this, &SJNES::onToggleRecording);
    QString biosPath = QCoreApplication::applicationDirPath() + "/disksys.rom";
    emuThread = new QThread(this);
    worker = new EmuWorker();
    worker->nes_cpu.ConnectBus(&worker->nes_bus);
    worker->nes_bus.cpu = &worker->nes_cpu;
    worker->nes_bus.ppu = &worker->nes_ppu;
    worker->moveToThread(emuThread);
    connect(worker, &EmuWorker::frameReady, this, &SJNES::onFrameReady);
    connect(worker, &EmuWorker::audioReady, this, &SJNES::onAudioReady);
    connect(worker, &EmuWorker::debugChannelsReady, this, &SJNES::onDebugChannelsReady);
    connect(worker, &EmuWorker::gameFrameTicked, this, &SJNES::onGameFrameTicked);
    connect(worker, &EmuWorker::realGameFrameTicked, this, &SJNES::onRealGameFrameTicked);
    connect(worker, &EmuWorker::nsfConsoleMessage, this, &SJNES::onNsfConsoleMessage);
    connect(worker, &EmuWorker::nsfTrackChanged, this, &SJNES::onNsfTrackChanged);
    connect(worker, &EmuWorker::nsfExpansionDetected, this, &SJNES::onNsfExpansionDetected);
    connect(worker, &EmuWorker::romLoadResult, this,
        [this](bool ok, QString fileName, QString errorMsg, uint32_t mapperID, uint16_t startPC) {
            if (!ok)
            {
                QMessageBox::warning(this, "Lỗi ROM", errorMsg);
                return;
            }
            g_debugPcLogCount = 0;
            addRecentRom(fileName);
            ui.txtConsole->appendPlainText(">>> DA NAP ROM VA RESET HE THONG: " + fileName);
            ui.txtConsole->appendPlainText(QString("MAPPER ID: %1").arg(mapperID));
            ui.txtConsole->appendPlainText(
                QString("START -> PC: 0x%1").arg(startPC, 4, 16, QChar('0')).toUpper()
            );
            ui.actPause->setText("Dừng (Pause)");
        });

    emuThread->start();
    ui.actSmoothSaw->setCheckable(true);
    ui.actSmoothSaw->setChecked(false);
    worker->nes_bus.n_apu.SetSmoothSaw(false);
    ui.actdmcreverse->setCheckable(true);
    ui.actdmcreverse->setChecked(false);

    // Tạo action bằng code thay vì .ui, vì không muốn đụng vào Qt Designer.
    // "this" làm parent để Qt tự quản lý lifetime (không cần delete tay).
    QAction* actDmcReducePopping = new QAction("Reduce popping sounds on the DMC channel", this);
    actDmcReducePopping->setCheckable(true);
    actDmcReducePopping->setChecked(false);


    connect(ui.actdmcreverse, &QAction::toggled, this, [this](bool checked) {
        worker->setReverseDPCM(checked);
        ui.txtConsole->appendPlainText(
            checked ? "Reverse DPCM Bit Order: ON" : "Reverse DPCM Bit Order: OFF"
        );
        });
    connect(actDmcReducePopping, &QAction::toggled, this, [this](bool checked) {
        worker->setDMCReducePopping(checked);
        ui.txtConsole->appendPlainText(
            checked ? "Reduce DMC Popping: ON" : "Reduce DMC Popping: OFF"
        );
        });
    connect(ui.actEqualizer, &QAction::triggered, this, [this]() {
        if (!eqWindow) {
            eqWindow = new EQWindow(worker, eqGains, eqEnabled, nullptr);
            eqWindow->setAttribute(Qt::WA_DeleteOnClose);
            connect(eqWindow, &QObject::destroyed, this, [this]() { eqWindow = nullptr; });
        }
        connect(eqWindow, &EQWindow::bandGainChanged, this, [this](int band, float gainDB) {
            if (band >= 0 && band < 9) eqGains[band] = gainDB;
            });

        connect(eqWindow, &EQWindow::enabledChanged, this, [this](bool on) {
            eqEnabled = on;
            });
        eqWindow->show();
        eqWindow->raise();
        eqWindow->activateWindow();
        });


    connect(ui.actSmoothSaw, &QAction::toggled, this, [this](bool checked) {
        worker->nes_bus.n_apu.SetSmoothSaw(checked);

        if (worker->nes_bus.cart && worker->nes_bus.cart->pMapper)
        {
            auto vrc6 = dynamic_cast<Mapper_024*>(worker->nes_bus.cart->pMapper.get());
            if (vrc6)
                vrc6->setSmoothSaw(checked);
        }

        worker->setNsfSmoothSaw(checked);
        });
    connect(ui.chkAutoA, &QAction::toggled,
        this, &SJNES::on_chkAutoA_toggled);

    connect(ui.chkAutoB, &QAction::toggled,
        this, &SJNES::on_chkAutoB_toggled);
    connect(ui.actPower,
        &QAction::triggered,
        this,
        &SJNES::on_actPower_triggered);
    connect(ui.actWaveN163, &QAction::triggered, this, [this]() {
        if (n163WaveWindow && n163WaveWindow->parent() != nullptr)
        {
            n163WaveWindow->disconnect();
            n163WaveWindow->close();
            n163WaveWindow = nullptr;
        }
        if (!n163WaveWindow)
        {
            n163WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::N163, nullptr);
            n163WaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            n163WaveWindow->setFixedSize(1152, 648); 

            connect(n163WaveWindow, &QObject::destroyed, this, [this]() {
                n163WaveWindow = nullptr;
                });
        }

        n163WaveWindow->show();
        n163WaveWindow->raise();
        n163WaveWindow->activateWindow();
        });

    connect(ui.actWaveMMC5, &QAction::triggered, this, [this]() {
        if (mmc5WaveWindow && mmc5WaveWindow->parent() != nullptr)
        {
            mmc5WaveWindow->disconnect();
            mmc5WaveWindow->close();
            mmc5WaveWindow = nullptr;
        }
        if (!mmc5WaveWindow)
        {
            mmc5WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::MMC5, nullptr);
            mmc5WaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            mmc5WaveWindow->setFixedSize(1152, 648); // 16:9 cố định, chỉ dùng ở game mode

            connect(mmc5WaveWindow, &QObject::destroyed, this, [this]() {
                mmc5WaveWindow = nullptr;
                });
        }

        mmc5WaveWindow->show();
        mmc5WaveWindow->raise();
        mmc5WaveWindow->activateWindow();
        });

    connect(ui.actWaveFDS, &QAction::triggered, this, [this]() {
        if (fdsWaveWindow && fdsWaveWindow->parent() != nullptr)
        {
            fdsWaveWindow->disconnect();
            fdsWaveWindow->close();
            fdsWaveWindow = nullptr;
        }
        if (!fdsWaveWindow)
        {
            fdsWaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::FDS, nullptr);
            fdsWaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            fdsWaveWindow->setFixedSize(1152, 648); // 16:9 cố định, chỉ dùng ở game mode

            connect(fdsWaveWindow, &QObject::destroyed, this, [this]() {
                fdsWaveWindow = nullptr;
                });
        }

        fdsWaveWindow->show();
        fdsWaveWindow->raise();
        fdsWaveWindow->activateWindow();
        });

    ui.actNES->setShortcut(QKeySequence("Ctrl+W"));
    ui.actNES->setShortcutContext(Qt::ApplicationShortcut);

    connect(ui.actNES, &QAction::triggered, this, [this]() {
        if (nesWaveWindow && nesWaveWindow->parent() != nullptr)
        {
            nesWaveWindow->disconnect();
            nesWaveWindow->close();
            nesWaveWindow = nullptr;
        }

        if (!nesWaveWindow)
        {
            nesWaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::NES, nullptr);
            nesWaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            nesWaveWindow->setFixedSize(1152, 648); // 16:9 cố định, chỉ dùng ở game mode

            connect(nesWaveWindow, &QObject::destroyed, this, [this]() {
                nesWaveWindow = nullptr;
                });
        }

        nesWaveWindow->show();
        nesWaveWindow->raise();
        nesWaveWindow->activateWindow();
        });

    connect(ui.actWaveVRC6, &QAction::triggered, this, [this]() {
        if (vrc6WaveWindow && vrc6WaveWindow->parent() != nullptr)
        {
            vrc6WaveWindow->disconnect();
            vrc6WaveWindow->close();
            vrc6WaveWindow = nullptr;
        }
        if (!vrc6WaveWindow)
        {
            vrc6WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::VRC6, nullptr);
            vrc6WaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            vrc6WaveWindow->setFixedSize(1152, 648); // 16:9 cố định, chỉ dùng ở game mode

            connect(vrc6WaveWindow, &QObject::destroyed, this, [this]() {
                vrc6WaveWindow = nullptr;
                });
        }

        vrc6WaveWindow->show();
        vrc6WaveWindow->raise();
        vrc6WaveWindow->activateWindow();
        });

    connect(ui.actWaveVRC7, &QAction::triggered, this, [this]() {
        if (vrc7WaveWindow && vrc7WaveWindow->parent() != nullptr)
        {
            vrc7WaveWindow->disconnect();
            vrc7WaveWindow->close();
            vrc7WaveWindow = nullptr;
        }
        if (!vrc7WaveWindow)
        {
            vrc7WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::VRC7, nullptr);
            vrc7WaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            vrc7WaveWindow->setFixedSize(1152, 648); // 16:9 cố định, chỉ dùng ở game mode

            connect(vrc7WaveWindow, &QObject::destroyed, this, [this]() {
                vrc7WaveWindow = nullptr;
                });
        }

        vrc7WaveWindow->show();
        vrc7WaveWindow->raise();
        vrc7WaveWindow->activateWindow();
        });

    connect(ui.actWaveS5B, &QAction::triggered, this, [this]() {
        if (s5bWaveWindow && s5bWaveWindow->parent() != nullptr)
        {
            s5bWaveWindow->disconnect();
            s5bWaveWindow->close();
            s5bWaveWindow = nullptr;
        }
        if (!s5bWaveWindow)
        {
            s5bWaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::S5B, nullptr);
            s5bWaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            s5bWaveWindow->setFixedSize(1152, 648); // 16:9 cố định, chỉ dùng ở game mode

            connect(s5bWaveWindow, &QObject::destroyed, this, [this]() {
                s5bWaveWindow = nullptr;
                });
        }

        s5bWaveWindow->show();
        s5bWaveWindow->raise();
        s5bWaveWindow->activateWindow();
        });

    connect(ui.actAboutSJNES, &QAction::triggered, this, [this]() {
        QDialog dialog(this);
        dialog.setWindowTitle("About SJNES");
        dialog.resize(520, 360);

        QVBoxLayout* layout = new QVBoxLayout(&dialog);

        QLabel* title = new QLabel("SJNES - Nintendo Entertainment System Emulator");
        QFont titleFont = title->font();
        titleFont.setPointSize(13);
        titleFont.setBold(true);
        title->setFont(titleFont);

        QTextBrowser* infoText = new QTextBrowser();
        infoText->setOpenExternalLinks(true);

        infoText->setHtml(
            "<h3>SJNES Emulator</h3>"
            "<p><b>phiên bản:</b> 2.3.1(hot fix)</p>"
            "<p><b>lần đầu phát hành:</b> 9/4/2026</p>"
            "<p><b>ngày cập nhật phiên bản mới nhất:5/7/2026</b>"
            "<p><b>Developer:</b> Nguyễn Quyết Chiến,</p>"
            "<p><b>ngôn ngữ lập trình:</b> C++ / Qt / C</p>"
            "<p><b>rom hỗ trợ:</b> .nes, .zip</p>"
            "<p><b>License:</b> "
            "<a href='https://github.com/newbie1412-mmb/SJNES/blob/main/LICENSE'>View License</a>"
            "</p>"
            "<hr>"
            "<p>SJNES là một phần mềm mã nguồn mở giả lập NES(Nintendo Entertainment System) hiện phần mềm này đang trong quá trình phát triển </p>"
        );

        QPushButton* okButton = new QPushButton("OK");

        layout->addWidget(title);
        layout->addWidget(infoText);
        layout->addWidget(okButton);

        connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);

        dialog.exec();
        });

    QTimer::singleShot(300, this, [this]() {
        ShowQuickStartDialog(this);
        });
    initGamepad();
    updateRecentRomMenu();
    qApp->installEventFilter(this);
    fpsTimer.start();

    ui.txtConsole->setReadOnly(true);
    ui.txtConsole->setFocusPolicy(Qt::NoFocus);

    ui.txtConsole->setParent(nullptr);
    ui.txtConsole->setWindowTitle("SJNES Console Log");
    ui.txtConsole->resize(420, 500);
    ui.txtConsole->move(50, 80);
    ui.txtConsole->show();


    restartAudioSink();

    connect(&audioDevices, &QMediaDevices::audioOutputsChanged,
        this, [this]() {
            restartAudioSink();
        });
    // MENU: FILE
    connect(ui.actOpenROM, &QAction::triggered,
        this, &SJNES::onOpenROMClicked);

    connect(ui.actReset, &QAction::triggered,
        this, &SJNES::onResetClicked);

    connect(ui.actPause, &QAction::triggered,
        this, &SJNES::onStepClicked);

    connect(ui.actExit, &QAction::triggered,
        this, &QWidget::close);

    // FDS: File -> Disk -> Disk A / Disk B (lật đĩa).
    // Chạy trên luồng EmuWorker (queued): rút đĩa ra, đợi ~77 frame như Mesen
    // để BIOS thấy $4032 = "không có đĩa", rồi mới cắm mặt mới vào.
    auto swapFdsDisk = [this](int side) {
        QMetaObject::invokeMethod(worker, [this, side]() {
            if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper) return;
            auto* fds = dynamic_cast<Mapper_020*>(worker->nes_bus.cart->pMapper.get());
            if (!fds) return;                               // không phải game FDS
            if (side < 0 || side >= fds->GetSideCount()) return;
            if (fds->GetCurrentSide() == side) return;      // đang ở mặt này rồi

            fds->InsertDisk(-1);                            // rút đĩa
            QTimer::singleShot(1300, worker, [this, side]() {
                if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper) return;
                if (auto* f = dynamic_cast<Mapper_020*>(worker->nes_bus.cart->pMapper.get()))
                    f->InsertDisk(side);                    // cắm mặt mới
                });
            }, Qt::QueuedConnection);
        ui.txtConsole->appendPlainText(QString("FDS: chuyen sang mat %1").arg(side == 0 ? "A" : "B"));
        };
    connect(ui.act_Disk_A, &QAction::triggered, this, [swapFdsDisk]() { swapFdsDisk(0); });
    connect(ui.act_Disk_B, &QAction::triggered, this, [swapFdsDisk]() { swapFdsDisk(1); });

    connect(ui.actQuickStart, &QAction::triggered, this, [this]() {
        ShowQuickStartDialog(this, true);
        });
    // MENU: AUDIO - Mono / Stereo
    ui.actMono->setCheckable(true);
    ui.actStereo->setCheckable(true);

    ui.actStereo->setChecked(is_stereo);
    ui.actMono->setChecked(!is_stereo);

    connect(ui.actStereo, &QAction::triggered, this, [this]() {
        is_stereo = true;
        ui.actStereo->setChecked(true);
        ui.actMono->setChecked(false);
        onStereoToggled(true);
        });

    connect(ui.actMono, &QAction::triggered, this, [this]() {
        is_stereo = false;
        ui.actMono->setChecked(true);
        ui.actStereo->setChecked(false);
        onStereoToggled(false);
        });

    ui.actSmoothTriangle->setCheckable(true);
    ui.actSmoothTriangle->setChecked(false);
    worker->nes_bus.n_apu.SetSmoothTriangle(false);

    connect(ui.actSmoothTriangle, &QAction::toggled, this, [this](bool checked) {
        worker->nes_bus.n_apu.SetSmoothTriangle(checked);
        });

    ui.actScanline->setCheckable(true);
    ui.actScanline->setChecked(false);

    connect(ui.actScanline, &QAction::toggled, this, [this](bool checked) {
        ui.gameScreen->setScanlineEnabled(checked);
        });

    ui.actCrtLite->setCheckable(true);
    ui.actCrtLite->setChecked(false);

    connect(ui.actCrtLite, &QAction::toggled, this, [this](bool checked) {
        ui.gameScreen->setCrtLiteEnabled(checked);

        if (checked) {
            ui.actScanline->setChecked(false);
            ui.gameScreen->setScanlineEnabled(false);
        }
        });

    ui.actPulse1->setCheckable(true);
    ui.actPulse2->setCheckable(true);
    ui.actTriangle->setCheckable(true);
    ui.actNoise->setCheckable(true);
    ui.actDMC->setCheckable(true);
    ui.actVRC6->setCheckable(true);

    ui.actPulse1->setChecked(true);
    ui.actPulse2->setChecked(true);
    ui.actTriangle->setChecked(true);
    ui.actNoise->setChecked(true);
    ui.actDMC->setChecked(true);
    ui.actVRC6->setChecked(true);

    connect(ui.actPulse1, &QAction::toggled, this, [this](bool checked) {
        worker->nes_bus.n_apu.mutePulse1 = !checked;
        });

    connect(ui.actPulse2, &QAction::toggled, this, [this](bool checked) {
        worker->nes_bus.n_apu.mutePulse2 = !checked;
        });

    connect(ui.actTriangle, &QAction::toggled, this, [this](bool checked) {
        worker->nes_bus.n_apu.muteTriangle = !checked;
        });

    connect(ui.actNoise, &QAction::toggled, this, [this](bool checked) {
        worker->nes_bus.n_apu.muteNoise = !checked;
        });

    connect(ui.actDMC, &QAction::toggled, this, [this](bool checked) {
        worker->nes_bus.n_apu.muteDMC = !checked;
        });

    connect(ui.actVRC6, &QAction::toggled, this, [this](bool checked) {
        if (worker->isNsfMode())
        {
            worker->setNsfMuteVRC6(!checked);
            return;
        }

        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
            return;

        if (auto* vrc6 = dynamic_cast<Mapper_024*>(worker->nes_bus.cart->pMapper.get())) {
            vrc6->muteVRC6 = !checked;
        }
        });
    // Zapper (Controller 2)
    actZapperEnabled = new QAction(this);
    actZapperEnabled->setCheckable(true);
    actZapperEnabled->setChecked(false);

    connect(actZapperEnabled, &QAction::toggled, worker, &EmuWorker::setZapperEnabled);

    connect(actZapperEnabled, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            ui.gameScreen->setCursor(Qt::CrossCursor);
        }
        else {
            ui.gameScreen->unsetCursor();
        }
        });

    connect(ui.gameScreen, &GpuScreenWidget::zapperPositionChanged,
        worker, &EmuWorker::setZapperPosition);
    connect(ui.gameScreen, &GpuScreenWidget::zapperTriggerChanged,
        worker, &EmuWorker::setZapperTrigger);
    //Mute riêng từng kênh con VRC6 (Pulse1/Pulse2/Saw)
    // Tạo động bằng code (giống recentRomActions), không cần sửa file .ui.
    actVRC6Pulse1 = new QAction(this);
    actVRC6Pulse2 = new QAction(this);
    actVRC6Saw = new QAction(this);
    actVRC6Pulse1->setCheckable(true);
    actVRC6Pulse2->setCheckable(true);
    actVRC6Saw->setCheckable(true);
    actVRC6Pulse1->setChecked(true);
    actVRC6Pulse2->setChecked(true);
    actVRC6Saw->setChecked(true);

    connect(actVRC6Pulse1, &QAction::toggled, this, [this](bool checked) {
        if (worker->isNsfMode())
        {
            worker->setNsfMuteVRC6Pulse1(!checked);
            return;
        }
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
            return;
        if (auto* vrc6 = dynamic_cast<Mapper_024*>(worker->nes_bus.cart->pMapper.get())) {
            vrc6->muteVRC6Pulse1 = !checked;
        }
        });

    connect(actVRC6Pulse2, &QAction::toggled, this, [this](bool checked) {
        if (worker->isNsfMode())
        {
            worker->setNsfMuteVRC6Pulse2(!checked);
            return;
        }
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
            return;
        if (auto* vrc6 = dynamic_cast<Mapper_024*>(worker->nes_bus.cart->pMapper.get())) {
            vrc6->muteVRC6Pulse2 = !checked;
        }
        });

    connect(actVRC6Saw, &QAction::toggled, this, [this](bool checked) {
        if (worker->isNsfMode())
        {
            worker->setNsfMuteVRC6Saw(!checked);
            return;
        }
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
            return;
        if (auto* vrc6 = dynamic_cast<Mapper_024*>(worker->nes_bus.cart->pMapper.get())) {
            vrc6->muteVRC6Saw = !checked;
        }
        });

    // --- Mute riêng từng kênh S5B (Tone A/B/C) ---
    actS5BToneA = new QAction(this);
    actS5BToneB = new QAction(this);
    actS5BToneC = new QAction(this);
    actS5BToneA->setCheckable(true);
    actS5BToneB->setCheckable(true);
    actS5BToneC->setCheckable(true);
    actS5BToneA->setChecked(true);
    actS5BToneB->setChecked(true);
    actS5BToneC->setChecked(true);

    connect(actS5BToneA, &QAction::toggled, this, [this](bool checked) {
        if (worker->isNsfMode())
        {
            worker->setNsfMuteS5BToneA(!checked);
            return;
        }
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
            return;
        if (auto* s5b = dynamic_cast<Mapper_069*>(worker->nes_bus.cart->pMapper.get())) {
            s5b->muteToneA = !checked;
        }
        });

    connect(actS5BToneB, &QAction::toggled, this, [this](bool checked) {
        if (worker->isNsfMode())
        {
            worker->setNsfMuteS5BToneB(!checked);
            return;
        }
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
            return;
        if (auto* s5b = dynamic_cast<Mapper_069*>(worker->nes_bus.cart->pMapper.get())) {
            s5b->muteToneB = !checked;
        }
        });

    connect(actS5BToneC, &QAction::toggled, this, [this](bool checked) {
        if (worker->isNsfMode())
        {
            worker->setNsfMuteS5BToneC(!checked);
            return;
        }
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
            return;
        if (auto* s5b = dynamic_cast<Mapper_069*>(worker->nes_bus.cart->pMapper.get())) {
            s5b->muteToneC = !checked;
        }
        });

    // --- Mute riêng từng kênh N163 (tối đa 8 kênh) ---
    for (int i = 0; i < 8; i++)
    {
        actN163[i] = new QAction(this);
        actN163[i]->setCheckable(true);
        actN163[i]->setChecked(true);

        connect(actN163[i], &QAction::toggled, this, [this, i](bool checked) {
            if (worker->isNsfMode())
            {
                worker->setNsfMuteN163Channel(i, !checked);
                return;
            }
            if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
                return;
            if (auto* n163 = dynamic_cast<Mapper_019*>(worker->nes_bus.cart->pMapper.get())) {
                n163->SetN163ChannelMuted(i, !checked);
            }
            });
    }

    // --- Chế độ âm thanh N163: 8 kênh ghép qua 1 DAC (chính xác) hay chia trung bình ---
    actN163Accurate = new QAction(this);
    actN163Accurate->setCheckable(true);
    {
        QSettings s("chienz", "NesEmulator");
        const bool accurate = s.value("n163AccurateAudio", false).toBool();
        actN163Accurate->setChecked(accurate);
        // Giữ lựa chọn cho các ROM nạp sau (Mapper_019::reset() đọc giá trị này)
        Mapper_019::accurateAudioDefault = accurate;
    }
    connect(actN163Accurate, &QAction::toggled, this, [this](bool checked) {
        Mapper_019::accurateAudioDefault = checked;

        QSettings s("chienz", "NesEmulator");
        s.setValue("n163AccurateAudio", checked);

        });

    // Áp dụng ngay cho game/NSF đang chạy: slot chạy trên emuThread (queued),
    // xử lý cả mapper 19 của ROM thường lẫn nsfN163 của NSF mode.
    connect(actN163Accurate, &QAction::toggled, worker, &EmuWorker::setN163Accurate);

    // --- Mute riêng từng kênh VRC7 (6 kênh FM) ---
    for (int i = 0; i < 6; i++)
    {
        actVRC7[i] = new QAction(this);
        actVRC7[i]->setCheckable(true);
        actVRC7[i]->setChecked(true);

        connect(actVRC7[i], &QAction::toggled, this, [this, i](bool checked) {
            if (worker->isNsfMode())
            {
                worker->setNsfMuteVRC7Channel(i, !checked);
                return;
            }
            if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
                return;
            if (auto* vrc7 = dynamic_cast<Mapper_085*>(worker->nes_bus.cart->pMapper.get())) {
                vrc7->SetVrc7ChannelMuted(i, !checked);
            }
            });
    }

    // --- Mute riêng từng kênh MMC5 (Pulse1/Pulse2/PCM) ---
    actMMC5Pulse1 = new QAction(this);
    actMMC5Pulse2 = new QAction(this);
    actMMC5PCM = new QAction(this);
    actMMC5Pulse1->setCheckable(true);
    actMMC5Pulse2->setCheckable(true);
    actMMC5PCM->setCheckable(true);
    actMMC5Pulse1->setChecked(true);
    actMMC5Pulse2->setChecked(true);
    actMMC5PCM->setChecked(true);

    connect(actMMC5Pulse1, &QAction::toggled, this, [this](bool checked) {
        if (worker->isNsfMode())
        {
            worker->setNsfMuteMMC5Pulse1(!checked);
            return;
        }
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
            return;
        if (auto* mmc5 = dynamic_cast<Mapper_005*>(worker->nes_bus.cart->pMapper.get())) {
            mmc5->muteMMC5Pulse1 = !checked;
        }
        });

    connect(actMMC5Pulse2, &QAction::toggled, this, [this](bool checked) {
        if (worker->isNsfMode())
        {
            worker->setNsfMuteMMC5Pulse2(!checked);
            return;
        }
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
            return;
        if (auto* mmc5 = dynamic_cast<Mapper_005*>(worker->nes_bus.cart->pMapper.get())) {
            mmc5->muteMMC5Pulse2 = !checked;
        }
        });

    connect(actMMC5PCM, &QAction::toggled, this, [this](bool checked) {
        if (worker->isNsfMode())
        {
            worker->setNsfMuteMMC5PCM(!checked);
            return;
        }
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
            return;
        if (auto* mmc5 = dynamic_cast<Mapper_005*>(worker->nes_bus.cart->pMapper.get())) {
            mmc5->muteMMC5PCM = !checked;
        }
        });

    ui.actPixelPerfect->setCheckable(true);
    ui.actPixelPerfect->setChecked(pixelPerfectMode);

    connect(ui.actPixelPerfect, &QAction::toggled, this, [this](bool checked) {
        pixelPerfectMode = checked;
        resizeEvent(nullptr);
        });

    connect(ui.actMapperViewer, &QAction::triggered, this, [this]() {
        if (!mapperViewerWindow)
        {
            mapperViewerWindow = new MapperViewerWindow(&worker->nes_bus, nullptr);
            mapperViewerWindow->setAttribute(Qt::WA_DeleteOnClose);

            connect(mapperViewerWindow, &QObject::destroyed, this, [this]() {
                mapperViewerWindow = nullptr;
                });
        }

        mapperViewerWindow->show();
        mapperViewerWindow->raise();
        mapperViewerWindow->activateWindow();
        });

    connect(ui.actPaletteViewer, &QAction::triggered, this, [this]() {
        if (!paletteViewerWindow)
        {
            paletteViewerWindow = new PaletteViewerWindow(&worker->nes_ppu, nullptr);
            paletteViewerWindow->setAttribute(Qt::WA_DeleteOnClose);
            connect(paletteViewerWindow, &QObject::destroyed, this, [this]() {
                paletteViewerWindow = nullptr;
                });
        }
        paletteViewerWindow->show();
        paletteViewerWindow->raise();
        paletteViewerWindow->activateWindow();
        });

    connect(ui.actSpriteViewer, &QAction::triggered, this, [this]() {
        if (!spriteViewerWindow)
        {
            spriteViewerWindow = new SpriteViewerWindow(&worker->nes_bus, nullptr);
            spriteViewerWindow->setAttribute(Qt::WA_DeleteOnClose);

            connect(spriteViewerWindow, &QObject::destroyed, this, [this]() {
                spriteViewerWindow = nullptr;
                });
        }

        spriteViewerWindow->show();
        spriteViewerWindow->raise();
        spriteViewerWindow->activateWindow();
        });

    ui.action60fps->setCheckable(true);
    ui.action60fps->setChecked(video60fps);
    worker->setVideo60fps(video60fps);

    connect(ui.action60fps, &QAction::toggled, this, [this](bool checked) {
        video60fps = checked;
        worker->setVideo60fps(checked);
        });

    QActionGroup* overclockGroup = new QActionGroup(this);

    overclockGroup->addAction(ui.actionOverclockOff);
    overclockGroup->addAction(ui.actionOverclock50);
    overclockGroup->addAction(ui.actionOverclock100);
    overclockGroup->addAction(ui.actionOverclock200);
    overclockGroup->addAction(ui.actionOverclock250);
    overclockGroup->setExclusive(true);
    ui.actionOverclockOff->setCheckable(true);
    ui.actionOverclock50->setCheckable(true);
    ui.actionOverclock100->setCheckable(true);
    ui.actionOverclock200->setCheckable(true);
    ui.actionOverclockOff->setChecked(true);
    ui.actOpenROM->setShortcutContext(Qt::ApplicationShortcut);
    ui.actReset->setShortcutContext(Qt::ApplicationShortcut);
    ui.actPause->setShortcutContext(Qt::ApplicationShortcut);

    ui.actMono->setShortcutContext(Qt::ApplicationShortcut);
    ui.actStereo->setShortcutContext(Qt::ApplicationShortcut);

    ui.actPulse1->setShortcutContext(Qt::ApplicationShortcut);
    ui.actPulse2->setShortcutContext(Qt::ApplicationShortcut);
    ui.actTriangle->setShortcutContext(Qt::ApplicationShortcut);
    ui.actNoise->setShortcutContext(Qt::ApplicationShortcut);
    ui.actDMC->setShortcutContext(Qt::ApplicationShortcut);
    ui.actVRC6->setShortcutContext(Qt::ApplicationShortcut);

    ui.actScanline->setShortcutContext(Qt::ApplicationShortcut);
    ui.actCrtLite->setShortcutContext(Qt::ApplicationShortcut);
    ui.actPixelPerfect->setShortcutContext(Qt::ApplicationShortcut);

    ui.actWaveVRC6->setEnabled(true);
    ui.actWaveVRC7->setEnabled(true);
    ui.actWaveS5B->setEnabled(true);
    ui.actWaveMMC5->setEnabled(true);
    ui.actMapperViewer->setShortcutContext(Qt::ApplicationShortcut);
    ui.actSpriteViewer->setShortcutContext(Qt::ApplicationShortcut);

    ui.action60fps->setShortcutContext(Qt::ApplicationShortcut);
    connect(ui.actionOverclockOff, &QAction::triggered, this, [this]() {
        worker->nes_ppu.SetExtraScanlinesBeforeNMI(0);
        ui.txtConsole->appendPlainText("Overclock: OFF");
        });

    connect(ui.actionOverclock50, &QAction::triggered, this, [this]() {
        worker->nes_ppu.SetExtraScanlinesBeforeNMI(50);
        ui.txtConsole->appendPlainText("ép xung cpu thêm 50");
        });

    connect(ui.actionOverclock100, &QAction::triggered, this, [this]() {
        worker->nes_ppu.SetExtraScanlinesBeforeNMI(100);
        ui.txtConsole->appendPlainText("ép xung cpu thêm 100");
        });

    connect(ui.actionOverclock200, &QAction::triggered, this, [this]() {
        worker->nes_ppu.SetExtraScanlinesBeforeNMI(200);
        ui.txtConsole->appendPlainText("ép xung cpu lên 200");
        });

    connect(ui.actionOverclock250, &QAction::triggered, this, [this]() {
        worker->nes_ppu.SetExtraScanlinesBeforeNMI(250);
        ui.txtConsole->appendPlainText("ép xung cpu lên 250");
        });

    ui.actRemoveSpriteLimit->setCheckable(true);
    ui.actRemoveSpriteLimit->setChecked(false);

    connect(ui.actRemoveSpriteLimit, &QAction::toggled, this, [this](bool checked)
        {
            worker->nes_ppu.SetRemoveSpriteLimit(checked);

            ui.txtConsole->appendPlainText(
                checked ? "Remove 8 Sprite Limit: ON" : "Remove 8 Sprite Limit: OFF"
            );
        });

    QShortcut* fpsOverlayShortcut = new QShortcut(QKeySequence("F10"), this);
    fpsOverlayShortcut->setContext(Qt::ApplicationShortcut);

    connect(fpsOverlayShortcut, &QShortcut::activated, this, [this]() {
        // F10: tắt -> FPS -> FPS + CPU/PPU debug -> tắt
        if (!showFpsOverlay) { showFpsOverlay = true; showDebugOverlay = false; }
        else if (!showDebugOverlay) { showDebugOverlay = true; }
        else { showFpsOverlay = false; showDebugOverlay = false; }

        if (!showFpsOverlay)
        {
            ui.gameScreen->setOverlayText("");
            ui.gameScreen->update();
            ui.txtConsole->appendPlainText("FPS Overlay: OFF");
        }
        else
        {
            ui.txtConsole->appendPlainText(showDebugOverlay ? "FPS Overlay: ON + CPU/PPU debug" : "FPS Overlay: ON");
        }
        });

    setFocusPolicy(Qt::StrongFocus);
    ui.gameScreen->setFocusPolicy(Qt::NoFocus);

    QShortcut* fullScreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fullScreenShortcut->setContext(Qt::ApplicationShortcut);
    connect(fullScreenShortcut, &QShortcut::activated, this, &SJNES::toggleGameFullScreen);

    // F9: tách khung NES ra cửa sổ riêng (cho OBS / MSI Afterburner), bấm lại để gộp về
    QShortcut* gameWindowShortcut = new QShortcut(QKeySequence(Qt::Key_F9), this);
    gameWindowShortcut->setContext(Qt::ApplicationShortcut);
    connect(gameWindowShortcut, &QShortcut::activated, this, &SJNES::toggleGameWindow);

    auto addAppShortcut = [this](const QKeySequence& key, QAction* action)
        {
            QShortcut* sc = new QShortcut(key, this);
            sc->setContext(Qt::ApplicationShortcut);

            connect(sc, &QShortcut::activated, this, [action]() {
                if (action)
                    action->trigger();
                });
        };
    // Audio mode
    addAppShortcut(QKeySequence("Ctrl+M"), ui.actMono);
    addAppShortcut(QKeySequence("Ctrl+Shift+M"), ui.actStereo);
    // Audio channels
    addAppShortcut(QKeySequence("Ctrl+1"), ui.actPulse1);
    addAppShortcut(QKeySequence("Ctrl+2"), ui.actPulse2);
    addAppShortcut(QKeySequence("Ctrl+3"), ui.actTriangle);
    addAppShortcut(QKeySequence("Ctrl+4"), ui.actNoise);
    addAppShortcut(QKeySequence("Ctrl+5"), ui.actDMC);
    addAppShortcut(QKeySequence("Ctrl+6"), ui.actVRC6);
    ui.actWaveVRC6->setShortcut(QKeySequence("Alt+6"));
    ui.actWaveVRC7->setShortcut(QKeySequence("Alt+7"));
    ui.actWaveS5B->setShortcut(QKeySequence("Alt+5"));
    ui.actWaveMMC5->setShortcut(QKeySequence("Alt+8"));
    ui.actWaveN163->setShortcut(QKeySequence("Alt+9"));
    ui.actWaveN163->setShortcutContext(Qt::ApplicationShortcut);
    addAppShortcut(QKeySequence("Ctrl+O"), ui.actOpenROM);
    addAppShortcut(QKeySequence("Ctrl+R"), ui.actReset);
    addAppShortcut(QKeySequence("Space"), ui.actPause);
    // FPS
    addAppShortcut(QKeySequence("Ctrl+F"), ui.action60fps);
    //ép xung máy
    addAppShortcut(QKeySequence("Ctrl+7"), ui.actionOverclockOff);
    addAppShortcut(QKeySequence("Ctrl+8"), ui.actionOverclock50);
    addAppShortcut(QKeySequence("Ctrl+9"), ui.actionOverclock100);
    addAppShortcut(QKeySequence("Ctrl+0"), ui.actionOverclock200);
    addAppShortcut(QKeySequence("Ctrl+-"), ui.actionOverclock250);

    // Nút Setting mới: mở SettingsDialog gom hết Video/Audio/Performance/Input +
    // Audio Channel vào 1 cửa sổ, thay vì menu lồng nhau như trước. Non-modal (show,
    // không phải exec) để không chặn tương tác với cửa sổ game/EQ khác khi đang mở.
    // Tab Audio giờ nhúng thẳng EQPanel, nối signal để đồng bộ ngược lại eqGains[]/
    // eqEnabled y hệt cách eqWindow (cửa sổ EQ riêng) vẫn đang làm.
    connect(ui.actOpenSettings, &QAction::triggered, this, [this, actDmcReducePopping]() {
        if (!settingsDialog)
        {
            SettingsDialog::Actions a;
            a.pixelPerfect = ui.actPixelPerfect;
            a.scanline = ui.actScanline;
            a.crtLite = ui.actCrtLite;
            a.video60fps = ui.action60fps;

            a.mono = ui.actMono;
            a.stereo = ui.actStereo;
            a.smoothTriangle = ui.actSmoothTriangle;
            a.smoothSaw = ui.actSmoothSaw;
            a.dmcReverse = ui.actdmcreverse;
            a.dmcReducePopping = actDmcReducePopping;

            a.overclockOff = ui.actionOverclockOff;
            a.overclock50 = ui.actionOverclock50;
            a.overclock100 = ui.actionOverclock100;
            a.overclock200 = ui.actionOverclock200;
            a.overclock250 = ui.actionOverclock250;
            a.removeSpriteLimit = ui.actRemoveSpriteLimit;

            a.autoA = ui.chkAutoA;
            a.autoB = ui.chkAutoB;
            a.zapperEnabled = actZapperEnabled;
            a.n163Accurate = actN163Accurate;

            a.chPulse1 = ui.actPulse1;
            a.chPulse2 = ui.actPulse2;
            a.chTriangle = ui.actTriangle;
            a.chNoise = ui.actNoise;
            a.chDMC = ui.actDMC;

            a.chVRC6Pulse1 = actVRC6Pulse1;
            a.chVRC6Pulse2 = actVRC6Pulse2;
            a.chVRC6Saw = actVRC6Saw;

            a.chS5BToneA = actS5BToneA;
            a.chS5BToneB = actS5BToneB;
            a.chS5BToneC = actS5BToneC;

            for (int i = 0; i < 8; i++)
                a.chN163[i] = actN163[i];

            for (int i = 0; i < 6; i++)
                a.chVRC7[i] = actVRC7[i];

            a.chMMC5Pulse1 = actMMC5Pulse1;
            a.chMMC5Pulse2 = actMMC5Pulse2;
            a.chMMC5PCM = actMMC5PCM;

            settingsDialog = new SettingsDialog(a, worker, eqGains, eqEnabled, channelGains, this);
            settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(settingsDialog, &QObject::destroyed, this, [this]() { settingsDialog = nullptr; });

            connect(settingsDialog, &SettingsDialog::eqBandGainChanged, this, [this](int band, float gainDB) {
                if (band >= 0 && band < 9) eqGains[band] = gainDB;
                });
            connect(settingsDialog, &SettingsDialog::eqEnabledChanged, this, [this](bool on) {
                eqEnabled = on;
                });
            // Lưu lại gain mới nhất vào channelGains (chỉ sống trong RAM, mất khi
            // đóng app) để lần mở SettingsDialog kế tiếp trong cùng phiên chạy
            // hiển thị đúng giá trị đã chỉnh, thay vì luôn hiện lại 100%.
            connect(settingsDialog, &SettingsDialog::channelGainChanged, this,
                [this](QString channelId, float gain) {
                    channelGains[channelId] = gain;
                    worker->setChannelGain(channelId, gain);
                });
        }

        settingsDialog->show();
        settingsDialog->raise();
        settingsDialog->activateWindow();
        });
}


SJNES::~SJNES()
{
    if (worker)
    {
        // worker/timer song tren emuThread - goi thang worker->stop() tu UI
        // thread (destructor chay tren UI thread) khong tu chuyen thread, se
        // race voi emuThread dang co the chay runFrame() cung luc -> hong
        // QTimer noi bo -> crash Qt6Core.dll luc tat app. Dung invokeMethod +
        // BlockingQueuedConnection de stop() thuc su chay tren emuThread.
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
    }

    if (emuThread)
    {
        emuThread->quit();
        emuThread->wait();
    }

    shutdownGamepad();
}

void SJNES::initGamepad()
{
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
    {
        ui.txtConsole->appendPlainText(
            QString("SDL2 init failed: %1").arg(SDL_GetError())
        );
        return;
    }

    ui.txtConsole->appendPlainText("SDL2 init OK");

    int count = SDL_NumJoysticks();

    for (int i = 0; i < count; i++)
    {
        if (SDL_IsGameController(i))
        {
            gameController = SDL_GameControllerOpen(i);

            if (gameController)
            {
                QString name = SDL_GameControllerName(gameController);
                ui.txtConsole->appendPlainText("Gamepad connected: " + name);
                return;
            }
        }
    }

    ui.txtConsole->appendPlainText("No SDL gamepad detected");
}

void SJNES::shutdownGamepad()
{
    if (gameController)
    {
        SDL_GameControllerClose(gameController);
        gameController = nullptr;
    }

    SDL_Quit();
}

void SJNES::updateGamepadInput()
{
    static QElapsedTimer gamepadScanTimer;

    if (!gamepadScanTimer.isValid())
        gamepadScanTimer.start();

    gamepadState1 = 0x00;
    gamepadState2 = 0x00;

    if (!gamepadEnabled)
    {
        return;
    }

    SDL_PumpEvents();

    if (gameController && !SDL_GameControllerGetAttached(gameController))
    {
        ui.txtConsole->appendPlainText("Gamepad disconnected");

        SDL_GameControllerClose(gameController);
        gameController = nullptr;

        gamepadState1 = 0x00;
        gamepadState2 = 0x00;


        return;
    }

    if (!gameController && gamepadScanTimer.elapsed() >= 1000)
    {
        gamepadScanTimer.restart();

        int count = SDL_NumJoysticks();

        for (int i = 0; i < count; i++)
        {
            if (SDL_IsGameController(i))
            {
                gameController = SDL_GameControllerOpen(i);

                if (gameController)
                {
                    QString name = QString::fromUtf8(SDL_GameControllerName(gameController));
                    ui.txtConsole->appendPlainText("Gamepad connected: " + name);
                    break;
                }
            }
        }
    }

    if (!gameController)
    {

        return;
    }

    // 0x80 = A
    // 0x40 = B
    // 0x20 = Select
    // 0x10 = Start
    // 0x08 = Up
    // 0x04 = Down
    // 0x02 = Left
    // 0x01 = Right

    if (SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_A))
        gamepadState1 |= 0x80; // PS4 Cross / Xbox A -> NES A

    if (SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_X))
        gamepadState1 |= 0x40; // PS4 Square / Xbox X -> NES B

    if (SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_BACK))
        gamepadState1 |= 0x20; // PS4 Share / Xbox Back -> Select

    if (SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_START))
        gamepadState1 |= 0x10; // PS4 Options / Xbox Start -> Start

    if (SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_DPAD_UP))
        gamepadState1 |= 0x08;

    if (SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
        gamepadState1 |= 0x04;

    if (SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
        gamepadState1 |= 0x02;

    if (SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        gamepadState1 |= 0x01;

    Sint16 lx = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTX);
    Sint16 ly = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTY);

    const int deadzone = 8000;

    if (lx < -deadzone) gamepadState1 |= 0x02; // Left
    if (lx > deadzone) gamepadState1 |= 0x01; // Right
    if (ly < -deadzone) gamepadState1 |= 0x08; // Up
    if (ly > deadzone) gamepadState1 |= 0x04; // Down


}

static std::vector<uint8_t> LoadNesFromZip(const QString& zipPath)
{
    std::vector<uint8_t> result;

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    QByteArray pathBytes = zipPath.toLocal8Bit();

    if (!mz_zip_reader_init_file(&zip, pathBytes.constData(), 0))
    {
        return result;
    }

    int fileCount = (int)mz_zip_reader_get_num_files(&zip);

    for (int i = 0; i < fileCount; i++)
    {
        mz_zip_archive_file_stat stat;

        if (!mz_zip_reader_file_stat(&zip, i, &stat))
            continue;

        QString fileName = QString::fromUtf8(stat.m_filename);

        if (!fileName.endsWith(".nes", Qt::CaseInsensitive))
            continue;

        size_t uncompSize = 0;

        void* data = mz_zip_reader_extract_to_heap(&zip, i, &uncompSize, 0);

        if (data && uncompSize > 0)
        {
            uint8_t* bytes = (uint8_t*)data;
            result.assign(bytes, bytes + uncompSize);
            mz_free(data);
            break;
        }

        if (data)
            mz_free(data);
    }

    mz_zip_reader_end(&zip);
    return result;
}

void SJNES::loadRomFile(const QString& fileName)
{
    // Rời chế độ NSF -> trả lại khả năng resize tự do như bình thường.
    bool wasNsf = nsfUiActive;
    nsfUiActive = false;
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    // Nếu vừa rời NSF (cửa sổ đang ở khung 16:9), trả cửa sổ về đúng size
    // trước lúc vào NSF -> gameScreen letterbox 4:3 sẽ lấp đầy cửa sổ như cũ,
    // không còn 2 mép trắng 2 bên nữa.
    if (wasNsf && g_preNsfWindowSize.isValid() && !g_preNsfWindowSize.isEmpty())
        resize(g_preNsfWindowSize);

    // Rời chế độ NSF -> hiện lại màn hình game, ẩn hết oscilloscope nhúng.
    ui.gameScreen->show();
    if (nesWaveWindow)  nesWaveWindow->hide();
    if (vrc6WaveWindow) vrc6WaveWindow->hide();
    if (vrc7WaveWindow) vrc7WaveWindow->hide();
    if (s5bWaveWindow)  s5bWaveWindow->hide();
    if (mmc5WaveWindow) mmc5WaveWindow->hide();
    if (n163WaveWindow) n163WaveWindow->hide();
    if (fdsWaveWindow)  fdsWaveWindow->hide();
    for (auto* cw : g_waveClones) if (cw) cw->hide();

    if (fileName.isEmpty())
        return;

    // Toàn bộ việc đọc file + parse Cartridge + reset hệ thống giờ chạy hẳn
    // bên emuThread (xem EmuWorker::loadRom), UI thread trả quyền điều khiển
    // ngay lập tức - không còn đơ lúc chọn ROM xong. Kết quả (thành công hay
    // lỗi) được báo lại qua signal romLoadResult, xử lý trong constructor.
    QMetaObject::invokeMethod(worker, "loadRom", Qt::QueuedConnection,
        Q_ARG(QString, fileName), Q_ARG(bool, ui.actSmoothSaw->isChecked()));
}
void SJNES::onOpenROMClicked()
{
    QSettings settings("chienz", "NesEmulator");
    QString lastPath = settings.value("LastRomPath", "D:\\").toString();
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open ROM",
        lastPath,
        "NES / NSF / FDS Files (*.nes *.zip *.nsf *.fds);;NES Files (*.nes);;ZIP Files (*.zip);;NSF Files (*.nsf);;FDS Disk (*.fds);;All Files (*.*)"
    );

    if (fileName.isEmpty())
        return; // user bam Cancel, khong in log

    qDebug() << "[ROM] Da chon xong ROM:" << fileName;

    if (fileName.endsWith(".nsf", Qt::CaseInsensitive))
    {
        loadNSFFile(fileName);
        return;
    }

    QFileInfo fileInfo(fileName);
    settings.setValue("LastRomPath", fileInfo.absolutePath());

    loadRomFile(fileName);
}

void SJNES::loadNSFFile(const QString& fileName)
{
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        ui.txtConsole->appendPlainText("Không mở được file NSF.");
        return;
    }
    QByteArray qdata = file.readAll();
    std::vector<uint8_t> buffer(qdata.begin(), qdata.end());

    NSFFile nsf;
    std::string error;
    if (!nsf.LoadFromBuffer(buffer, &error)) {
        ui.txtConsole->appendPlainText(QString::fromStdString(error));
        return;
    }

    // NSF không có hình game: gộp khung NES về cửa sổ chính trước
    if (gameWindow)
        gameWindow->close();

    ui.txtConsole->appendPlainText("===== NSF PLAYER MODE =====");

    QMetaObject::invokeMethod(worker, "loadNSF", Qt::QueuedConnection, Q_ARG(NSFFile, nsf));

    isPaused = false;
    ui.actPause->setText("Dừng (Pause)");

    // NSF không có hình game -> chuyển sang khung 16:9 (resizeEvent() sẽ tự
    // letterbox waveArea về đúng 16:9 bên trong bất kể cửa sổ to/nhỏ ra sao).
    // CHỈ resize() 1 LẦN lúc mới bước vào NSF mode (từ game mode chuyển qua) -
    // nếu đã đang ở NSF mode rồi (chọn file NSF khác) thì giữ nguyên size
    // hiện tại người dùng đang để, không ép resize lại mỗi lần đổi file.
    if (!nsfUiActive)
    {
        g_preNsfWindowSize = this->size(); // để trả lại đúng size này lúc rời NSF
        constexpr int NSF_WINDOW_W = 1152;
        constexpr int NSF_WINDOW_H = 648; // 16:9
        resize(NSF_WINDOW_W, NSF_WINDOW_H);
    }
    nsfUiActive = true;
}

void SJNES::onNsfConsoleMessage(QString msg)
{
    ui.txtConsole->appendPlainText(msg);
}

void SJNES::onNsfTrackChanged(int current, int total)
{
    ui.txtConsole->appendPlainText(QString("NSF Track: %1 / %2").arg(current).arg(total));
}

void SJNES::onNsfExpansionDetected(bool vrc6, bool s5b, bool vrc7, bool mmc5, bool n163, bool fds)
{
    // Chỉ lo việc TẠO / ẩn-hiện đúng cửa sổ oscilloscope ở đây. KHÔNG tự tính
    // toán geometry riêng nữa — trước đây có 1 bản copy công thức center ở
    // đây, tính 1 LẦN DUY NHẤT tại thời điểm này (có thể trước khi cửa sổ đã
    // resize/maximize xong), nên dễ bị lệch tâm so với game mode (nơi
    // resizeEvent() luôn tính lại theo kích thước THẬT SỰ hiện tại). Giờ
    // dùng chung đúng 1 nguồn công thức duy nhất: gọi resizeEvent(nullptr)
    // ở cuối hàm để tự căn giữa, y hệt cách game mode đang làm đúng.
    auto ensureEmbedded = [&](AudioWaveWindow*& win, AudioWaveWindow::WaveMode mode, bool showNes = false)
        {
            if (win && win->parent() == nullptr)
            {
                win->disconnect();

                win->close(); // Đóng cửa sổ cũ
                win = nullptr; // Đưa về null để bắt buộc tạo lại dạng nhúng
            }

            if (!win)
            {
                win = new AudioWaveWindow(mode, ui.centralWidget);
                win->setMinimumSize(0, 0);
            }
            // NSF: o chip chi ve kenh cua chip (showNes=false); o NES rieng ve 5 kenh NES.
            win->setShowNesChannels(showNes);
            win->show();
            win->raise();
        };
    if (vrc6) ensureEmbedded(vrc6WaveWindow, AudioWaveWindow::WaveMode::VRC6);
    else if (vrc6WaveWindow) vrc6WaveWindow->hide();

    if (s5b) ensureEmbedded(s5bWaveWindow, AudioWaveWindow::WaveMode::S5B);
    else if (s5bWaveWindow) s5bWaveWindow->hide();

    if (vrc7) ensureEmbedded(vrc7WaveWindow, AudioWaveWindow::WaveMode::VRC7);
    else if (vrc7WaveWindow) vrc7WaveWindow->hide();

    if (mmc5) ensureEmbedded(mmc5WaveWindow, AudioWaveWindow::WaveMode::MMC5);
    else if (mmc5WaveWindow) mmc5WaveWindow->hide();

    if (n163) ensureEmbedded(n163WaveWindow, AudioWaveWindow::WaveMode::N163);
    else if (n163WaveWindow) n163WaveWindow->hide();

    if (fds) ensureEmbedded(fdsWaveWindow, AudioWaveWindow::WaveMode::FDS);
    else if (fdsWaveWindow) fdsWaveWindow->hide();

    // NSF: luon co dung 1 o NES (5 kenh), moi chip trong header NSF them 1 o rieng chi
    // gom kenh cua chip do -> khong con ve lap lai kenh NES o moi o.
    ensureEmbedded(nesWaveWindow, AudioWaveWindow::WaveMode::NES, true);
    ui.gameScreen->hide();

    // Căn giữa TẤT CẢ cửa sổ vừa show/hide ở trên bằng đúng công thức game
    // mode đang dùng đúng, thay vì tính riêng.
    resizeEvent(nullptr);
}

void SJNES::addRecentRom(const QString& filePath)
{
    QSettings settings("chienz", "NesEmulator");

    QStringList recent = settings.value("RecentROMs").toStringList();

    recent.removeAll(filePath);
    recent.prepend(filePath);

    while (recent.size() > 10)
        recent.removeLast();

    settings.setValue("RecentROMs", recent);

    updateRecentRomMenu();
}
void SJNES::updateRecentRomMenu()
{
    QSettings settings("chienz", "NesEmulator");
    QStringList recent = settings.value("RecentROMs").toStringList();

    ui.menuRecentROMs_2->clear();

    for (const QString& path : recent)
    {
        QFileInfo info(path);

        QAction* action = new QAction(info.fileName(), this);
        action->setToolTip(path);

        connect(action, &QAction::triggered, this, [this, path]() {
            loadRomFile(path);
            });

        ui.menuRecentROMs_2->addAction(action);
    }

    if (!recent.isEmpty())
    {
        ui.menuRecentROMs_2->addSeparator();

        QAction* clearAction = new QAction("Clear Recent ROMs", this);

        connect(clearAction, &QAction::triggered, this, [this]() {
            QSettings settings("chienz", "NesEmulator");
            settings.remove("RecentROMs");
            updateRecentRomMenu();

            ui.txtConsole->appendPlainText("Recent ROMs cleared");
            });

        ui.menuRecentROMs_2->addAction(clearAction);
    }

    ui.menuRecentROMs_2->setEnabled(true);
}
void SJNES::onResetClicked()
{
    if (worker->nes_bus.cart == nullptr || worker->nes_bus.cart->pMapper == nullptr) {
        return;
    }

    bool wasRunning = !isPaused;
    QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);


    // HARD RESET / POWER RESET

    // Reset clock hệ thống
    system_clock_counter = 0;
    dma_dummy_counter = 0;
    videoFrameCounter = 0;

    // Clear CPU RAM 2KB
    for (int i = 0; i < 2048; i++)
        worker->nes_bus.ram[i] = 0x00;

    // Reset DMA
    worker->nes_bus.dma_page = 0x00;
    worker->nes_bus.dma_addr = 0x00;
    worker->nes_bus.dma_data = 0x00;
    worker->nes_bus.dma_dummy = true;
    worker->nes_bus.dma_transfer = false;

    // Reset controller
    keyboardState1 = 0x00;
    keyboardState2 = 0x00;
    gamepadState1 = 0x00;
    gamepadState2 = 0x00;

    worker->nes_bus.controller_state = 0x00;
    worker->nes_bus.controller_state2 = 0x00;
    worker->nes_bus.controller_strobe = 0;
    worker->nes_bus.controller_shift = 0;
    worker->nes_bus.controller_shift2 = 0;

    // Reset PPU / APU / Mapper / CPU
    worker->nes_ppu.reset();
    worker->nes_bus.n_apu.reset();

    if (worker->nes_bus.cart->pMapper != nullptr) {
        worker->nes_bus.cart->pMapper->reset();
    }

    worker->nes_cpu.reset();

    ui.txtConsole->appendPlainText(">>> HARD RESET / POWER RESET NES");
    ui.txtConsole->appendPlainText(
        QString("START -> PC: 0x%1")
        .arg(worker->nes_cpu.pc, 4, 16, QChar('0'))
        .toUpper()
    );

    if (wasRunning) {
        QMetaObject::invokeMethod(worker, "start", Qt::BlockingQueuedConnection);
    }
}
void SJNES::onStepClicked() {
    if (!isPaused) {
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        isPaused = true;
        ui.actPause->setText("Chạy (Auto)");
    }
    else {
        QMetaObject::invokeMethod(worker, "start", Qt::BlockingQueuedConnection);
        isPaused = false;
        ui.actPause->setText("Dừng (Pause)");
    }
}
void SJNES::onFrameReady(QImage frame)
{
    videoFpsCounter++;

    // DEBUG TẠM - ghi thẳng ra file (không cần mở Output/Debug trong VS,
    // chạy được cả bản Release). Ghi 1 lần mỗi ~1 giây, tự dừng sau 5 dòng.
    {
        static int dbgCounter = 0;
        static int dbgLinesWritten = 0;
        static int dbgFrameCounter = 0;
        if ((dbgFrameCounter++ % 30) == 0) {
            QRgb px = frame.pixel(128, 50); // toạ độ vùng nền/trời trên title screen
            QFile dbgFile("C:/sjnes_debug.txt");
            if (dbgFile.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&dbgFile);
                out << "frame#" << dbgLinesWritten
                    << " format=" << (int)frame.format()
                    << " pixel(128,50) R=" << qRed(px)
                    << " G=" << qGreen(px)
                    << " B=" << qBlue(px) << "\n";
                out.flush();
                dbgFile.close();
                dbgLinesWritten++;
            }
            else {
                // Nếu không mở được C:\, thử ghi cạnh file .exe luôn
                QFile dbgFile2(QCoreApplication::applicationDirPath() + "/sjnes_debug.txt");
                if (dbgFile2.open(QIODevice::Append | QIODevice::Text)) {
                    QTextStream out2(&dbgFile2);
                    out2 << "frame#" << dbgLinesWritten
                        << " format=" << (int)frame.format()
                        << " pixel(128,50) R=" << qRed(px)
                        << " G=" << qGreen(px)
                        << " B=" << qBlue(px) << "\n";
                    out2.flush();
                    dbgFile2.close();
                    dbgLinesWritten++;
                }
            }
        }
    }

    ui.gameScreen->setFrame(frame);

    if (aviWriter.isRecording()) {
        // FIX màu bị lệch (xanh dương -> xanh lá trong video xuất ra):
        // Format_RGB32 là dạng "giá trị 32-bit" (0xAARRGGBB), thứ tự byte trong bộ nhớ
        // phụ thuộc endianness -> tự parse tay như cũ dễ sai/nhầm lẫn kênh màu.
        // Format_RGBA8888 là byte-order format: Qt đảm bảo tuyệt đối thứ tự byte
        // là R,G,B,A bất kể máy gì -> convert qua đây rồi trích theo đúng thứ tự
        // tường minh sẽ luôn cho màu khớp với những gì hiển thị live (GpuScreenWidget
        // cũng đang convert sang RGBA8888 để lên GPU, nên đây là màu "chuẩn").
        QImage rgba = frame.convertToFormat(QImage::Format_RGBA8888);
        int w = rgba.width();
        int h = rgba.height();

        static thread_local std::vector<uint8_t> packed;
        packed.resize(static_cast<size_t>(w) * h * 3);

        for (int y = 0; y < h; ++y) {
            const uint8_t* src = rgba.constScanLine(y); // byte0=R,1=G,2=B,3=A (cố định, không phụ thuộc endian)
            uint8_t* dst = &packed[static_cast<size_t>(y) * w * 3];
            for (int x = 0; x < w; ++x) {
                // BGR24 chuẩn (ffmpeg -pix_fmt bgr24): byte0=B, byte1=G, byte2=R
                dst[x * 3 + 0] = src[x * 4 + 2]; // B
                dst[x * 3 + 1] = src[x * 4 + 1]; // G
                dst[x * 3 + 2] = src[x * 4 + 0]; // R
            }
        }

        aviWriter.writeVideoFrame(packed.data(), packed.size());
    }
}

void SJNES::onAudioReady(std::vector<float> samples, bool stereo)
{
    if (!audio_sink || !audio_device || samples.empty())
        return;

    audio_device->write(
        reinterpret_cast<const char*>(samples.data()),
        static_cast<qint64>(samples.size() * sizeof(float))
    );

    if (aviWriter.isRecording()) {
        std::vector<int16_t> pcm(samples.size());
        for (size_t i = 0; i < samples.size(); ++i) {
            float s = samples[i];
            if (s > 1.0f) s = 1.0f;
            if (s < -1.0f) s = -1.0f;
            pcm[i] = static_cast<int16_t>(s * 32767.0f);
        }
        aviWriter.writeAudioSamples(pcm.data(), pcm.size());
    }
}

void SJNES::onDebugChannelsReady(std::vector<AudioDebugChannels> dbgBatch)
{
    for (const auto& dbg : dbgBatch)
    {
        if (n163WaveWindow && n163WaveWindow->isVisible()) n163WaveWindow->pushChannels(dbg);
        if (nesWaveWindow && nesWaveWindow->isVisible()) nesWaveWindow->pushChannels(dbg);
        if (vrc6WaveWindow && vrc6WaveWindow->isVisible()) vrc6WaveWindow->pushChannels(dbg);
        if (vrc7WaveWindow && vrc7WaveWindow->isVisible()) vrc7WaveWindow->pushChannels(dbg);
        if (s5bWaveWindow && s5bWaveWindow->isVisible()) s5bWaveWindow->pushChannels(dbg);
        if (mmc5WaveWindow && mmc5WaveWindow->isVisible()) mmc5WaveWindow->pushChannels(dbg);
        if (fdsWaveWindow && fdsWaveWindow->isVisible()) fdsWaveWindow->pushChannels(dbg);
        for (auto* cw : g_waveClones) if (cw && cw->isVisible()) cw->pushChannels(dbg);
    }
}

void SJNES::onRealGameFrameTicked()
{
    gameFpsCounter++;
}

void SJNES::onGameFrameTicked()
{
    const int FPS_UPDATE_MS = 500;
    if (fpsTimer.elapsed() >= FPS_UPDATE_MS)
    {
        double elapsedSec = fpsTimer.elapsed() / 1000.0;
        videoFpsValue = videoFpsCounter / elapsedSec;
        gameFpsValue = gameFpsCounter / elapsedSec;
        videoFpsCounter = 0;
        gameFpsCounter = 0;
        fpsTimer.restart();

        fpsOverlayText = QString("VIDEO %1 FPS\nGAME %2 FPS").arg(videoFpsValue, 0, 'f', 1).arg(gameFpsValue, 0, 'f', 1);
    }

    // Cập nhật overlay MỖI FRAME (không chỉ mỗi 500ms) để PC/PPU chạy real-time
    if (showFpsOverlay)
    {
        QString text = fpsOverlayText;
        if (showDebugOverlay)
        {
            // Chỉ làm tươi dòng debug mỗi DEBUG_REFRESH_MS để mắt đọc được (CPU chạy
            // hàng triệu lệnh/giây nên mỗi frame đọc sẽ ra 1 số khác hẳn).
            constexpr int DEBUG_REFRESH_MS = 250;
            static QElapsedTimer debugRefreshTimer;
            static QString cachedDebugText;
            if (!debugRefreshTimer.isValid() || debugRefreshTimer.elapsed() >= DEBUG_REFRESH_MS)
            {
                cachedDebugText = BuildDebugOverlayText(worker);
                debugRefreshTimer.restart();
            }
            text += "\n" + cachedDebugText;
        }
        ui.gameScreen->setOverlayText(text);
    }

    uint8_t p1 = keyboardState1 | gamepadState1;
    uint8_t p2 = keyboardState2 | gamepadState2;

    worker->setControllerState(p1, p2);

    updateGamepadInput();

    bool needWaveDebug =
        (nesWaveWindow && nesWaveWindow->isVisible()) ||
        (vrc6WaveWindow && vrc6WaveWindow->isVisible()) ||
        (vrc7WaveWindow && vrc7WaveWindow->isVisible()) ||
        (s5bWaveWindow && s5bWaveWindow->isVisible()) ||
        (mmc5WaveWindow && mmc5WaveWindow->isVisible()) ||
        (n163WaveWindow && n163WaveWindow->isVisible()) ||
        (fdsWaveWindow && fdsWaveWindow->isVisible());

    bool needOutputAudio = !fastForward && audio_sink != nullptr && audio_device != nullptr;

    worker->setNeedAudioMix(needOutputAudio, needWaveDebug);
}
void SJNES::onStereoToggled(bool checked)
{
    is_stereo = checked;
    ui.actStereo->setChecked(checked);
    ui.actMono->setChecked(!checked);

    worker->setStereo(checked);
    restartAudioSink();
}
void SJNES::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    // Lấy đúng không gian khả dụng của widget trung tâm (đã tự động trừ Menu Bar)
    int availableW = ui.centralWidget->width();
    int availableH = ui.centralWidget->height();

    if (availableW < 256) availableW = 256;
    if (availableH < 240) availableH = 240;

    int screenW = 0;
    int screenH = 0;

    if (nsfUiActive)
    {
        // NSF không có hình game thật để giữ tỷ lệ 256:240 -> dùng 16:9 cho
        // khung oscilloscope, đẹp và tận dụng không gian ngang tốt hơn hẳn.
        screenH = availableH;
        screenW = screenH * 16 / 9;

        if (screenW > availableW) {
            screenW = availableW;
            screenH = screenW * 9 / 16;
        }
    }
    else if (pixelPerfectMode)
    {
        int scaleX = availableW / 256;
        int scaleY = availableH / 240;
        int scale = std::min(scaleX, scaleY);

        if (scale < 1) scale = 1;

        screenW = 256 * scale;
        screenH = 240 * scale;
    }
    else
    {
        screenH = availableH;
        screenW = screenH * 256 / 240;

        if (screenW > availableW) {
            screenW = availableW;
            screenH = screenW * 240 / 256;
        }
    }

    int x = (availableW - screenW) / 2;
    int y = (availableH - screenH) / 2;

    if (!gameWindow)   // đang ở cửa sổ game riêng thì GameWindow tự lo geometry
        ui.gameScreen->setGeometry(x, y, screenW, screenH);

    QRect waveArea(x, y, screenW, screenH);

    AudioWaveWindow* embeddedWindows[] = {
        nesWaveWindow, vrc6WaveWindow, vrc7WaveWindow, s5bWaveWindow,
        mmc5WaveWindow, n163WaveWindow, fdsWaveWindow
    };

    // NSF nhieu expansion: moi chip 1 cua so scope. Truoc day tat ca bi setGeometry
    // cung 1 waveArea -> chong len nhau, chi thay cua so tren cung. Gio chia luoi.
    std::vector<AudioWaveWindow*> visibleWaves;
    for (auto* w : embeddedWindows)
    {
        if (w && w->isVisible() && w->parent() != nullptr)
            visibleWaves.push_back(w);
    }

    const int waveCount = static_cast<int>(visibleWaves.size());
    if (waveCount > 0)
    {
        // Xep TOAN BO kenh cua moi chip lien tuc thanh luoi 2 cot (khong chua o trong giua cac chip).
        // Moi cua so chip duoc cat thanh cac khoi hinh chu nhat KHONG chong nhau (cua so OpenGL chong nhau
        // khong trong suot duoc):
        //   - khoi dau  (1 kenh, nua phai)  khi chip bat dau o cot phai cua mot hang dang do,
        //   - khoi than (cac cap kenh, full chieu rong),
        //   - khoi duoi (1 kenh, nua trai)  khi con le 1 kenh.
        // Khoi dau tien cua chip dung chinh cua so chip, cac khoi con lai la ban sao (g_waveClones).
        // Chi o cuoi cung cua ca luoi duoc phep trong khi tong so kenh le.
        QVector<int> chCount(waveCount), loIdx(waveCount), tileOf(waveCount);
        int totalCh = 0;
        for (int i = 0; i < waveCount; ++i)
        {
            chCount[i] = visibleWaves[i]->totalChannelCount();
            loIdx[i] = visibleWaves[i]->firstShownChannel();
            totalCh += chCount[i];
        }

        // Qua nhieu hang -> chia 2 cot lon (cac cua so lien tiep chia can bang theo so kenh).
        const int tileCols = (((totalCh + 1) / 2) > 12) ? 2 : 1;
        const int halfCh = (totalCh + 1) / 2;
        int accCh = 0;
        int chInTile[2] = { 0, 0 };
        for (int i = 0; i < waveCount; ++i)
        {
            tileOf[i] = (tileCols == 1 || accCh < halfCh) ? 0 : 1;
            accCh += chCount[i];
            chInTile[tileOf[i]] += chCount[i];
        }

        const int cellW = waveArea.width() / tileCols;
        int rowH[2] = { 1, 1 };
        for (int t = 0; t < 2; ++t)
        {
            const int rowsInTile = std::max(1, (chInTile[t] + 1) / 2);
            rowH[t] = std::max(1, waveArea.height() / rowsInTile);
        }

        struct Seg { int win; int first; int last; int row; int rows; int tile; int kind; bool fill; };
        // kind: 0 = than (full chieu rong), 1 = dau (nua phai), 2 = duoi (nua trai)
        QVector<Seg> segs;
        int placed[2] = { 0, 0 };   // so kenh da xep trong tung cot lon
        for (int i = 0; i < waveCount; ++i)
        {
            const int t = tileOf[i];
            int g = placed[t];
            int idx = loIdx[i];
            int rem = chCount[i];

            if (rem > 0 && (g % 2) == 1)
            {
                segs.push_back(Seg{ i, idx, idx, g / 2, 1, t, 1, false });
                ++g; ++idx; --rem;
            }
            if (rem >= 2)
            {
                const int pairs = rem / 2;
                segs.push_back(Seg{ i, idx, idx + pairs * 2 - 1, g / 2, pairs, t, 0, false });
                g += pairs * 2; idx += pairs * 2; rem -= pairs * 2;
            }
            if (rem == 1)
            {
                const bool fill = (g + 1 == chInTile[t]) && (chInTile[t] % 2 == 1);
                // o cuoi cung cua ca luoi (tong le): cho ca hang, o phai de trong nhung van co nen den + truc
                segs.push_back(Seg{ i, idx, idx, g / 2, 1, t, fill ? 0 : 2, fill });
                ++g; ++idx; --rem;
            }
            placed[t] = g;
        }

        // Chu ky cac khoi phu (ban sao): doi ke hoach -> tao lai, giu nguyen -> dung lai.
        QString sig;
        QVector<bool> winHasMain(waveCount, false);
        for (const Seg& s : segs)
        {
            if (!winHasMain[s.win]) { winHasMain[s.win] = true; continue; }
            AudioWaveWindow* src = visibleWaves[s.win];
            sig += QString("%1:%2:%3:%4;").arg(int(src->waveMode())).arg(src->showsNesChannels() ? 1 : 0).arg(s.first).arg(s.last);
        }
        if (sig != g_waveClonesSig)
        {
            for (auto* cw : g_waveClones)
            {
                if (cw) { cw->hide(); cw->deleteLater(); }
            }
            g_waveClones.clear();
            g_waveClonesSig = sig;

            QVector<bool> mainUsed(waveCount, false);
            for (const Seg& s : segs)
            {
                if (!mainUsed[s.win]) { mainUsed[s.win] = true; continue; }
                AudioWaveWindow* src = visibleWaves[s.win];
                AudioWaveWindow* cw = new AudioWaveWindow(src->waveMode(), ui.centralWidget);
                cw->setMinimumSize(0, 0);
                cw->setShowNesChannels(src->showsNesChannels());
                g_waveClones.push_back(cw);
            }
        }

        QVector<bool> mainAssigned(waveCount, false);
        int cloneIdx = 0;
        for (const Seg& s : segs)
        {
            AudioWaveWindow* sw = nullptr;
            if (!mainAssigned[s.win]) { mainAssigned[s.win] = true; sw = visibleWaves[s.win]; }
            else if (cloneIdx < g_waveClones.size()) sw = g_waveClones[cloneIdx++];
            if (!sw) continue;

            const int t = s.tile;
            const int x0 = waveArea.x() + t * cellW;
            int sx = x0, sWidth = cellW;
            if (s.kind == 1) { sx = x0 + cellW / 2; sWidth = cellW - cellW / 2; }
            else if (s.kind == 2) { sWidth = cellW / 2; }

            sw->setChannelRange(s.first, s.last);
            sw->setTileLayout(s.kind == 1 ? 1 : 0, s.kind == 0 && totalCh > 1, s.fill);
            sw->setGeometry(sx, waveArea.y() + s.row * rowH[t], sWidth, s.rows * rowH[t]);
            sw->show();
        }
    }
}

bool SJNES::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Tab)
        {
            fastForward = true;
            worker->setFastForward(true, fastForwardMultiplier);
            return true;
        }
    }

    if (event->type() == QEvent::KeyRelease)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Tab)
        {
            fastForward = false;
            worker->setFastForward(false, fastForwardMultiplier);
            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}
// XỬ LÝ PHÍM BẤM
void SJNES::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    if (worker->nes_bus.nsfMode && event->key() == Qt::Key_F3)
    {
        previousNSFTrack();
        return;
    }

    if (worker->nes_bus.nsfMode && event->key() == Qt::Key_F4)
    {
        nextNSFTrack();
        return;
    }
    if (event->key() == Qt::Key_F1)
    {
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
        {
            ui.txtConsole->appendPlainText("Save State failed: chua nap ROM");
            return;
        }

        QString path = QCoreApplication::applicationDirPath() + "/slot1.sjs";

        if (saveStateMapper0(path))
            ui.txtConsole->appendPlainText("Save State OK: F1 -> slot1.sjs");
        else
            ui.txtConsole->appendPlainText("Save State failed");

        return;
    }

    if (event->key() == Qt::Key_F2)
    {
        if (!worker->nes_bus.cart || !worker->nes_bus.cart->pMapper)
        {
            ui.txtConsole->appendPlainText("Load State failed: chua nap ROM");
            return;
        }

        QString path = QCoreApplication::applicationDirPath() + "/slot1.sjs";

        bool wasRunning = !isPaused;
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);

        if (loadStateMapper0(path))
            ui.txtConsole->appendPlainText("Load State OK: F2 <- slot1.sjs");
        else
            ui.txtConsole->appendPlainText("Load State failed");

        if (wasRunning)
            QMetaObject::invokeMethod(worker, "start", Qt::BlockingQueuedConnection);

        return;
    }

    if (event->key() == Qt::Key_Tab)
    {
        fastForward = true;
        worker->setFastForward(true, fastForwardMultiplier);
        return;
    }

    bool isNumpad = event->modifiers() & Qt::KeypadModifier;

    // PLAYER 1
    if (event->key() == Qt::Key_K)      keyboardState1 |= 0x80; // A
    if (event->key() == Qt::Key_J)      keyboardState1 |= 0x40; // B
    if (event->key() == Qt::Key_U)      keyboardState1 |= 0x20; // Select
    if (event->key() == Qt::Key_Return) keyboardState1 |= 0x10; // Start
    if (event->key() == Qt::Key_W)      keyboardState1 |= 0x08; // Up
    if (event->key() == Qt::Key_S)      keyboardState1 |= 0x04; // Down
    if (event->key() == Qt::Key_A)      keyboardState1 |= 0x02; // Left
    if (event->key() == Qt::Key_D)      keyboardState1 |= 0x01; // Right

    // PLAYER 2 - hướng bằng phím mũi tên
    if (event->key() == Qt::Key_Up)     keyboardState2 |= 0x08; // Up
    if (event->key() == Qt::Key_Down)   keyboardState2 |= 0x04; // Down
    if (event->key() == Qt::Key_Left)   keyboardState2 |= 0x02; // Left
    if (event->key() == Qt::Key_Right)  keyboardState2 |= 0x01; // Right

    // PLAYER 2 - nút bằng numpad
    if (isNumpad && event->key() == Qt::Key_6) keyboardState2 |= 0x80; // A
    if (isNumpad && event->key() == Qt::Key_5) keyboardState2 |= 0x40; // B
    if (isNumpad && event->key() == Qt::Key_7) keyboardState2 |= 0x20; // Select
    if (isNumpad && event->key() == Qt::Key_9) keyboardState2 |= 0x10; // Start
}

void SJNES::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    if (event->key() == Qt::Key_Tab)
    {
        fastForward = false;
        worker->setFastForward(false, fastForwardMultiplier);
        return;
    }

    bool isNumpad = event->modifiers() & Qt::KeypadModifier;

    // PLAYER 1
    if (event->key() == Qt::Key_K)      keyboardState1 &= ~0x80;
    if (event->key() == Qt::Key_J)      keyboardState1 &= ~0x40;
    if (event->key() == Qt::Key_U)      keyboardState1 &= ~0x20;
    if (event->key() == Qt::Key_Return) keyboardState1 &= ~0x10;
    if (event->key() == Qt::Key_W)      keyboardState1 &= ~0x08;
    if (event->key() == Qt::Key_S)      keyboardState1 &= ~0x04;
    if (event->key() == Qt::Key_A)      keyboardState1 &= ~0x02;
    if (event->key() == Qt::Key_D)      keyboardState1 &= ~0x01;

    // PLAYER 2 - hướng
    if (event->key() == Qt::Key_Up)     keyboardState2 &= ~0x08;
    if (event->key() == Qt::Key_Down)   keyboardState2 &= ~0x04;
    if (event->key() == Qt::Key_Left)   keyboardState2 &= ~0x02;
    if (event->key() == Qt::Key_Right)  keyboardState2 &= ~0x01;

    // PLAYER 2 - numpad
    if (isNumpad && event->key() == Qt::Key_6) keyboardState2 &= ~0x80;
    if (isNumpad && event->key() == Qt::Key_5) keyboardState2 &= ~0x40;
    if (isNumpad && event->key() == Qt::Key_7) keyboardState2 &= ~0x20;
    if (isNumpad && event->key() == Qt::Key_9) keyboardState2 &= ~0x10;


}


void SJNES::enterGameFullScreen()
{
    gameFullScreen = true;
    showFullScreen();
    menuBar()->hide();
    statusBar()->hide();
    if (ui.txtConsole)
        ui.txtConsole->hide();
    if (centralWidget())
    {
        centralWidget()->setContentsMargins(0, 0, 0, 0);
        centralWidget()->setStyleSheet("background-color: black;");
    }

    if (centralWidget() && centralWidget()->layout())
    {
        centralWidget()->layout()->setContentsMargins(0, 0, 0, 0);
        centralWidget()->layout()->setSpacing(0);
    }
    ui.gameScreen->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui.gameScreen->setMinimumSize(0, 0);
    activateWindow();
    raise();
    setFocus();
}

void SJNES::exitGameFullScreen()
{
    gameFullScreen = false;
    menuBar()->show();

    if (ui.txtConsole)
        ui.txtConsole->show();

    if (centralWidget())
    {
        centralWidget()->setStyleSheet("");
    }
    if (centralWidget() && centralWidget()->layout())
    {
        centralWidget()->layout()->setContentsMargins(9, 9, 9, 9);
        centralWidget()->layout()->setSpacing(6);
    }
    showNormal();
    activateWindow();
    raise();
    setFocus();
}

void SJNES::toggleGameFullScreen()
{
    // Đang dùng cửa sổ game riêng: F11 fullscreen cho chính cửa sổ đó
    if (gameWindow)
    {
        if (gameWindow->isFullScreen())
            gameWindow->showNormal();
        else
            gameWindow->showFullScreen();
        return;
    }

    if (gameFullScreen)
        exitGameFullScreen();
    else
        enterGameFullScreen();
}

void SJNES::toggleGameWindow()
{
    // Đang mở -> đóng (onClosing sẽ trả gameScreen về cửa sổ chính)
    if (gameWindow)
    {
        gameWindow->close();
        return;
    }

    if (nsfUiActive)
        return; // NSF mode không có hình game

    if (gameFullScreen)
        exitGameFullScreen();

    QSize sz = ui.gameScreen->size();
    if (sz.isEmpty() || sz.width() < 256 || sz.height() < 240)
        sz = QSize(768, 720); // 3x

    auto* win = new GameWindow(ui.gameScreen);
    gameWindow = win;

    win->onKeyPress = [this](QKeyEvent* e) { keyPressEvent(e); };
    win->onKeyRelease = [this](QKeyEvent* e) { keyReleaseEvent(e); };
    win->onClosing = [this]() {
        ui.gameScreen->setParent(ui.centralWidget);
        ui.gameScreen->show();
        gameWindow = nullptr;
        resizeEvent(nullptr);   // căn lại gameScreen trong cửa sổ chính
        raise();
        activateWindow();
        setFocus();
        };

    ui.gameScreen->setParent(win);
    win->resize(sz.height() * 256 / 240, sz.height());   // đúng tỉ lệ 256:240
    ui.gameScreen->setGeometry(0, 0, win->width(), win->height());
    ui.gameScreen->show();

    win->move(x() + 40, y() + 40);
    win->show();
    win->raise();
    win->activateWindow();
    win->setFocus();
}

void SJNES::restartAudioSink()
{
    if (audio_sink)
    {
        audio_sink->stop();
        audio_device = nullptr;
        delete audio_sink;
        audio_sink = nullptr;
        audio_device = nullptr;
    }

    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    int actualRate = outputDevice.preferredFormat().sampleRate();
    if (actualRate <= 0) actualRate = 44100; 

    QAudioFormat format;
    format.setSampleRate(actualRate);
    format.setChannelCount(is_stereo ? 2 : 1);
    format.setSampleFormat(QAudioFormat::Float);

    audio_sink = new QAudioSink(outputDevice, format, this);
    audio_sink->setBufferSize(32768);

    audio_device = audio_sink->start();
    if (worker)
    {
        QMetaObject::invokeMethod(worker, "setOutputSampleRate", Qt::QueuedConnection,
            Q_ARG(double, (double)actualRate));
    }

    connect(audio_sink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::StoppedState && audio_sink && audio_sink->error() != QAudio::NoError
            && !audioRestartPending)
        {
            audioRestartPending = true;
            QTimer::singleShot(0, this, [this]() {
                audioRestartPending = false;
                restartAudioSink();
                });
        }
        });

    ui.txtConsole->appendPlainText(
        "Audio output: " + outputDevice.description()
    );
    g_lastKnownAudioDeviceRate = actualRate;
    static QTimer* audioRateWatchdog = nullptr;
    if (!audioRateWatchdog)
    {
        audioRateWatchdog = new QTimer(this);
        connect(audioRateWatchdog, &QTimer::timeout, this, [this]() {
            if (audioRestartPending) return;
            QAudioDevice liveDevice = QMediaDevices::defaultAudioOutput();
            int liveRate = liveDevice.preferredFormat().sampleRate();
            if (liveRate > 0 && liveRate != g_lastKnownAudioDeviceRate)
            {
                audioRestartPending = true;
                QTimer::singleShot(0, this, [this]() {
                    audioRestartPending = false;
                    restartAudioSink();
                    });
            }
            });
        audioRateWatchdog->start(1000);
    }
}


void SJNES::restartCurrentNSFTrack()
{
    QMetaObject::invokeMethod(worker, "restartCurrentNSFTrack", Qt::QueuedConnection);
}
void SJNES::nextNSFTrack()
{
    QMetaObject::invokeMethod(worker, "nsfNextTrack", Qt::QueuedConnection);
}
void SJNES::previousNSFTrack()
{
    QMetaObject::invokeMethod(worker, "nsfPrevTrack", Qt::QueuedConnection);
}
bool SJNES::saveStateMapper0(const QString& path)
{
    BinaryWriter out;

    out.writeRaw("SJST", 4);

    uint32_t version = 1;
    uint32_t mapperID = worker->nes_bus.cart ? worker->nes_bus.cart->nMapperID : 999;
    out << version << mapperID;

    out << system_clock_counter << dma_dummy_counter << videoFrameCounter;

    for (int i = 0; i < 2048; i++)
        out << worker->nes_bus.ram[i];

    out << worker->nes_bus.dma_page << worker->nes_bus.dma_addr << worker->nes_bus.dma_data
        << worker->nes_bus.dma_dummy << worker->nes_bus.dma_transfer;

    out << keyboardState1 << keyboardState2 << gamepadState1 << gamepadState2;

    out << worker->nes_bus.controller_state << worker->nes_bus.controller_state2
        << worker->nes_bus.controller_strobe << worker->nes_bus.controller_shift
        << worker->nes_bus.controller_shift2;

    worker->nes_cpu.SaveState(out);
    worker->nes_ppu.SaveState(out);

    if (worker->nes_bus.cart) {
        worker->nes_bus.cart->SaveState(out);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(reinterpret_cast<const char*>(out.buffer.data()), out.buffer.size());
    return true;
}

bool SJNES::loadStateMapper0(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray raw = file.readAll();
    BinaryReader in(reinterpret_cast<const uint8_t*>(raw.constData()), raw.size());

    char magic[4];
    if (!in.readRaw(magic, 4) || memcmp(magic, "SJST", 4) != 0)
        return false;

    uint32_t version = 0, mapperID = 999;
    in >> version >> mapperID;

    if (version != 1)
        return false;

    if (!worker->nes_bus.cart || worker->nes_bus.cart->nMapperID != mapperID)
        return false;

    in >> system_clock_counter >> dma_dummy_counter >> videoFrameCounter;

    for (int i = 0; i < 2048; i++)
        in >> worker->nes_bus.ram[i];

    in >> worker->nes_bus.dma_page >> worker->nes_bus.dma_addr >> worker->nes_bus.dma_data
        >> worker->nes_bus.dma_dummy >> worker->nes_bus.dma_transfer;

    in >> keyboardState1 >> keyboardState2 >> gamepadState1 >> gamepadState2;

    in >> worker->nes_bus.controller_state >> worker->nes_bus.controller_state2
        >> worker->nes_bus.controller_strobe >> worker->nes_bus.controller_shift
        >> worker->nes_bus.controller_shift2;

    worker->nes_cpu.LoadState(in);
    worker->nes_ppu.LoadState(in);

    if (worker->nes_bus.cart) {
        worker->nes_bus.cart->LoadState(in);
    }

    worker->nes_bus.n_apu.reset();

    return in.ok;
}
void SJNES::closeEvent(QCloseEvent* event)
{
    if (worker && emuThread && emuThread->isRunning())
        QMetaObject::invokeMethod(worker, "saveFdsDisk", Qt::BlockingQueuedConnection);

    if (gameWindow)
        gameWindow->close();

    if (timer)
        timer->stop();
    if (nesWaveWindow)
    {
        nesWaveWindow->close();
        nesWaveWindow = nullptr;
    }

    if (mmc5WaveWindow)
    {
        mmc5WaveWindow->close();
        mmc5WaveWindow = nullptr;
    }
    if (vrc6WaveWindow)
    {
        vrc6WaveWindow->close();
        vrc6WaveWindow = nullptr;
    }

    if (vrc7WaveWindow)
    {
        vrc7WaveWindow->close();
        vrc7WaveWindow = nullptr;
    }

    if (s5bWaveWindow)
    {
        s5bWaveWindow->close();
        s5bWaveWindow = nullptr;
    }

    if (n163WaveWindow)
    {
        n163WaveWindow->close();
        n163WaveWindow = nullptr;
    }

    if (fdsWaveWindow)
    {
        fdsWaveWindow->close();
        fdsWaveWindow = nullptr;
    }

    if (mapperViewerWindow)
    {
        mapperViewerWindow->close();
        mapperViewerWindow = nullptr;
    }

    if (spriteViewerWindow)
    {
        spriteViewerWindow->close();
        spriteViewerWindow = nullptr;
    }
    if (settingsDialog)
    {
        settingsDialog->close(); settingsDialog = nullptr;
    }

    if (eqWindow)
    {
        eqWindow->close(); eqWindow = nullptr;
    }

    if (ui.txtConsole)
    {
        ui.txtConsole->close();
    }

    QMainWindow::closeEvent(event);
}
void SJNES::on_actPower_triggered()
{
    machinePowered = !machinePowered;
    worker->setPowered(machinePowered);

    if (machinePowered)
    {
        restartAudioSink();
        onResetClicked();
        QMetaObject::invokeMethod(worker, "start", Qt::BlockingQueuedConnection);
        ui.actPower->setText("Power Off");
        ui.txtConsole->appendPlainText("Power ON");
    }
    else
    {
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        if (audio_sink)
            audio_sink->stop();
        QImage black(256, 240, QImage::Format_RGB32);
        black.fill(Qt::black);
        ui.gameScreen->setFrame(black);
        ui.actPower->setText("Power On");
        ui.txtConsole->appendPlainText("Power OFF");
    }
}
void SJNES::on_chkAutoA_toggled(bool checked)
{
    worker->setTurboA(checked);
}

void SJNES::on_chkAutoB_toggled(bool checked)
{
    worker->setTurboB(checked);
}

void SJNES::onToggleRecording() {
    if (!aviWriter.isRecording()) {
        QString path = QFileDialog::getSaveFileName(this, "Lưu video", "", "MP4 Files (*.mp4)");
        if (path.isEmpty()) return;
        if (!path.endsWith(".mp4", Qt::CaseInsensitive))
            path += ".mp4";

        int w = 256, h = 240;      // sửa lại nếu PPU output khác kích thước
        int fps = 60;
        int sampleRate = g_lastKnownAudioDeviceRate > 0 ? g_lastKnownAudioDeviceRate : 44100; // rate thật của audio sink
        int channels = is_stereo ? 2 : 1; // khớp đúng với QAudioFormat thật đang dùng
        if (!aviWriter.start(path, w, h, fps, sampleRate, channels)) {
            QMessageBox::warning(this, "Không quay được", aviWriter.lastError());
            return;
        }

        ui.actionrecord->setText("Dừng quay ");
    }
    else {
        // stop() không chặn UI: ffmpeg hoàn tất file ở nền, xong thì callback báo vào console.
        aviWriter.setOnFinished([this](bool ok, const QString& p) {
            ui.txtConsole->appendPlainText(ok ? "Đã lưu video: " + p
                : "Xuất video thất bại: " + p);
            });
        aviWriter.stop();
        ui.txtConsole->appendPlainText("Đang xuất MP4 ở nền, vui lòng chờ...");
        ui.actionrecord->setText("Ghi video");
    }
}