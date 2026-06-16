#pragma once

#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVector>
#include <QMutex>
#include <QString>
#include "APU.h"

class AudioWaveWindow : public QOpenGLWidget
{
    Q_OBJECT

public:
    enum class WaveMode
    {
        VRC6,
        VRC7,
        S5B
    };

    AudioWaveWindow(WaveMode mode, QWidget* parent = nullptr);

    void pushChannels(const AudioDebugChannels& ch);
    void clearSamples();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    static constexpr int CHANNEL_COUNT = 11;
    static constexpr int MAX_SAMPLES = 8192;
    static constexpr int DISPLAY_SAMPLES = 2048;

    QString names[CHANNEL_COUNT];
    QVector<float> buffers[CHANNEL_COUNT];
    float lastVisual[CHANNEL_COUNT]{};

    QTimer* refreshTimer = nullptr;
    QMutex mutex;

    WaveMode mode;
    int activeChannelCount = 8;
 
};