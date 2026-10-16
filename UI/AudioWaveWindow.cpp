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
#include <QHash>
#include <QCoreApplication>
#include <QVector>

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
    float t = fidx - i1;

    int i0 = std::max(i1 - 1, 0);
    int i2 = std::min(i1 + 1, n - 1);
    int i3 = std::min(i1 + 2, n - 1);

    return catmullRom(buf[i0], buf[i1], buf[i2], buf[i3], t);
}
static float sampleLinear(const QVector<float>& buf, float fidx)
{
    int n = buf.size();
    int i0 = std::clamp((int)fidx, 0, n - 1);
    int i1 = std::clamp(i0 + 1, 0, n - 1);
    float t = fidx - float(i0);
    return buf[i0] + (buf[i1] - buf[i0]) * t;
}

// Catmull-Rom cho VRC6 Saw: cong mượt như corrscope trong từng đoạn ramp,
// nhưng chặn không cho control point kéo qua điểm reset (falling edge)
// để tránh overshoot giả ở đỉnh ramp / đáy reset.
// `buf` là dữ liệu để LẤY GIÁ TRỊ nội suy (có thể đã qua RC filter),
// `rawBuf` là dữ liệu GỐC dùng để PHÁT HIỆN điểm reset chính xác
// (vì sau khi lọc RC, chênh lệch tại điểm reset có thể bị làm mượt bớt,
// khiến ngưỡng phát hiện không còn đáng tin cậy nếu đo trên buf đã lọc).
static float sampleSawSmooth(const QVector<float>& buf, const QVector<float>& rawBuf, float fidx)
{
    int n = buf.size();
    int i1 = std::clamp((int)fidx, 0, n - 1);
    float t = fidx - float((int)fidx);

    int i0 = std::max(i1 - 1, 0);
    int i2 = std::min(i1 + 1, n - 1);
    int i3 = std::min(i1 + 2, n - 1);

    const float kResetDrop = 0.06f; // cùng ngưỡng dùng khi detect trigger falling edge

    auto isReset = [&](int a, int b) -> bool
        {
            return (rawBuf[a] - rawBuf[b]) > kResetDrop;
        };

    // Nếu chính đoạn đang nội suy (i1 -> i2) là điểm reset,
    // dùng linear thẳng để giữ cạnh rơi sắc nét, không làm mượt qua nó.
    if (isReset(i1, i2))
        return buf[i1] + (buf[i2] - buf[i1]) * t;

    float v0 = buf[i0];
    float v1 = buf[i1];
    float v2 = buf[i2];
    float v3 = buf[i3];

    if (isReset(i0, i1))
        v0 = v1;

    // Tương tự cho i3 nếu nó đã sang chu kỳ kế tiếp.
    if (isReset(i2, i3))
        v3 = v2;

    return catmullRom(v0, v1, v2, v3, t);
}

// ---- Tùy chọn khóa sóng (đổi true/false để so sánh) ----
// N163: true = khóa pha bằng phase 24-bit THẬT của chip (n163PhaseN); false = bản cũ (cạnh lên + period).
constexpr bool kN163PhaseLock = true;
// N163 (chỉ khi kN163PhaseLock): true = dịch thêm trong 1 chu kỳ để cạnh LÊN cắt mức 0 nằm đúng
// giao điểm hai trục (giống DMC); false = đầu wavetable (phase 0) nằm giữa khung.
constexpr bool kN163SnapRisingEdge = true;
// VRC7: true = chấm điểm ứng viên bằng NCC (bỏ DC/gain); false = MSE như cũ.
constexpr bool kVrc7NccScore = true;
// VRC7: true = mẫu tham chiếu cập nhật chậm kiểu EMA (như template DMC), chỉ khi khớp tốt.
constexpr bool  kVrc7TemplateEma = true;
constexpr float kVrc7EmaKeepErr = 0.15f; // sai số hình dạng tối đa để còn cập nhật EMA (NCC: 1-ncc)
constexpr float kVrc7EmaRate = 0.08f;
// VRC6 Saw: true = khóa trigger giống Pulse/Triangle (findLockedCenterEdge, bám period thật từ APU);
// false = bản cũ (dò cạnh reset từng frame, không lock).
constexpr bool kVrc6SawEdgeLock = false;
// VRC6 Saw (ưu tiên hơn kVrc6SawEdgeLock): true = khóa trực tiếp vào CẠNH RESET (rơi) thật, đặt ngay giữa khung,
// không dùng cạnh lên cắt mức giữa (sóng bậc thang 7 nấc nên mức giữa trùng 1 nấc -> nhảy giữa 2 nấc -> rung).
constexpr bool kVrc6SawResetLock = false;

static bool validPhase01(float p)
{
    return std::isfinite(p) && p >= 0.0f && p <= 1.0f;
}

// Tạo fingerprint nhỏ cho một cửa sổ waveform VRC7.
// Đây là kiểu CorrScope: không bắt một cạnh zero-cross duy nhất nữa, mà so cả hình sóng
// quanh vùng hiển thị với hình của frame trước. Nhờ vậy VRC7/FM không bị nhảy sang
// cạnh phụ khác nhau mỗi frame.
static void buildVrc7CorrRef(
    const QVector<float>& wave,
    float start,
    int visibleSamples,
    QVector<float>& out
)
{
    const int n = wave.size();
    const int refPoints = 128;
    out.resize(refPoints);

    if (n < 8 || visibleSamples < 8)
    {
        for (int i = 0; i < refPoints; ++i) out[i] = 0.0f;
        return;
    }

    start = std::clamp(start, 0.0f, float(std::max(0, n - visibleSamples - 2)));
    int a = std::clamp(int(std::floor(start)), 0, n - 1);
    int b = std::clamp(int(std::ceil(start + float(visibleSamples))), 0, n - 1);

    float mn = wave[a];
    float mx = wave[a];
    for (int i = a + 1; i <= b; ++i)
    {
        mn = std::min(mn, wave[i]);
        mx = std::max(mx, wave[i]);
    }

    const float center = (mn + mx) * 0.5f;
    const float half = std::max((mx - mn) * 0.5f, 0.001f);

    for (int i = 0; i < refPoints; ++i)
    {
        float t = float(i) / float(refPoints - 1);
        float idx = start + t * float(visibleSamples);
        idx = std::clamp(idx, 0.0f, float(n - 2));

        float v = sampleInterp(wave, idx);
        out[i] = std::clamp((v - center) / half, -1.35f, 1.35f);
    }
}

static float vrc7RefError(const QVector<float>& a, const QVector<float>& b)
{
    if (a.size() != b.size() || a.isEmpty())
        return 1e30f;

    if (kVrc7NccScore)
    {
        // NCC: trừ trung bình rồi chuẩn hóa năng lượng -> không phụ thuộc DC/gain, chỉ so HÌNH DẠNG.
        // 1 - ncc có thang gần với MSE của ref đã chuẩn hóa nên các hệ số phạt bên dưới giữ nguyên.
        const int cnt = a.size();
        double ma = 0.0, mb = 0.0;
        for (int i = 0; i < cnt; ++i) { ma += a[i]; mb += b[i]; }
        ma /= double(cnt);
        mb /= double(cnt);

        double dot = 0.0, ea = 0.0, eb = 0.0;
        for (int i = 0; i < cnt; ++i)
        {
            const double x = double(a[i]) - ma;
            const double y = double(b[i]) - mb;
            dot += x * y;
            ea += x * x;
            eb += y * y;
        }

        if (ea < 1e-9 || eb < 1e-9)
            return (ea < 1e-9 && eb < 1e-9) ? 0.0f : 1.0f; // phẳng vs phẳng = khớp

        return float(std::clamp(1.0 - dot / std::sqrt(ea * eb), 0.0, 2.0));
    }

    float err = 0.0f;
    for (int i = 0; i < a.size(); ++i)
    {
        float d = a[i] - b[i];
        err += d * d;
    }
    return err / float(a.size());
}

static float findVrc7CorrscopeStart(
    const QVector<float>& wave,
    const QVector<float>& phase,
    int n,
    int visibleSamples,
    float periodHint,
    const QVector<float>& prevRef,
    bool hasPrevRef,
    QVector<float>& newRef,
    bool& ok,
    qint64 absBase,
    qint64& lastAnchorAbs,
    bool& hasLastAnchorAbs
)
{
    ok = false;
    newRef.clear();

    if (phase.size() != n || wave.size() != n || n < visibleSamples + 8)
        return float(n - visibleSamples);

    const float defaultStart = float(n - visibleSamples);
    const bool hasHint = (periodHint >= 4.0f && periodHint <= 8192.0f);
    const float anchorOffset = float(visibleSamples) * 0.50f;
    const float desiredAnchor = defaultStart + anchorOffset;

    QVector<float> anchorCandidates;
    anchorCandidates.reserve(256);

    int radius = visibleSamples * 3;
    if (hasHint)
        radius = std::clamp(int(periodHint * 10.0f), visibleSamples, visibleSamples * 5);

    const int scanStart = std::max(1, int(desiredAnchor) - radius);
    const int scanEnd = std::min(n - 2, int(desiredAnchor) + radius);

    for (int i = scanStart + 1; i <= scanEnd; ++i)
    {
        float prev = phase[i - 1];
        float cur = phase[i];

        if (!validPhase01(prev) || !validPhase01(cur))
            continue;

        // Phase wrap thật của carrier: gần 1.0 nhảy về 0.0.
        if (prev > 0.55f && cur < 0.45f && (prev - cur) > 0.25f)
        {
            float denom = (1.0f - prev) + cur;
            float frac = (denom > 0.000001f) ? (1.0f - prev) / denom : 0.0f;
            frac = std::clamp(frac, 0.0f, 1.0f);
            float idx = float(i - 1) + frac;

            // Candidate là phase-wrap sẽ được đặt vào đúng X anchor cố định.
            float candidateStart = idx - anchorOffset;
            if (candidateStart >= 0.0f && candidateStart <= float(n - visibleSamples - 2))
                anchorCandidates.push_back(idx);
        }
    }

    // Fallback: nếu chưa bắt được wrap trong vùng, dùng phase ngay tại X anchor để suy ra phase-0.
    if (anchorCandidates.isEmpty() && hasHint)
    {
        int ai = std::clamp(int(std::round(desiredAnchor)), 0, n - 1);
        float ph = phase[ai];
        if (validPhase01(ph))
        {
            const float maxStart = float(n - visibleSamples - 2);
            const float baseAnchor = desiredAnchor - ph * periodHint;

            for (int k = -16; k <= 16; ++k)
            {
                float anchor = baseAnchor + float(k) * periodHint;
                float candidateStart = anchor - anchorOffset;

                if (candidateStart >= 0.0f && candidateStart <= maxStart)
                    anchorCandidates.push_back(anchor);
            }

            if (anchorCandidates.isEmpty())
            {
                float anchor = baseAnchor;
                while (anchor < desiredAnchor - periodHint * 0.5f)
                    anchor += periodHint;
                while (anchor > desiredAnchor + periodHint * 0.5f)
                    anchor -= periodHint;

                float candidateStart = anchor - anchorOffset;
                candidateStart = std::clamp(candidateStart, 0.0f, maxStart);
                anchorCandidates.push_back(candidateStart + anchorOffset);
            }
        }
    }

    if (anchorCandidates.isEmpty())
        return defaultStart;

    float bestStart = defaultStart;
    float bestScore = 1e30f;
    float bestShapeErr = 1e30f;
    QVector<float> bestRef;

    for (float anchor : anchorCandidates)
    {
        float candStart = anchor - anchorOffset;
        candStart = std::clamp(candStart, 0.0f, float(n - visibleSamples - 2));

        QVector<float> ref;
        buildVrc7CorrRef(wave, candStart, visibleSamples, ref);

        float score = 0.0f;
        float shapeErr = 1e30f;
        if (hasPrevRef && prevRef.size() == ref.size())
        {
            shapeErr = vrc7RefError(ref, prevRef);
            score = shapeErr;
            score += std::abs(anchor - desiredAnchor) / std::max(1.0f, float(visibleSamples)) * 0.035f;
        }
        else
        {
            score = std::abs(anchor - desiredAnchor);
        }
        if (hasLastAnchorAbs)
        {
            float anchorAbs = float(absBase) + anchor;
            float driftFromLast = std::abs(anchorAbs - float(lastAnchorAbs));
            score += driftFromLast / std::max(1.0f, float(visibleSamples)) * 0.6f;
        }

        if (score < bestScore)
        {
            bestScore = score;
            bestStart = candStart;
            bestShapeErr = shapeErr;
            bestRef = ref;
        }
    }

    if (!bestRef.isEmpty())
    {
        newRef = bestRef;
        // Template-lock kiểu DMC: mẫu tham chiếu chỉ trôi CHẬM theo hình sóng khi còn khớp tốt,
        // nên không bị kéo theo từng frame (sai số không cộng dồn). Khớp kém -> thay hẳn.
        if (kVrc7TemplateEma && hasPrevRef && prevRef.size() == bestRef.size() && bestShapeErr < kVrc7EmaKeepErr)
        {
            for (int k = 0; k < newRef.size(); ++k)
                newRef[k] = prevRef[k] + (bestRef[k] - prevRef[k]) * kVrc7EmaRate;
        }
        ok = true;
        lastAnchorAbs = absBase + (qint64)std::llround(bestStart + anchorOffset);
        hasLastAnchorAbs = true;

        return bestStart;
    }

    return defaultStart;
}


