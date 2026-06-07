#include "AudioWaveWindow.h"

#include <QPainter>
#include <QMutexLocker>
#include <QPen>
#include <QColor>
#include <algorithm>
#include <cmath>
AudioWaveWindow::AudioWaveWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Audio Waveform Debug");
    resize(900, 700);

    names[0] = "Pulse 1";
    names[1] = "Pulse 2";
    names[2] = "Triangle";
    names[3] = "Noise";
    names[4] = "DMC";
    names[5] = "VRC6 Pulse 1";
    names[6] = "VRC6 Pulse 2";
    names[7] = "VRC6 Saw";

    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        buffers[i].reserve(MAX_SAMPLES);
    }

    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, [this]() {
        update();
        });
    refreshTimer->start(16);
}

void AudioWaveWindow::pushChannels(const AudioDebugChannels& ch)
{
    QMutexLocker lock(&mutex);

    float values[CHANNEL_COUNT] = {
        ch.pulse1,
        ch.pulse2,
        ch.triangle,
        ch.noise,
        ch.dmc,
        ch.vrc6Pulse1,
        ch.vrc6Pulse2,
        ch.vrc6Saw
    };

    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        float v = std::clamp(values[i], -1.0f, 1.0f);

        // Noise: làm nổi rõ hơn
        if (i == 3)
        {
            v *= 1.6f;
            v = std::clamp(v, -1.0f, 1.0f);
        }

        // DMC
        if (i == 4)
        {
            v *= 1.6f;
            v = std::clamp(v, -1.0f, 1.0f);
        }

        // VRC6 Saw: nâng biên độ
        if (i == 7)
        {
            v *= 2.5f;
            v = std::clamp(v, -1.0f, 1.0f);
        }

        buffers[i].push_back(v);

        if (buffers[i].size() > MAX_SAMPLES + 512)
        {
            buffers[i].remove(0, buffers[i].size() - MAX_SAMPLES);
        }
    }
}

void AudioWaveWindow::clearSamples()
{
    QMutexLocker lock(&mutex);

    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        buffers[i].clear();
        lastVisual[i] = 0.0f;
    }
}

void AudioWaveWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QVector<float> copy[CHANNEL_COUNT];

    {
        QMutexLocker lock(&mutex);

        for (int i = 0; i < CHANNEL_COUNT; i++)
        {
            copy[i] = buffers[i];
        }
    }

    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 18));
    p.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();

    if (w <= 0 || h <= 0)
        return;

    int panelH = h / CHANNEL_COUNT;

    for (int c = 0; c < CHANNEL_COUNT; c++)
    {
        int top = c * panelH;
        int midY = top + panelH / 2;

        p.fillRect(0, top, w, panelH, QColor(25, 25, 25));

        p.setPen(QColor(60, 60, 60));
        p.drawRect(0, top, w - 1, panelH - 1);

        p.setPen(QColor(80, 80, 80));
        p.drawLine(0, midY, w, midY);

        p.setPen(QColor(230, 230, 230));
        p.drawText(10, top + 18, names[c]);

        if (copy[c].size() < 2)
            continue;

        if (c == 3 || c == 4)
        {
            // Noise và DMC vẽ dày hơn
            p.setPen(QPen(QColor(0, 220, 120), 2));
        }
        else
        {
            p.setPen(QPen(QColor(0, 220, 120), 1));
        }

        const int n = copy[c].size();

        if (n < DISPLAY_SAMPLES + 2)
            continue;

        // Mặc định vẽ đoạn mới nhất
        int start = n - DISPLAY_SAMPLES;

        // Trigger level
        // Trigger tự động theo biên độ thật của từng kênh
        float minV = copy[c][0];
        float maxV = copy[c][0];

        for (int i = 1; i < n; i++)
        {
            minV = std::min(minV, copy[c][i]);
            maxV = std::max(maxV, copy[c][i]);
        }

        float range = maxV - minV;

        // Nếu sóng gần như phẳng thì cứ vẽ đoạn mới nhất
        if (range > 0.02f)
        {
            float triggerLevel = (minV + maxV) * 0.5f;

            // Tìm cạnh đi lên gần cuối buffer nhất
            for (int i = n - DISPLAY_SAMPLES - 1; i >= 1; i--)
            {
                if (copy[c][i - 1] < triggerLevel && copy[c][i] >= triggerLevel)
                {
                    start = i;
                    break;
                }
            }
        }

        // Vẽ cố định DISPLAY_SAMPLES mẫu
        for (int i = 1; i < DISPLAY_SAMPLES; i++)
        {
            int idx1 = start + i - 1;
            int idx2 = start + i;

            float x1 = float(i - 1) / float(DISPLAY_SAMPLES - 1) * w;
            float x2 = float(i) / float(DISPLAY_SAMPLES - 1) * w;

            float amp = panelH * 0.35f;

            if (c == 3 || c == 4)
            {
                amp = panelH * 0.38f;
            }

            float y1 = midY - copy[c][idx1] * amp;
            float y2 = midY - copy[c][idx2] * amp;

            p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
        }
    }
}