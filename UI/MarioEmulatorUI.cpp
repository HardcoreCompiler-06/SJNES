#include "MarioEmulatorUI.h"
#include <QFileDialog>
#include <QMessageBox>
#include <algorithm>
#include "Mapper.h"
#include <QSettings>
#include <QFileInfo>
#include "LogBuffer.h"
#include "GpuScreenWidget.h"
#include <QCheckBox>
#include "Mapper_024.h"
MarioEmulatorUI::MarioEmulatorUI(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    // BƯỚC 1: HÀN DÂY PHẦN CỨNG
    nes_cpu.ConnectBus(&nes_bus);
    nes_bus.cpu = &nes_cpu;
    nes_bus.ppu = &nes_ppu;

    // BƯỚC 2: KẾT NỐI NÚT BẤM GIAO DIỆN
    connect(ui.btnOpenROM, &QPushButton::clicked, this, &MarioEmulatorUI::onOpenROMClicked);
    connect(ui.btnStep, &QPushButton::clicked, this, &MarioEmulatorUI::onStepClicked);
    connect(ui.btnStereo, &QPushButton::toggled, this, &MarioEmulatorUI::onStereoToggled);
    connect(ui.btnReset, &QPushButton::clicked, this, &MarioEmulatorUI::onResetClicked);
    // BƯỚC 3: HÌNH ẢNH (60 FPS ~ 16ms/frame)
    timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, &MarioEmulatorUI::runFrame);

    // BƯỚC 4: LẮP ĐỘNG CƠ ÂM THANH
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    audio_sink = new QAudioSink(format, this);
    audio_sink->setBufferSize(32768);
    audio_device = audio_sink->start();
    
    //debug âm thanh (2A03)
    ui.chkPulse1->setChecked(true);
    ui.chkPulse2->setChecked(true);
    ui.chkTriangle->setChecked(true);
    ui.chkNoise->setChecked(true);
    ui.chkDMC->setChecked(true);

    connect(ui.chkPulse1, &QCheckBox::toggled, this, [this](bool checked) {
        nes_bus.n_apu.mutePulse1 = !checked;
        });

    connect(ui.chkPulse2, &QCheckBox::toggled, this, [this](bool checked) {
        nes_bus.n_apu.mutePulse2 = !checked;
        });

    connect(ui.chkTriangle, &QCheckBox::toggled, this, [this](bool checked) {
        nes_bus.n_apu.muteTriangle = !checked;
        });

    connect(ui.chkNoise, &QCheckBox::toggled, this, [this](bool checked) {
        nes_bus.n_apu.muteNoise = !checked;
        });

    connect(ui.chkDMC, &QCheckBox::toggled, this, [this](bool checked) {
        nes_bus.n_apu.muteDMC = !checked;
        });
    //âm thanh mở rộng VRC6
    ui.chkVRC6->setChecked(true);

    connect(ui.chkVRC6, &QCheckBox::toggled, this, [this](bool checked) {
        if (!nes_bus.cart || !nes_bus.cart->pMapper)
            return;

        if (auto* vrc6 = dynamic_cast<Mapper_024*>(nes_bus.cart->pMapper.get())) {
            vrc6->muteVRC6 = !checked;
        }
        });

    //Filter
    connect(ui.chkPixelPerfect, &QCheckBox::toggled, this, [this](bool checked) {
        pixelPerfectMode = checked;
        resizeEvent(nullptr);
        });
}

MarioEmulatorUI::~MarioEmulatorUI() {}

// =======================================================================
// QUÁ TRÌNH KHỞI ĐỘNG MÁY NES (RESET SEQUENCE CHUẨN OLC)
// =======================================================================
void MarioEmulatorUI::onOpenROMClicked()
{
    QSettings settings("chienz", "NesEmulator");
    QString lastPath = settings.value("LastRomPath", "D:\\").toString();

    QString fileName = QFileDialog::getOpenFileName(this, "Chọn ROM", lastPath, "NES Files (*.nes)");

    if (!fileName.isEmpty()) {
        // Lưu lại thư mục vừa chọn vào Registry để lần sau mở đúng chỗ này
        QFileInfo fileInfo(fileName);
        settings.setValue("LastRomPath", fileInfo.absolutePath());

        // 1. Dập cầu dao, dừng hệ thống
        timer->stop();
        system_clock_counter = 0;
        dma_dummy_counter = 0;

        for (int i = 0; i < 2048; i++) nes_bus.ram[i] = 0x00;

        // 2. Nạp băng game
        std::shared_ptr<Cartridge> cart = std::make_shared<Cartridge>(fileName.toStdString());
        if (cart == nullptr || cart->pMapper == nullptr) {
            QMessageBox::warning(this, "Lỗi ROM", "Không thể nạp ROM hoặc Mapper chưa được hỗ trợ.");
            return;
        }

        // 3. Cắm băng vào Bus và PPU
        nes_bus.insertCartridge(cart);
        nes_ppu.ConnectCartridge(cart);

        // 4. RESET TOÀN BỘ HỆ THỐNG
        // Thứ tự cực kỳ quan trọng: PPU -> Mapper -> CPU
        nes_ppu.reset();
        if (cart->pMapper != nullptr) cart->pMapper->reset();
        nes_cpu.reset();
        uint8_t irqLo = nes_bus.cpuRead(0xFFFE);
        uint8_t irqHi = nes_bus.cpuRead(0xFFFF);
        uint16_t irqVec = (irqHi << 8) | irqLo;
        ui.txtConsole->appendPlainText(
            QString("IRQ Vector: 0x%1").arg(irqVec, 4, 16, QChar('0')).toUpper());

        // 5. In log ra màn hình
        ui.txtConsole->appendPlainText(">>> DA NAP ROM VA RESET HE THONG: " + fileName);
        ui.txtConsole->appendPlainText(QString("MAPPER ID: %1").arg(cart->nMapperID));
        ui.txtConsole->appendPlainText(QString("START -> PC: 0x%1").arg(nes_cpu.pc, 4, 16, QChar('0')).toUpper());

        // 6. Bật điện!
        timer->start(16);
        ui.btnStep->setText("Dừng (Pause)");
    }
}