// =====================================================================================
// N163 - TẦNG 1: khóa pha bằng PHASE THẬT của chip
// -------------------------------------------------------------------------------------
// Wavetable 4-bit tùy ý có nhiều cạnh lên mỗi chu kỳ nên dò cạnh trên waveform dễ khóa nhầm
// cạnh phụ. Chip có sẵn phase 24-bit mỗi kênh (đã chuẩn hóa 0..1, cùng chỉ số với buffer):
// mẫu i có phase phi_i nghĩa là nó cách ĐẦU wavetable phi_i*P mẫu => đầu chu kỳ chứa i là
// w_i = i - phi_i*P. Mọi w_i (sau khi dời bội số P) phải trùng nhau; lấy TRUNG BÌNH để triệt
// bước lượng tử của DAC ghép kênh (mỗi kênh chỉ cập nhật phase mỗi 15*N chu kỳ CPU ~ 3 mẫu).
// wStart: toạ độ buffer của một đầu wavetable sao cho [wStart, wStart+P+3] nằm trọn trong buffer.
// offset: (tùy chọn) độ dời trong chu kỳ để cạnh LÊN cắt mức 0 của chính đường đang vẽ nằm giữa khung.
// Vị trí giữa khung = wStart + offset (mod P); vì sóng lặp nên chọn cạnh nào hình cũng như nhau.
// =====================================================================================


static bool n163PhaseLock(
    const QVector<float>& phase,
    const QVector<float>& wave,   // filteredBuf (đúng thứ đang vẽ)
    int n,
    float P,
    bool snapRisingEdge,
    float& wStart,
    float& offset,
    float& lastFrac,
    bool& hasLastFrac)
{
    if (n < 64 || P < 8.0f || P > 8192.0f || phase.size() != n || wave.size() != n)
        return false;

    const int last = n - 1;
    if (!validPhase01(phase[last]))
        return false;

    const float wRef = float(last) - phase[last] * P;
    const int K = std::clamp(int(P * 2.0f), 64, 2048);
    const int i0 = std::max(0, last - K);
    const float tol = std::max(6.0f, P * 0.15f);

    double acc = 0.0;
    int cnt = 0;
    for (int i = i0; i <= last; ++i)
    {
        const float ph = phase[i];
        if (!validPhase01(ph))
            continue;

        float wi = float(i) - ph * P;
        wi += std::round((wRef - wi) / P) * P;
        const float d = wi - wRef;
        if (std::abs(d) <= tol)
        {
            acc += double(d);
            ++cnt;
        }
    }

    float w = wRef;
    if (cnt >= 8)
        w += float(acc / double(cnt));

    // Dời về chu kỳ mới nhất mà [w, w+P+3] vẫn nằm trọn trong buffer.
    while (w + P + 3.0f > float(last) && w - P >= 1.0f)
        w -= P;
    if (w < 1.0f || w + P + 3.0f > float(last))
        return false;

    offset = 0.0f;
    if (snapRisingEdge)
    {
        // Lấy đúng 1 chu kỳ [w, w+P) ở S điểm, làm mượt TUẦN HOÀN, rồi tìm cạnh lên dốc nhất
        // cắt mức 0 (lên đều cả hai phía). Hòa (>= 90% mạnh nhất) thì lấy cạnh sớm nhất sau đầu wavetable.
        const int S = std::clamp(int(std::lround(P)), 32, 1024);
        QVector<float> cyc(S), sm(S);
        for (int k = 0; k < S; ++k)
            cyc[k] = sampleInterp(wave, w + float(k) * P / float(S));

        const int Ks = std::max(1, S / 64);
        float mn = 1e30f, mx = -1e30f;
        for (int k = 0; k < S; ++k)
        {
            float s = 0.0f;
            for (int j = -Ks; j <= Ks; ++j)
                s += cyc[(k + j + S) % S];
            sm[k] = s / float(2 * Ks + 1);
            mn = std::min(mn, sm[k]);
            mx = std::max(mx, sm[k]);
        }

        const float range = mx - mn;
        if (range > 1e-4f)
        {
            const int Kr = std::max(2, S / 16);
            const float minRise = std::max(1e-5f, range * 0.05f);

            QVector<float> cPos, cRise;
            float maxRise = 0.0f;
            for (int k = 0; k < S; ++k)
            {
                const float a = sm[(k - 1 + S) % S];
                const float b = sm[k];
                if (a < 0.0f && b >= 0.0f)
                {
                    const float rise = std::min(b - sm[(k - Kr + S) % S], sm[(k + Kr) % S] - b);
                    if (rise >= minRise)
                    {
                        cPos.push_back(float(k - 1) + (-a) / (b - a));
                        cRise.push_back(rise);
                        maxRise = std::max(maxRise, rise);
                    }
                }
            }

            // Hysteresis: nếu đã có cạnh đang bám, giữ cạnh gần nó nhất (miễn còn >= 50% cạnh mạnh nhất
            // và lệch < 10% chu kỳ). Chỉ khi mất cạnh đó (đổi nốt/đổi wavetable) mới chọn lại.
            const bool hadLast = hasLastFrac;
            hasLastFrac = false;
            int pick = -1;

            if (hadLast)
            {
                float bestD = 1e30f;
                for (int q = 0; q < int(cPos.size()); ++q)
                {
                    if (cRise[q] < maxRise * 0.5f)
                        continue;
                    float fr = cPos[q] / float(S);
                    fr -= std::floor(fr);
                    float d = std::abs(fr - lastFrac);
                    d = std::min(d, 1.0f - d);
                    if (d < bestD)
                    {
                        bestD = d;
                        pick = q;
                    }
                }
                if (bestD > 0.10f)
                    pick = -1;
            }

            if (pick < 0)
            {
                for (int q = 0; q < int(cPos.size()); ++q)
                {
                    if (cRise[q] >= maxRise * 0.9f)
                    {
                        pick = q;
                        break;
                    }
                }
            }

            if (pick >= 0)
            {
                offset = cPos[pick] * P / float(S);
                float fr = cPos[pick] / float(S);
                fr -= std::floor(fr);
                lastFrac = fr;
                hasLastFrac = true;
            }
        }
    }

    wStart = w;
    return true;
}

static float resolveLockedTrigger(
    AudioWaveWindow::TrigLockState& lock,
    qint64 absBase,                 // = totalPushed[c] - n  (abs index của copy[c][0])
    const QVector<float>& idxs,
    const QVector<float>& slopes,
    float periodHint,               // sample / chu kỳ lấy từ timer thật, 0 nếu không có
    float minPeriod, float maxPeriod,
    float n)
{
    if (idxs.isEmpty())
        return -1.0f;

    const bool hasPeriodHint = periodHint >= minPeriod && periodHint <= 8192.0f;

    if (!lock.locked)
    {
        // Acquisition lần đầu (hoặc sau khi mất lock): lấy cạnh mạnh nhất làm mốc.
        int best = 0;
        for (int i = 1; i < idxs.size(); i++)
            if (slopes[i] > slopes[best]) best = i;

        lock.lockAbsPos = absBase + (qint64)std::llround(idxs[best]);
        lock.period = hasPeriodHint ? periodHint : 0.0f;
        lock.locked = true;
        return idxs[best];
    }

    // Nếu mapper/APU gửi period thật thì dùng nó làm anchor tuyệt đối.
    // Không low-pass period đo từ waveform nữa, vì đó chính là nguồn gây drift.
    if (hasPeriodHint)
        lock.period = periodHint;

    // Quy đổi lock cũ sang index của buffer hiện tại (an toàn qua các lần trim vì dùng absBase).
    float lockIdx = float(lock.lockAbsPos - absBase);

    float target = lockIdx;
    if (lock.period >= minPeriod)
    {
        float k = std::round((n - lockIdx) / lock.period);
        target = lockIdx + k * lock.period;
    }

    int best = -1;
    float bestDist = 1e30f;
    for (int i = 0; i < idxs.size(); i++)
    {
        float dist = std::abs(idxs[i] - target);
        if (dist < bestDist) { bestDist = dist; best = i; }
    }

    float maxJitter = (lock.period >= minPeriod) ? lock.period * 0.5f : n * 0.5f;
    if (best < 0 || bestDist > maxJitter)
    {
        // Ứng viên gần nhất vẫn lệch quá xa dự đoán => coi như mất lock, acquisition lại.
        int strongest = 0;
        for (int i = 1; i < idxs.size(); i++)
            if (slopes[i] > slopes[strongest]) strongest = i;

        lock.lockAbsPos = absBase + (qint64)std::llround(idxs[strongest]);
        lock.period = hasPeriodHint ? periodHint : 0.0f;
        return idxs[strongest];
    }

    float selected = idxs[best];

    if (!hasPeriodHint)
    {
        // Fallback cũ: chỉ dùng khi channel chưa có period thật.
        // Low-pass cho period đo từ waveform. Cái này giảm jitter nhưng vẫn có thể drift,
        // nên các kênh có timer thật sẽ không đi nhánh này nữa.
        float measuredPeriod = selected - lockIdx;
        if (lock.period >= minPeriod)
        {
            float kk = std::round(measuredPeriod / lock.period);
            if (kk > 0.0f)
            {
                float onePeriod = measuredPeriod / kk;
                if (onePeriod >= minPeriod && onePeriod <= maxPeriod)
                    lock.period = lock.period * 0.9f + onePeriod * 0.1f;
            }
        }
        else if (measuredPeriod >= minPeriod && measuredPeriod <= maxPeriod)
        {
            lock.period = measuredPeriod;
        }
    }

    // Phase correction kiểu PLL. Khi có periodHint, gain thấp hơn để chỉ căn pha,
    // không cho crossing đo từ waveform kéo lệch period thật.
    float phaseError = selected - target;
    float correctionGain = hasPeriodHint ? 0.18f : 0.35f;
    float corrected = target + phaseError * correctionGain;

    lock.lockAbsPos = absBase + (qint64)std::llround(corrected);
    return corrected;

}

// =====================================================================================
// HARD PERIOD ANCHOR
// -------------------------------------------------------------------------------------
// resolveLockedTrigger phía trên vẫn là PLL: nó còn cho crossing kéo phase từng frame.
// Cách đó giảm rung, nhưng vẫn có thể trôi nếu crossing đo được bị bias.
// Với các kênh đã có period thật từ APU/mapper, ta lock 1 cạnh làm mốc duy nhất,
// sau đó dự đoán vị trí các cạnh tiếp theo bằng period thật. Crossing chỉ dùng để
// acquire ban đầu hoặc khi note đổi mạnh; tuyệt đối không kéo phase mỗi frame nữa.
// =====================================================================================
static int pickLatestStrongCandidate(const QVector<float>& idxs, const QVector<float>& slopes)
{
    if (idxs.isEmpty())
        return -1;

    int strongest = 0;
    for (int i = 1; i < slopes.size(); ++i)
        if (slopes[i] > slopes[strongest])
            strongest = i;

    float gate = slopes[strongest] * 0.55f;

    // Ưu tiên cạnh mới nhất nhưng vẫn đủ mạnh, để lúc acquire lấy chu kỳ gần hiện tại.
    for (int i = idxs.size() - 1; i >= 0; --i)
        if (slopes[i] >= gate)
            return i;

    return strongest;
}

static bool updateHardPeriodAnchor(
    AudioWaveWindow::TrigLockState& lock,
    qint64 absBase,
    const QVector<float>& idxs,
    const QVector<float>& slopes,
    float periodHint,
    float minPeriod,
    float maxPeriod,
    float changeRatio,
    float periodAlpha = 1.0f) // 1.0 = ghi đè cứng (behavior cũ); nhỏ hơn = low-pass
{
    const bool hasHint = periodHint >= minPeriod && periodHint <= maxPeriod;
    if (!hasHint)
        return false;

    bool needAcquire = !lock.locked || lock.period < minPeriod;

    if (!needAcquire)
    {
        float diff = std::abs(periodHint - lock.period);
        float limit = std::max(1.25f, lock.period * changeRatio);
        if (diff > limit)
            needAcquire = true;
    }


    if (needAcquire)
    {
        int best = pickLatestStrongCandidate(idxs, slopes);
        if (best < 0)
            return false;

        lock.lockAbsPos = absBase + (qint64)std::llround(idxs[best]);
        lock.period = periodHint;
        lock.locked = true;
        return true;
    }

    lock.period += (periodHint - lock.period) * periodAlpha;
    return true;
}


static float predictEdgeNearEnd(
    const AudioWaveWindow::TrigLockState& lock,
    qint64 absBase,
    float n)
{
    if (!lock.locked || lock.period < 1.0f)
        return -1.0f;

    float p = lock.period;
    float lockIdx = float(lock.lockAbsPos - absBase);
    float k = std::floor((n - 2.0f - lockIdx) / p);
    float edge = lockIdx + k * p;

    while (edge < 1.0f) edge += p;
    while (edge > n - 2.0f) edge -= p;

    return edge;
}

