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
        NES,
        VRC6,
        VRC7,
        S5B,
        MMC5,
        N163
    };
    AudioWaveWindow(WaveMode mode, QWidget* parent = nullptr);
    void pushChannels(const AudioDebugChannels& ch);
    void clearSamples();
    struct TrigLockState
    {
        qint64 lockAbsPos = 0;
        float  period = 0.0f;
        bool   locked = false;
    };
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
    QVector<float> phaseBuffers[CHANNEL_COUNT];
    float lastVisual[CHANNEL_COUNT]{};
    float periodHint[CHANNEL_COUNT]{};
    QVector<float> n163SmoothY[CHANNEL_COUNT];
    bool n163SmoothValid[CHANNEL_COUNT]{};
    float n163LastTrigger[CHANNEL_COUNT]{};
    float n163Period[CHANNEL_COUNT]{};
    int n163LastBufferSize[CHANNEL_COUNT]{};
    bool n163HasTrigger[CHANNEL_COUNT]{};
    quint64 totalPushed[CHANNEL_COUNT]{};
    TrigLockState genericTrigLock[CHANNEL_COUNT]; 
    TrigLockState n163TrigLock[CHANNEL_COUNT];    
    float vrc7LastPhaseStart[CHANNEL_COUNT]{};
    bool vrc7HasPhaseStart[CHANNEL_COUNT]{};
    QVector<float> vrc7CorrRef[CHANNEL_COUNT];
    bool vrc7CorrValid[CHANNEL_COUNT]{};
    QTimer* refreshTimer = nullptr;
    QMutex mutex;
    WaveMode mode;
    int activeChannelCount = 8;
    bool triangleZeroGate[CHANNEL_COUNT]{};
};