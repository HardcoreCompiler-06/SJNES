#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MarioEmulatorUI.h"
#include <QTimer>
#include <QAudioSink>
#include <memory>
#include <cstdint>


// Include các thành phần giả lập
#include "CPU6502.h"
#include "PPU.h"
#include "Bus.h"
#include "Cartridge.h"

class MarioEmulatorUI : public QMainWindow
{
    Q_OBJECT

public:
    MarioEmulatorUI(QWidget* parent = nullptr);
    ~MarioEmulatorUI();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
private slots:
    void onOpenROMClicked();
    void onStepClicked();
    void onStereoToggled(bool checked);
    void runFrame();

private:
    bool pendingIRQClear = false;
    Ui::MarioEmulatorUIClass ui;
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
};