static float predictEdgeNearStart(
    const AudioWaveWindow::TrigLockState& lock,
    qint64 absBase,
    float desiredStart,
    float minStart,
    float maxStart)
{
    if (!lock.locked || lock.period < 1.0f)
        return -1.0f;

    float p = lock.period;
    float lockIdx = float(lock.lockAbsPos - absBase);
    float k = std::round((desiredStart - lockIdx) / p);
    float edge = lockIdx + k * p;

    while (edge < minStart) edge += p;
    while (edge > maxStart) edge -= p;

    if (edge < minStart || edge > maxStart)
        return -1.0f;

    return edge;
}

// Độ tăng của tín hiệu quanh một giao cắt đi lên: buf[i+K] - buf[i-1-K].
// Slope TRÊN 1 SAMPLE chỉ đúng với sóng có cạnh dốc/bậc thang. Triangle ở chế độ "Smooth Triangle"
// là đường dốc mượt: mỗi sample chỉ tăng ~2*biên_độ/period (vài %), nên ngưỡng slope 5% làm
// MẤT HẾT cạnh -> trigger không bám được. Đo độ tăng trên một đoạn ~period/8 thì cả bậc thang,
// dốc mượt lẫn cạnh vuông đều đạt, còn nhiễu nhỏ thì bị loại.
static inline float risingSpan(const QVector<float>& buf, int n, int i, float periodHint)
{
    const int K = std::clamp(int(periodHint / 16.0f), 2, 512);
    const int a = std::max(0, i - 1 - K);
    const int b = std::min(n - 1, i + K);
    return buf[b] - buf[a];
}

// =====================================================================================
// TRIGGER DÙNG CHUNG "KIỂU FDS" cho Pulse / Triangle / FDS
// -------------------------------------------------------------------------------------
// - Mức trigger + ngưỡng slope tính CỤC BỘ trên vùng cuối buffer (không dùng min/max cả
//   buffer: envelope/đổi nốt làm mức giữa cũ lệch khỏi sóng hiện tại).
// - period hint (từ APU/mapper) chỉ dùng để chọn ĐÚNG cạnh (cạnh nào trong chu kỳ) gần
//   tâm khung hình nhất -> không nhảy giữa các cạnh khi sóng có nhiều cạnh lên.
// - Vị trí cuối cùng là crossing ĐO ĐƯỢC (nội suy sub-sample) và mốc được gắn lại vào đó
//   mỗi frame -> sai số period không tích luỹ, không trôi.
// - Mất dự đoán (đổi nốt, sai period hint...) -> bỏ mốc, rơi về cạnh lên mới nhất.
// Trả về vị trí cạnh (toạ độ buffer hiện tại) sẽ nằm GIỮA khung, hoặc -1 nếu không có cạnh.
// =====================================================================================
static float findLockedCenterEdge(
    const QVector<float>& buf,
    int n,
    int visibleSamples,
    qint64 absBase,
    float periodHint,
    AudioWaveWindow::TrigLockState& lock)
{
    const float halfVisible = float(visibleSamples) * 0.5f;
    const float minEdge = halfVisible;
    const float maxEdge = float(n) - halfVisible - 2.0f;
    if (n < 8 || maxEdge <= minEdge)
        return -1.0f;

    const int scanStart = std::max(1, n - visibleSamples * 4);
    const int scanEnd = n - 2;

    float lMin = buf[scanStart];
    float lMax = lMin;
    for (int si = scanStart; si < n; si++)
    {
        lMin = std::min(lMin, buf[si]);
        lMax = std::max(lMax, buf[si]);
    }
    const float lRange = lMax - lMin;
    if (lRange < 0.0001f)
        return -1.0f;

    const float level = (lMin + lMax) * 0.5f;
    const float minRise = std::max(0.0001f, lRange * 0.05f);

    QVector<float> idxs;
    QVector<float> slopes;
    idxs.reserve(128);
    slopes.reserve(128);

    for (int si = scanStart + 1; si <= scanEnd; si++)
    {
        const float prev = buf[si - 1];
        const float cur = buf[si];
        if (prev < level && cur >= level)
        {
            const float rise = risingSpan(buf, n, si, periodHint);
            if (rise >= minRise)
            {
                const float frac = std::clamp((level - prev) / (cur - prev), 0.0f, 1.0f);
                idxs.push_back(float(si - 1) + frac);
                slopes.push_back(rise);
            }
        }
    }

    const bool locked = updateHardPeriodAnchor(
        lock, absBase, idxs, slopes, periodHint, 8.0f, 8192.0f, 0.06f, 0.25f);

    float chosen = -1.0f;

    if (locked && lock.period >= 8.0f)
    {
        const float period = lock.period;
        const float predicted = predictEdgeNearStart(
            lock, absBase, float(n) - halfVisible, minEdge, maxEdge);

        if (predicted >= 0.0f)
        {
            float bestDist = std::max(3.0f, period * 0.25f);
            for (int i = 0; i < idxs.size(); i++)
            {
                if (idxs[i] < minEdge || idxs[i] > maxEdge)
                    continue;
                const float d = std::abs(idxs[i] - predicted);
                if (d < bestDist)
                {
                    bestDist = d;
                    chosen = idxs[i];
                }
            }
        }

        if (chosen >= 0.0f)
            lock.lockAbsPos = absBase + (qint64)std::llround(chosen); // re-anchor vào crossing thật
        else
            lock.locked = false; // mất dự đoán -> acquire lại; GIỮ lock.period (cycle view dùng khi nốt bị ngắt)
    }

    if (chosen < 0.0f)
    {
        // Không có period hint hợp lệ / vừa mất lock: lấy cạnh lên mới nhất còn nằm trong khung.
        for (int i = idxs.size() - 1; i >= 0; i--)
        {
            if (idxs[i] >= minEdge && idxs[i] <= maxEdge)
            {
                chosen = idxs[i];
                break;
            }
        }
    }

    return chosen;
}

// =====================================================================================
// VRC6 SAW: khóa trigger vào CẠNH RESET (bước rơi 1 sample, duy nhất mỗi chu kỳ)
// -------------------------------------------------------------------------------------
// Giống findLockedCenterEdge (hard period anchor + re-anchor vào cạnh đo được) nhưng đo trên cạnh
// reset thay vì cạnh lên cắt mức giữa. Saw VRC6 là sóng bậc thang nên mức giữa trùng đúng 1 nấc,
// cạnh lên sẽ nhảy qua lại giữa 2 nấc kề nhau (lệch ~P/7). Cạnh reset thì luôn ở đúng một chỗ.
// Trả về vị trí reset (toạ độ buffer, nằm giữa 2 sample) sẽ đặt đúng GIỮA khung, hoặc -1 nếu không có.
// =====================================================================================
static float findLockedCenterReset(
    const QVector<float>& raw,
    int n,
    int visibleSamples,
    qint64 absBase,
    float periodHint,
    AudioWaveWindow::TrigLockState& lock)
{
    const float halfVisible = float(visibleSamples) * 0.5f;
    const float minEdge = halfVisible;
    const float maxEdge = float(n) - halfVisible - 2.0f;
    if (n < 8 || maxEdge <= minEdge)
        return -1.0f;

    const int scanStart = std::max(1, n - visibleSamples * 4);

    float lMin = raw[scanStart];
    float lMax = lMin;
    for (int si = scanStart; si < n; si++)
    {
        lMin = std::min(lMin, raw[si]);
        lMax = std::max(lMax, raw[si]);
    }
    const float lRange = lMax - lMin;
    if (lRange < 0.0001f)
        return -1.0f;

    // Reset = cú rơi từ đỉnh xuống đáy, cỡ cả biên độ (mỗi nấc chỉ bằng ~1/6 biên độ) -> ngưỡng tương đối,
    // không phụ thuộc volume.
    const float minDrop = std::max(0.0001f, lRange * 0.5f);

    QVector<float> idxs;
    QVector<float> slopes;
    idxs.reserve(128);
    slopes.reserve(128);

    for (int si = scanStart + 1; si <= n - 1; si++)
    {
        const float drop = raw[si - 1] - raw[si];
        if (drop >= minDrop)
        {
            idxs.push_back(float(si) - 0.5f); // bước nhảy nằm giữa sample si-1 và si
            slopes.push_back(drop);
        }
    }

    const bool locked = updateHardPeriodAnchor(
        lock, absBase, idxs, slopes, periodHint, 8.0f, 8192.0f, 0.06f, 0.25f);

    float chosen = -1.0f;

    if (locked && lock.period >= 8.0f)
    {
        const float period = lock.period;
        const float predicted = predictEdgeNearStart(
            lock, absBase, float(n) - halfVisible, minEdge, maxEdge);

        if (predicted >= 0.0f)
        {
            float bestDist = std::max(3.0f, period * 0.25f);
            for (int i = 0; i < idxs.size(); i++)
            {
                if (idxs[i] < minEdge || idxs[i] > maxEdge)
                    continue;
                const float d = std::abs(idxs[i] - predicted);
                if (d < bestDist)
                {
                    bestDist = d;
                    chosen = idxs[i];
                }
            }
        }

        if (chosen >= 0.0f)
            lock.lockAbsPos = absBase + (qint64)std::llround(chosen); // re-anchor vào reset thật
        else
            lock.locked = false; // mất dự đoán -> acquire lại; giữ lock.period
    }

    if (chosen < 0.0f)
    {
        // Không có period hint hợp lệ / vừa mất lock: lấy reset mới nhất còn nằm trong khung.
        for (int i = idxs.size() - 1; i >= 0; i--)
        {
            if (idxs[i] >= minEdge && idxs[i] <= maxEdge)
            {
                chosen = idxs[i];
                break;
            }
        }
    }

    return chosen;
}

// =====================================================================================
// CYCLE-LOCKED VIEW (hiển thị đối xứng trái/phải) cho sóng vuông + tam giác
// -------------------------------------------------------------------------------------
// Hiển thị cuộn thông thường: mép phải là dữ liệu MỚI, mép trái là dữ liệu CŨ hơn cả một
// cửa sổ -> đổi nốt / đổi volume / ngắt tiếng về 0 thì bên phải đổi trước, bên trái đổi
// sau (vệt "quét" từ phải sang trái).
// Cycle view: chỉ lấy MỘT chu kỳ mới nhất (bắt đầu tại một cạnh lên đo được) rồi lặp lại
// nó trên toàn khung theo period hiện tại, cạnh tham chiếu đặt đúng giữa khung. Vì mọi
// điểm trên khung đều suy ra từ cùng một chu kỳ mới nhất nên hai nửa đổi ĐỒNG THỜI.
// =====================================================================================
constexpr bool kDmcTriggerRising = true; // DMC: true = cạnh LÊN (/) đi qua giao điểm 2 trục ở giữa; false = cạnh XUỐNG (\)
constexpr bool kCycleLockedView = true; // true = lặp 1 chu kỳ trên cả khung (đối xứng, chuyển động "cứng"); false = cuộn tự nhiên

struct CycleViewInfo
{
    bool ok = false;
    float ref = 0.0f;    // toạ độ buffer của cạnh lên bắt đầu chu kỳ mới nhất
    float period = 0.0f; // số sample mỗi chu kỳ
};

