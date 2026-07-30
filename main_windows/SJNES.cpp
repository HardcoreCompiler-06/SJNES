#include "SJNES.h"
#include <cstdint>
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
#include <QDir>
#include <QFile>
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
#include <QTextBrowser>
#include <QFileInfo>
#include <QElapsedTimer>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "EQWindow.h"
#include "BinaryIO.h"
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
        "phần mềm được code bởi Nguyễn chiến và sự hỗ trợ từ Nguyễn Đức An, Phạm Đăng Hoàn. "
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
        "   ctrl + F mở khóa 60FPS\n"
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
        "   ctrl + 1 hoặc 2,3,4,5,6 = tắt các âm sóng (hiện chưa hỗ trợ tắt sóng vrc7 và S5B)\n\n"
        "5. Debug:\n"
        "   Menu Debug có Audio Waveform, Mapper Viewer, Sprite Viewer và Audio Channel Debug.\n\n"
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
    emuThread = new QThread(this);
    worker = new EmuWorker();
    worker->nes_cpu.ConnectBus(&worker->nes_bus);
    worker->nes_bus.cpu = &worker->nes_cpu;
    worker->nes_bus.ppu = &worker->nes_ppu;
    worker->moveToThread(emuThread);
    connect(emuThread, &QThread::started, worker, &EmuWorker::start);
    connect(worker, &EmuWorker::frameReady, this, &SJNES::onFrameReady);
    connect(worker, &EmuWorker::audioReady, this, &SJNES::onAudioReady);
    connect(worker, &EmuWorker::debugChannelsReady, this, &SJNES::onDebugChannelsReady);
    connect(worker, &EmuWorker::gameFrameTicked, this, &SJNES::onGameFrameTicked);
    connect(worker, &EmuWorker::nsfConsoleMessage, this, &SJNES::onNsfConsoleMessage);
    connect(worker, &EmuWorker::nsfTrackChanged, this, &SJNES::onNsfTrackChanged);
    connect(worker, &EmuWorker::nsfExpansionDetected, this, &SJNES::onNsfExpansionDetected);
    emuThread->start();
    ui.actSmoothSaw->setCheckable(true);
    ui.actSmoothSaw->setChecked(false);
    worker->nes_bus.n_apu.SetSmoothSaw(false);

    ui.actdmcreverse->setCheckable(true);
    ui.actdmcreverse->setChecked(false);
    worker->nes_bus.n_apu.SetReverseDpcmBits(false);

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
        worker->nes_bus.n_apu.SetSmoothSaw(checked); // giữ lại, không hại gì dù đường này có thể không tới đích

        if (worker->nes_bus.cart && worker->nes_bus.cart->pMapper)
        {
            auto vrc6 = dynamic_cast<Mapper_024*>(worker->nes_bus.cart->pMapper.get());
            if (vrc6)
                vrc6->setSmoothSaw(checked);
        }

        worker->setNsfSmoothSaw(checked);
        });

    connect(ui.actdmcreverse, &QAction::toggled, this, [this](bool checked) {
        worker->nes_bus.n_apu.SetReverseDpcmBits(checked);
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
        if (!n163WaveWindow)
        {
            n163WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::N163, this);

            n163WaveWindow->setWindowFlag(Qt::Window, true);

            n163WaveWindow->setAttribute(Qt::WA_DeleteOnClose);

            connect(n163WaveWindow, &QObject::destroyed, this, [this]() {
                n163WaveWindow = nullptr;
                });
        }

        n163WaveWindow->show();
        n163WaveWindow->raise();
        n163WaveWindow->activateWindow();
        });

    connect(ui.actWaveMMC5, &QAction::triggered, this, [this]() {
        if (!mmc5WaveWindow)
        {
            mmc5WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::MMC5, nullptr);
            mmc5WaveWindow->setAttribute(Qt::WA_DeleteOnClose);

            connect(mmc5WaveWindow, &QObject::destroyed, this, [this]() {
                mmc5WaveWindow = nullptr;
                });
        }

        mmc5WaveWindow->show();
        mmc5WaveWindow->raise();
        mmc5WaveWindow->activateWindow();
        });

    ui.actNES->setShortcut(QKeySequence("Ctrl+W"));
    ui.actNES->setShortcutContext(Qt::ApplicationShortcut);

    connect(ui.actNES, &QAction::triggered, this, [this]() {
        if (!nesWaveWindow)
        {
            nesWaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::NES, nullptr);
            nesWaveWindow->setAttribute(Qt::WA_DeleteOnClose);

            connect(nesWaveWindow, &QObject::destroyed, this, [this]() {
                nesWaveWindow = nullptr;
                });
        }

        nesWaveWindow->show();
        nesWaveWindow->raise();
        nesWaveWindow->activateWindow();
        });

    connect(ui.actWaveVRC6, &QAction::triggered, this, [this]() {
        if (!vrc6WaveWindow)
        {
            vrc6WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::VRC6, nullptr);
            vrc6WaveWindow->setAttribute(Qt::WA_DeleteOnClose);

            connect(vrc6WaveWindow, &QObject::destroyed, this, [this]() {
                vrc6WaveWindow = nullptr;
                });
        }

        vrc6WaveWindow->show();
        vrc6WaveWindow->raise();
        vrc6WaveWindow->activateWindow();
        });

    connect(ui.actWaveVRC7, &QAction::triggered, this, [this]() {
        if (!vrc7WaveWindow)
        {
            vrc7WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::VRC7, nullptr);
            vrc7WaveWindow->setAttribute(Qt::WA_DeleteOnClose);

            connect(vrc7WaveWindow, &QObject::destroyed, this, [this]() {
                vrc7WaveWindow = nullptr;
                });
        }

        vrc7WaveWindow->show();
        vrc7WaveWindow->raise();
        vrc7WaveWindow->activateWindow();
        });

    connect(ui.actWaveS5B, &QAction::triggered, this, [this]() {
        if (!s5bWaveWindow)
        {
            s5bWaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::S5B, nullptr);
            s5bWaveWindow->setAttribute(Qt::WA_DeleteOnClose);

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
            "<p><b>phiên bản:</b> 2.4</p>"
            "<p><b>lần đầu phát hành:</b> 9/4/2026</p>"
            "<p><b>ngày cập nhật phiên bản mới nhất:30/7/2026</b>"
            "<p><b>Developer:</b> Nguyễn Quyết Chiến, Nguyễn Dức An, Phạm Đăng Hoàn</p>"
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
    ui.action60fps->setChecked(false); // mặc định 30fps

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
        showFpsOverlay = !showFpsOverlay;

        if (!showFpsOverlay)
        {
            ui.gameScreen->setOverlayText("");
            ui.gameScreen->update();
            ui.txtConsole->appendPlainText("FPS Overlay: OFF");
        }
        else
        {
            ui.txtConsole->appendPlainText("FPS Overlay: ON");
        }
        });

    setFocusPolicy(Qt::StrongFocus);
    ui.gameScreen->setFocusPolicy(Qt::NoFocus);

    QShortcut* fullScreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fullScreenShortcut->setContext(Qt::ApplicationShortcut);
    connect(fullScreenShortcut, &QShortcut::activated, this, &SJNES::toggleGameFullScreen);

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
    // Debug / filter nếu muốn
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
}