void MarioEmulatorUI::onResetClicked()
{
    if (nes_bus.cart == nullptr || nes_bus.cart->pMapper == nullptr) {
        return;
    }

    // Dừng tạm để reset không bị chạy giữa frame
    bool wasRunning = timer->isActive();
    timer->stop();

    // Reset clock hệ thống
    system_clock_counter = 0;
    dma_dummy_counter = 0;

    // Reset DMA
    nes_bus.dma_page = 0x00;
    nes_bus.dma_addr = 0x00;
    nes_bus.dma_data = 0x00;
    nes_bus.dma_dummy = true;
    nes_bus.dma_transfer = false;

    // Reset controller shift
    nes_bus.controller_strobe = 0;
    nes_bus.controller_shift = 0;

    // Reset PPU -> Mapper -> CPU
    nes_ppu.reset();

    if (nes_bus.cart->pMapper != nullptr) {
        nes_bus.cart->pMapper->reset();
    }

    nes_cpu.reset();
    ui.txtConsole->appendPlainText(">>> RESET MAY NES");
    ui.txtConsole->appendPlainText(
        QString("START -> PC: 0x%1")
        .arg(nes_cpu.pc, 4, 16, QChar('0'))
        .toUpper()
    );

    if (wasRunning) {
        timer->start(16);
    }
}