// periodHint CHỈ để biết "cỡ" chu kỳ (chọn vùng quét). Chu kỳ dùng để vẽ được ĐO TRỰC TIẾP từ các
// cạnh lên của chính tín hiệu, nên hint sai/rung hay pitch đổi cực nhanh đều không làm hình bị
// cắt cụt hoặc dính hai chu kỳ vào nhau:
//  - nốt giữ nguyên (3-4 khoảng cạnh gần nhất lệch < 4%): lấy trung bình cho mượt, ổn định;
//  - pitch đang đổi nhanh: lấy ĐÚNG chu kỳ hoàn chỉnh mới nhất [cạnh trước, cạnh sau].
static CycleViewInfo buildCycleView(const QVector<float>& raw, int n, float periodHint)
{
    CycleViewInfo cv;
    if (n < 64 || periodHint < 8.0f || periodHint > 8192.0f)
        return cv;

    const int span = std::min(n - 2, std::clamp(int(periodHint * 4.0f), 512, 8192)); // đủ ~3-4 chu kỳ kể cả nốt rất trầm
    const int lo = n - span;

    float mn = raw[lo];
    float mx = mn;
    for (int i = lo; i < n; i++)
    {
        mn = std::min(mn, raw[i]);
        mx = std::max(mx, raw[i]);
    }
    const float range = mx - mn;
    const float level = (mn + mx) * 0.5f;
    const float minRise = std::max(0.0001f, range * 0.05f);

    // Các cạnh lên (nội suy sub-sample), cũ -> mới.
    QVector<float> xs;
    if (range >= 0.0001f)
    {
        for (int i = lo + 1; i <= n - 2; i++)
        {
            const float prev = raw[i - 1];
            const float cur = raw[i];
            if (prev < level && cur >= level && risingSpan(raw, n, i, periodHint) >= minRise)
                xs.push_back(float(i - 1) + std::clamp((level - prev) / (cur - prev), 0.0f, 1.0f));
        }
    }

    float period = periodHint;
    float ref = float(n) - period - 2.0f; // mặc định: đoạn mới nhất (kênh im -> mẫu phẳng)

    if (xs.size() >= 2)
    {
        const int m = xs.size() - 1;
        const float lastLen = xs[m] - xs[m - 1];

        if (lastLen >= 4.0f)
        {
            if (float(n) - xs[m] > lastLen * 1.5f + 4.0f)
            {
                // Quá lâu không có cạnh mới: nốt bị ngắt (hoặc đổi quá trầm). Lấy đoạn mới nhất để
                // khi tín hiệu về 0 thì cả khung phẳng cùng lúc.
                period = lastLen;
                ref = float(n) - period - 2.0f;
            }
            else
            {
                // Tối đa 4 khoảng cạnh gần nhất.
                float sum = lastLen, smin = lastLen, smax = lastLen;
                int cnt = 1;
                for (int i = m - 1; i >= 1 && cnt < 4; --i, ++cnt)
                {
                    const float d = xs[i] - xs[i - 1];
                    sum += d;
                    smin = std::min(smin, d);
                    smax = std::max(smax, d);
                }
                const float mean = sum / float(cnt);
                const bool steady = cnt >= 3 && (smax - smin) <= mean * 0.04f;

                period = steady ? mean : lastLen;
                ref = xs[m - 1];

                // Có period THẬT từ APU (chính xác tới phần lẻ sample) và khớp với số đo
                // -> dùng nó làm period vẽ, thay vì period đo từ cạnh. Cạnh bậc thang của
                // tam giác chỉ rơi vào số nguyên sample (lệch +-1) nên period đo được rung
                // theo từng frame; vì khung lặp ~20-60 chu kỳ nên các chu kỳ ở xa tâm bị
                // trượt qua lại ("trôi"), lúc nặng lúc nhẹ tùy nốt có gần số nguyên hay không.
                // Mốc ref cũng được fit trung bình từ vài cạnh gần nhất để bớt rung.
                if (periodHint >= 8.0f && std::abs(period - periodHint) <= periodHint * 0.06f)
                {
                    const float P = periodHint;
                    const float tol = std::max(1.5f, P * 0.02f);
                    float acc = 0.0f;
                    int used = 0;
                    for (int j = m; j >= 0 && j >= m - 7; --j)
                    {
                        const float d = xs[m - 1] - xs[j];
                        const float q = std::round(d / P);
                        if (std::abs(d - q * P) <= tol)
                        {
                            acc += xs[j] + q * P;
                            ++used;
                        }
                    }
                    period = P;
                    if (used > 0)
                        ref = acc / float(used);
                }
            }
        }
    }

    if (period < 4.0f || ref < 1.0f)
        return cv;
    if (ref + period > float(n - 2))
        ref = std::max(1.0f, float(n - 2) - period);

    cv.ok = true;
    cv.ref = ref;
    cv.period = period;
    return cv;
}

constexpr bool  kSmoothRelease = true;
constexpr float kReleaseDecay = 0.90f; // nhân mỗi lần vẽ (~100 lần/giây): nhỏ hơn = tắt nhanh hơn

// Đuôi buffer phẳng lâu hơn mức một nốt đang kêu có thể phẳng => kênh đã bị ngắt.
static bool tailIsSilent(const QVector<float>& raw, int n, float periodHint, float globalRange)
{
    if (n < 64)
        return false;

    const bool hasPeriod = periodHint >= 8.0f && periodHint <= 8192.0f;
    // Sóng vuông/tam giác đang kêu không bao giờ phẳng quá ~1 chu kỳ (duty 75% -> 0.75 chu kỳ).
    // Chưa biết period thì dùng ngưỡng dài an toàn để không ngắt nhầm nốt trầm.
    const int lim = hasPeriod ? std::max(48, int(periodHint * 1.25f)) : 2400;

    const float eps = std::max(0.0005f, globalRange * 0.03f);
    const float last = raw[n - 1];
    const int maxScan = std::min(n, lim + 2);

    int len = 0;
    for (int i = n - 1; i >= n - maxScan && std::abs(raw[i] - last) <= eps; --i)
        ++len;

    return len > lim;
}

namespace
{
    struct ReleaseState
    {
        bool active = false;      // đang thu biên độ / đã thu xong và kênh vẫn im
        float gain = 1.0f;
        QVector<float> cur;       // drawV của lần vẽ gần nhất khi còn tiếng
        QVector<float> snap;      // khung hình cuối cùng trước khi bị ngắt
    };
    struct ReleaseBank { ReleaseState ch[32]; };

    // Trạng thái theo từng cửa sổ (không cần sửa AudioWaveWindow.h).
    QHash<const void*, ReleaseBank>& releaseBanks()
    {
        static QHash<const void*, ReleaseBank> banks;
        return banks;
    }
}

// Mot timer chung cho moi cua so oscilloscope: tat ca update() roi vao cung mot vong lap su kien
// nen Qt gop lai thanh mot lan compose/swap cho ca cua so, thay vi ~10 lan lech nhip nhau.
static QTimer* sharedWaveRefreshTimer()
{
    static QTimer* t = nullptr;
    if (!t)
    {
        t = new QTimer(QCoreApplication::instance());
        t->setTimerType(Qt::PreciseTimer);
        t->start(16);
    }
    return t;
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
    else if (mode == WaveMode::NES)
    {
        resize(1100, 560);
        setMinimumSize(900, 500);
    }
    else
    {
        resize(1100, 780);
        setMinimumSize(900, 700);
    }

    QSurfaceFormat fmt;
    fmt.setSamples(parent ? 4 : 4);
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
    else if (mode == WaveMode::FDS)
    {
        setWindowTitle("NES + FDS Waveform Debug");
        activeChannelCount = 6;

        names[5] = "FDS Wave";
    }
    for (int i = 0; i < activeChannelCount; i++)
        buffers[i].reserve(MAX_SAMPLES);

    refreshTimer = new QTimer(this);
    // refreshTimer rieng khong con chay; dung timer chung (xem sharedWaveRefreshTimer).
    connect(sharedWaveRefreshTimer(), &QTimer::timeout, this, [this]() { update(); });
    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        dmcSmoothY[i].clear();
        dmcSmoothValid[i] = false;
    }
}

void AudioWaveWindow::pushChannels(const AudioDebugChannels& ch)
{
    QMutexLocker lock(&mutex);

    float values[CHANNEL_COUNT]{};
    float periods[CHANNEL_COUNT]{};
    float phases[CHANNEL_COUNT]{};
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

        periods[0] = ch.n163Period1;
        periods[1] = ch.n163Period2;
        periods[2] = ch.n163Period3;
        periods[3] = ch.n163Period4;
        periods[4] = ch.n163Period5;
        periods[5] = ch.n163Period6;
        periods[6] = ch.n163Period7;
        periods[7] = ch.n163Period8;

        phases[0] = ch.n163Phase1;
        phases[1] = ch.n163Phase2;
        phases[2] = ch.n163Phase3;
        phases[3] = ch.n163Phase4;
        phases[4] = ch.n163Phase5;
        phases[5] = ch.n163Phase6;
        phases[6] = ch.n163Phase7;
        phases[7] = ch.n163Phase8;
    }
    else if (mode == WaveMode::VRC7)
    {
        values[0] = ch.vrc7Wave1;
        values[1] = ch.vrc7Wave2;
        values[2] = ch.vrc7Wave3;
        values[3] = ch.vrc7Wave4;
        values[4] = ch.vrc7Wave5;
        values[5] = ch.vrc7Wave6;

        periods[0] = ch.vrc7Wave1Period;
        periods[1] = ch.vrc7Wave2Period;
        periods[2] = ch.vrc7Wave3Period;
        periods[3] = ch.vrc7Wave4Period;
        periods[4] = ch.vrc7Wave5Period;
        periods[5] = ch.vrc7Wave6Period;

        phases[0] = ch.vrc7Wave1Phase;
        phases[1] = ch.vrc7Wave2Phase;
        phases[2] = ch.vrc7Wave3Phase;
        phases[3] = ch.vrc7Wave4Phase;
        phases[4] = ch.vrc7Wave5Phase;
        phases[5] = ch.vrc7Wave6Phase;
    }
    else
    {
        values[0] = ch.pulse1;
        values[1] = ch.pulse2;
        values[2] = ch.triangle;
        values[3] = ch.noise;
        values[4] = ch.dmc;

        periods[0] = ch.pulse1Period;
        periods[1] = ch.pulse2Period;
        periods[2] = ch.trianglePeriod;
        periods[3] = ch.noisePeriod;
        periods[4] = ch.dmcPeriod;

        // Mốc loop-restart của DMC trên trục của scope (totalPushed): sample đang push có chỉ số
        // totalPushed[4], lần restart xảy ra `age` sample trước. -1e18 = chưa đo được loop nào.
        dmcRestartAbs[4] = (ch.dmcAgeSamples >= 0.0)
            ? double(totalPushed[4]) - ch.dmcAgeSamples
            : -1e18;

        if (mode == WaveMode::VRC6)
        {
            values[5] = ch.vrc6Pulse1;
            values[6] = ch.vrc6Pulse2;
            values[7] = ch.vrc6Saw;

            periods[5] = ch.vrc6Pulse1Period;
            periods[6] = ch.vrc6Pulse2Period;
            periods[7] = ch.vrc6SawPeriod;
        }
        else if (mode == WaveMode::S5B)
        {
            values[5] = ch.s5bToneA;
            values[6] = ch.s5bToneB;
            values[7] = ch.s5bToneC;

            periods[5] = ch.s5bToneAPeriod;
            periods[6] = ch.s5bToneBPeriod;
            periods[7] = ch.s5bToneCPeriod;
        }
        else if (mode == WaveMode::MMC5)
        {
            values[5] = ch.mmc5Pulse1;
            values[6] = ch.mmc5Pulse2;
            values[7] = ch.mmc5PCM;

            periods[5] = ch.mmc5Pulse1Period;
            periods[6] = ch.mmc5Pulse2Period;
            periods[7] = 0.0f;
        }
        else if (mode == WaveMode::FDS)
        {
            values[5] = ch.fdsWave;
            periods[5] = ch.fdsWavePeriod;
        }
    }

    for (int i = 0; i < activeChannelCount; i++)
    {
        // Bo qua kenh khong hien thi (cua so chip NSF khong ve 5 kenh NES, khoi phu chi ve 1-2 kenh...):
        // khong loc RC, khong push_back, khong lap lai lich su cho kenh ma cua so nay khong ve.
        if (!isChannelShown(i))
            continue;
        periodHint[i] = std::clamp(periods[i], 0.0f, 8192.0f);
        const bool isNesTriangle =
            (mode != WaveMode::VRC7 && mode != WaveMode::N163 && i == 2);
        const bool triangleOff =
            isNesTriangle && std::abs(values[i]) <= 0.0001f && periods[i] <= 0.0f;

        if (triangleOff)
        {

            periodHint[i] = 0.0f;

            if (!triangleZeroGate[i] && genericTrigLock[i].locked &&
                genericTrigLock[i].period >= 8.0f)
            {
                // Chụp 1 chu kỳ cuối (đã lọc, giống hệt thứ cycle view đang vẽ), BẮT ĐẦU đúng
                // tại cạnh lên của mốc lock và giữ period dạng float. Khi vẽ, chu kỳ này được
                // ghim vào giữa khung (xem sampleTriFrozen) nên không còn trượt theo thời gian.
                const float P = genericTrigLock[i].period;
                const int bsz = int(buffers[i].size());
                const int fsz = int(filteredBuffers[i].size());
                const int usable = std::min(bsz, fsz);
                const int L = int(std::ceil(P)) + 4;
                const qint64 absBase = (qint64)totalPushed[i] - (qint64)bsz;
                const float lockIdx = float(genericTrigLock[i].lockAbsPos - absBase);
                const float kEdge = std::floor((float(usable - 2 - L) - lockIdx) / P);
                const float e = lockIdx + kEdge * P; // cạnh lên mới nhất mà [e, e+L] còn nằm trọn trong buffer
                if (e >= 1.0f && int(e) + L + 1 < usable)
                {
                    const int s0 = int(e);
                    const float fr = e - float(s0);
                    triFrozenCycle[i].resize(L);
                    for (int q = 0; q < L; q++)
                    {
                        const float a = filteredBuffers[i][s0 + q];
                        const float b = filteredBuffers[i][s0 + q + 1];
                        triFrozenCycle[i][q] = a + (b - a) * fr;
                    }
                    triFrozenPeriod[i] = P;
                    triFreezeActive[i] = true;
                    triFreezeAmp[i] = 1.0f;
                }
            }
            triangleZeroGate[i] = true;
        }
        else if (isNesTriangle && triangleZeroGate[i])
        {
            // Kênh bật lại: reset trạng thái tam giác, giữ nguyên buffer. 
            genericTrigLock[i] = TrigLockState{};
            triangleZeroGate[i] = false;
            triFreezeActive[i] = false;
        }

        float v = values[i];
        if (triangleOff)
        {
            v = 0.0f;
        }

        if (mode != WaveMode::VRC7)
        {
            v = std::clamp(v, -1.0f, 1.0f);
        }

        // VRC6: dòng 5/6 là pulse, dòng 7 là saw
        if (mode == WaveMode::VRC6)
        {
            if (i == 5 || i == 6) { v *= 1.4f; v = std::clamp(v, -1.0f, 1.0f); }
            if (i == 7) { v *= 1.4f; }
        }

        // VRC7-only: giữ raw debug sample, KHÔNG scale/clamp ở pushChannels.
        // Lý do: nếu scale rồi clamp/tanh ở đây thì đỉnh sóng bị bẹt từ dữ liệu buffer,
        // paintGL không thể phục hồi lại được. VRC7 sẽ được phóng to bằng auto-gain khi vẽ.
        if (mode == WaveMode::VRC7)
        {
            // VRC7 debug polarity test:
            // Reference oscilloscope shows the flat/limited side on the bottom,
            // while SJNES was showing it on the top. Flip only VRC7 here.
            // Do NOT clamp/scale here, paintGL will auto-gain it.
            if (!std::isfinite(v))
                v = 0.0f;

            v = -v;
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

        // Lọc NGAY tại đây, một lần duy nhất mỗi sample, state không reset —
        // đây là điểm mấu chốt để filter có đủ lịch sử settle vào trạng thái
        // AC cân bằng quanh 0 thay vì bị "khởi động lại" mỗi frame vẽ.
        {
            RcFilterState& st = filterState[i];
            if (!st.initialized)
            {
                st.prevIn1 = v;
                st.initialized = true;
            }
            st.hp1 = 0.999f * (st.hp1 + v - st.prevIn1);      st.prevIn1 = v;
            st.hp2 = 0.998f * (st.hp2 + st.hp1 - st.prevIn2); st.prevIn2 = st.hp1;
            st.lp += 0.948f * (st.hp2 - st.lp);
            filteredBuffers[i].push_back(st.lp);
        }

        if (mode == WaveMode::VRC7 || mode == WaveMode::N163)
        {
            float ph = phases[i];
            if (!std::isfinite(ph))
                ph = 0.0f;
            ph = ph - std::floor(ph);
            phaseBuffers[i].push_back(ph);
        }

        totalPushed[i]++;

        if (buffers[i].size() > MAX_SAMPLES + 512)
        {
            int removeCount = buffers[i].size() - MAX_SAMPLES;
            buffers[i].remove(0, removeCount);
            // filteredBuffers[i] phải bị trim CÙNG SỐ LƯỢNG, cùng đầu, để giữ
            // khớp index 1-1 với buffers[i] (raw). Không reset filterState ở đây —
            // state là "bộ nhớ tụ điện" phải sống xuyên suốt, không liên quan gì
            // đến việc ring buffer hiển thị bị cắt bớt phần cũ.
            if (filteredBuffers[i].size() >= removeCount)
                filteredBuffers[i].remove(0, removeCount);
            if ((mode == WaveMode::VRC7 || mode == WaveMode::N163) && phaseBuffers[i].size() >= removeCount)
                phaseBuffers[i].remove(0, removeCount);
        }
    }
}

