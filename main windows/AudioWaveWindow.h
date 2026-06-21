#pragma once
// OLD_FEEL_EXCEPT_N163 + VRC7 CorrScope lock + fixed X anchor: giữ bản ổn, chỉ đổi trôi ngang thành co/dãn ngang.

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

    // Trạng thái phase-lock (PLL) cho trigger — dùng chung cho nhánh VRC7/S5B/MMC5/Pulse
    // (genericTrigLock) và riêng cho N163 row-sync (n163TrigLock). Lưu theo absolute
    // sample index (qua totalPushed) để không bị lệch khi buffers[] bị trim phần đầu.
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

    // VRC7: lưu phase carrier song song với từng sample waveform.
    // Nếu chỉ dùng phase hiện tại mà không có phase theo từng sample, đoạn history vẫn bị lệch/trôi.
    QVector<float> phaseBuffers[CHANNEL_COUNT];
    float lastVisual[CHANNEL_COUNT]{};

    // Period hint thật từ APU/mapper, tính theo audio sample / chu kỳ.
    // AudioWaveWindow dùng cái này làm anchor để chống trôi ngang, thay vì đo period từ waveform.
    float periodHint[CHANNEL_COUNT]{};

    // N163: làm mượt theo từng điểm X, nhưng cả hàng cập nhật cùng một frame.
    QVector<float> n163SmoothY[CHANNEL_COUNT];
    bool n163SmoothValid[CHANNEL_COUNT]{};

    // N163: giữ trigger ổn định để sóng không trôi ngang/rung ngang
    // (legacy — không còn được dùng để tính fStart, chỉ còn reset trong clearSamples()).
    float n163LastTrigger[CHANNEL_COUNT]{};
    float n163Period[CHANNEL_COUNT]{};
    int n163LastBufferSize[CHANNEL_COUNT]{};
    bool n163HasTrigger[CHANNEL_COUNT]{};

    // Bộ đếm tuyệt đối, KHÔNG reset khi buffers[] bị trim phần đầu — dùng để quy đổi
    // lockAbsPos sang index của buffer hiện tại một cách an toàn qua các lần trim.
    quint64 totalPushed[CHANNEL_COUNT]{};

    // PLL trigger-lock: tránh chọn lại cạnh trigger khác nhau mỗi frame khi có nhiều
    // ứng viên gần nhau (VRC7 multi-operator FM, N163 wavetable nhiều bậc).
    TrigLockState genericTrigLock[CHANNEL_COUNT]; // VRC7 / S5B / MMC5 / Pulse 1-2...
    TrigLockState n163TrigLock[CHANNEL_COUNT];    // riêng cho N163 row-sync

    // VRC7: phase start cuối cùng, dùng khi phase buffer chưa đủ dữ liệu trong vài frame đầu.
    float vrc7LastPhaseStart[CHANNEL_COUNT]{};
    bool vrc7HasPhaseStart[CHANNEL_COUNT]{};

    // VRC7 CorrScope-style lock: fingerprint của frame trước để chọn phase-wrap ổn định nhất.
    // Không smooth waveform, chỉ chọn fStart giống CorrScope hơn.
    QVector<float> vrc7CorrRef[CHANNEL_COUNT];
    bool vrc7CorrValid[CHANNEL_COUNT]{};

    QTimer* refreshTimer = nullptr;
    QMutex mutex;

    WaveMode mode;
    int activeChannelCount = 8;

};