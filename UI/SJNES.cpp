#include "SJNES.h"
#include <QFileDialog>
#include <QMessageBox>
#include <algorithm>
#include "Mapper.h"
#include <QSettings>
#include <QFileInfo>
#include "LogBuffer.h"
#include "GpuScreenWidget.h"
#include "Mapper_024.h"
#include <QAction>
SJNES::SJNES(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    ui.txtConsole->setReadOnly(true);
    ui.txtConsole->setFocusPolicy(Qt::NoFocus);
    nes_cpu.ConnectBus(&nes_bus);
    nes_bus.cpu = &nes_cpu;
    nes_bus.ppu = &nes_ppu;
    timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, &SJNES::runFrame);
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    audio_sink = new QAudioSink(format, this);
    audio_sink->setBufferSize(32768);
    audio_device = audio_sink->start();
// MENU: FILE
    connect(ui.actOpenROM, &QAction::triggered,
        this, &SJNES::onOpenROMClicked);

    connect(ui.actReset, &QAction::triggered,
        this, &SJNES::onResetClicked);

    connect(ui.actPause, &QAction::triggered,
        this, &SJNES::onStepClicked);

    connect(ui.actExit, &QAction::triggered,
        this, &QWidget::close);
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

    // =======================
    // MENU: AUDIO DEBUG
    // checked = nghe, unchecked = mute
    // =======================
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
        nes_bus.n_apu.mutePulse1 = !checked;
        });

    connect(ui.actPulse2, &QAction::toggled, this, [this](bool checked) {
        nes_bus.n_apu.mutePulse2 = !checked;
        });

    connect(ui.actTriangle, &QAction::toggled, this, [this](bool checked) {
        nes_bus.n_apu.muteTriangle = !checked;
        });

    connect(ui.actNoise, &QAction::toggled, this, [this](bool checked) {
        nes_bus.n_apu.muteNoise = !checked;
        });

    connect(ui.actDMC, &QAction::toggled, this, [this](bool checked) {
        nes_bus.n_apu.muteDMC = !checked;
        });

    connect(ui.actVRC6, &QAction::toggled, this, [this](bool checked) {
        if (!nes_bus.cart || !nes_bus.cart->pMapper)
            return;

        if (auto* vrc6 = dynamic_cast<Mapper_024*>(nes_bus.cart->pMapper.get())) {
            vrc6->muteVRC6 = !checked;
        }
        });

    ui.actPixelPerfect->setCheckable(true);
    ui.actPixelPerfect->setChecked(pixelPerfectMode);

    connect(ui.actPixelPerfect, &QAction::toggled, this, [this](bool checked) {
        pixelPerfectMode = checked;
        resizeEvent(nullptr);
        });

    ui.actAudioWaveform->setCheckable(true);
    ui.actAudioWaveform->setChecked(false);

    connect(ui.actAudioWaveform, &QAction::toggled, this, [this](bool checked) {
        if (checked)
        {
            if (!audioWaveWindow)
            {
                audioWaveWindow = new AudioWaveWindow(nullptr); // cửa sổ riêng
                audioWaveWindow->setAttribute(Qt::WA_DeleteOnClose);

                connect(audioWaveWindow, &QObject::destroyed, this, [this]() {
                    audioWaveWindow = nullptr;
                    ui.actAudioWaveform->setChecked(false);
                    });
            }

            audioWaveWindow->show();
            audioWaveWindow->raise();
            audioWaveWindow->activateWindow();
        }
        else
        {
            if (audioWaveWindow)
            {
                audioWaveWindow->close();
                audioWaveWindow = nullptr;
            }
        }
        });

    connect(ui.actMapperViewer, &QAction::triggered, this, [this]() {
        if (!mapperViewerWindow)
        {
            mapperViewerWindow = new MapperViewerWindow(&nes_bus, nullptr);
            mapperViewerWindow->setAttribute(Qt::WA_DeleteOnClose);

            connect(mapperViewerWindow, &QObject::destroyed, this, [this]() {
                mapperViewerWindow = nullptr;
                });
        }

        mapperViewerWindow->show();
        mapperViewerWindow->raise();
        mapperViewerWindow->activateWindow();
        });

}

SJNES::~SJNES() {}

// =======================================================================
// QUÁ TRÌNH KHỞI ĐỘNG MÁY NES (RESET SEQUENCE CHUẨN OLC)
// =======================================================================
void SJNES::onOpenROMClicked()
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
        ui.actPause->setText("Dừng (Pause)");
    }
}

void SJNES::onResetClicked()
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