void AudioWaveWindow::clearSamples()
{
    QMutexLocker lock(&mutex);
    for (int i = 0; i < activeChannelCount; i++)
    {
        buffers[i].clear();
        phaseBuffers[i].clear();
        filteredBuffers[i].clear();
        filterState[i] = RcFilterState{};
        periodHint[i] = 0.0f;
        vrc7LastPhaseStart[i] = 0.0f;
        vrc7HasPhaseStart[i] = false;
        vrc7CorrRef[i].clear();
        vrc7CorrValid[i] = false;
        vrc7HasLastAnchorAbs[i] = false;
        vrc7LastAnchorAbs[i] = 0;
        triangleZeroGate[i] = false;
        triFreezeActive[i] = false;
        triFrozenCycle[i].clear();
        triFreezeAmp[i] = 0.0f;
        triFrozenPeriod[i] = 0.0f;
        n163SmoothY[i].clear();
        n163SmoothValid[i] = false;
        n163SnapValid[i] = false;

        totalPushed[i] = 0;
        genericTrigLock[i] = TrigLockState{};
        n163TrigLock[i] = TrigLockState{};
        dmcSmoothY[i].clear();
        dmcSmoothValid[i] = false;
        dmcCorrRef[i].clear();
        dmcCorrValid[i] = false;
        dmcRestartAbs[i] = -1e18;
    }
}

