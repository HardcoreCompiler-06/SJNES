#include "AudioWaveWindow.h"
#include <QPainterPath>
#include <QPainter>
#include <QMutexLocker>
#include <QPen>
#include <QColor>
#include <algorithm>
#include <cmath>
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QOpenGLFunctions>

// Màu sóng: trắng đồng bộ
static const QColor CHANNEL_COLORS[] = {
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
};

// Catmull-Rom cubic interpolation
static float catmullRom(float p0, float p1, float p2, float p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}

// Lấy sample với Catmull-Rom (index có thể là float)
static float sampleInterp(const QVector<float>& buf, float fidx)
{
    int n = buf.size();
    int i1 = (int)fidx;
    float t  = fidx - i1;

    int i0 = std::max(i1 - 1, 0);
    int i2 = std::min(i1 + 1, n - 1);
    int i3 = std::min(i1 + 2, n - 1);

    return catmullRom(buf[i0], buf[i1], buf[i2], buf[i3], t);
}

// CorScope-style trigger: tìm rising zero-crossing ổn định nhất
// Quét ngược trong cửa sổ tìm kiếm, ưu tiên crossing có slope mạnh nhất
// Trả về index float (sub-sample precision)
static float findTrigger(const QVector<float>& buf, int searchStart, int searchEnd, float trigLevel)
{
    float bestIdx   = float(searchStart);
    float bestSlope = -1.0f;
    bool  found     = false;

    for (int i = searchEnd; i >= searchStart + 1; i--)
    {
        float prev = buf[i - 1];
        float cur  = buf[i];

        if (prev < trigLevel && cur >= trigLevel)
        {
            float slope = cur - prev;  // manh hơn = ổn định hơn

            if (!found || slope > bestSlope)
            {
                bestSlope = slope;
                float frac = (slope > 0.0f) ? (trigLevel - prev) / slope : 0.0f;
                bestIdx = float(i - 1) + frac;
                found = true;

                // Nếu slope đủ mạnh rồi thì dừng ngay, không cần quét tiếp
                if (slope > 0.3f)
                    break;
            }
        }
    }

    return bestIdx;
}

AudioWaveWindow::AudioWaveWindow(WaveMode mode, QWidget* parent)
    : QOpenGLWidget(parent), mode(mode)
{
    setWindowTitle("Audio Waveform Debug");
    if (mode == WaveMode::VRC7)
    {
        resize(1100, 900);
        setMinimumSize(900, 820);
    }
    else
    {
        resize(1100, 780);
        setMinimumSize(900, 700);
    }

    QSurfaceFormat fmt;
    fmt.setSamples(4);
    setFormat(fmt);

    activeChannelCount = 8;

    names[0] = "Pulse 1";
    names[1] = "Pulse 2";
    names[2] = "Triangle";
    names[3] = "Noise";
    names[4] = "DMC";

    if (mode == WaveMode::VRC6)
    {
        setWindowTitle("NES + VRC6 Waveform Debug");
        activeChannelCount = 8;
        names[5] = "VRC6 Pulse 1";
        names[6] = "VRC6 Pulse 2";
        names[7] = "VRC6 Saw";
    }
    else if (mode == WaveMode::VRC7)
    {
        setWindowTitle("VRC7 Waveform Debug");

        // VRC7 mode: chỉ hiện 6 kênh VRC7, bỏ 5 sóng NES
        activeChannelCount = 6;

        names[0] = "VRC7 CH 1";
        names[1] = "VRC7 CH 2";
        names[2] = "VRC7 CH 3";
        names[3] = "VRC7 CH 4";
        names[4] = "VRC7 CH 5";
        names[5] = "VRC7 CH 6";
    }
    else if (mode == WaveMode::S5B)
    {
        setWindowTitle("NES + Sunsoft 5B Waveform Debug");
        activeChannelCount = 8;
        names[5] = "S5B Tone A";
        names[6] = "S5B Tone B";
        names[7] = "S5B Tone C";
    }

    for (int i = 0; i < activeChannelCount; i++)
        buffers[i].reserve(MAX_SAMPLES);

    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, [this]() { update(); });
    refreshTimer->start(16);
}

