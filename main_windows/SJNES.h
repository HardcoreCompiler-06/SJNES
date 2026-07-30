#pragma once
#include "APU.h"
#include <QtWidgets/QMainWindow>
#include "NSFFile.h"
#include <vector>
#include "ui_SJNES.h"
#include "EQWindow.h"
#include <QTimer>
#include <QAudioSink>
#include <memory>
#include <cstdint>
#include "AudioWaveWindow.h"
#include "MapperViewerWindow.h"
#include "CPU6502.h"
#include "PPU.h"
#include "Bus.h"
#include "Cartridge.h"
#include "SpriteViewerWindow.h"
#include <QElapsedTimer>
#include <QImage>
#include <QMediaDevices>
#include <QList>
#include <QAction>
#include <QCloseEvent>
#include "PaletteViewerWindow.h"
#include "Mapper_024.h"
#include "Mapper_069.h"
#include "Mapper_085.h"
#include "Mapper_005.h"
#include "Mapper_019.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <QThread>
#include "EmuWorker.h"
class SJNES : public QMainWindow
{
    Q_OBJECT

public:
    SJNES(QWidget* parent = nullptr);
    ~SJNES();

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
private slots:
    void onOpenROMClicked();
    void onStepClicked();
    void onStereoToggled(bool checked);
    void onFrameReady(QImage frame);
    void onAudioReady(std::vector<float> samples, bool stereo);
    void onDebugChannelsReady(std::vector<AudioDebugChannels> dbgBatch);
    void onGameFrameTicked();
    void onNsfConsoleMessage(QString msg);
    void onNsfTrackChanged(int current, int total);
    void onNsfExpansionDetected(bool vrc6, bool s5b, bool vrc7, bool mmc5, bool n163);
private:
    EQWindow* eqWindow = nullptr;
    float eqGains[9] = {};
    bool eqEnabled = false;
    QThread* emuThread = nullptr;
    EmuWorker* worker = nullptr;
    void restartCurrentNSFTrack();
    void nextNSFTrack();
    void previousNSFTrack();
    void loadNSFFile(const QString& fileName);
    QList<QAction*> recentRomActions;
    void addRecentRom(const QString& filePath);
    void updateRecentRomMenu();
    void loadRomFile(const QString& fileName);
    QElapsedTimer fpsTimer;
    int videoFpsCounter = 0;
    int gameFpsCounter = 0;
    bool showFpsOverlay = false;
    double videoFpsValue = 0.0;
    double gameFpsValue = 0.0;
    QImage lastGameFrame;
    bool pendingIRQClear = false;
    Ui::SJNESClass ui;
    void onResetClicked();
    // Các thiết bị NES
    Bus nes_bus;
    CPU6502 nes_cpu;
    PPU nes_ppu;

    // Động cơ thời gian UI
    QTimer* timer;

    // Biến đếm nhịp hệ thống (Bí kíp của OLC)
    uint32_t system_clock_counter = 0;
    uint16_t dma_dummy_counter = 0;

    // Hệ thống âm thanh
    QAudioSink* audio_sink = nullptr;
    QIODevice* audio_device = nullptr;
    bool is_stereo = true;
    //filter
    bool pixelPerfectMode = false;
    AudioWaveWindow* nesWaveWindow = nullptr;
    AudioWaveWindow* vrc6WaveWindow = nullptr;
    AudioWaveWindow* vrc7WaveWindow = nullptr;
    AudioWaveWindow* s5bWaveWindow = nullptr;
    AudioWaveWindow* mmc5WaveWindow = nullptr;
    AudioWaveWindow* n163WaveWindow = nullptr;
    MapperViewerWindow* mapperViewerWindow = nullptr;
    SpriteViewerWindow* spriteViewerWindow = nullptr;
    PaletteViewerWindow* paletteViewerWindow = nullptr;
    bool gameFullScreen = false;
    void enterGameFullScreen();
    void exitGameFullScreen();
    void toggleGameFullScreen();
    bool isPaused = false;
    bool video60fps = false;
    uint64_t videoFrameCounter = 0;
    void restartAudioSink();
    QMediaDevices audioDevices;
    bool fastForward = false;
    int fastForwardMultiplier = 3;
    void initGamepad();
    void shutdownGamepad();
    void updateGamepadInput();
    uint8_t keyboardState2 = 0x00;
    uint8_t gamepadState2 = 0x00;
    SDL_GameController* gameController = nullptr;
    bool gamepadEnabled = true;
    uint8_t keyboardState1 = 0x00;
    uint8_t gamepadState1 = 0x00;
    bool saveStateMapper0(const QString& path);
    bool loadStateMapper0(const QString& path);
    bool machinePowered = true;
    void on_actPower_triggered();
	bool holdturboA = false;
	bool holdturboB = false;
    void on_chkAutoA_toggled(bool checked);
    void on_chkAutoB_toggled(bool checked);
};