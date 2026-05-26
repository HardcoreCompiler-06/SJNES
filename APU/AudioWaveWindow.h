#pragma once

#include <QWidget>
#include <QVector>
#include <QMutex>
#include <QTimer>
#include <QString>
#include "APU.h"

class AudioWaveWindow : public QWidget
{
    Q_OBJECT

public:
    explicit AudioWaveWindow(QWidget* parent = nullptr);

    void pushChannels(const AudioDebugChannels& ch);
    void clearSamples();
    void paintEvent(QPaintEvent* event) override;

private:
    static constexpr int MAX_SAMPLES = 16384;
    static constexpr int DISPLAY_SAMPLES = 1024;
    static constexpr int CHANNEL_COUNT = 8;
    QVector<float> buffers[CHANNEL_COUNT];
    QString names[CHANNEL_COUNT];
    float lastVisual[CHANNEL_COUNT] = {};
    QMutex mutex;
    QTimer* refreshTimer = nullptr;
};