SJNES::~SJNES()
{
    if (worker)
        worker->stop();

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
    worker->nes_bus.nsfMode = false;
    worker->nes_bus.DisableNSFMode();

    if (fileName.isEmpty())
        return;

    // 1. Dừng hệ thống
    QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
    system_clock_counter = 0;
    dma_dummy_counter = 0;

    for (int i = 0; i < 2048; i++)
        worker->nes_bus.ram[i] = 0x00;

    // 2. Nạp băng game
    std::shared_ptr<Cartridge> cart = nullptr;

    if (fileName.endsWith(".zip", Qt::CaseInsensitive))
    {
        std::vector<uint8_t> romData = LoadNesFromZip(fileName);

        if (romData.empty())
        {
            QMessageBox::warning(this, "Lỗi ROM", "Không tìm thấy file .nes trong file .zip.");
            return;
        }

        cart = std::make_shared<Cartridge>(romData);
    }
    else
    {
        cart = std::make_shared<Cartridge>(fileName.toStdString());
    }

    if (cart == nullptr || !cart->ImageValid() || cart->pMapper == nullptr)
    {
        QMessageBox::warning(this, "Lỗi ROM", "Không thể nạp ROM hoặc Mapper chưa được hỗ trợ.");
        return;
    }

    // 3. Cắm băng vào Bus và PPU
    worker->nes_bus.insertCartridge(cart);
    worker->nes_ppu.ConnectCartridge(cart);

    // 4. Reset hệ thống
    worker->nes_ppu.reset();
    worker->nes_bus.n_apu.reset();

    if (cart->pMapper != nullptr)
        cart->pMapper->reset();

    worker->nes_cpu.reset();
    worker->nes_bus.n_apu.SetSmoothSaw(ui.actSmoothSaw->isChecked());

    if (cart->pMapper != nullptr)
    {
        auto vrc6 = dynamic_cast<Mapper_024*>(cart->pMapper.get());
        if (vrc6)
            vrc6->setSmoothSaw(ui.actSmoothSaw->isChecked());
    }
    uint8_t irqLo = worker->nes_bus.cpuRead(0xFFFE);
    uint8_t irqHi = worker->nes_bus.cpuRead(0xFFFF);
    uint16_t irqVec = (irqHi << 8) | irqLo;

    ui.txtConsole->appendPlainText(">>> DA NAP ROM VA RESET HE THONG: " + fileName);
    ui.txtConsole->appendPlainText(QString("MAPPER ID: %1").arg(cart->nMapperID));
    ui.txtConsole->appendPlainText(
        QString("START -> PC: 0x%1").arg(worker->nes_cpu.pc, 4, 16, QChar('0')).toUpper()
    );

    addRecentRom(fileName);

    QMetaObject::invokeMethod(worker, "start", Qt::BlockingQueuedConnection);
    ui.actPause->setText("Dừng (Pause)");
}
void SJNES::onOpenROMClicked()
{
    QSettings settings("chienz", "NesEmulator");
    QString lastPath = settings.value("LastRomPath", "D:\\").toString();
    QString fileName = QFileDialog::getOpenFileName(
        nullptr,
        "Open ROM",
        lastPath,
        "NES / NSF Files (*.nes *.zip *.nsf);;NES Files (*.nes);;ZIP Files (*.zip);;NSF Files (*.nsf);;All Files (*.*)"
    );

    if (fileName.endsWith(".nsf", Qt::CaseInsensitive))
    {
        loadNSFFile(fileName);
        return;
    }

    if (!fileName.isEmpty()) {
        QFileInfo fileInfo(fileName);
        settings.setValue("LastRomPath", fileInfo.absolutePath());

        loadRomFile(fileName);
    }
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

    ui.txtConsole->appendPlainText("===== NSF PLAYER MODE =====");

    QMetaObject::invokeMethod(worker, "loadNSF", Qt::QueuedConnection, Q_ARG(NSFFile, nsf));

    isPaused = false;
    ui.actPause->setText("Dừng (Pause)");
}

