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
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVector>
#include <QMutex>
#include <QString>
#include "APU.h"

class QPainter;

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
        N163,
        FDS
    };
    AudioWaveWindow(WaveMode mode, QWidget* parent = nullptr);
    void pushChannels(const AudioDebugChannels& ch);
    void clearSamples();
    // NSF mode: ô của từng chip chỉ vẽ kênh của chính chip đó (bỏ 5 kênh NES gốc, vì đã có ô NES riêng).
    // Mặc định true = game mode giữ nguyên (NES + expansion trong 1 cửa sổ; VRC7/N163 vốn không có kênh NES).
    void setShowNesChannels(bool on) { showNesChannels = on; }
    int visibleChannelCount() const;
    // Xep kenh lien tuc qua nhieu cua so (NSF nhieu chip): startCol = o dau tien cua cua so nay nam o cot 0/1
    // cua hang dau; fillTrailingCell = ve o cuoi cung (neu trong) bang nen den + truc.
    void setTileLayout(int startCol, bool twoCols, bool fillTrailingCell);
    // Chi ve cac kenh co chi so trong [first, last] (de cat 1 chip thanh nhieu khoi khong chong nhau).
    void setChannelRange(int first, int last) { rangeFirst = first; rangeLast = last; }
    int totalChannelCount() const;     // so kenh cua chip, bo qua setChannelRange
    int firstShownChannel() const;     // chi so kenh dau tien cua chip, bo qua setChannelRange
    WaveMode waveMode() const { return mode; }
    bool showsNesChannels() const { return showNesChannels; }
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

    struct RcFilterState
    {
        float hp1 = 0.0f, prevIn1 = 0.0f;
        float hp2 = 0.0f, prevIn2 = 0.0f;
        float lp = 0.0f;
        bool  initialized = false;
    };
    RcFilterState filterState[CHANNEL_COUNT];
    QVector<float> filteredBuffers[CHANNEL_COUNT];
    float periodHint[CHANNEL_COUNT]{};
    QVector<float> n163SmoothY[CHANNEL_COUNT];
    bool n163SmoothValid[CHANNEL_COUNT]{};
    quint64 totalPushed[CHANNEL_COUNT]{};
    TrigLockState genericTrigLock[CHANNEL_COUNT];
    TrigLockState n163TrigLock[CHANNEL_COUNT];
    float vrc7LastPhaseStart[CHANNEL_COUNT]{};
    bool vrc7HasPhaseStart[CHANNEL_COUNT]{};
    QVector<float> vrc7CorrRef[CHANNEL_COUNT];
    bool vrc7CorrValid[CHANNEL_COUNT]{};
    qint64 vrc7LastAnchorAbs[CHANNEL_COUNT]{};
    bool vrc7HasLastAnchorAbs[CHANNEL_COUNT]{};
    QTimer* refreshTimer = nullptr;
    QMutex mutex;
    WaveMode mode;
    int activeChannelCount = 8;
    bool showNesChannels = true;
    bool tileMode = false;
    int rangeFirst = 0;
    int rangeLast = 1000;
    int layoutOffset = 0;
    bool tileTwoCols = true;
    bool fillTrailing = false;
    bool isChannelShown(int c) const;

    // ---- Cac buoc tach khoi tu paintGL (moi ham lam dung 1 viec, doc/sua doc lap) ----
    // DMC: tinh vi tri bat dau khung (fStart) khoa theo moc Restart() phan cung, du phong bang
    // template cross-correlation. Tra ve fStart moi (tra lai fStart cu neu khong khoa duoc).
    float findDmcTriggerStart(int c, const QVector<float>& raw, const QVector<float>& filteredBuf,
                              int n, int visibleSamples, float dmcHint, double dmcRestart,
                              qint64 chanAbsBase, float fStart);
    // N163: ve 1 kenh (khoa pha bang phase that cua chip, du phong hard-anchor / bat canh len, va hieu ung release).
    // Tra ve true = da ve xong kenh nay (paintGL bo qua phan ve chung); false = roi xuong ve chung.
    bool drawN163Channel(QPainter& p, int c, int left, int midY, int panelW, float amp,
                         const QVector<float>& raw, const QVector<float>& filteredBuf,
                         const QVector<float>& phase, int n, int visibleSamples,
                         float range, float triggerLevel, quint64 pushed, float hint);
    // So sample hien thi (muc zoom) cua kenh c; `raw` la buffer goc cua kenh (n = raw.size()).
    int visibleSamplesFor(int c, int n, const QVector<float>& raw) const;
    // Bien do ve (pixel) cua kenh c trong o co chieu cao panelH.
    float ampFor(int c, int panelH) const;
    // Nho canh da snap o frame truoc (dang phan chu ky 0..1) de N163 khong nhay giua 2 canh len ngang nhau.
    float n163SnapFrac[CHANNEL_COUNT]{};
    bool n163SnapValid[CHANNEL_COUNT]{};
    bool triangleZeroGate[CHANNEL_COUNT]{};
    QVector<float> triFrozenCycle[CHANNEL_COUNT];
    float triFrozenPeriod[CHANNEL_COUNT]{};
    float triFreezeAmp[CHANNEL_COUNT]{};
    bool triFreezeActive[CHANNEL_COUNT]{};
    QVector<float> dmcSmoothY[CHANNEL_COUNT];
    bool dmcSmoothValid[CHANNEL_COUNT]{};
    QVector<float> dmcCorrRef[CHANNEL_COUNT];
    bool dmcCorrValid[CHANNEL_COUNT]{};
    double dmcRestartAbs[CHANNEL_COUNT]{};
};