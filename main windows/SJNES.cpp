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
#include <QDataStream>
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
#include <QTextBrowser>
#include <QFileInfo>
#define SDL_MAIN_HANDLED
#include <SDL.h>
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
        "phần mềm được code bởi Nguyễn chiến. "
        "nếu thấy ok vui lòng cho mình 1 sao ở github nhé=)\n"
        "1. Mở ROM:\n"
        "   Vào File -> Open ROM để chọn file .nes hoặc .zip.\n\n"
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
        "   Tab = Tua nhanh\n"
        "   F1 = Save State Mapper 0\n"
        "   F2 = Load State Mapper 0\n"
        "   F11 = Fullscreen\n\n"
        "5. Debug:\n"
        "   Menu Debug có Audio Waveform, Mapper Viewer, Sprite Viewer và Audio Channel Debug.\n\n"
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
            settings.setValue("HideQuickStart", true);

        dialog.accept();
        });

    dialog.exec();
}
SJNES::SJNES(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

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
                "<p><b>phiên bản:</b> 1.8</p>"
                "<p><b>lần đầu phát hành:</b> 9/4/2026</p>"
                "<p><b>Developer:</b> Nguyễn Quyết Chiến</p>"
                "<p><b>ngôn ngữ lập trình:</b> C++ / Qt</p>"
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

    nes_cpu.ConnectBus(&nes_bus);
    nes_bus.cpu = &nes_cpu;
    nes_bus.ppu = &nes_ppu;
    timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, &SJNES::runFrame);
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


    connect(ui.actSpriteViewer, &QAction::triggered, this, [this]() {
        if (!spriteViewerWindow)
        {
            spriteViewerWindow = new SpriteViewerWindow(&nes_bus, nullptr);
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
    ui.actMapperViewer->setShortcutContext(Qt::ApplicationShortcut);
    ui.actSpriteViewer->setShortcutContext(Qt::ApplicationShortcut);

    ui.action60fps->setShortcutContext(Qt::ApplicationShortcut);
    connect(ui.actionOverclockOff, &QAction::triggered, this, [this]() {
        nes_ppu.SetExtraScanlinesBeforeNMI(0);
        ui.txtConsole->appendPlainText("Overclock: OFF");
        });

    connect(ui.actionOverclock50, &QAction::triggered, this, [this]() {
        nes_ppu.SetExtraScanlinesBeforeNMI(50);
        ui.txtConsole->appendPlainText("ép xung cpu thêm 50");
        });

    connect(ui.actionOverclock100, &QAction::triggered, this, [this]() {
        nes_ppu.SetExtraScanlinesBeforeNMI(100);
        ui.txtConsole->appendPlainText("ép xung cpu thêm 100");
        });
    
    connect(ui.actionOverclock200, &QAction::triggered, this, [this]() {
        nes_ppu.SetExtraScanlinesBeforeNMI(200);
        ui.txtConsole->appendPlainText("ép xung cpu lên 200");
        });

    connect(ui.actionOverclock250, &QAction::triggered, this, [this]() {
        nes_ppu.SetExtraScanlinesBeforeNMI(250);
        ui.txtConsole->appendPlainText("ép xung cpu lên 250");
        });

    ui.actRemoveSpriteLimit->setCheckable(true);
    ui.actRemoveSpriteLimit->setChecked(false);

    connect(ui.actRemoveSpriteLimit, &QAction::toggled, this, [this](bool checked)
        {
            nes_ppu.SetRemoveSpriteLimit(checked);

            ui.txtConsole->appendPlainText(
                checked ? "Remove 8 Sprite Limit: ON" : "Remove 8 Sprite Limit: OFF"
            );
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
    gamepadState1 = 0x00;
    gamepadState2 = 0x00;

    if (!gamepadEnabled)
    {
        nes_bus.controller_state = keyboardState1;
        nes_bus.controller_state2 = keyboardState2;
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

        nes_bus.controller_state = keyboardState1;
        nes_bus.controller_state2 = keyboardState2;
        return;
    }
    if (!gameController)
    {
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
        nes_bus.controller_state = keyboardState1;
        nes_bus.controller_state2 = keyboardState2;
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

    nes_bus.controller_state = keyboardState1 | gamepadState1;
    nes_bus.controller_state2 = keyboardState2 | gamepadState2;
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
    if (fileName.isEmpty())
        return;

    // 1. Dừng hệ thống
    timer->stop();
    system_clock_counter = 0;
    dma_dummy_counter = 0;

    for (int i = 0; i < 2048; i++)
        nes_bus.ram[i] = 0x00;

    // 2. Nạp băng game
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
    nes_bus.insertCartridge(cart);
    nes_ppu.ConnectCartridge(cart);

    // 4. Reset hệ thống
    nes_ppu.reset();
    nes_bus.n_apu.reset();

    if (cart->pMapper != nullptr)
        cart->pMapper->reset();

    nes_cpu.reset();

    uint8_t irqLo = nes_bus.cpuRead(0xFFFE);
    uint8_t irqHi = nes_bus.cpuRead(0xFFFF);
    uint16_t irqVec = (irqHi << 8) | irqLo;

    ui.txtConsole->appendPlainText(
        QString("IRQ Vector: 0x%1").arg(irqVec, 4, 16, QChar('0')).toUpper()
    );

    ui.txtConsole->appendPlainText(">>> DA NAP ROM VA RESET HE THONG: " + fileName);
    ui.txtConsole->appendPlainText(QString("MAPPER ID: %1").arg(cart->nMapperID));
    ui.txtConsole->appendPlainText(
        QString("START -> PC: 0x%1").arg(nes_cpu.pc, 4, 16, QChar('0')).toUpper()
    );

    addRecentRom(fileName);

    timer->start(16);
    ui.actPause->setText("Dừng (Pause)");
}
void SJNES::onOpenROMClicked()
{
    QSettings settings("chienz", "NesEmulator");
    QString lastPath = settings.value("LastRomPath", "D:\\").toString();

    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open ROM",
        lastPath,
        "All Files (*.*);;NES Files (*.nes);;ZIP Files (*.zip)"
    );

    if (!fileName.isEmpty()) {
        QFileInfo fileInfo(fileName);
        settings.setValue("LastRomPath", fileInfo.absolutePath());

        loadRomFile(fileName);
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
    if (nes_bus.cart == nullptr || nes_bus.cart->pMapper == nullptr) {
        return;
    }

    bool wasRunning = timer->isActive();
    timer->stop();

    // =========================
    // HARD RESET / POWER RESET
    // =========================

    // Reset clock hệ thống
    system_clock_counter = 0;
    dma_dummy_counter = 0;
    videoFrameCounter = 0;

    // Clear CPU RAM 2KB
    for (int i = 0; i < 2048; i++)
        nes_bus.ram[i] = 0x00;

    // Reset DMA
    nes_bus.dma_page = 0x00;
    nes_bus.dma_addr = 0x00;
    nes_bus.dma_data = 0x00;
    nes_bus.dma_dummy = true;
    nes_bus.dma_transfer = false;

    // Reset controller
    keyboardState1 = 0x00;
    keyboardState2 = 0x00;
    gamepadState1 = 0x00;
    gamepadState2 = 0x00;

    nes_bus.controller_state = 0x00;
    nes_bus.controller_state2 = 0x00;
    nes_bus.controller_strobe = 0;
    nes_bus.controller_shift = 0;
    nes_bus.controller_shift2 = 0;

    // Reset PPU / APU / Mapper / CPU
    nes_ppu.reset();
    nes_bus.n_apu.reset();

    if (nes_bus.cart->pMapper != nullptr) {
        nes_bus.cart->pMapper->reset();
    }

    nes_cpu.reset();

    ui.txtConsole->appendPlainText(">>> HARD RESET / POWER RESET NES");
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
    updateGamepadInput();
    int framesToRun = fastForward ? fastForwardMultiplier : 1;

    for (int ff = 0; ff < framesToRun; ff++)
    {
        static QByteArray audio_buffer;
        static double audio_accumulator = 0.0;
        const float MASTER_VOLUME = 1.8f; // thử 1.5f, 1.8f, 2.0f
        if (audio_sink != nullptr && audio_sink->bytesFree() < 8192) {
            return;
        }

        const int PPU_CYCLES_PER_FRAME = fastForward ? (89342 * fastForwardMultiplier) : 89342;

        for (int i = 0; i < PPU_CYCLES_PER_FRAME; i++) {

            nes_bus.ppu->Step();

            if (system_clock_counter % 3 == 0) {

                // VRC6 IRQ/audio chạy mỗi CPU cycle, đặt TRƯỚC CPU clock
                if (nes_bus.cart != nullptr && nes_bus.cart->pMapper != nullptr) {
                    nes_bus.cart->pMapper->irqStep();

                    if (nes_bus.cart->pMapper->irqState())
                    {
                        nes_cpu.SetIrqSource(CPU6502::IRQ_EXTERNAL);
                    }
                    else
                    {
                        nes_cpu.ClearIrqSource(CPU6502::IRQ_EXTERNAL);
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
                        if (nes_bus.cart && nes_bus.cart->pMapper)
                        {
                            exp = nes_bus.cart->pMapper->GetExpansionAudio();
                        }

                        AudioDebugChannels dbg = nes_bus.n_apu.GetDebugChannels();

                        if (nes_bus.cart && nes_bus.cart->pMapper)
                        {
                            if (auto* m85 = dynamic_cast<Mapper_085*>(nes_bus.cart->pMapper.get()))
                            {
                                m85->GetVrc7DebugChannels(
                                    dbg.vrc7Wave1,
                                    dbg.vrc7Wave2,
                                    dbg.vrc7Wave3,
                                    dbg.vrc7Wave4,
                                    dbg.vrc7Wave5,
                                    dbg.vrc7Wave6
                                );
                            }
                            else if (auto* m69 = dynamic_cast<Mapper_069*>(nes_bus.cart->pMapper.get()))
                            {
                                m69->GetExpansionDebugChannels(
                                    dbg.s5bToneA,
                                    dbg.s5bToneB,
                                    dbg.s5bToneC
                                );
                            }
                            else
                            {
                                nes_bus.cart->pMapper->GetExpansionDebugChannels(
                                    dbg.vrc6Pulse1,
                                    dbg.vrc6Pulse2,
                                    dbg.vrc6Saw
                                );
                            }
                        }

                        if (vrc6WaveWindow && vrc6WaveWindow->isVisible())
                            vrc6WaveWindow->pushChannels(dbg);

                        if (vrc7WaveWindow && vrc7WaveWindow->isVisible())
                            vrc7WaveWindow->pushChannels(dbg);

                        if (s5bWaveWindow && s5bWaveWindow->isVisible())
                            s5bWaveWindow->pushChannels(dbg);

                        left += exp;
                        right += exp;

                        left = std::clamp(left * MASTER_VOLUME, -1.0f, 1.0f);
                        right = std::clamp(right * MASTER_VOLUME, -1.0f, 1.0f);

                        audio_buffer.append(reinterpret_cast<const char*>(&left), sizeof(float));
                        audio_buffer.append(reinterpret_cast<const char*>(&right), sizeof(float));
                    }
                    else {
                        float sample = nes_bus.n_apu.GetOutputSample();

                        // Quan trọng: gọi expansion audio trước để VRC7 OPLL_calc() chạy
                        float exp = 0.0f;
                        if (nes_bus.cart && nes_bus.cart->pMapper)
                        {
                            exp = nes_bus.cart->pMapper->GetExpansionAudio();
                        }

                        AudioDebugChannels dbg = nes_bus.n_apu.GetDebugChannels();

                        if (nes_bus.cart && nes_bus.cart->pMapper)
                        {
                            if (auto* m85 = dynamic_cast<Mapper_085*>(nes_bus.cart->pMapper.get()))
                            {
                                m85->GetVrc7DebugChannels(
                                    dbg.vrc7Wave1,
                                    dbg.vrc7Wave2,
                                    dbg.vrc7Wave3,
                                    dbg.vrc7Wave4,
                                    dbg.vrc7Wave5,
                                    dbg.vrc7Wave6
                                );
                            }
                            else if (auto* m69 = dynamic_cast<Mapper_069*>(nes_bus.cart->pMapper.get()))
                            {
                                m69->GetExpansionDebugChannels(
                                    dbg.s5bToneA,
                                    dbg.s5bToneB,
                                    dbg.s5bToneC
                                );
                            }
                            else
                            {
                                nes_bus.cart->pMapper->GetExpansionDebugChannels(
                                    dbg.vrc6Pulse1,
                                    dbg.vrc6Pulse2,
                                    dbg.vrc6Saw
                                );
                            }
                        }

                        if (vrc6WaveWindow && vrc6WaveWindow->isVisible())
                        {
                            vrc6WaveWindow->pushChannels(dbg);
                        }

                        if (vrc7WaveWindow && vrc7WaveWindow->isVisible())
                        {
                            vrc7WaveWindow->pushChannels(dbg);
                        }

                        if (s5bWaveWindow && s5bWaveWindow->isVisible())
                        {
                            s5bWaveWindow->pushChannels(dbg);
                        }

                        sample += exp;

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

        int extraScanlines = nes_ppu.GetExtraScanlinesBeforeNMI();
        int extraCpuCycles = (extraScanlines * 341) / 3;

        for (int i = 0; i < extraCpuCycles; i++)
        {
            if (nes_bus.cart != nullptr && nes_bus.cart->pMapper != nullptr)
            {
                nes_bus.cart->pMapper->irqStep(); {
                    if (nes_bus.cart->pMapper->irqState())
                    {
                        nes_cpu.SetIrqSource(CPU6502::IRQ_EXTERNAL);
                    }
                    else
                    {
                        nes_cpu.ClearIrqSource(CPU6502::IRQ_EXTERNAL);
                    }
                }
            }
            if (nes_bus.dma_transfer)
            {
                if (dma_dummy_counter < 512)
                {
                    dma_dummy_counter++;
                }
                else
                {
                    nes_bus.dma_transfer = false;
                    dma_dummy_counter = 0;
                }
            }
            else
            {
                nes_cpu.clock();
            }
        }

        if (!fastForward && audio_device != nullptr && !audio_buffer.isEmpty()) {
            audio_device->write(audio_buffer.data(), audio_buffer.size());
            audio_buffer.clear();
        }

        if (fastForward) {
            audio_buffer.clear();
        }

        QImage frameImage = nes_bus.ppu->GetScreen();

        // GAME FPS: frame hình có thay đổi thật hay không
        if (lastGameFrame.isNull() || frameImage != lastGameFrame)
        {
            gameFpsCounter++;
            lastGameFrame = frameImage.copy();
        }

        // VIDEO FPS: số frame thực sự đưa ra màn hình
        bool shouldOutputVideoFrame = video60fps || (videoFrameCounter % 2 == 0);

        if (shouldOutputVideoFrame)
        {
            videoFpsCounter++;
            ui.gameScreen->setFrame(frameImage);
        }

        // Cập nhật FPS mỗi 1 giây
        const int FPS_UPDATE_MS = 250; // 250ms = cập nhật nhanh hơn

        if (fpsTimer.elapsed() >= FPS_UPDATE_MS)
        {
            double elapsedSec = fpsTimer.elapsed() / 1000.0;

            videoFpsValue = videoFpsCounter / elapsedSec;
            gameFpsValue = gameFpsCounter / elapsedSec;

            videoFpsCounter = 0;
            gameFpsCounter = 0;
            fpsTimer.restart();

            ui.gameScreen->setOverlayText(
                QString("VIDEO %1 FPS\nGAME %2 FPS")
                .arg(videoFpsValue, 0, 'f', 1)
                .arg(gameFpsValue, 0, 'f', 1)
            );
        }
        videoFrameCounter++;
        gLogBuffer.clear();
    }
} 
void SJNES::onStereoToggled(bool checked)
{
    is_stereo = checked;
    ui.actStereo->setChecked(checked);
    ui.actMono->setChecked(!checked);

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
            return true; // chặn Qt dùng Tab để chuyển focus
        }
    }

    if (event->type() == QEvent::KeyRelease)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Tab)
        {
            fastForward = false;
            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}
// XỬ LÝ PHÍM BẤM
void SJNES::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    if (event->key() == Qt::Key_F1)
    {
        if (!nes_bus.cart || !nes_bus.cart->pMapper)
        {
            ui.txtConsole->appendPlainText("Save State failed: chua nap ROM");
            return;
        }

        if (nes_bus.cart->nMapperID != 0)
        {
            ui.txtConsole->appendPlainText("Save State failed: hien chi ho tro Mapper 0");
            return;
        }

        QString path = QCoreApplication::applicationDirPath() + "/slot1_mapper0.sjs";

        if (saveStateMapper0(path))
            ui.txtConsole->appendPlainText("Save State OK: F1 -> slot1_mapper0.sjs");
        else
            ui.txtConsole->appendPlainText("Save State failed");

        return;
    }

    if (event->key() == Qt::Key_F2)
    {
        if (!nes_bus.cart || !nes_bus.cart->pMapper)
        {
            ui.txtConsole->appendPlainText("Load State failed: chua nap ROM");
            return;
        }

        if (nes_bus.cart->nMapperID != 0)
        {
            ui.txtConsole->appendPlainText("Load State failed: hien chi ho tro Mapper 0");
            return;
        }

        QString path = QCoreApplication::applicationDirPath() + "/slot1_mapper0.sjs";

        bool wasRunning = timer->isActive();
        timer->stop();

        if (loadStateMapper0(path))
            ui.txtConsole->appendPlainText("Load State OK: F2 <- slot1_mapper0.sjs");
        else
            ui.txtConsole->appendPlainText("Load State failed");

        if (wasRunning)
            timer->start(16);

        return;
    }

    if (event->key() == Qt::Key_Tab)
    {
        fastForward = true;
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

    nes_bus.controller_state = keyboardState1 | gamepadState1;
    nes_bus.controller_state2 = keyboardState2 | gamepadState2;
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
    audio_sink->setBufferSize(32768);
    audio_device = audio_sink->start();

    ui.txtConsole->appendPlainText(
        "Audio output: " + outputDevice.description()
    );
}

void SJNES::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    if (event->key() == Qt::Key_Tab)
    {
        fastForward = false;
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

    nes_bus.controller_state = keyboardState1 | gamepadState1;
    nes_bus.controller_state2 = keyboardState2 | gamepadState2;
}
bool SJNES::saveStateMapper0(const QString& path)
{
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly))
        return false;

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_0);

    out.writeRawData("SJST", 4);

    quint32 version = 1;
    quint32 mapperID = nes_bus.cart ? nes_bus.cart->nMapperID : 999;

    out << version;
    out << mapperID;

    out << system_clock_counter;
    out << dma_dummy_counter;
    out << videoFrameCounter;

    for (int i = 0; i < 2048; i++)
        out << nes_bus.ram[i];

    out << nes_bus.dma_page;
    out << nes_bus.dma_addr;
    out << nes_bus.dma_data;
    out << nes_bus.dma_dummy;
    out << nes_bus.dma_transfer;

    out << keyboardState1;
    out << keyboardState2;
    out << gamepadState1;
    out << gamepadState2;

    out << nes_bus.controller_state;
    out << nes_bus.controller_state2;
    out << nes_bus.controller_strobe;
    out << nes_bus.controller_shift;
    out << nes_bus.controller_shift2;

    nes_cpu.SaveState(out);
    nes_ppu.SaveState(out);

    return out.status() == QDataStream::Ok;
}

bool SJNES::loadStateMapper0(const QString& path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_0);

    char magic[4];

    if (in.readRawData(magic, 4) != 4)
        return false;

    if (memcmp(magic, "SJST", 4) != 0)
        return false;

    quint32 version = 0;
    quint32 mapperID = 999;

    in >> version;
    in >> mapperID;

    if (version != 1)
        return false;

    if (mapperID != 0)
        return false;

    if (!nes_bus.cart || nes_bus.cart->nMapperID != 0)
        return false;

    in >> system_clock_counter;
    in >> dma_dummy_counter;
    in >> videoFrameCounter;

    for (int i = 0; i < 2048; i++)
        in >> nes_bus.ram[i];

    in >> nes_bus.dma_page;
    in >> nes_bus.dma_addr;
    in >> nes_bus.dma_data;
    in >> nes_bus.dma_dummy;
    in >> nes_bus.dma_transfer;

    in >> keyboardState1;
    in >> keyboardState2;
    in >> gamepadState1;
    in >> gamepadState2;

    in >> nes_bus.controller_state;
    in >> nes_bus.controller_state2;
    in >> nes_bus.controller_strobe;
    in >> nes_bus.controller_shift;
    in >> nes_bus.controller_shift2;

    nes_cpu.LoadState(in);
    nes_ppu.LoadState(in);

    nes_bus.n_apu.reset();

    return in.status() == QDataStream::Ok;
}
void SJNES::closeEvent(QCloseEvent* event)
{
    // Dừng giả lập trước
    if (timer)
        timer->stop();

    // Đóng cửa sổ Audio Waveform
    if (vrc6WaveWindow)
        vrc6WaveWindow->clearSamples();

    if (vrc7WaveWindow)
        vrc7WaveWindow->clearSamples();

    if (s5bWaveWindow)
        s5bWaveWindow->clearSamples();

    // Đóng cửa sổ Mapper Viewer
    if (mapperViewerWindow)
    {
        mapperViewerWindow->close();
        mapperViewerWindow = nullptr;
    }

    // Đóng cửa sổ Sprite Viewer
    if (spriteViewerWindow)
    {
        spriteViewerWindow->close();
        spriteViewerWindow = nullptr;
    }

    // Đóng Console Log riêng
    if (ui.txtConsole)
    {
        ui.txtConsole->close();
    }

    QMainWindow::closeEvent(event);
}