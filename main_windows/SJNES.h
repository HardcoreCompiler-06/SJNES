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
#include "SettingsDialog.h"
#include "MapperViewerWindow.h"
#include "CPU6502.h"
#include "PPU.h"
#include "Bus.h"
#include <QMap>
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
#include "Aviwriter.h"
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
    void onRealGameFrameTicked();
    void onNsfConsoleMessage(QString msg);
    void onNsfTrackChanged(int current, int total);
    void onNsfExpansionDetected(bool vrc6, bool s5b, bool vrc7, bool mmc5, bool n163, bool fds);
    void onToggleRecording();
private:
    EQWindow* eqWindow = nullptr;
    SettingsDialog* settingsDialog = nullptr;

    QAction* actVRC6Pulse1 = nullptr;
    QAction* actVRC6Pulse2 = nullptr;
    QAction* actVRC6Saw = nullptr;
    QAction* actS5BToneA = nullptr;
    QAction* actS5BToneB = nullptr;
    QAction* actS5BToneC = nullptr;
    // N163: true = 8 kênh ghép qua 1 DAC như chip thật, false = chia trung bình (mặc định)
    QAction* actN163Accurate = nullptr;
    QAction* actN163[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    QAction* actVRC7[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    QAction* actMMC5Pulse1 = nullptr;
    QAction* actMMC5Pulse2 = nullptr;
    QAction* actMMC5PCM = nullptr;
    QAction* actZapperEnabled = nullptr;
    float eqGains[9] = {};
    QMap<QString, float> channelGains;
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
    bool showDebugOverlay = false;   // F10 lần 2: thêm dòng CPU/PPU debug
    QString fpsOverlayText;          // 2 dòng VIDEO/GAME FPS, cập nhật mỗi 500ms
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
    AudioWaveWindow* fdsWaveWindow = nullptr;
    MapperViewerWindow* mapperViewerWindow = nullptr;
    SpriteViewerWindow* spriteViewerWindow = nullptr;
    PaletteViewerWindow* paletteViewerWindow = nullptr;
    bool gameFullScreen = false;
    void enterGameFullScreen();
    void exitGameFullScreen();
    void toggleGameFullScreen();
    // Cửa sổ game riêng (chỉ chứa khung NES) cho OBS / RTSS - bật tắt bằng F9
    QWidget* gameWindow = nullptr;
    void toggleGameWindow();
    bool isPaused = false;
    bool video60fps = true;
    uint64_t videoFrameCounter = 0;
    void restartAudioSink();
    bool audioRestartPending = false;
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
    bool nsfUiActive = false;
    void on_actPower_triggered();
    bool holdturboA = false;
    bool holdturboB = false;
    void on_chkAutoA_toggled(bool checked);
    void on_chkAutoB_toggled(bool checked);
    AviWriter aviWriter;

};