void SJNES::onNsfConsoleMessage(QString msg)
{
    ui.txtConsole->appendPlainText(msg);
}

void SJNES::onNsfTrackChanged(int current, int total)
{
    ui.txtConsole->appendPlainText(QString("NSF Track: %1 / %2").arg(current).arg(total));
}

void SJNES::onNsfExpansionDetected(bool vrc6, bool s5b, bool vrc7, bool mmc5, bool n163)
{
    if (vrc6) {
        if (!vrc6WaveWindow) {
            vrc6WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::VRC6, nullptr);
            vrc6WaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            connect(vrc6WaveWindow, &QObject::destroyed, this, [this]() { vrc6WaveWindow = nullptr; });
        }
        vrc6WaveWindow->show(); vrc6WaveWindow->raise(); vrc6WaveWindow->activateWindow();
    }
    if (s5b) {
        if (!s5bWaveWindow) {
            s5bWaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::S5B, nullptr);
            s5bWaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            connect(s5bWaveWindow, &QObject::destroyed, this, [this]() { s5bWaveWindow = nullptr; });
        }
        s5bWaveWindow->show(); s5bWaveWindow->raise(); s5bWaveWindow->activateWindow();
    }
    if (vrc7) {
        if (!vrc7WaveWindow) {
            vrc7WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::VRC7, nullptr);
            vrc7WaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            connect(vrc7WaveWindow, &QObject::destroyed, this, [this]() { vrc7WaveWindow = nullptr; });
        }
        vrc7WaveWindow->show(); vrc7WaveWindow->raise(); vrc7WaveWindow->activateWindow();
    }
    if (mmc5) {
        if (!mmc5WaveWindow) {
            mmc5WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::MMC5, nullptr);
            mmc5WaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            connect(mmc5WaveWindow, &QObject::destroyed, this, [this]() { mmc5WaveWindow = nullptr; });
        }
        mmc5WaveWindow->show(); mmc5WaveWindow->raise(); mmc5WaveWindow->activateWindow();
    }
    if (n163) {
        if (!n163WaveWindow) {
            n163WaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::N163, nullptr);
            n163WaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            connect(n163WaveWindow, &QObject::destroyed, this, [this]() { n163WaveWindow = nullptr; });
        }
        n163WaveWindow->show(); n163WaveWindow->raise(); n163WaveWindow->activateWindow();
    }

    bool anyExpansion = vrc6 || s5b || vrc7 || mmc5 || n163;
    if (anyExpansion) {
        if (nesWaveWindow) nesWaveWindow->hide();
    }
    else {
        if (!nesWaveWindow) {
            nesWaveWindow = new AudioWaveWindow(AudioWaveWindow::WaveMode::NES, nullptr);
            nesWaveWindow->setAttribute(Qt::WA_DeleteOnClose);
            connect(nesWaveWindow, &QObject::destroyed, this, [this]() { nesWaveWindow = nullptr; });
        }
        nesWaveWindow->show(); nesWaveWindow->raise(); nesWaveWindow->activateWindow();
    }
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
    ui.gameScreen->setFrame(frame);
}