void AudioWaveWindow::paintGL()
{
    QVector<float> copy[CHANNEL_COUNT];
    QVector<float> filteredCopy[CHANNEL_COUNT];
    QVector<float> phaseCopy[CHANNEL_COUNT];
    float hintCopy[CHANNEL_COUNT]{};
    double dmcRestartCopy[CHANNEL_COUNT]{};
    quint64 pushedCopy[CHANNEL_COUNT]{};
    {
        QMutexLocker lock(&mutex);
        for (int i = 0; i < CHANNEL_COUNT; i++)
        {
            if (i >= activeChannelCount || !isChannelShown(i))
                continue;   // chi lay snapshot cac kenh thuc su duoc ve
            copy[i] = buffers[i];
            filteredCopy[i] = filteredBuffers[i];
            phaseCopy[i] = phaseBuffers[i];
            hintCopy[i] = periodHint[i];
            dmcRestartCopy[i] = dmcRestartAbs[i];
            pushedCopy[i] = totalPushed[i];
        }
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(0, 0, 0));

    int w = width();
    int h = height();
    int topMargin = tileMode ? 0 : 8;
    int bottomMargin = tileMode ? 0 : 8;
    int drawH = h - topMargin - bottomMargin;

    if (w <= 0 || h <= 0) return;

    // Bố cục dạng lưới 2 cột cho mọi chế độ (NES/VRC6/S5B/MMC5/VRC7/N163).
    // Chi ve cac kenh duoc phep hien thi (NSF: o chip bo 5 kenh NES). Hang/cot tinh theo
    // THU TU trong danh sach, con `c` van la chi so kenh that cho moi logic ben duoi.
    QVector<int> shownChannels;
    for (int c = 0; c < activeChannelCount; c++)
        if (isChannelShown(c))
            shownChannels.push_back(c);

    const int shownCount = shownChannels.size();
    if (shownCount == 0) return;

    int numCols = (tileMode ? tileTwoCols : (shownCount > 1)) ? 2 : 1;
    const int slotOffset = (tileMode && numCols == 2) ? layoutOffset : 0;
    int numRows = (shownCount + slotOffset + numCols - 1) / numCols;

    int panelW = w / numCols;
    int panelH = drawH / numRows;

    // Ô trống (khi số kênh lẻ, ví dụ DMC / VRC6 Saw): vẫn vẽ đủ trục X, trục Y và vạch chia cột
    // để lưới kín hoàn toàn, chỉ là không có sóng trong ô cuối cùng bên phải.
    for (int slot = shownCount + slotOffset; (fillTrailing || !tileMode) && slot < numRows * numCols; slot++)
    {
        const int row = slot / numCols;
        const int col = slot % numCols;
        const int top = topMargin + row * panelH;
        const int left = col * panelW;

        p.fillRect(left, top, panelW, panelH, QColor(0, 0, 0));
        p.setPen(QPen(QColor(70, 70, 70), 1));
        p.drawLine(left + panelW / 2, top, left + panelW / 2, top + panelH);   // trục Y giữa ô
        p.drawLine(left, top + panelH / 2, left + panelW, top + panelH / 2);   // trục X giữa ô
        if (numCols > 1 && col > 0)
            p.drawLine(left, top, left, top + panelH);                         // vạch chia cột
    }

    for (int slot = 0; slot < shownCount; slot++)
    {
        const int c = shownChannels[slot];
        int row = (slot + slotOffset) / numCols;
        int col = (slot + slotOffset) % numCols;
        int top = topMargin + row * panelH;
        int left = col * panelW;
        int midY = top + panelH / 2;

        p.fillRect(left, top, panelW, panelH, QColor(0, 0, 0));

        // Trục dọc (Y-axis) tại điểm trigger - luôn nằm giữa panel
        int midX = left + panelW / 2;
        p.setPen(QPen(QColor(70, 70, 70), 1));
        p.drawLine(midX, top, midX, top + panelH);
        // Đường kẻ ngang (trục 0) ở giữa panel của mỗi channel
        p.drawLine(left, midY, left + panelW, midY);
        // Đường kẻ dọc phân cách cột (chỉ khi có >1 cột; khối nửa phải 1 cột cũng cần vạch chia bên trái)
        if ((numCols > 1 && col > 0) || (tileMode && numCols == 1 && layoutOffset == 1))
        {
            p.setPen(QPen(QColor(70, 70, 70), 1));
            p.drawLine(left, top, left, top + panelH);
        }
        // Tên kênh màu theo channel
        p.setPen(CHANNEL_COLORS[c]);
        p.drawText(left + 10, top + 18, names[c]);

        if (copy[c].size() < 32)
            continue;

        // --- Số sample hiển thị ---
        const int n = copy[c].size();

        // Buffer đã qua filter thật, dùng để VẼ giá trị (không dùng để detect
        // trigger/range — detect vẫn dựa trên copy[c] gốc để không lệch ngưỡng).
        // Filter được tính LIÊN TỤC ngay tại pushChannels() (state không reset
        // mỗi frame), ở đây chỉ lấy phần snapshot đã lọc sẵn khớp với copy[c].
        QVector<float> filteredBuf = filteredCopy[c];

        // Zoom theo kenh: xem visibleSamplesFor().
        int visibleSamples = visibleSamplesFor(c, n, copy[c]);

        if (visibleSamples < 32 || n < visibleSamples + 4)
            continue;

        float minV = copy[c][0];
        float maxV = copy[c][0];

        for (int i = 1; i < n; i++)
        {
            minV = std::min(minV, copy[c][i]);
            maxV = std::max(maxV, copy[c][i]);
        }

        float range = maxV - minV;
        float triggerLevel = (minV + maxV) * 0.5f;
        float fStart = float(n - visibleSamples);
        qint64 chanAbsBase = (qint64)pushedCopy[c] - (qint64)n;

        bool isNoiseDMC = (mode != WaveMode::VRC7 && mode != WaveMode::N163 && (c == 3 || c == 4));
        bool isVrc6Saw = (mode == WaveMode::VRC6 && c == 7);
        bool isFdsWave = (mode == WaveMode::FDS && c == 5);
        bool isTriangle = (mode != WaveMode::VRC7 && mode != WaveMode::N163 && c == 2);
        const bool isSquareLike =
            (mode != WaveMode::VRC7 && mode != WaveMode::N163 && (c == 0 || c == 1)) ||
            (mode == WaveMode::VRC6 && (c == 5 || c == 6)) ||
            (mode == WaveMode::S5B && c >= 5) ||
            (mode == WaveMode::MMC5 && (c == 5 || c == 6));

        if (mode == WaveMode::VRC7)
        {
            bool corrOk = false;
            QVector<float> newRef;
            float corrStart = findVrc7CorrscopeStart(
                copy[c], phaseCopy[c], n, visibleSamples, hintCopy[c],
                vrc7CorrRef[c], vrc7CorrValid[c], newRef, corrOk,
                chanAbsBase, vrc7LastAnchorAbs[c], vrc7HasLastAnchorAbs[c]
            );

            if (corrOk)
            {
                fStart = corrStart;
                vrc7LastPhaseStart[c] = corrStart;
                vrc7HasPhaseStart[c] = true;
                vrc7CorrRef[c] = newRef;
                vrc7CorrValid[c] = true;
            }
            else if (vrc7HasPhaseStart[c])
            {
                fStart = vrc7LastPhaseStart[c];
            }
        }
        else if (range > 0.02f && !isNoiseDMC)
        {
            if (mode == WaveMode::N163)
            {
                // N163 tự khóa pha ở khối vẽ riêng bên dưới.
            }
            else if (isVrc6Saw && kVrc6SawResetLock)
            {
                // Khóa thẳng vào cạnh reset: reset nằm đúng trục dọc giữa khung, không cần dời P/2.
                const float edge = findLockedCenterReset(
                    copy[c], n, visibleSamples, chanAbsBase, hintCopy[c], genericTrigLock[c]);
                if (edge >= 0.0f)
                    fStart = edge - float(visibleSamples) * 0.5f;
            }
            else if (isVrc6Saw && kVrc6SawEdgeLock)
            {
                // Saw là sóng dạng cố định (1 cạnh dốc lên mỗi chu kỳ rồi reset), nên dùng chung trigger
                // khóa pha với Pulse/Triangle: mốc neo bằng period thật, re-anchor vào crossing đo được.
                // Cạnh lên cắt mức giữa nằm ở GIỮA đoạn dốc, còn cạnh reset nằm sau nó nửa chu kỳ.
                // Dời thêm P/2 để điểm reset vẫn nằm đúng trục dọc giữa khung như bản cũ.
                const float edge = findLockedCenterEdge(
                    copy[c], n, visibleSamples, chanAbsBase, hintCopy[c], genericTrigLock[c]);
                if (edge >= 0.0f)
                {
                    float P = genericTrigLock[c].period;
                    if (!(P >= 8.0f && P <= 8192.0f))
                        P = hintCopy[c];

                    if (P >= 8.0f && P <= 8192.0f)
                    {
                        fStart = edge + P * 0.5f - float(visibleSamples) * 0.5f;
                        const float maxStart = float(n - visibleSamples - 2);
                        while (fStart > maxStart && fStart - P >= 0.0f)
                            fStart -= P;
                    }
                    else
                    {
                        fStart = edge - float(visibleSamples) * 0.5f;
                    }
                }
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
                        fStart = float(i - 1) - float(visibleSamples) * 0.50f;
                        break;
                    }
                }
            }
            else if (isFdsWave || isTriangle || isSquareLike)
            {
                // Pulse (NES/VRC6/MMC5/S5B), Triangle, FDS: cùng một trigger khóa pha (xem findLockedCenterEdge).
                const float edge = findLockedCenterEdge(
                    copy[c], n, visibleSamples, chanAbsBase, hintCopy[c], genericTrigLock[c]);
                if (edge >= 0.0f)
                    fStart = edge - float(visibleSamples) * 0.5f;
            }
            else
            {
                int searchStart = n - visibleSamples - 1;

                if (searchStart < 1)
                    searchStart = 1;

                for (int i = searchStart; i >= 1; i--)
                {
                    if (copy[c][i - 1] < triggerLevel && copy[c][i] >= triggerLevel)
                    {
                        float slope = copy[c][i] - copy[c][i - 1];
                        float frac = (slope > 0.0f) ? (triggerLevel - copy[c][i - 1]) / slope : 0.0f;
                        fStart = float(i - 1) + frac - float(visibleSamples) * 0.50f;
                        break;
                    }
                }
            }
        }
        else if (isNoiseDMC && c == 4)
        {
            // Toan bo logic khoa trigger DMC nam trong findDmcTriggerStart().
            fStart = findDmcTriggerStart(c, copy[c], filteredBuf, n, visibleSamples,
                                         hintCopy[c], dmcRestartCopy[c], chanAbsBase, fStart);
        }
        if ((isFdsWave || isVrc6Saw) && hintCopy[c] < 8.0f)
            genericTrigLock[c] = TrigLockState{}; // FDS im lặng: nốt sau acquire lại mốc mới

        fStart = std::clamp(fStart, 0.0f, float(n - visibleSamples - 2));

        // Bien do ve theo kenh: xem ampFor().
        float amp = ampFor(c, panelH);

        if (mode == WaveMode::N163)
        {
            // Toan bo logic ve N163 nam trong drawN163Channel().
            if (drawN163Channel(p, c, left, midY, panelW, amp, copy[c], filteredBuf, phaseCopy[c],
                                n, visibleSamples, range, triggerLevel, pushedCopy[c], hintCopy[c]))
                continue;
        }

        // VRC6 Saw dùng chung filteredBuf (đã lọc liên tục tại pushChannels()).
        QVector<float> vrc6SawFiltered;
        if (isVrc6Saw)
        {
            vrc6SawFiltered = filteredBuf;
        }

        QPen pen(CHANNEL_COLORS[c]);
        if (mode == WaveMode::VRC7)
            pen.setWidthF(2.0f);
        else if (mode == WaveMode::VRC6 && c == 7)
            pen.setWidthF(2.0f);
        else
            pen.setWidthF((c == 0 || c == 1 || c >= 5) ? 1.8f : 1.2f);
        pen.setCapStyle(Qt::FlatCap);     // round cap/join lam stroker sinh them rat nhieu hinh hoc tren duong ~1000 doan
        pen.setJoinStyle(Qt::BevelJoin);
        p.setPen(pen);

        QPainterPath path;



        bool useFrozenCycle = isTriangle && triFreezeActive[c] && !triFrozenCycle[c].isEmpty();
        float freezeAmpNow = triFreezeAmp[c];

        auto sampleTriFrozen = [&](float fIdx) -> float
            {
                // GHIM vào giữa khung, giống cycle view: vị trí chỉ phụ thuộc khoảng cách tới
                // tâm, KHÔNG phụ thuộc thời gian. Bản cũ tính theo vị trí tuyệt đối (chanAbsBase +
                // fIdx) nên khi tín hiệu ngắt, fStart rơi về mặc định (n - visibleSamples) và
                // trượt 1 sample mỗi sample -> chu kỳ đóng băng bị cuộn ngang trong lúc mờ dần.
                const QVector<float>& fc = triFrozenCycle[c];
                const float P = triFrozenPeriod[c];
                if (fc.size() < 8 || P < 8.0f)
                    return 0.0f;
                float rel = (fIdx - (fStart + float(visibleSamples) * 0.5f)) / P;
                rel -= std::floor(rel);
                const float pos = std::min(rel * P, float(fc.size() - 3));
                return sampleInterp(fc, pos) * freezeAmpNow;
            };

        // Cycle view cho sóng vuông + tam giác (xem buildCycleView). Không dùng khi tam giác đang
        // chạy hiệu ứng freeze riêng, hoặc khi chưa biết period (kênh im lần đầu).
        bool cycleView = false;
        float cvRef = 0.0f, cvPeriod = 0.0f, cvCenter = 0.0f;
        if (kCycleLockedView && (isSquareLike || isTriangle) && !useFrozenCycle && range > 0.02f)
        {
            float period = hintCopy[c];
            if (!(period >= 8.0f && period <= 8192.0f))
                period = genericTrigLock[c].period; // kênh vừa bị ngắt: dùng period cuối đã biết
            CycleViewInfo cv = buildCycleView(copy[c], n, period);
            if (cv.ok)
            {
                cycleView = true;
                cvRef = cv.ref;
                cvPeriod = cv.period;
                cvCenter = fStart + float(visibleSamples) * 0.5f; // cạnh được trigger nằm giữa khung
            }
        }

        // ---- Release mềm: cập nhật trạng thái theo đuôi buffer ----
        ReleaseState* rs = nullptr;
        const bool relEnabled = kSmoothRelease && (isSquareLike || isTriangle) && !cycleView && c < 32;
        if (relEnabled)
        {
            rs = &releaseBanks()[this].ch[c];

            if (useFrozenCycle)
            {
                // Tam giác đang chạy hiệu ứng freeze riêng: coi như đã release, tránh fade đúp sau đó.
                rs->active = true;
                rs->gain = 0.0f;
            }
            else
            {
                float relPeriod = hintCopy[c];
                if (!(relPeriod >= 8.0f && relPeriod <= 8192.0f))
                    relPeriod = genericTrigLock[c].period;

                const bool silent = tailIsSilent(copy[c], n, relPeriod, range);
                if (silent)
                {
                    if (!rs->active && !rs->cur.isEmpty())
                    {
                        rs->active = true;
                        rs->gain = 1.0f;
                        rs->snap = rs->cur;
                    }
                }
                else
                {
                    rs->active = false;
                }
            }
        }

        int drawPoints = std::max(2, panelW);   // ~1 diem/pixel la du, truoc day 2-3 diem/pixel
        int triDrawPoints = std::max(2, panelW * 2);
        int currentPoints = isTriangle ? triDrawPoints : drawPoints;
        if (c == 4)
        {
            if (dmcSmoothY[c].size() != currentPoints)
            {
                dmcSmoothY[c].resize(currentPoints);
                dmcSmoothValid[c] = false;
            }
        }
        if (rs && !rs->active)
        {
            if (rs->cur.size() != currentPoints)
                rs->cur = QVector<float>(currentPoints, 0.0f);
        }
        const bool releasing = rs && rs->active && rs->snap.size() == currentPoints;

        int viewStart = std::clamp(int(fStart), 0, n - 1);
        int viewEnd = std::clamp(int(fStart + visibleSamples), 0, n - 1);
        float fMin = filteredBuf[viewStart];
        float fMax = filteredBuf[viewStart];
        for (int i = viewStart; i <= viewEnd; i++)
        {
            fMin = std::min(fMin, filteredBuf[i]);
            fMax = std::max(fMax, filteredBuf[i]);
        }
        if (cycleView)
        {
            // Trục giữa lấy từ CHÍNH chu kỳ đang lặp, không lấy từ cả khung (khung cuộn có thể lẫn cũ/mới).
            const int a = std::clamp(int(cvRef), 0, n - 1);
            const int b = std::clamp(int(cvRef + cvPeriod) + 1, 0, n - 1);
            fMin = filteredBuf[a];
            fMax = fMin;
            for (int i = a; i <= b; i++)
            {
                fMin = std::min(fMin, filteredBuf[i]);
                fMax = std::max(fMax, filteredBuf[i]);
            }
        }
        float fCenter = (fMin + fMax) * 0.5f;
        for (int i = 0; i < currentPoints; i++)
        {
            float fIdx = fStart + (float(i) / float(currentPoints - 1)) * float(visibleSamples);

            if (fIdx < 0 || fIdx >= float(n - 1)) continue;

            float x = left + float(i) / float(currentPoints - 1) * float(panelW);
            float y;
            auto drawSample = [&](float s) -> float
                {
                    if (mode == WaveMode::VRC7)
                    {
                        const float halfRange = std::max(range * 0.5f, 0.0015f);
                        float vrc7 = (s - triggerLevel) / halfRange;
                        return std::clamp(vrc7, -1.18f, 1.18f);
                    }
                    bool isPulseChannel = (c == 0 || c == 1 ||
                        (mode == WaveMode::VRC6 && (c == 5 || c == 6 || c == 7)) ||
                        (mode == WaveMode::S5B && c >= 5) ||
                        (mode == WaveMode::MMC5 && (c == 5 || c == 6)));

                    if (isVrc6Saw)
                    {
                        if (range <= 0.02f)
                            return 0.0f;
                        constexpr float kSawGain = 1.3f; // chỉnh to/nhỏ toàn bộ dáng sóng
                        const float lim = (panelH * 0.5f - 2.0f) / amp; // không cho tràn panel
                        return std::clamp(s * kSawGain, -lim, lim);
                    }

                    if (isPulseChannel)
                    {
                        if (range > 0.02f)
                        {
                            return s;
                        }
                        else
                        {
                            return 0.0f;
                        }
                    }
                    return s;
                };

            float s = 0.0f;
            if (useFrozenCycle)
            {
                s = sampleTriFrozen(fIdx);
            }
            else if (cycleView)
            {
                float rel = (fIdx - cvCenter) / cvPeriod;
                rel -= std::floor(rel);
                const float pos = cvRef + rel * cvPeriod;
                s = isTriangle ? sampleInterp(filteredBuf, pos) : sampleLinear(filteredBuf, pos);
            }
            else if (mode == WaveMode::VRC6 && c == 7)
            {
                s = sampleSawSmooth(vrc6SawFiltered, copy[c], fIdx);
            }
            else if (isNoiseDMC && c == 4)
            {
                s = sampleLinear(filteredBuf, fIdx);
            }
            else if (c == 0 || c == 1 || (mode == WaveMode::VRC6 && c >= 5) || (mode == WaveMode::S5B && c >= 5) || (mode == WaveMode::MMC5 && (c == 5 || c == 6)))
            {
                s = sampleLinear(filteredBuf, fIdx);
            }
            else
            {
                s = sampleInterp(filteredBuf, fIdx);
            }
            float drawV = drawSample(s);
            // VRC6: lật ngược sóng (đảo dấu quanh tâm đã căn giữa).
            if (mode == WaveMode::VRC6 && c >= 5)
                drawV = drawV;
            if (rs)
            {
                if (releasing)
                    drawV = rs->snap[i] * rs->gain;    
                else if (!rs->active && i < rs->cur.size())
                    rs->cur[i] = drawV;         
            }
            y = midY - drawV * amp;

            if (c == 4)
            {
                if (dmcSmoothValid[c])
                {
                    y = dmcSmoothY[c][i] * 0.10f + y * 0.90f;
                }
                dmcSmoothY[c][i] = y;
            }
            if (i == 0) path.moveTo(x, y);
            else        path.lineTo(x, y);
        }
        if (c == 4)
        {
            dmcSmoothValid[c] = true;
        }

        p.drawPath(path);

        if (rs && rs->active && !useFrozenCycle)
        {
            rs->gain *= kReleaseDecay;
            if (rs->gain < 0.004f)
                rs->gain = 0.0f;
        }

        if (isTriangle && triFreezeActive[c])
        {
            triFreezeAmp[c] *= 0.93f;
            if (triFreezeAmp[c] < 0.01f)
            {
                triFreezeActive[c] = false;
                triFrozenCycle[c].clear();
            }
        }
    }
}