void AudioWaveWindow::pushChannels(const AudioDebugChannels& ch)
{
    QMutexLocker lock(&mutex);

    float values[CHANNEL_COUNT]{};

    if (mode == WaveMode::VRC7)
    {
        // VRC7 mode: bỏ 5 kênh NES, đưa 6 kênh VRC7 lên 6 dòng đầu
        values[0] = ch.vrc7Wave1;
        values[1] = ch.vrc7Wave2;
        values[2] = ch.vrc7Wave3;
        values[3] = ch.vrc7Wave4;
        values[4] = ch.vrc7Wave5;
        values[5] = ch.vrc7Wave6;
    }
    else
    {
        // VRC6/S5B mode: vẫn giữ 5 kênh NES ở trên
        values[0] = ch.pulse1;
        values[1] = ch.pulse2;
        values[2] = ch.triangle;
        values[3] = ch.noise;
        values[4] = ch.dmc;

        if (mode == WaveMode::VRC6)
        {
            values[5] = ch.vrc6Pulse1;
            values[6] = ch.vrc6Pulse2;
            values[7] = ch.vrc6Saw;
        }
        else if (mode == WaveMode::S5B)
        {
            values[5] = ch.s5bToneA;
            values[6] = ch.s5bToneB;
            values[7] = ch.s5bToneC;
        }
    }

    for (int i = 0; i < activeChannelCount; i++)
    {
        float v = std::clamp(values[i], -1.0f, 1.0f);

        if (mode != WaveMode::VRC7 && (i == 3 || i == 4)) { v *= 1.6f; v = std::clamp(v, -1.0f, 1.0f); }

        if (mode == WaveMode::VRC6)
        {
            if (i == 5 || i == 6) { v *= 1.4f; v = std::clamp(v, -1.0f, 1.0f); }
            if (i == 7)           { v *= 1.65f; v = std::clamp(v, -1.0f, 1.0f); }
        }
        if (mode == WaveMode::VRC7)
        {
            // VRC7 only: giảm scale để CH3/CH4 không bị kịch đỉnh
            float vrc7Scale = 18.0f;

            // CH3/CH4 thường mạnh hơn, giảm riêng một chút
            if (i == 2 || i == 3)
                vrc7Scale = 14.0f;

            v *= vrc7Scale;
            v = std::clamp(v, -1.0f, 1.0f);
        }
        if (mode == WaveMode::S5B  && i >= 5) { v *= 1.4f;  v = std::clamp(v, -1.0f, 1.0f); }

        buffers[i].push_back(v);

        if (buffers[i].size() > MAX_SAMPLES + 512)
            buffers[i].remove(0, buffers[i].size() - MAX_SAMPLES);
    }
}

void AudioWaveWindow::clearSamples()
{
    QMutexLocker lock(&mutex);
    for (int i = 0; i < activeChannelCount; i++)
    {
        buffers[i].clear();
        lastVisual[i] = 0.0f;
    }
}