void SJNES::onAudioReady(std::vector<float> samples, bool stereo)
{
    if (!audio_sink || !audio_device || samples.empty())
        return;

    audio_device->write(
        reinterpret_cast<const char*>(samples.data()),
        static_cast<qint64>(samples.size() * sizeof(float))
    );
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
    }
}

void SJNES::onGameFrameTicked()
{
    gameFpsCounter++;

    const int FPS_UPDATE_MS = 500;
    if (fpsTimer.elapsed() >= FPS_UPDATE_MS)
    {
        double elapsedSec = fpsTimer.elapsed() / 1000.0;
        videoFpsValue = videoFpsCounter / elapsedSec;
        gameFpsValue = gameFpsCounter / elapsedSec;
        videoFpsCounter = 0;
        gameFpsCounter = 0;
        fpsTimer.restart();

        if (showFpsOverlay)
            ui.gameScreen->setOverlayText(QString("VIDEO %1 FPS\nGAME %2 FPS").arg(videoFpsValue, 0, 'f', 1).arg(gameFpsValue, 0, 'f', 1));
        else
            ui.gameScreen->setOverlayText("");
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
        (n163WaveWindow && n163WaveWindow->isVisible());

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

    int areaW = ui.centralWidget->width();
    int areaH = ui.centralWidget->height();

    const int topBarH = 45;
    const int bottomMargin = 0;

    // Chừa panel debug/log bên trái và audio checkbox bên phải
    const int leftPanelW = 130;
    const int rightPanelW = 20;

    int availableW = areaW - leftPanelW - rightPanelW;
    int availableH = areaH - topBarH - bottomMargin;

    if (availableW < 256) availableW = 256;
    if (availableH < 240) availableH = 240;

    int screenW = 0;
    int screenH = 0;

    if (pixelPerfectMode)
    {
        // Perfect Pixel: chỉ scale theo số nguyên 1x, 2x, 3x, 4x...
        int scaleX = availableW / 256;
        int scaleY = availableH / 240;
        int scale = std::min(scaleX, scaleY);

        if (scale < 1)
            scale = 1;

        screenW = 256 * scale;
        screenH = 240 * scale;
    }
    else
    {
        // Fit Screen: phóng to sát vùng trống, giữ tỉ lệ nhưng không pixel perfect tuyệt đối
        screenH = availableH;
        screenW = screenH * 256 / 240;

        if (screenW > availableW) {
            screenW = availableW;
            screenH = screenW * 240 / 256;
        }
    }

    // Căn giữa trong vùng giữa, không đè panel trái/phải
    int x = leftPanelW + (availableW - screenW) / 2 - 50;
    int y = topBarH + (availableH - screenH) / 2;

    ui.gameScreen->setGeometry(x, y, screenW, screenH);


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

    menuBar()->hide();
    statusBar()->hide();

    // Ẩn khung log/text bên trái nếu có
    if (ui.txtConsole)
        ui.txtConsole->hide();

    // Ép centralWidget không còn viền trắng/xám
    if (centralWidget())
    {
        centralWidget()->setContentsMargins(0, 0, 0, 0);
        centralWidget()->setStyleSheet("background-color: black;");
    }

    // Ép layout không margin/spacing
    if (centralWidget() && centralWidget()->layout())
    {
        centralWidget()->layout()->setContentsMargins(0, 0, 0, 0);
        centralWidget()->layout()->setSpacing(0);
    }

    // Ép gameScreen chiếm toàn bộ
    ui.gameScreen->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui.gameScreen->setMinimumSize(0, 0);
    showFullScreen();

    activateWindow();
    raise();
    setFocus();
}

void SJNES::exitGameFullScreen()
{
    gameFullScreen = false;

    showNormal();

    menuBar()->show();
    statusBar()->show();

    if (ui.txtConsole)
        ui.txtConsole->show();

    // Nếu muốn trả nền bình thường thì để trắng/xám lại
    if (centralWidget())
    {
        centralWidget()->setStyleSheet("");
    }

    activateWindow();
    raise();
    setFocus();
}

void SJNES::toggleGameFullScreen()
{
    if (gameFullScreen)
        exitGameFullScreen();
    else
        enterGameFullScreen();
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

    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(is_stereo ? 2 : 1);
    format.setSampleFormat(QAudioFormat::Float);

    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();

    audio_sink = new QAudioSink(outputDevice, format, this);

    // Tăng buffer để khi kéo/di chuyển cửa sổ, audio còn dữ liệu dự phòng nên đỡ khựng/rè hơn.
    // Gốc là 32768.
    audio_sink->setBufferSize(32768);

    audio_device = audio_sink->start();

    ui.txtConsole->appendPlainText(
        "Audio output: " + outputDevice.description()
    );
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
    holdturboA = checked;
}

void SJNES::on_chkAutoB_toggled(bool checked)
{
    holdturboB = checked;
}