void MarioEmulatorUI::onStepClicked() {
    if (timer->isActive()) {
        timer->stop();
        ui.btnStep->setText("Chạy (Auto)");
    }
    else {
        timer->start(16);
        ui.btnStep->setText("Dừng (Pause)");
    }
}
void MarioEmulatorUI::runFrame() {
    static QByteArray audio_buffer;
    static double audio_accumulator = 0.0;
    const float MASTER_VOLUME = 1.8f; // thử 1.5f, 1.8f, 2.0f
    if (audio_sink != nullptr && audio_sink->bytesFree() < 8192) {
        return;
    }

    const int PPU_CYCLES_PER_FRAME = 89342;

    for (int i = 0; i < PPU_CYCLES_PER_FRAME; i++) {

        nes_bus.ppu->Step();

        if (system_clock_counter % 3 == 0) {

            // VRC6 IRQ/audio chạy mỗi CPU cycle, đặt TRƯỚC CPU clock
            if (nes_bus.cart != nullptr && nes_bus.cart->pMapper != nullptr) {
                nes_bus.cart->pMapper->irqStep();

                if (nes_bus.cart->pMapper->irqState()) {
                    nes_bus.cart->pMapper->irqClear();
                    nes_cpu.irq();
                }
            }

            if (nes_bus.dma_transfer) {
                if (dma_dummy_counter < 512) {
                    dma_dummy_counter++;
                }
                else {
                    nes_bus.dma_transfer = false;
                    dma_dummy_counter = 0;
                }
            }
            else {
                nes_cpu.clock();
            }

            nes_bus.n_apu.Step();

            audio_accumulator += 44100.0 / 1789773.0;
            if (audio_accumulator >= 1.0) {
                audio_accumulator -= 1.0;

                if (is_stereo) {
                    float left, right;
                    nes_bus.n_apu.GetOutputSampleStereo(left, right);

                    float exp = 0.0f;
                    if (nes_bus.cart && nes_bus.cart->pMapper) {
                        exp = nes_bus.cart->pMapper->GetExpansionAudio();
                    }

                    left += exp;
                    right += exp;

                    left = std::clamp(left * MASTER_VOLUME, -1.0f, 1.0f);
                    right = std::clamp(right * MASTER_VOLUME, -1.0f, 1.0f);

                    audio_buffer.append(reinterpret_cast<const char*>(&left), sizeof(float));
                    audio_buffer.append(reinterpret_cast<const char*>(&right), sizeof(float));
                }
                else {
                    float sample = nes_bus.n_apu.GetOutputSample();

                    if (nes_bus.cart && nes_bus.cart->pMapper) {
                        sample += nes_bus.cart->pMapper->GetExpansionAudio();
                    }

                    sample = std::clamp(sample * MASTER_VOLUME, -1.0f, 1.0f);

                    audio_buffer.append(reinterpret_cast<const char*>(&sample), sizeof(float));
                }
            }
        }

     
        if (nes_bus.ppu->nmi_requested) {
            nes_bus.ppu->nmi_requested = false;
            nes_cpu.nmi();
        }

        system_clock_counter++;
    } 


    if (audio_device != nullptr && !audio_buffer.isEmpty()) {
        audio_device->write(audio_buffer.data(), audio_buffer.size());
        audio_buffer.clear();
    
    }

    QImage frameImage = nes_bus.ppu->GetScreen();
    ui.gameScreen->setFrame(frameImage);
    gLogBuffer.clear();

} 
void MarioEmulatorUI::onStereoToggled(bool checked) {
    is_stereo = checked;
    ui.btnStereo->setText(checked ? "🔊 Stereo" : "🔈 Mono");

    // Restart audio sink với đúng channel count
    if (audio_sink) {
        audio_sink->stop();
        delete audio_sink;
        audio_sink = nullptr;
    }
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(is_stereo ? 2 : 1);
    format.setSampleFormat(QAudioFormat::Float);
    audio_sink = new QAudioSink(format, this);
    audio_sink->setBufferSize(32768);
    audio_device = audio_sink->start();
}
void MarioEmulatorUI::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    int areaW = ui.centralWidget->width();
    int areaH = ui.centralWidget->height();

    const int topBarH = 45;
    const int bottomMargin = 0;

    // Chừa panel debug/log bên trái và audio checkbox bên phải
    const int leftPanelW = 130;
    const int rightPanelW = 130;

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
    int x = leftPanelW + (availableW - screenW) / 2;
    int y = topBarH + (availableH - screenH) / 2;

    ui.gameScreen->setGeometry(x, y, screenW, screenH);

    // Audio debug checkbox bám rìa phải
    int chkX = areaW - rightPanelW + 15;
    int chkY = topBarH + 20;
    int chkW = 110;
    int chkH = 24;
    int gap = 28;

    ui.chkPulse1->setGeometry(chkX, chkY + gap * 0, chkW, chkH);
    ui.chkPulse2->setGeometry(chkX, chkY + gap * 1, chkW, chkH);
    ui.chkTriangle->setGeometry(chkX, chkY + gap * 2, chkW, chkH);
    ui.chkNoise->setGeometry(chkX, chkY + gap * 3, chkW, chkH);
    ui.chkDMC->setGeometry(chkX, chkY + gap * 4, chkW, chkH);
    ui.chkVRC6->setGeometry(chkX, chkY + gap * 5, chkW, chkH);
}
// XỬ LÝ PHÍM BẤM
void MarioEmulatorUI::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) return;
    if (event->key() == Qt::Key_K)      nes_bus.controller_state |= 0x80;
    if (event->key() == Qt::Key_J)      nes_bus.controller_state |= 0x40;
    if (event->key() == Qt::Key_U)      nes_bus.controller_state |= 0x20;
    if (event->key() == Qt::Key_Return) nes_bus.controller_state |= 0x10;
    if (event->key() == Qt::Key_W)      nes_bus.controller_state |= 0x08;
    if (event->key() == Qt::Key_S)      nes_bus.controller_state |= 0x04;
    if (event->key() == Qt::Key_A)      nes_bus.controller_state |= 0x02;
    if (event->key() == Qt::Key_D)      nes_bus.controller_state |= 0x01;
}

void MarioEmulatorUI::keyReleaseEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) return;
    if (event->key() == Qt::Key_K)      nes_bus.controller_state &= ~0x80;
    if (event->key() == Qt::Key_J)      nes_bus.controller_state &= ~0x40;
    if (event->key() == Qt::Key_U)      nes_bus.controller_state &= ~0x20;
    if (event->key() == Qt::Key_Return) nes_bus.controller_state &= ~0x10;
    if (event->key() == Qt::Key_W)      nes_bus.controller_state &= ~0x08;
    if (event->key() == Qt::Key_S)      nes_bus.controller_state &= ~0x04;
    if (event->key() == Qt::Key_A)      nes_bus.controller_state &= ~0x02;
    if (event->key() == Qt::Key_D)      nes_bus.controller_state &= ~0x01;
}