// =====================================================================================
// DMC: khoa trigger, 2 tang
//  1) Co loop: khoa theo moc Restart() phan cung (dmcRestart) + modulo chu ky,
//     roi bam canh len gan moc.
//  2) Khong co loop: template-lock bang normalized cross-correlation + snap vao canh len that.
// Tra ve fStart (toa do buffer cua mep trai khung); khong khoa duoc thi tra lai fStart truyen vao.
// =====================================================================================
float AudioWaveWindow::findDmcTriggerStart(int c, const QVector<float>& raw, const QVector<float>& filteredBuf,
    int n, int visibleSamples, float dmcHint, double dmcRestart,
    qint64 chanAbsBase, float fStart)
{
    bool dmcHardLocked = false;

    // Luu template zero-mean (da tru DC) cho cross-correlation fallback.
    auto storeDmcRef = [&](const QVector<float>& b, int s)
        {
            if (s < 0 || s + visibleSamples > b.size()) return;
            double m = 0.0;
            for (int k = 0; k < visibleSamples; k++) m += b[s + k];
            m /= double(visibleSamples);
            dmcCorrRef[c].resize(visibleSamples);
            for (int k = 0; k < visibleSamples; k++)
                dmcCorrRef[c][k] = float(double(b[s + k]) - m);
            dmcCorrValid[c] = true;
        };

    if (dmcHint >= 8.0f && dmcHint <= 8192.0f && dmcRestart > -1e17)
    {
        const double P = double(dmcHint);
        const double H = double(visibleSamples) * 0.5;
        const double nowAbs = double(chanAbsBase) + double(n);

        // Pha hien tai so voi moc Restart() gan nhat, tinh bang modulo thuan tuy.
        double phase = std::fmod(nowAbs - dmcRestart, P);
        if (phase < 0.0) phase += P;

        // Vi tri (cuc bo, trong buffer hien tai) cua lan Restart() gan nowAbs nhat.
        double edgeLocal = (nowAbs - phase) - double(chanAbsBase);

        // Chon canh MUON NHAT ma cua so [edge-H, edge+H] van nam trong buffer
        // (sample lap theo chu ky P nen chon canh nao hinh cung giong het nhau).
        const double hi = double(n) - H - 2.0;
        if (edgeLocal > hi)
            edgeLocal -= std::ceil((edgeLocal - hi) / P) * P;

        // Tinh chinh: moc Restart() la su kien nap lai DMA, con buoc nhay that o
        // ngo ra DMC den muon/som hon mot doan (buffer 1 byte + shift register).
        // Bam vao canh cat truc 0 doc nhat (xem kDmcTriggerRising) quanh moc phan cung, do tren tin hieu da loc + lam muot.
        // Tin hieu lap theo chu ky P nen vi tri nay on dinh giua cac frame.
        {
            // Trigger dung kieu oscilloscope: canh doc nhat cat MUC 0 cua CHINH duong dang ve
            // (filteredBuf, truc ngang = 0) nen duong cheo dam thang qua giao diem 2 truc.
            // Do tren ban lam muot (box) de gon +-1 bit cua DPCM khong tao ra nhieu cat gia.
            // Cua so tim = 1 chu ky P quanh moc Restart() nen lua chon lap lai dung moi chu ky.
            constexpr float dir = kDmcTriggerRising ? 1.0f : -1.0f;
            const QVector<float>& fb = filteredBuf;
            if (fb.size() == n)
            {
                const int Ks = std::clamp(int(P / 40.0), 3, 24);
                const double Rw = P * 0.5;
                const int lo = std::max(Ks + 2, int(std::ceil(std::max(H, edgeLocal - Rw))));
                const int hiI = std::min(n - Ks - 3, int(std::floor(std::min(hi, edgeLocal + Rw))));
                if (hiI > lo)
                {
                    QVector<double> ps(n + 1);
                    ps[0] = 0.0;
                    for (int i = 0; i < n; i++)
                        ps[i + 1] = ps[i] + double(fb[i]);
                    auto sm = [&](int i) -> float
                        {
                            const int a = std::max(0, i - Ks);
                            const int b = std::min(n - 1, i + Ks);
                            return float((ps[b + 1] - ps[a]) / double(b - a + 1));
                        };

                    float sMin = sm(lo);
                    float sMax = sMin;
                    for (int i = lo; i <= hiI; i++)
                    {
                        const float v = sm(i);
                        sMin = std::min(sMin, v);
                        sMax = std::max(sMax, v);
                    }
                    const float minRise = std::max(1e-5f, (sMax - sMin) * 0.05f);

                    double bestIdx = -1.0;
                    float bestRise = 0.0f;
                    for (int i = lo; i <= hiI; i++)
                    {
                        const float a = dir * sm(i - 1);
                        const float b = dir * sm(i);
                        if (a < 0.0f && b >= 0.0f)
                        {
                            const float rise = dir * (sm(i + Ks) - sm(i - 1 - Ks));
                            if (rise >= minRise && rise > bestRise)
                            {
                                bestRise = rise;
                                bestIdx = double(i - 1) + double(-a / (b - a));
                            }
                        }
                    }
                    if (bestIdx >= H && bestIdx <= hi)
                        edgeLocal = bestIdx;
                }
            }
        }

        if (edgeLocal >= H)
        {
            fStart = float(edgeLocal - H);
            dmcHardLocked = true;

            // Giu reference cua fallback luon "tuoi" de luc mat khoa khong nhay
            // sang mot doan cu khong lien quan.
            storeDmcRef(raw, std::clamp(int(fStart), 0, std::max(0, n - visibleSamples - 1)));
        }
    }
    else
    {
        // Chua co loop / sample one-shot / vua doi $4010-$4013 -> moc cu
        // khong con nghia ly gi, de cross-correlation fallback tu lo.
        genericTrigLock[c] = TrigLockState{};
    }

    if (!dmcHardLocked)
    {
        // Template-lock: chon MOT LAN canh len manh nhat lam moc, cac frame sau chi bam
        // template bang normalized cross-correlation (khong do lai canh moi frame).
        // On dinh voi song nhieu canh len moi chu ky nhu Sunsoft bass.
        const QVector<float>& fb = filteredBuf;
        if (fb.size() == n)
        {
            const int H = visibleSamples / 2;
            const int hiS = n - visibleSamples - 2;              // fStart lon nhat hop le
            const int loS = std::max(1, hiS - visibleSamples);   // chi xet ~1 khung moi nhat
            constexpr int STRIDE = 4;

            // Template: tru DC, chuan hoa RMS = 1
            auto normalizeTpl = [&](QVector<float>& t)
                {
                    double m = 0.0;
                    for (float v : t) m += v;
                    m /= double(t.size());
                    double e = 0.0;
                    for (float& v : t) { v = float(v - m); e += double(v) * v; }
                    const float inv = float(1.0 / std::sqrt(std::max(e / double(t.size()), 1e-12)));
                    for (float& v : t) v *= inv;
                };
            auto cutTpl = [&](int s, QVector<float>& t)
                {
                    t.resize(visibleSamples);
                    for (int k = 0; k < visibleSamples; ++k) t[k] = fb[s + k];
                    normalizeTpl(t);
                };

            // Chon MOT LAN: canh len manh nhat cat truc 0 (hoa thi lay cai moi nhat).
            // Tra ve toa do buffer cua canh (se nam giua khung), hoac -1.
            auto findEdge = [&](int loIn, int hiIn, float guess) -> float
                {
                    const int Ks = 20; // lam muot co "macro": bo qua gon DPCM nho, chi thay canh lon
                    const int loC = std::max({ Ks + 2, H, loIn });
                    const int hiI = std::min({ n - H - 2, n - Ks - 3, hiIn });
                    if (hiI <= loC) return -1.0f;

                    QVector<double> ps(n + 1);
                    ps[0] = 0.0;
                    for (int i = 0; i < n; ++i) ps[i + 1] = ps[i] + double(fb[i]);
                    auto sm = [&](int i) -> float
                        {
                            const int a = std::max(0, i - Ks), b = std::min(n - 1, i + Ks);
                            return float((ps[b + 1] - ps[a]) / double(b - a + 1));
                        };

                    QVector<float> cx, cr;
                    float maxRise = 0.0f;
                    for (int i = loC; i <= hiI; ++i)
                    {
                        const float a = sm(i - 1), b = sm(i);
                        if (a < 0.0f && b >= 0.0f)
                        {
                            // Canh hop le phai LEN DEU ca hai phia diem cat (lay min trai/phai): diem cat nam trong
                            // gon phang ngay truoc mot canh doc co phia trai ~0 nen bi loai, khong an gian do doc.
                            const float rise = std::min(sm(i) - sm(i - Ks), sm(i + Ks) - sm(i));
                            if (rise > 1e-4f)
                            {
                                cx.push_back(float(i - 1) + (-a) / (b - a));
                                cr.push_back(rise);
                                maxRise = std::max(maxRise, rise);
                            }
                        }
                    }
                    // Trong cac canh manh ngang nhau (>= 90% canh manh nhat), lay cai gan `guess` nhat.
                    float bestX = -1.0f, bestD = 1e30f;
                    for (int k = 0; k < cx.size(); ++k)
                    {
                        const float d = std::abs(cx[k] - guess);
                        if (cr[k] >= maxRise * 0.9f && d < bestD) { bestD = d; bestX = cx[k]; }
                    }
                    return bestX;
                };

            QVector<float>& ref = dmcCorrRef[c];
            bool locked = false;

            if (dmcCorrValid[c] && ref.size() == visibleSamples && hiS > loS)
            {
                int cnt = 0; double rs = 0.0, rss = 0.0;
                for (int k = 0; k < visibleSamples; k += STRIDE)
                {
                    rs += ref[k]; rss += double(ref[k]) * ref[k]; ++cnt;
                }
                const double refVar = std::max(rss - rs * rs / double(cnt), 1e-9);

                QVector<float> sc(hiS - loS + 1);
                float best = -2.0f;
                for (int s = loS; s <= hiS; ++s)
                {
                    double dot = 0.0, sx = 0.0, sxx = 0.0;
                    for (int k = 0; k < visibleSamples; k += STRIDE)
                    {
                        const double x = fb[s + k];
                        dot += double(ref[k]) * x; sx += x; sxx += x * x;
                    }
                    const double varX = std::max(sxx - sx * sx / double(cnt), 1e-9);
                    const float score = float((dot - rs * sx / double(cnt)) / std::sqrt(varX * refVar));
                    sc[s - loS] = score;
                    best = std::max(best, score);
                }

                if (best >= 0.6f)
                {
                    // Cac chu ky giong het nhau -> lay dinh cuc bo MOI NHAT con gan diem tot nhat.
                    int pick = -1;
                    for (int i = sc.size() - 1; i >= 0; --i)
                    {
                        const float l = (i > 0) ? sc[i - 1] : -2.0f;
                        const float r = (i + 1 < sc.size()) ? sc[i + 1] : -2.0f;
                        if (sc[i] >= best - 0.08f && sc[i] >= l && sc[i] >= r) { pick = i; break; }
                    }
                    if (pick >= 0)
                    {
                        float frac = 0.0f; // noi suy parabol duoi-sample
                        if (pick > 0 && pick + 1 < sc.size())
                        {
                            const float a = sc[pick - 1], b = sc[pick], d = sc[pick + 1];
                            const float den = a - 2.0f * b + d;
                            if (den < -1e-6f) frac = std::clamp(0.5f * (a - d) / den, -0.5f, 0.5f);
                        }
                        fStart = float(loS + pick) + frac;
                        locked = true;

                        // Bam lai vao CANH THAT gan tam: correlation chi cho vi tri xap xi, canh do duoc
                        // moi dam bao duong cheo dam dung giao diem 2 truc va template khong bi troi dan.
                        {
                            const float center = fStart + float(H);
                            const int R = std::clamp(visibleSamples / 24, 16, 64);
                            const float snapped = findEdge(int(center) - R, int(center) + R, center);
                            if (snapped >= 0.0f)
                                fStart = snapped - float(H);
                        }

                        if (best >= 0.85f) // template theo kip envelope/pitch rat cham
                        {
                            QVector<float> cur;
                            cutTpl(std::clamp(int(std::lround(fStart)), loS, hiS), cur);
                            for (int k = 0; k < visibleSamples; ++k)
                                ref[k] = 0.95f * ref[k] + 0.05f * cur[k];
                            normalizeTpl(ref);
                        }
                    }
                }
            }

            if (!locked)
            {
                const int hiC0 = n - H - 2;
                const float e = findEdge(hiC0 - visibleSamples * 2, hiC0, float(hiC0));
                if (e >= 0.0f)
                {
                    fStart = e - float(H);
                    cutTpl(std::clamp(int(std::lround(fStart)), 1, std::max(1, hiS)), ref);
                    dmcCorrValid[c] = true;
                }
                else
                {
                    fStart = float(n - visibleSamples);
                    dmcCorrValid[c] = false;
                }
            }
        }
    }

    return fStart;
}