void AudioWaveWindow::paintGL()
{
    QVector<float> copy[CHANNEL_COUNT];
    {
        QMutexLocker lock(&mutex);
        for (int i = 0; i < CHANNEL_COUNT; i++)
            copy[i] = buffers[i];
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(0, 0, 0));

    int w = width();
    int h = height();
    int topMargin    = 8;
    int bottomMargin = 8;
    int drawH = h - topMargin - bottomMargin;

    if (w <= 0 || h <= 0) return;

    int panelH = drawH / activeChannelCount;

    for (int c = 0; c < activeChannelCount; c++)
    {
        int top  = topMargin + c * panelH;
        int midY = top + panelH / 2;

        p.fillRect(0, top, w, panelH, QColor(0, 0, 0));

        // Đường kẻ giữa mờ
        p.setPen(QPen(QColor(40, 40, 40), 1));
        p.drawLine(0, midY, w, midY);

        // Tên kênh màu theo channel
        p.setPen(CHANNEL_COLORS[c]);
        p.drawText(10, top + 18, names[c]);

        if (copy[c].size() < 32)
            continue;

        // --- Số sample hiển thị ---
        const int n = copy[c].size();

        int visibleSamples = DISPLAY_SAMPLES;

        if (mode == WaveMode::VRC7)
        {
            // VRC7 mode chỉ có 6 kênh VRC7, cho mỗi sóng to và dễ nhìn hơn
            visibleSamples = DISPLAY_SAMPLES / 2;
        }
        else
        {
            // NES Pulse: trigger và zoom vừa phải
            if (c == 0 || c == 1)
            {
                visibleSamples = DISPLAY_SAMPLES / 3;
            }

            // Noise/DMC: KHÔNG trigger, cho thưa bớt để đỡ rối
            if (c == 3 || c == 4)
            {
                visibleSamples = DISPLAY_SAMPLES / 3;
            }
        }

        // Expansion channels của VRC6/S5B
        if (mode != WaveMode::VRC7 && c >= 5)
        {
            if (mode == WaveMode::VRC6)
            {
                if (c == 5 || c == 6)
                {
                    // VRC6 Pulse
                    visibleSamples = DISPLAY_SAMPLES / 3;
                }
                else if (c == 7)
                {
                    // VRC6 Saw: giữ trigger bị khóa, nhưng zoom không bị khóa cứng.
                    // Mặc định để sóng chạy tự nhiên; chỉ tự chỉnh nhẹ khi quá ít/quá nhiều răng.
                    visibleSamples = DISPLAY_SAMPLES;

                    const auto& buf = copy[c];
                    int resetA = -1;
                    int resetB = -1;

                    // Tìm 2 cạnh rơi/reset gần nhất để ước lượng chu kỳ hiện tại.
                    for (int i = n - 1; i >= 1; i--)
                    {
                        float prev = buf[i - 1];
                        float cur = buf[i];

                        if ((prev - cur) > 0.06f)
                        {
                            if (resetB < 0)
                                resetB = i;
                            else
                            {
                                resetA = i;
                                break;
                            }
                        }
                    }

                    if (resetA >= 0 && resetB > resetA)
                    {
                        int period = resetB - resetA;
                        period = std::clamp(period, 8, DISPLAY_SAMPLES * 2);

                        // Soft adaptive:
                        // - Âm thấp: nếu thấy dưới 4 răng thì nới cửa sổ để đỡ to ngang.
                        // - Âm cao: nếu thấy trên 9 răng thì thu cửa sổ lại để đỡ dày đặc.
                        // - Ở giữa thì giữ DISPLAY_SAMPLES để sóng vẫn tự do, không bị cố định quá.
                        float teeth = float(visibleSamples) / float(period);

                        if (teeth < 4.0f)
                            visibleSamples = period * 4;
                        else if (teeth > 9.0f)
                            visibleSamples = period * 9;

                        visibleSamples = std::clamp(visibleSamples, 128, DISPLAY_SAMPLES * 3);

                        if (visibleSamples > n - 4)
                            visibleSamples = n - 4;
                    }
                }
            }
            else if (mode == WaveMode::S5B)
            {
                visibleSamples = DISPLAY_SAMPLES / 3;
            }
            else if (mode == WaveMode::VRC7)
            {
                visibleSamples = DISPLAY_SAMPLES / 2;
            }
        }

        if (visibleSamples < 32 || n < visibleSamples + 4)
            continue;

        // --- Tính min/max để trigger ---
        float minV = copy[c][0];
        float maxV = copy[c][0];

        for (int i = 1; i < n; i++)
        {
            minV = std::min(minV, copy[c][i]);
            maxV = std::max(maxV, copy[c][i]);
        }

        float range = maxV - minV;
        float triggerLevel = (minV + maxV) * 0.5f;

        // --- Tìm điểm bắt đầu bằng trigger ---
        float fStart = float(n - visibleSamples); // fallback

        bool isNoiseDMC = (mode != WaveMode::VRC7 && (c == 3 || c == 4));
        bool isVrc6Saw = (mode == WaveMode::VRC6 && c == 7);

        // Tất cả đều có trigger, trừ Noise và DMC
        if (range > 0.02f && !isNoiseDMC)
        {
            if (isVrc6Saw)
            {
                // VRC6 Saw: khóa trigger theo cạnh rơi/reset của răng cưa.
                // Zoom chỉ soft-adaptive ở phần visibleSamples, nên sóng vẫn tự do hơn.
                int searchStart = n - visibleSamples - 1;

                if (searchStart < 1)
                    searchStart = 1;

                for (int i = searchStart; i >= 1; i--)
                {
                    float prev = copy[c][i - 1];
                    float cur = copy[c][i];

                    // Saw reset: giá trị tụt mạnh
                    if ((prev - cur) > 0.06f)
                    {
                        // Đặt điểm reset hơi lệch vào trong màn hình, nhìn ổn định hơn
                        fStart = float(i) - float(visibleSamples) * 0.10f;
                        break;
                    }
                }
            }
            else
            {
                // Các kênh còn lại: trigger theo cạnh đi lên qua mức giữa
                int searchStart = n - visibleSamples - 1;

                if (searchStart < 1)
                    searchStart = 1;

                for (int i = searchStart; i >= 1; i--)
                {
                    if (copy[c][i - 1] < triggerLevel && copy[c][i] >= triggerLevel)
                    {
                        float slope = copy[c][i] - copy[c][i - 1];
                        float frac = (slope > 0.0f) ? (triggerLevel - copy[c][i - 1]) / slope : 0.0f;
                        fStart = float(i - 1) + frac;
                        break;
                    }
                }
            }
        }

        fStart = std::clamp(fStart, 0.0f, float(n - visibleSamples - 2));

        // --- Amp ---
        float amp = panelH * 0.45f;

        if (mode == WaveMode::VRC7)
        {
            // Chỉ còn 6 sóng VRC7 nên cho biên độ hiển thị to hơn một chút
            amp = panelH * 0.50f;
        }
        else
        {
            if (c == 0 || c == 1)   amp = panelH * 0.48f;
            if (c == 3 || c == 4)   amp = panelH * 0.42f;
            if (c >= 5)
            {
                if (mode == WaveMode::VRC6)
                    amp = (c == 7) ? panelH * 0.58f : panelH * 0.48f;
                else if (mode == WaveMode::S5B)
                    amp = panelH * 0.30f;
            }
        }

        // --- Vẽ với Catmull-Rom interpolation ---
        bool stepWave = (c == 0 || c == 1)
            || (mode == WaveMode::VRC6 && c >= 5)
            || (mode == WaveMode::S5B  && c >= 5);

        QPen pen(CHANNEL_COLORS[c]);
        if (mode == WaveMode::VRC7)
            pen.setWidthF(2.0f);
        else if (mode == WaveMode::VRC6 && c == 7)
            pen.setWidthF(2.0f);
        else
            pen.setWidthF((c == 0 || c == 1 || c >= 5) ? 1.8f : 1.2f);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);

        QPainterPath path;

        // Số điểm vẽ: nhiều hơn để mượt
        int drawPoints = w * 2; // sub-pixel resolution

        for (int i = 0; i < drawPoints; i++)
        {
            float fIdx = fStart + (float(i) / float(drawPoints - 1)) * float(visibleSamples);

            if (fIdx < 0 || fIdx >= float(n - 1)) continue;

            float x = float(i) / float(drawPoints - 1) * float(w);
            float y;

            if (stepWave)
            {
                // Step wave: dùng nearest sample (không interpolate)
                int idx = int(fIdx);
                idx = std::clamp(idx, 0, n - 1);
                y = midY - copy[c][idx] * amp;

                if (i == 0) { path.moveTo(x, y); }
                else
                {
                    float prevFIdx = fStart + (float(i-1) / float(drawPoints-1)) * float(visibleSamples);
                    int prevIdx = std::clamp(int(prevFIdx), 0, n-1);
                    float prevY = midY - copy[c][prevIdx] * amp;
                    path.lineTo(x, prevY);
                    path.lineTo(x, y);
                }
            }
            else
            {
                // Smooth wave: Catmull-Rom interpolation
                y = midY - sampleInterp(copy[c], fIdx) * amp;
                if (i == 0) path.moveTo(x, y);
                else        path.lineTo(x, y);
            }
        }

        p.drawPath(path);
    }
}

void AudioWaveWindow::initializeGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void AudioWaveWindow::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);
}
