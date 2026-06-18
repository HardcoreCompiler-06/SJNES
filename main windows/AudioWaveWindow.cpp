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
        if (mode == WaveMode::NES)
        {
            resize(1100, 560);
            setMinimumSize(900, 500);
        }
        else if (mode == WaveMode::VRC7)
        {
            resize(1100, 720);
            setMinimumSize(900, 620);
        }
        else
        {
            resize(1100, 780);
            setMinimumSize(900, 700);
        }
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

    if (mode == WaveMode::NES)
    {
        setWindowTitle("NES 5 Channels Waveform Debug");
        activeChannelCount = 5;
    }
    else if (mode == WaveMode::VRC6)
    {
        setWindowTitle("NES + VRC6 Waveform Debug");
        activeChannelCount = 8;

        names[5] = "VRC6 Pulse 1";
        names[6] = "VRC6 Pulse 2";
        names[7] = "VRC6 Saw";
    }
    else if (mode == WaveMode::VRC7)
    {
        setWindowTitle("VRC7 6 Channels Waveform Debug");
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
    else if (mode == WaveMode::MMC5)
    {
        setWindowTitle("NES + MMC5 Waveform Debug");
        activeChannelCount = 8;

        names[5] = "MMC5 Pulse 1";
        names[6] = "MMC5 Pulse 2";
        names[7] = "MMC5 PCM";
    }
    else if (mode == WaveMode::N163)
    {
        setWindowTitle("Namco 163 Waveform Debug");

        activeChannelCount = 8;

        names[0] = "N163 CH 1";
        names[1] = "N163 CH 2";
        names[2] = "N163 CH 3";
        names[3] = "N163 CH 4";
        names[4] = "N163 CH 5";
        names[5] = "N163 CH 6";
        names[6] = "N163 CH 7";
        names[7] = "N163 CH 8";
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
    if (mode == WaveMode::N163)
    {
        values[0] = ch.n163Wave1;
        values[1] = ch.n163Wave2;
        values[2] = ch.n163Wave3;
        values[3] = ch.n163Wave4;
        values[4] = ch.n163Wave5;
        values[5] = ch.n163Wave6;
        values[6] = ch.n163Wave7;
        values[7] = ch.n163Wave8;
    }
    else if (mode == WaveMode::VRC7)
    {
        // VRC7-only: 6 kênh VRC7 nằm ở dòng 0..5
        values[0] = ch.vrc7Wave1;
        values[1] = ch.vrc7Wave2;
        values[2] = ch.vrc7Wave3;
        values[3] = ch.vrc7Wave4;
        values[4] = ch.vrc7Wave5;
        values[5] = ch.vrc7Wave6;
    }
    else
    {
        // NES / VRC6 / S5B đều có 5 kênh NES ở dòng 0..4
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
        else if (mode == WaveMode::MMC5)
        {
            values[5] = ch.mmc5Pulse1;
            values[6] = ch.mmc5Pulse2;
            values[7] = ch.mmc5PCM;
        }
    }

    for (int i = 0; i < activeChannelCount; i++)
    {
        float v = std::clamp(values[i], -1.0f, 1.0f);

        if (mode != WaveMode::VRC7 && (i == 3 || i == 4))
        {
            v *= 1.6f;
            v = std::clamp(v, -1.0f, 1.0f);
        }

        // VRC6: dòng 5/6 là pulse, dòng 7 là saw
        if (mode == WaveMode::VRC6)
        {
            if (i == 5 || i == 6)
            {
                v *= 1.4f;
                v = std::clamp(v, -1.0f, 1.0f);
            }

            if (i == 7)
            {
                v *= 1.65f;
                v = std::clamp(v, -1.0f, 1.0f);
            }
        }

        // VRC7-only: 6 kênh nằm ở dòng 0..5
        if (mode == WaveMode::VRC7)
        {
            float vrc7Scale = 7.0f;

            // CH3/CH4 dễ kịch đầu nên giảm riêng
            if (i == 2 || i == 3)
                vrc7Scale = 5.5f;

            v *= vrc7Scale;

            // chừa headroom để không đập sát mép trên/dưới
            v = std::clamp(v, -0.95f, 0.95f);
        }

        if (mode == WaveMode::N163)
        {
            // N163 raw thường nhỏ, cần phóng lên để nhìn rõ
            float n163Scale = 6.0f;

            v *= n163Scale;
            v = std::clamp(v, -0.95f, 0.95f);
        }


        // S5B: dòng 5..7
        if (mode == WaveMode::S5B && i >= 5)
        {
            v *= 1.4f;
            v = std::clamp(v, -1.0f, 1.0f);
        }

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

        n163LastTrigger[i] = 0.0f;
        n163Period[i] = 0.0f;
        n163LastBufferSize[i] = 0;
        n163HasTrigger[i] = false;
        n163SmoothY[i].clear();
        n163SmoothValid[i] = false;
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
        else if (mode == WaveMode::N163)
        {
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
        if (mode != WaveMode::VRC7 && mode != WaveMode::N163 && c >= 5)
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
            else if (mode == WaveMode::MMC5)
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
        float fStart = float(n - visibleSamples); 

        bool isNoiseDMC = (mode != WaveMode::VRC7 && mode != WaveMode::N163 && (c == 3 || c == 4));
        bool isVrc6Saw = (mode == WaveMode::VRC6 && c == 7);

        if (range > 0.02f && !isNoiseDMC)
        {
            if (mode == WaveMode::N163)
            {
                // N163 wavetable có nhiều điểm crossing giống nhau.
                // Trigger thường sẽ nhảy qua lại giữa các crossing => nhìn như trôi ngang/rung ngang.
                // Cách này vẫn vẽ sóng thật từ buffer như cũ, nhưng chỉ chọn crossing mạnh và gần phase cũ nhất.
                const float preTrigger = float(visibleSamples) * 0.10f;
                const float maxTrigger = float(n) - float(visibleSamples) + preTrigger;

                if (n163LastBufferSize[c] > 0 && n < n163LastBufferSize[c])
                {
                    // Buffer vừa bị cắt đầu, index cũ không còn đáng tin.
                    n163HasTrigger[c] = false;
                }

                QVector<float> trigIdx;
                QVector<float> trigSlope;
                trigIdx.reserve(128);
                trigSlope.reserve(128);

                int scanStart = std::max(1, n - visibleSamples * 4);
                int scanEnd = std::min(n - 2, int(maxTrigger));

                float minSlope = std::max(0.0001f, range * 0.08f);
                float strongestSlope = 0.0f;

                for (int i = scanStart + 1; i <= scanEnd; i++)
                {
                    float prev = copy[c][i - 1];
                    float cur = copy[c][i];

                    if (prev < triggerLevel && cur >= triggerLevel)
                    {
                        float slope = cur - prev;
                        if (slope >= minSlope)
                        {
                            float frac = (triggerLevel - prev) / slope;
                            float idx = float(i - 1) + frac;

                            trigIdx.push_back(idx);
                            trigSlope.push_back(slope);
                            strongestSlope = std::max(strongestSlope, slope);
                        }
                    }
                }

                if (!trigIdx.isEmpty())
                {
                    float slopeGate = strongestSlope * 0.55f;
                    float target = maxTrigger;

                    if (n163HasTrigger[c] && n163Period[c] >= 8.0f)
                    {
                        // Dự đoán cạnh cùng pha gần vị trí hiển thị hiện tại nhất.
                        float p = n163Period[c];
                        float k = std::round((target - n163LastTrigger[c]) / p);
                        target = n163LastTrigger[c] + k * p;

                        while (target > maxTrigger)
                            target -= p;
                        while (target < scanStart && target + p <= maxTrigger)
                            target += p;
                    }

                    int best = -1;
                    float bestScore = 1e30f;

                    for (int i = 0; i < trigIdx.size(); i++)
                    {
                        if (trigSlope[i] < slopeGate)
                            continue;

                        float dist = std::abs(trigIdx[i] - target);
                        float slopeBonus = trigSlope[i] / std::max(strongestSlope, 0.0001f);
                        float score = dist - slopeBonus * 4.0f;

                        if (score < bestScore)
                        {
                            bestScore = score;
                            best = i;
                        }
                    }

                    if (best < 0)
                    {
                        // Nếu gate quá gắt thì lấy crossing gần target nhất.
                        for (int i = 0; i < trigIdx.size(); i++)
                        {
                            float dist = std::abs(trigIdx[i] - target);
                            if (dist < bestScore)
                            {
                                bestScore = dist;
                                best = i;
                            }
                        }
                    }

                    if (best >= 0)
                    {
                        float selected = trigIdx[best];

                        if (n163HasTrigger[c])
                        {
                            float diff = selected - n163LastTrigger[c];
                            float absDiff = std::abs(diff);

                            // Cập nhật period chỉ khi khoảng cách hợp lý, tránh bắt nhầm nửa chu kỳ.
                            if (absDiff >= 8.0f && absDiff <= float(visibleSamples))
                            {
                                float newPeriod = absDiff;
                                if (n163Period[c] <= 0.0f)
                                    n163Period[c] = newPeriod;
                                else
                                    n163Period[c] = n163Period[c] * 0.85f + newPeriod * 0.15f;
                            }
                        }

                        n163LastTrigger[c] = selected;
                        n163HasTrigger[c] = true;
                        fStart = selected - preTrigger;
                    }
                }
                else if (n163HasTrigger[c])
                {
                    // Không tìm thấy crossing mới thì giữ vị trí phase gần nhất, đừng quay lại auto-scroll.
                    fStart = n163LastTrigger[c] - float(visibleSamples) * 0.10f;
                }

                n163LastBufferSize[c] = n;
            }
            else if (isVrc6Saw)
            {
                int searchStart = n - visibleSamples - 1;

                if (searchStart < 1)
                    searchStart = 1;

                for (int i = searchStart; i >= 1; i--)
                {
                    float prev = copy[c][i - 1];
                    float cur = copy[c][i];

                    if ((prev - cur) > 0.06f)
                    {
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

        float amp = panelH * 0.45f;

        if (mode == WaveMode::VRC7)
        {
            amp = panelH * 1.0f;
        }
        else if (mode == WaveMode::N163)
        {
            // Chừa biên để sóng không chạm trên/dưới
            amp = panelH * 0.42f;
        }
        else
        {
            if (c == 0 || c == 1)   amp = panelH * 0.36f; 
            if (c == 2)             amp = panelH * 0.36f; 
            if (c == 3 || c == 4)   amp = panelH * 0.32f;
            if (c >= 5)
            {
                if (mode == WaveMode::VRC6)
                    amp = (c == 7) ? panelH * 0.58f : panelH * 0.48f;
                else if (mode == WaveMode::S5B)
                    amp = panelH * 0.30f;
                else if (mode == WaveMode::MMC5)
                    amp = (c == 7) ? panelH * 0.36f : panelH * 0.44f;
            }
        }
        // N163 ROW-SYNC MODE:
        // Không vẽ cả đoạn history dài theo thời gian nữa, vì khi biên độ/note thay đổi
        // thì mép phải là sample mới còn mép trái là sample cũ -> nhìn như cả hàng bị lệch tick.
        // Ở đây vẫn dùng sóng thật mới nhất, nhưng chỉ lấy MỘT chu kỳ gần trigger hiện tại,
        // rồi lặp chu kỳ đó trên toàn hàng. Vì vậy toàn bộ hàng cập nhật cùng lúc theo frame.
        if (mode == WaveMode::N163)
        {
            int drawPoints = std::max(2, w * 2);

            QPen n163Pen(CHANNEL_COLORS[c]);
            n163Pen.setWidthF(1.35f);
            n163Pen.setCapStyle(Qt::RoundCap);
            n163Pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(n163Pen);

            // Tìm các rising crossing đủ mạnh trong vùng gần cuối buffer.
            QVector<float> idxs;
            QVector<float> slopes;
            idxs.reserve(128);
            slopes.reserve(128);

            int scanStart = std::max(1, n - visibleSamples * 4);
            int scanEnd = n - 2;
            float minSlope = std::max(0.0001f, range * 0.08f);
            float strongestSlope = 0.0f;

            for (int si = scanStart + 1; si <= scanEnd; si++)
            {
                float prev = copy[c][si - 1];
                float cur = copy[c][si];

                if (prev < triggerLevel && cur >= triggerLevel)
                {
                    float slope = cur - prev;
                    if (slope >= minSlope)
                    {
                        float frac = (triggerLevel - prev) / slope;
                        float idx = float(si - 1) + frac;
                        idxs.push_back(idx);
                        slopes.push_back(slope);
                        strongestSlope = std::max(strongestSlope, slope);
                    }
                }
            }

            bool drewN163 = false;

            if (idxs.size() >= 2)
            {
                // Ưu tiên cạnh mạnh gần cuối nhất, rồi tìm cạnh cùng kiểu trước nó để lấy period.
                float slopeGate = strongestSlope * 0.55f;

                int anchor = -1;
                for (int si = idxs.size() - 1; si >= 0; si--)
                {
                    if (slopes[si] >= slopeGate)
                    {
                        anchor = si;
                        break;
                    }
                }

                int prevEdge = -1;
                if (anchor > 0)
                {
                    for (int si = anchor - 1; si >= 0; si--)
                    {
                        float periodTry = idxs[anchor] - idxs[si];
                        if (slopes[si] >= slopeGate && periodTry >= 8.0f && periodTry <= float(visibleSamples))
                        {
                            prevEdge = si;
                            break;
                        }
                    }
                }

                if (anchor >= 0 && prevEdge >= 0)
                {
                    float cycleEnd = idxs[anchor];
                    float period = idxs[anchor] - idxs[prevEdge];
                    float cycleStart = cycleEnd - period;

                    if (cycleStart >= 1.0f && cycleEnd < float(n - 2) && period >= 8.0f)
                    {
                        // Giữ zoom giống visibleSamples cũ: số chu kỳ trên màn hình = visibleSamples / period.
                        float cyclesAcross = float(visibleSamples) / period;
                        cyclesAcross = std::clamp(cyclesAcross, 1.0f, 24.0f);

                        if (n163SmoothY[c].size() != drawPoints)
                        {
                            n163SmoothY[c].resize(drawPoints);
                            n163SmoothValid[c] = false;
                        }

                        QPainterPath n163Path;

                        for (int pi = 0; pi < drawPoints; pi++)
                        {
                            float t = float(pi) / float(drawPoints - 1);
                            float phase = std::fmod(t * cyclesAcross, 1.0f);
                            if (phase < 0.0f)
                                phase += 1.0f;

                            float fIdx = cycleStart + phase * period;
                            fIdx = std::clamp(fIdx, 0.0f, float(n - 2));

                            float sample = sampleInterp(copy[c], fIdx);
                            float yTarget = midY - sample * amp;

                            float y = yTarget;
                            if (n163SmoothValid[c])
                            {
                                // Bám nhanh theo frame, không relax chậm;
                                // mọi điểm X cập nhật cùng lúc, không lệch tick.
                                y = n163SmoothY[c][pi] * 0.15f + yTarget * 0.85f;
                            }

                            n163SmoothY[c][pi] = y;

                            float x = t * float(w);
                            if (pi == 0) n163Path.moveTo(x, y);
                            else         n163Path.lineTo(x, y);
                        }

                        n163SmoothValid[c] = true;
                        p.drawPath(n163Path);
                        drewN163 = true;
                    }
                }
            }

            if (drewN163)
                continue;

            // Không tìm được chu kỳ mới nghĩa là kênh đang tắt / quá nhỏ / đang chuyển note.
            // ĐỪNG nhảy thẳng về đường giữa. Giữ waveform frame trước và release dần về midY
            // để cảm giác hết âm mượt hơn, nhưng vẫn không trôi ngang.
            if (n163SmoothValid[c] && n163SmoothY[c].size() == drawPoints)
            {
                QPainterPath releasePath;

                bool stillVisible = false;
                const float releaseKeep = 0.72f;   // càng cao thì tắt càng chậm
                const float releaseToCenter = 1.0f - releaseKeep;

                for (int pi = 0; pi < drawPoints; pi++)
                {
                    float t = float(pi) / float(drawPoints - 1);

                    float y = n163SmoothY[c][pi] * releaseKeep + float(midY) * releaseToCenter;
                    n163SmoothY[c][pi] = y;

                    if (std::abs(y - float(midY)) > 0.75f)
                        stillVisible = true;

                    float x = t * float(w);
                    if (pi == 0) releasePath.moveTo(x, y);
                    else         releasePath.lineTo(x, y);
                }

                p.drawPath(releasePath);

                if (!stillVisible)
                    n163SmoothValid[c] = false;

                continue;
            }

            // Nếu chưa từng có waveform hợp lệ thì mới rơi về cách vẽ cũ để vẫn thấy tín hiệu.
            n163SmoothValid[c] = false;
        }

        // N163 không dùng stepWave để đường sóng chuyển động mượt hơn.
        // Trigger N163 ở trên vẫn giữ nguyên để không bị trôi ngang.
        bool stepWave = (c == 0 || c == 1)
            || (mode == WaveMode::VRC6 && c >= 5)
            || (mode == WaveMode::S5B && c >= 5)
            || (mode == WaveMode::MMC5 && (c == 5 || c == 6));

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

        // Số điểm vẽ: nhiều hơn để mượt.
        // Riêng N163 tăng điểm vẽ vì đã bỏ stepWave, giúp chuyển động mềm hơn.
        int drawPoints = (mode == WaveMode::N163) ? w * 3 : w * 2; // sub-pixel resolution

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