// =====================================================================================
// N163: ve 1 kenh. Cac tang khoa song (thu tu uu tien):
//   1) khoa pha bang phase 24-bit THAT cua chip (n163PhaseLock)
//   2) hard period anchor tren canh len do duoc (updateHardPeriodAnchor)
//   3) resolveLockedTrigger (chua co period hint hop le)
// Khong ve duoc thi lam hieu ung release (song tan ve duong giua).
// Tra ve true = da ve xong kenh nay; false = paintGL ve tiep bang duong ve chung.
// =====================================================================================
bool AudioWaveWindow::drawN163Channel(QPainter& p, int c, int left, int midY, int panelW, float amp,
    const QVector<float>& raw, const QVector<float>& filteredBuf,
    const QVector<float>& phase, int n, int visibleSamples,
    float range, float triggerLevel, quint64 pushed, float hint)
{
    int drawPoints = std::max(2, panelW);

    QPen n163Pen(CHANNEL_COLORS[c]);
    n163Pen.setWidthF(1.35f);
    n163Pen.setCapStyle(Qt::FlatCap);
    n163Pen.setJoinStyle(Qt::BevelJoin);
    p.setPen(n163Pen);

    // Tim cac rising crossing du manh trong vung gan cuoi buffer.
    QVector<float> idxs;
    QVector<float> slopes;
    idxs.reserve(128);
    slopes.reserve(128);

    int scanStart = std::max(1, n - visibleSamples * 4);
    int scanEnd = n - 2;
    float minSlope = std::max(0.0001f, range * 0.08f);

    for (int si = scanStart + 1; si <= scanEnd; si++)
    {
        float prev = raw[si - 1];
        float cur = raw[si];

        if (prev < triggerLevel && cur >= triggerLevel)
        {
            float slope = cur - prev;
            if (slope >= minSlope)
            {
                float frac = (triggerLevel - prev) / slope;
                float idx = float(si - 1) + frac;
                idxs.push_back(idx);
                slopes.push_back(slope);
            }
        }
    }

    bool drewN163 = false;

    qint64 absBase = (qint64)pushed - (qint64)n;

    // ===== TANG 1 (N163): khoa pha bang phase THAT cua chip =====
    // Khong do canh tren waveform (wavetable 4-bit tuy y co nhieu canh len moi chu ky).
    // Moc = dau wavetable suy tu n163PhaseN + period; xem n163PhaseLock().
    // Khong du dieu kien (chua co phase/period hop le) -> roi xuong duong do canh ben duoi.
    if (kN163PhaseLock && phase.size() == n && hint >= 8.0f && hint <= 8192.0f)
    {
        float wStart = 0.0f, wOffset = 0.0f;
        if (n163PhaseLock(phase, filteredBuf, n, hint, kN163SnapRisingEdge, wStart, wOffset, n163SnapFrac[c], n163SnapValid[c]))
        {
            const float period = hint;
            const float cyclesAcross = std::clamp(float(visibleSamples) / period, 1.0f, 24.0f);

            if (n163SmoothY[c].size() != drawPoints)
            {
                n163SmoothY[c].resize(drawPoints);
                n163SmoothValid[c] = false;
            }

            QPainterPath n163Path;
            for (int pi = 0; pi < drawPoints; pi++)
            {
                const float t = float(pi) / float(drawPoints - 1);
                float ph = std::fmod((t - 0.5f) * cyclesAcross, 1.0f);
                if (ph < 0.0f)
                    ph += 1.0f;

                float rel = wOffset + ph * period;
                rel -= std::floor(rel / period) * period;
                const float fIdx = std::clamp(wStart + rel, 0.0f, float(n - 2));

                const float yTarget = midY - sampleInterp(filteredBuf, fIdx) * amp;
                const float y = n163SmoothValid[c]
                    ? (n163SmoothY[c][pi] * 0.08f + yTarget * 0.92f)
                    : yTarget;
                n163SmoothY[c][pi] = y;

                const float x = left + t * float(panelW);
                if (pi == 0) n163Path.moveTo(x, y);
                else         n163Path.lineTo(x, y);
            }
            n163SmoothValid[c] = true;
            p.drawPath(n163Path);
            drewN163 = true;
        }
    }

    // TANG 2 (N163, khi tang 1 khong dung duoc): hard anchor tren canh len do duoc --
    // sau khi acquire canh dau tien, period that quyet dinh vi tri cac chu ky sau.
    bool hasHardAnchor = updateHardPeriodAnchor(
        n163TrigLock[c], absBase, idxs, slopes,
        hint, 8.0f, 8192.0f, 0.055f
    );

    if (!drewN163 && hasHardAnchor && n163TrigLock[c].period >= 8.0f)
    {
        float period = n163TrigLock[c].period;
        float cycleEnd = predictEdgeNearEnd(n163TrigLock[c], absBase, float(n));
        float cycleStart = cycleEnd - period;

        if (cycleEnd >= 0.0f && cycleStart >= 1.0f && cycleEnd < float(n - 2))
        {
            // Giu zoom giong visibleSamples cu: so chu ky tren man hinh = visibleSamples / period.
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
                // Dich moc trigger tu mep trai (t=0) ra DUNG GIUA (t=0.5) de
                // khop voi duong ke doc midX dang ve, giong cach Pulse/Triangle
                // can giua trigger point cua chung.
                float tShifted = t - 0.5f;
                float ph = std::fmod(tShifted * cyclesAcross, 1.0f);
                if (ph < 0.0f)
                    ph += 1.0f;

                float fIdx = cycleStart + ph * period;
                fIdx = std::clamp(fIdx, 0.0f, float(n - 2));

                float sample = sampleInterp(filteredBuf, fIdx);
                float yTarget = midY - sample * amp;

                float y = yTarget;
                if (n163SmoothValid[c])
                {
                    // Bam nhanh theo frame, khong relax cham;
                    // moi diem X cap nhat cung luc, khong lech tick.
                    y = n163SmoothY[c][pi] * 0.08f + yTarget * 0.92f;
                }

                n163SmoothY[c][pi] = y;

                float x = left + t * float(panelW);
                if (pi == 0) n163Path.moveTo(x, y);
                else         n163Path.lineTo(x, y);
            }

            n163SmoothValid[c] = true;
            p.drawPath(n163Path);
            drewN163 = true;
        }
    }

    // Fallback cho truong hop chua co period hint hop le.
    if (!drewN163 && !idxs.isEmpty())
    {
        float maxP = std::max(float(visibleSamples), hint * 2.0f);
        float lockedEdge = resolveLockedTrigger(n163TrigLock[c], absBase, idxs, slopes,
            hint, 8.0f, maxP, float(n));

        if (lockedEdge >= 0.0f && n163TrigLock[c].period >= 8.0f)
        {
            float period = n163TrigLock[c].period;
            float cycleEnd = lockedEdge;
            float cycleStart = cycleEnd - period;

            if (cycleStart >= 1.0f && cycleEnd < float(n - 2))
            {
                float cyclesAcross = std::clamp(float(visibleSamples) / period, 1.0f, 24.0f);

                if (n163SmoothY[c].size() != drawPoints)
                {
                    n163SmoothY[c].resize(drawPoints);
                    n163SmoothValid[c] = false;
                }

                QPainterPath n163Path;
                for (int pi = 0; pi < drawPoints; pi++)
                {
                    float t = float(pi) / float(drawPoints - 1);
                    float tShifted = t - 0.5f;
                    float ph = std::fmod(tShifted * cyclesAcross, 1.0f);
                    if (ph < 0.0f)
                        ph += 1.0f;
                    float fIdx = std::clamp(cycleStart + ph * period, 0.0f, float(n - 2));
                    float yTarget = midY - sampleInterp(filteredBuf, fIdx) * amp;
                    float y = n163SmoothValid[c] ? (n163SmoothY[c][pi] * 0.08f + yTarget * 0.92f) : yTarget;
                    n163SmoothY[c][pi] = y;
                    float x = left + t * float(panelW);
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
        return true;

    // Khong ve duoc frame nay: thu ve ve duong giua (release) cho den khi het thay.
    if (n163SmoothValid[c] && n163SmoothY[c].size() == drawPoints)
    {
        QPainterPath releasePath;

        bool stillVisible = false;
        const float releaseKeep = 0.72f;
        const float releaseToCenter = 1.0f - releaseKeep;

        for (int pi = 0; pi < drawPoints; pi++)
        {
            float t = float(pi) / float(drawPoints - 1);

            float y = n163SmoothY[c][pi] * releaseKeep + float(midY) * releaseToCenter;
            n163SmoothY[c][pi] = y;

            if (std::abs(y - float(midY)) > 0.75f)
                stillVisible = true;

            float x = left + t * float(panelW);
            if (pi == 0) releasePath.moveTo(x, y);
            else         releasePath.lineTo(x, y);
        }

        p.drawPath(releasePath);

        if (!stillVisible)
            n163SmoothValid[c] = false;

        return true;
    }

    n163SmoothValid[c] = false;
    return false;   // roi xuong duong ve chung trong paintGL
}

// =====================================================================================
// Muc zoom (so sample hien thi) cua tung kenh. Dieu chinh zoom cua mot loai kenh o day.
// =====================================================================================
int AudioWaveWindow::visibleSamplesFor(int c, int n, const QVector<float>& raw) const
{
    int visibleSamples = DISPLAY_SAMPLES;

    if (mode == WaveMode::VRC7)
    {
        // VRC7 mode chi co 6 kenh VRC7, cho moi song to va de nhin hon
        visibleSamples = DISPLAY_SAMPLES / 2;
    }
    else if (mode == WaveMode::N163)
    {
        visibleSamples = DISPLAY_SAMPLES / 2;
    }
    else
    {
        // NES Pulse: trigger va zoom vua phai
        if (c == 0 || c == 1)
        {
            visibleSamples = DISPLAY_SAMPLES / 3;
        }

        // Noise/DMC: KHONG trigger (Noise), cho thua bot de do roi
        if (c == 3)
        {
            visibleSamples = DISPLAY_SAMPLES * 1.1;
        }
        if (c == 4)
        {
            visibleSamples = DISPLAY_SAMPLES / 1.4;
        }
    }

    // Expansion channels cua VRC6/S5B/MMC5/FDS
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
                // VRC6 Saw: zoom theo so "rang cua" tren khung, uoc luong tu chu ky reset gan nhat.
                visibleSamples = DISPLAY_SAMPLES / 1.4;

                const auto& buf = raw;
                int resetA = -1;
                int resetB = -1;

                // Tim 2 canh roi/reset gan nhat de uoc luong chu ky hien tai.
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
                    period = std::clamp(period, 8, DISPLAY_SAMPLES * 1);

                    const float minTeeth = 1.0f;
                    const float maxTeeth = 9.0f;

                    float rawTeeth = float(visibleSamples) / float(period);
                    float teeth = std::clamp(rawTeeth, minTeeth, maxTeeth);

                    visibleSamples = int(std::round(float(period) * teeth));
                    visibleSamples = std::clamp(visibleSamples, 32, DISPLAY_SAMPLES * 3);

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
        else if (mode == WaveMode::FDS)
        {
            // FDS wave la wavetable 64-step, chu ky ngan -> zoom vua phai
            // giong VRC6 Pulse/S5B/MMC5 de nhin ro hinh dang song.
            visibleSamples = DISPLAY_SAMPLES / 3;
        }
    }

    return visibleSamples;
}

// =====================================================================================
// Bien do ve (pixel) cua tung kenh: he so nhan voi chieu cao o. Chinh do cao song o day.
// =====================================================================================
float AudioWaveWindow::ampFor(int c, int panelH) const
{
    float amp = panelH * 0.45f;

    if (mode == WaveMode::VRC7)
    {
        amp = panelH * 0.20f;
    }
    else if (mode == WaveMode::N163)
    {
        amp = panelH * 0.35f;
    }
    else
    {
        if (c == 0 || c == 1)   amp = panelH * 0.30f;
        if (c == 2)             amp = panelH * 0.30f;
        if (c == 3)             amp = panelH * 0.15f;
        if (c == 4)             amp = panelH * 0.30f;
        if (c >= 5)
        {
            if (mode == WaveMode::VRC6)
                amp = (c == 7) ? panelH * 0.40f : panelH * 0.48f;
            else if (mode == WaveMode::S5B)
                amp = panelH * 0.30f;
            else if (mode == WaveMode::MMC5)
                amp = (c == 7) ? panelH * 0.30f : panelH * 0.44f;
            else if (mode == WaveMode::FDS)
                amp = panelH * 0.42f;
        }
    }

    return amp;
}

bool AudioWaveWindow::isChannelShown(int c) const
{
    if (c < rangeFirst || c > rangeLast)
        return false;
    // VRC7 / N163 chi co kenh cua chinh chip. Cac mode con lai: kenh 0-4 la NES goc.
    if (mode == WaveMode::NES || mode == WaveMode::VRC7 || mode == WaveMode::N163)
        return true;
    return showNesChannels || c >= 5;
}

void AudioWaveWindow::setTileLayout(int startCol, bool twoCols, bool fillTrailingCell)
{
    tileMode = true;
    layoutOffset = (startCol > 0) ? 1 : 0;
    tileTwoCols = twoCols;
    fillTrailing = fillTrailingCell;
}

int AudioWaveWindow::totalChannelCount() const
{
    int count = 0;
    for (int c = 0; c < activeChannelCount; c++)
        if (mode == WaveMode::NES || mode == WaveMode::VRC7 || mode == WaveMode::N163 || showNesChannels || c >= 5)
            ++count;
    return count;
}

int AudioWaveWindow::firstShownChannel() const
{
    for (int c = 0; c < activeChannelCount; c++)
        if (mode == WaveMode::NES || mode == WaveMode::VRC7 || mode == WaveMode::N163 || showNesChannels || c >= 5)
            return c;
    return 0;
}

int AudioWaveWindow::visibleChannelCount() const
{
    int count = 0;
    for (int c = 0; c < activeChannelCount; c++)
        if (isChannelShown(c))
            ++count;
    return count;
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