void SJNES::onStepClicked() {
    if (timer->isActive()) {
        timer->stop();
        ui.actPause->setText("Chạy (Auto)");
    }
    else {
        timer->start(16);
        ui.actPause->setText("Dừng (Pause)");
    }
}
void SJNES::runFrame() {
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
                    AudioDebugChannels dbg = nes_bus.n_apu.GetDebugChannels();

                    if (nes_bus.cart && nes_bus.cart->pMapper)
                    {
                        nes_bus.cart->pMapper->GetExpansionDebugChannels(
                            dbg.vrc6Pulse1,
                            dbg.vrc6Pulse2,
                            dbg.vrc6Saw
                        );
                    }

                    if (audioWaveWindow && audioWaveWindow->isVisible())
                    {
                        audioWaveWindow->pushChannels(dbg);
                    }
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
                    AudioDebugChannels dbg = nes_bus.n_apu.GetDebugChannels();

                    if (nes_bus.cart && nes_bus.cart->pMapper)
                    {
                        nes_bus.cart->pMapper->GetExpansionDebugChannels(
                            dbg.vrc6Pulse1,
                            dbg.vrc6Pulse2,
                            dbg.vrc6Saw
                        );
                    }

                    if (audioWaveWindow && audioWaveWindow->isVisible())
                    {
                        audioWaveWindow->pushChannels(dbg);
                    }
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
void SJNES::onStereoToggled(bool checked) {
    is_stereo = checked;
    ui.actStereo->setChecked(checked);
    ui.actMono->setChecked(!checked);

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
// XỬ LÝ PHÍM BẤM
void SJNES::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) return;

    bool isNumpad = event->modifiers() & Qt::KeypadModifier;

    // PLAYER 1
    if (event->key() == Qt::Key_K)      nes_bus.controller_state |= 0x80; // A
    if (event->key() == Qt::Key_J)      nes_bus.controller_state |= 0x40; // B
    if (event->key() == Qt::Key_U)      nes_bus.controller_state |= 0x20; // Select
    if (event->key() == Qt::Key_Return) nes_bus.controller_state |= 0x10; // Start
    if (event->key() == Qt::Key_W)      nes_bus.controller_state |= 0x08; // Up
    if (event->key() == Qt::Key_S)      nes_bus.controller_state |= 0x04; // Down
    if (event->key() == Qt::Key_A)      nes_bus.controller_state |= 0x02; // Left
    if (event->key() == Qt::Key_D)      nes_bus.controller_state |= 0x01; // Right

    // PLAYER 2 - hướng bằng phím mũi tên
    if (event->key() == Qt::Key_Up)     nes_bus.controller_state2 |= 0x08; // Up
    if (event->key() == Qt::Key_Down)   nes_bus.controller_state2 |= 0x04; // Down
    if (event->key() == Qt::Key_Left)   nes_bus.controller_state2 |= 0x02; // Left
    if (event->key() == Qt::Key_Right)  nes_bus.controller_state2 |= 0x01; // Right

    // PLAYER 2 - nút bằng numpad
    if (isNumpad && event->key() == Qt::Key_6) nes_bus.controller_state2 |= 0x80; // A
    if (isNumpad && event->key() == Qt::Key_5) nes_bus.controller_state2 |= 0x40; // B
    if (isNumpad && event->key() == Qt::Key_7) nes_bus.controller_state2 |= 0x20; // Select
    if (isNumpad && event->key() == Qt::Key_9) nes_bus.controller_state2 |= 0x10; // Start
}

void SJNES::keyReleaseEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) return;

    bool isNumpad = event->modifiers() & Qt::KeypadModifier;

    // PLAYER 1
    if (event->key() == Qt::Key_K)      nes_bus.controller_state &= ~0x80;
    if (event->key() == Qt::Key_J)      nes_bus.controller_state &= ~0x40;
    if (event->key() == Qt::Key_U)      nes_bus.controller_state &= ~0x20;
    if (event->key() == Qt::Key_Return) nes_bus.controller_state &= ~0x10;
    if (event->key() == Qt::Key_W)      nes_bus.controller_state &= ~0x08;
    if (event->key() == Qt::Key_S)      nes_bus.controller_state &= ~0x04;
    if (event->key() == Qt::Key_A)      nes_bus.controller_state &= ~0x02;
    if (event->key() == Qt::Key_D)      nes_bus.controller_state &= ~0x01;

    // PLAYER 2 - hướng bằng phím mũi tên
    if (event->key() == Qt::Key_Up)     nes_bus.controller_state2 &= ~0x08;
    if (event->key() == Qt::Key_Down)   nes_bus.controller_state2 &= ~0x04;
    if (event->key() == Qt::Key_Left)   nes_bus.controller_state2 &= ~0x02;
    if (event->key() == Qt::Key_Right)  nes_bus.controller_state2 &= ~0x01;

    // PLAYER 2 - nút bằng numpad
    if (isNumpad && event->key() == Qt::Key_6) nes_bus.controller_state2 &= ~0x80; // A
    if (isNumpad && event->key() == Qt::Key_5) nes_bus.controller_state2 &= ~0x40; // B
    if (isNumpad && event->key() == Qt::Key_7) nes_bus.controller_state2 &= ~0x20; // Select
    if (isNumpad && event->key() == Qt::Key_9) nes_bus.controller_state2 &= ~0x10; // Start
}