#include "AudioWaveWindow.h"
// OLD_FEEL_EXCEPT_N163 + VRC7 CorrScope lock + fixed X anchor: chỉ sửa VRC7 trôi ngang thành co/dãn ngang.
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
    float t = fidx - i1;

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
    float bestIdx = float(searchStart);
    float bestSlope = -1.0f;
    bool  found = false;

    for (int i = searchEnd; i >= searchStart + 1; i--)
    {
        float prev = buf[i - 1];
        float cur = buf[i];

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

static bool validPhase01(float p)
{
    return std::isfinite(p) && p >= 0.0f && p <= 1.0f;
}

// VRC7 PHASE-BUFFER TRIGGER:
// Mỗi sample waveform có một phase carrier đi kèm. Ta tìm điểm phase wrap (1 -> 0)
// gần mép trái vùng hiển thị để làm fStart. Như vậy VRC7 đứng yên theo phase thật của
// emu2413, không còn phụ thuộc zero-crossing của dạng sóng FM nhiều cạnh phụ.
static float findVrc7PhaseStart(
    const QVector<float>& phase,
    int n,
    int visibleSamples,
    float periodHint,
    bool& ok
)
{
    ok = false;

    if (phase.size() != n || n < visibleSamples + 4)
        return float(n - visibleSamples);

    if (!(periodHint >= 4.0f && periodHint <= 8192.0f))
        return float(n - visibleSamples);

    const float desired = float(n - visibleSamples);
    const int center = std::clamp(int(std::round(desired)), 1, n - 2);
    const int radius = std::clamp(int(periodHint * 2.5f), 24, std::max(32, visibleSamples * 3));

    const int scanStart = std::max(1, center - radius);
    const int scanEnd = std::min(n - 2, center + radius);

    float bestIdx = -1.0f;
    float bestDist = 1e30f;

    for (int i = scanStart + 1; i <= scanEnd; ++i)
    {
        float prev = phase[i - 1];
        float cur = phase[i];

        if (!validPhase01(prev) || !validPhase01(cur))
            continue;

        // Wrap thật của carrier phase. Dùng ngưỡng rộng để tránh nhầm vibrato nhỏ thành wrap.
        if (prev > 0.55f && cur < 0.45f && (prev - cur) > 0.25f)
        {
            float denom = (1.0f - prev) + cur;
            float frac = (denom > 0.000001f) ? (1.0f - prev) / denom : 0.0f;
            frac = std::clamp(frac, 0.0f, 1.0f);

            float idx = float(i - 1) + frac;
            float dist = std::abs(idx - desired);

            if (dist < bestDist)
            {
                bestDist = dist;
                bestIdx = idx;
            }
        }
    }

    if (bestIdx >= 0.0f)
    {
        ok = true;
        return bestIdx;
    }

    // Fallback cho nốt quá trầm / chưa có wrap trong cửa sổ: dùng phase tại vị trí desired
    // để suy ra phase-0 gần đó. Fallback này vẫn dùng phase buffer theo sample, không dùng
    // zero-crossing waveform.
    int di = std::clamp(int(desired), 0, n - 1);
    float ph = phase[di];
    if (validPhase01(ph))
    {
        float start = desired - ph * periodHint;

        while (start < desired - periodHint * 0.5f)
            start += periodHint;
        while (start > desired + periodHint * 0.5f)
            start -= periodHint;

        ok = true;
        return start;
    }

    return float(n - visibleSamples);
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

    float err = 0.0f;
    for (int i = 0; i < a.size(); ++i)
    {
        float d = a[i] - b[i];
        err += d * d;
    }
    return err / float(a.size());
}

// CorrScope-style VRC7 start picker + FIXED X ANCHOR:
// Bản gốc đã đứng yên tốt, nhưng khi VRC7 vibrato/pitch uốn cao-thấp-cao-thấp,
// nếu lấy phase-wrap làm mép trái hiển thị thì cả hàng waveform sẽ bị kéo trôi ngang.
// Cách này vẫn giữ logic CorrScope correlation của bản ổn, nhưng đổi ý nghĩa candidate:
// candidate là phase-wrap thật được NEO ở một vị trí X cố định trong màn hình.
// Khi period thay đổi, waveform sẽ co/dãn ngang quanh điểm neo đó, không trượt cả hàng.
static float findVrc7CorrscopeStart(
    const QVector<float>& wave,
    const QVector<float>& phase,
    int n,
    int visibleSamples,
    float periodHint,
    const QVector<float>& prevRef,
    bool hasPrevRef,
    QVector<float>& newRef,
    bool& ok
)
{
    ok = false;
    newRef.clear();

    if (phase.size() != n || wave.size() != n || n < visibleSamples + 8)
        return float(n - visibleSamples);

    const float defaultStart = float(n - visibleSamples);
    const bool hasHint = (periodHint >= 4.0f && periodHint <= 8192.0f);

    // Neo không đặt ở mép trái. Đặt khoảng 38% chiều ngang để khi âm rung cao/thấp,
    // phần bên trái và bên phải cùng co/dãn quanh anchor, cảm giác giống CorrScope hơn.
    const float anchorOffset = float(visibleSamples) * 0.38f;
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
            float anchor = desiredAnchor - ph * periodHint;
            while (anchor < desiredAnchor - periodHint * 0.5f) anchor += periodHint;
            while (anchor > desiredAnchor + periodHint * 0.5f) anchor -= periodHint;

            float candidateStart = anchor - anchorOffset;
            candidateStart = std::clamp(candidateStart, 0.0f, float(n - visibleSamples - 2));
            anchor = candidateStart + anchorOffset;
            anchorCandidates.push_back(anchor);
        }
    }

    if (anchorCandidates.isEmpty())
        return defaultStart;

    float bestStart = defaultStart;
    float bestScore = 1e30f;
    QVector<float> bestRef;

    for (float anchor : anchorCandidates)
    {
        float candStart = anchor - anchorOffset;
        candStart = std::clamp(candStart, 0.0f, float(n - visibleSamples - 2));

        QVector<float> ref;
        buildVrc7CorrRef(wave, candStart, visibleSamples, ref);

        float score = 0.0f;
        if (hasPrevRef && prevRef.size() == ref.size())
        {
            score = vrc7RefError(ref, prevRef);

            // Phạt rất nhẹ nếu anchor rời xa X cố định. Correlation vẫn là chính,
            // penalty chỉ ngăn nó nhảy sang wrap quá xa gây cảm giác drift.
            score += std::abs(anchor - desiredAnchor) / std::max(1.0f, float(visibleSamples)) * 0.035f;
        }
        else
        {
            // Frame đầu: chọn wrap gần X anchor nhất.
            score = std::abs(anchor - desiredAnchor);
        }

        if (score < bestScore)
        {
            bestScore = score;
            bestStart = candStart;
            bestRef = ref;
        }
    }

    if (!bestRef.isEmpty())
    {
        newRef = bestRef;
        ok = true;
        return bestStart;
    }

    return defaultStart;
}


// Tìm đoạn VRC7 mới nhất bằng phase-wrap thật trong phase buffer.
// VRC7/FM không phải lúc nào cũng khớp đẹp sau đúng 1 carrier cycle vì có modulator,
// feedback/envelope. Nếu lặp 1 chu kỳ quá ngắn sẽ dễ tạo seam/gap, đặc biệt ở CH6.
// Vì vậy ta ưu tiên lấy một snapshot gồm vài chu kỳ carrier mới nhất, rồi lặp cả đoạn đó.
static bool findVrc7LatestCycle(
    const QVector<float>& phase,
    int n,
    int visibleSamples,
    float periodHint,
    float& cycleStart,
    float& cycleEnd
)
{
    cycleStart = 0.0f;
    cycleEnd = 0.0f;

    if (phase.size() != n || n < 64)
        return false;

    const int scanBack = std::clamp(visibleSamples * 8, 512, std::max(512, n - 2));
    const int scanStart = std::max(1, n - scanBack);
    const int scanEnd = n - 2;

    QVector<float> wraps;
    wraps.reserve(512);

    for (int i = scanStart + 1; i <= scanEnd; ++i)
    {
        float prev = phase[i - 1];
        float cur = phase[i];

        if (!validPhase01(prev) || !validPhase01(cur))
            continue;

        if (prev > 0.55f && cur < 0.45f && (prev - cur) > 0.25f)
        {
            float denom = (1.0f - prev) + cur;
            float frac = (denom > 0.000001f) ? (1.0f - prev) / denom : 0.0f;
            frac = std::clamp(frac, 0.0f, 1.0f);
            wraps.push_back(float(i - 1) + frac);
        }
    }

    if (wraps.size() < 2)
        return false;

    const bool hasHint = (periodHint >= 4.0f && periodHint <= 8192.0f);
    const int last = wraps.size() - 1;

    // Thử đoạn dài trước để CH6/sóng mềm không bị đứt tại seam.
    // Nếu không đủ wrap thì tự lùi xuống 3/2/1 chu kỳ.
    const int preferredCycles[] = { 6, 4, 3, 2, 1 };

    for (int wantCycles : preferredCycles)
    {
        if (last - wantCycles < 0)
            continue;

        float a = wraps[last - wantCycles];
        float b = wraps[last];
        float segment = b - a;
        float avgPeriod = segment / float(wantCycles);

        if (segment < 4.0f || segment > float(visibleSamples) * 3.5f)
            continue;

        if (hasHint)
        {
            if (avgPeriod < periodHint * 0.35f || avgPeriod > periodHint * 2.80f)
                continue;
        }

        // Kiểm tra các chu kỳ con không lệch quá vô lý, tránh ăn nhầm wrap lỗi.
        bool stable = true;
        for (int k = last - wantCycles + 1; k <= last; ++k)
        {
            float p = wraps[k] - wraps[k - 1];
            if (p < 4.0f || p > float(visibleSamples) * 2.5f)
            {
                stable = false;
                break;
            }

            if (hasHint && (p < periodHint * 0.25f || p > periodHint * 3.2f))
            {
                stable = false;
                break;
            }
        }

        if (!stable)
            continue;

        cycleStart = a;
        cycleEnd = b;
        return true;
    }

    return false;
}

// =====================================================================================
// PHASE-LOCK TRIGGER (PLL)
// -------------------------------------------------------------------------------------
// Vấn đề của trigger kiểu "lấy cạnh mới nhất / mạnh nhất" mỗi frame: khi sóng có nhiều
// cạnh ứng viên gần nhau quanh trigger level trong cùng 1 chu kỳ (VRC7 multi-operator FM,
// N163 wavetable nhiều bậc), mỗi frame có thể chọn 1 cạnh khác nhau trong đám đó
// => hình rung ngang liên tục, dù sóng thực tế không hề thay đổi nhiều.
//
// Cách sửa: giữ 1 vị trí "đã lock" xuyên qua nhiều frame. Mỗi frame, dự đoán cạnh kế tiếp
// sẽ rơi vào đâu dựa trên period đã lock (từ các frame trước), rồi chỉ chọn ứng viên
// GẦN dự đoán đó nhất — không phải ứng viên mới nhất hay mạnh nhất. Period được lọc thấp
// (low-pass), phase chỉ được sửa một phần sai số mỗi frame (đúng tinh thần PLL), không
// snap thẳng vào vị trí đo được.
//
// Lưu ý quan trọng: buffers[] bị cắt bớt phần đầu (remove) khi vượt MAX_SAMPLES + 512,
// nên KHÔNG thể lưu thẳng index tuyệt đối vào buffer qua các frame (sẽ lệch đúng bằng
// lượng bị cắt mỗi lần trim). Vì vậy lock được lưu theo "absolute sample index" quy đổi
// qua bộ đếm totalPushed[] không bao giờ reset, an toàn dù buffer có bị trim bao nhiêu lần.
// =====================================================================================
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
    float changeRatio)
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

    // Cập nhật period thật, KHÔNG sửa lockAbsPos.
    // Đây là điểm quan trọng để hết drift: phase anchor không bị crossing kéo đi nữa.
    lock.period = periodHint;
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
        // NES / VRC6 / S5B đều có 5 kênh NES ở dòng 0..4
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
    }

    for (int i = 0; i < activeChannelCount; i++)
    {
        periodHint[i] = std::clamp(periods[i], 0.0f, 8192.0f);

        // VRC7 không clamp sớm ở đây. Nếu clamp trước khi vẽ, đỉnh ch_out của emu2413
        // sẽ bị cắt thành mặt phẳng ngang và paintGL không thể phục hồi lại được.
        float v = values[i];
        if (mode != WaveMode::VRC7)
            v = std::clamp(v, -1.0f, 1.0f);

        if (mode != WaveMode::VRC7 && (i == 3 || i == 4))
        {
            v *= 1.6f;
            v = std::clamp(v, -1.0f, 1.0f);
        }

        // VRC6: dòng 5/6 là pulse, dòng 7 là saw
        if (mode == WaveMode::VRC6)
        {
            if (i == 5 || i == 6) { v *= 1.4f; v = std::clamp(v, -1.0f, 1.0f); }
            if (i == 7) { v *= 1.65f; v = std::clamp(v, -1.0f, 1.0f); }
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

        if (mode == WaveMode::VRC7)
        {
            float ph = phases[i];
            if (!std::isfinite(ph))
                ph = 0.0f;
            ph = ph - std::floor(ph);
            phaseBuffers[i].push_back(ph);
        }

        totalPushed[i]++;   // bộ đếm tuyệt đối, KHÔNG reset khi buffer bị trim phía dưới

        if (buffers[i].size() > MAX_SAMPLES + 512)
        {
            int removeCount = buffers[i].size() - MAX_SAMPLES;
            buffers[i].remove(0, removeCount);
            if (mode == WaveMode::VRC7 && phaseBuffers[i].size() >= removeCount)
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
        lastVisual[i] = 0.0f;
        periodHint[i] = 0.0f;
        vrc7LastPhaseStart[i] = 0.0f;
        vrc7HasPhaseStart[i] = false;
        vrc7CorrRef[i].clear();
        vrc7CorrValid[i] = false;

        n163LastTrigger[i] = 0.0f;
        n163Period[i] = 0.0f;
        n163LastBufferSize[i] = 0;
        n163HasTrigger[i] = false;
        n163SmoothY[i].clear();
        n163SmoothValid[i] = false;

        totalPushed[i] = 0;
        genericTrigLock[i] = TrigLockState{};
        n163TrigLock[i] = TrigLockState{};
    }
}

void AudioWaveWindow::paintGL()
{
    QVector<float> copy[CHANNEL_COUNT];
    QVector<float> phaseCopy[CHANNEL_COUNT];
    float hintCopy[CHANNEL_COUNT]{};
    quint64 pushedCopy[CHANNEL_COUNT]{};
    {
        QMutexLocker lock(&mutex);
        for (int i = 0; i < CHANNEL_COUNT; i++)
        {
            copy[i] = buffers[i];
            phaseCopy[i] = phaseBuffers[i];
            hintCopy[i] = periodHint[i];
            pushedCopy[i] = totalPushed[i];
        }
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(0, 0, 0));

    int w = width();
    int h = height();
    int topMargin = 8;
    int bottomMargin = 8;
    int drawH = h - topMargin - bottomMargin;

    if (w <= 0 || h <= 0) return;

    int panelH = drawH / activeChannelCount;

    for (int c = 0; c < activeChannelCount; c++)
    {
        int top = topMargin + c * panelH;
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

                    visibleSamples = DISPLAY_SAMPLES * 2 / 3;

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
                visibleSamples = DISPLAY_SAMPLES / 3;
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
        bool isTriangle = (mode != WaveMode::VRC7 && mode != WaveMode::N163 && c == 2);

        if (range > 0.02f && !isNoiseDMC)
        {
            if (mode == WaveMode::N163)
            {
                // Việc chọn fStart cho N163 không còn cần thiết ở đây: N163 vẽ bằng block
                // "row-sync" riêng phía dưới (dùng n163TrigLock + PLL), block đó tự tính
                // cycleStart/period và sẽ "continue" trước khi chạm tới đoạn vẽ chung dùng
                // fStart. Giữ fStart mặc định, không cần xử lý gì thêm ở đây.
            }
            else if (mode == WaveMode::VRC7)
            {
                // VRC7 CorrScope-style lock:
                // Không dùng zero-crossing và cũng không lặp snapshot. Mỗi frame chọn phase-wrap
                // có hình sóng giống frame trước nhất. Đây là thứ làm VRC7 đứng yên kiểu
                // oscilloscope/corrscope nhưng vẫn giữ live-buffer "old feel".
                bool corrOk = false;
                QVector<float> newRef;
                float corrStart = findVrc7CorrscopeStart(
                    copy[c], phaseCopy[c], n, visibleSamples, hintCopy[c],
                    vrc7CorrRef[c], vrc7CorrValid[c], newRef, corrOk
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
                // GIỮ OLD FEEL cho tất cả kênh không phải N163:
                // dùng trigger live-buffer đơn giản như bản anh gửi, không dùng hard-lock
                // timer/duty ở đây nữa. Như vậy Pulse/VRC6/S5B/MMC5/Triangle vẫn chuyển động
                // tự nhiên, không bị khô/cứng như CHIP_LOCK.
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
        else if (isTriangle)
        {
            QVector<float> idxs;
            QVector<float> slopes;
            idxs.reserve(128);
            slopes.reserve(128);

            int scanStart = std::max(1, n - visibleSamples * 4);
            int scanEnd = n - 2;

            float minSlope = std::max(0.0001f, range * 0.05f);

            for (int si = scanStart + 1; si <= scanEnd; si++)
            {
                float prev = copy[c][si - 1];
                float cur = copy[c][si];

                // Triangle: rising crossing qua level giữa
                if (prev < triggerLevel && cur >= triggerLevel)
                {
                    float slope = cur - prev;

                    if (slope >= minSlope)
                    {
                        float frac = (triggerLevel - prev) / slope;
                        frac = std::clamp(frac, 0.0f, 1.0f);

                        idxs.push_back(float(si - 1) + frac);
                        slopes.push_back(slope);
                    }
                }
            }

            qint64 absBase = (qint64)pushedCopy[c] - (qint64)n;
            float hint = hintCopy[c];

            bool locked = updateHardPeriodAnchor(
                genericTrigLock[c],
                absBase,
                idxs,
                slopes,
                hint,
                8.0f,
                8192.0f,
                0.06f
            );

            if (locked && genericTrigLock[c].period >= 8.0f)
            {
                float desiredStart = float(n - visibleSamples);
                float edge = predictEdgeNearStart(
                    genericTrigLock[c],
                    absBase,
                    desiredStart,
                    1.0f,
                    float(n - visibleSamples - 2)
                );

                if (edge >= 0.0f)
                    fStart = edge;
            }
            else if (!idxs.isEmpty())
            {
                // Fallback nếu chưa có period hint
                fStart = idxs.back();
            }
        }
        fStart = std::clamp(fStart, 0.0f, float(n - visibleSamples - 2));

        float amp = panelH * 0.45f;

        if (mode == WaveMode::VRC7)
        {
            // VRC7 sẽ được normalize theo range từng kênh ở lúc vẽ.
            // amp này là độ cao thực tế trên panel; tăng ở đây để sóng to hơn mà không clip.
            amp = panelH * 0.42f;
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
        //
        // Trigger/period của chu kỳ này được chọn bằng PLL phase-lock (n163TrigLock),
        // KHÔNG còn tính anchor/prevEdge từ đầu mỗi frame như trước -> hết rung ngang.
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
                    }
                }
            }

            bool drewN163 = false;

            qint64 absBase = (qint64)pushedCopy[c] - (qint64)n;
            float hint = hintCopy[c];

            // N163 dùng hard anchor thật sự: sau khi acquire cạnh đầu tiên, phase không còn
            // bị crossing kéo từng frame nữa. Period thật quyết định vị trí chu kỳ.
            bool hasHardAnchor = updateHardPeriodAnchor(
                n163TrigLock[c], absBase, idxs, slopes,
                hint, 8.0f, 8192.0f, 0.055f
            );

            if (hasHardAnchor && n163TrigLock[c].period >= 8.0f)
            {
                float period = n163TrigLock[c].period;
                float cycleEnd = predictEdgeNearEnd(n163TrigLock[c], absBase, float(n));
                float cycleStart = cycleEnd - period;

                if (cycleEnd >= 0.0f && cycleStart >= 1.0f && cycleEnd < float(n - 2))
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
                            y = n163SmoothY[c][pi] * 0.08f + yTarget * 0.92f;
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

            // Fallback cho trường hợp chưa có period hint hợp lệ.
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
                            float phase = std::fmod(t * cyclesAcross, 1.0f);
                            float fIdx = std::clamp(cycleStart + phase * period, 0.0f, float(n - 2));
                            float yTarget = midY - sampleInterp(copy[c], fIdx) * amp;
                            float y = n163SmoothValid[c] ? (n163SmoothY[c][pi] * 0.08f + yTarget * 0.92f) : yTarget;
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


        // VRC7 STROBE-CYCLE LOCK:
        if (false && mode == WaveMode::VRC7)
        {
            float cycleStart = 0.0f;
            float cycleEnd = 0.0f;
            bool gotCycle = findVrc7LatestCycle(phaseCopy[c], n, visibleSamples, hintCopy[c], cycleStart, cycleEnd);

            if (gotCycle)
            {
                float period = cycleEnd - cycleStart;
                int drawPoints = std::max(2, w * 2);

                int r0 = std::clamp(int(std::floor(cycleStart)) - 2, 0, n - 1);
                int r1 = std::clamp(int(std::ceil(cycleEnd)) + 2, 0, n - 1);

                float localMin = copy[c][r0];
                float localMax = copy[c][r0];
                for (int ri = r0 + 1; ri <= r1; ++ri)
                {
                    localMin = std::min(localMin, copy[c][ri]);
                    localMax = std::max(localMax, copy[c][ri]);
                }

                float localRange = localMax - localMin;
                float localCenter = (localMin + localMax) * 0.5f;
                float localHalf = std::max(localRange * 0.5f, 0.0015f);

                float cyclesAcross = float(visibleSamples) / std::max(period, 1.0f);
                cyclesAcross = std::clamp(cyclesAcross, 1.0f, 22.0f);

                QPen vrc7Pen(CHANNEL_COLORS[c]);
                vrc7Pen.setWidthF(2.0f);
                vrc7Pen.setCapStyle(Qt::RoundCap);
                vrc7Pen.setJoinStyle(Qt::RoundJoin);
                p.setPen(vrc7Pen);

                QPainterPath vrc7Path;
                bool pathStarted = false;

                auto normVrc7 = [&](float s) -> float
                {
                    float v = (s - localCenter) / localHalf;
                    return std::clamp(v, -1.12f, 1.12f);
                };

                // Seam guard: không cắt path nữa, vì cắt sẽ tạo cảm giác "đứt" ở CH6.
                // Thay vào đó, khi snapshot lặp lại, vài % đầu/cuối đoạn được kéo nhẹ về
                // cùng một seamValue để nối mượt mà không sinh vạch dọc.
                float startSample = sampleInterp(copy[c], std::clamp(cycleStart + 1.0f, 0.0f, float(n - 2)));
                float endSample = sampleInterp(copy[c], std::clamp(cycleEnd - 1.0f, 0.0f, float(n - 2)));
                float seamValue = (startSample + endSample) * 0.5f;
                float seamWidth = std::clamp(6.0f / std::max(period, 1.0f), 0.012f, 0.055f);

                auto smooth01 = [](float x) -> float
                {
                    x = std::clamp(x, 0.0f, 1.0f);
                    return x * x * (3.0f - 2.0f * x);
                };

                for (int pi = 0; pi < drawPoints; ++pi)
                {
                    float t = float(pi) / float(drawPoints - 1);
                    float phase = std::fmod(t * cyclesAcross, 1.0f);
                    if (phase < 0.0f)
                        phase += 1.0f;

                    float fIdx = cycleStart + phase * period;
                    fIdx = std::clamp(fIdx, 0.0f, float(n - 2));

                    float sample = sampleInterp(copy[c], fIdx);

                    if (phase < seamWidth)
                    {
                        float a = smooth01(phase / seamWidth);
                        sample = seamValue * (1.0f - a) + sample * a;
                    }
                    else if (phase > 1.0f - seamWidth)
                    {
                        float a = smooth01((phase - (1.0f - seamWidth)) / seamWidth);
                        sample = sample * (1.0f - a) + seamValue * a;
                    }

                    float x = t * float(w);
                    float y = midY - normVrc7(sample) * amp;

                    if (!pathStarted)
                    {
                        vrc7Path.moveTo(x, y);
                        pathStarted = true;
                    }
                    else
                    {
                        vrc7Path.lineTo(x, y);
                    }
                }

                if (pathStarted)
                    p.drawPath(vrc7Path);

                continue;
            }
            // Nếu chưa đủ 1 chu kỳ phase thì rơi xuống renderer cũ để vẫn thấy tín hiệu trong frame đầu.
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

            auto drawSample = [&](float s) -> float
            {
                if (mode == WaveMode::VRC7)
                {
                    // Auto-gain theo từng kênh VRC7: dùng range thật của buffer hiện tại,
                    // không hard-clamp ở pushChannels nên đỉnh không còn bị bẹt giả.
                    const float halfRange = std::max(range * 0.5f, 0.0015f);
                    float vrc7 = (s - triggerLevel) / halfRange;
                    return std::clamp(vrc7, -1.18f, 1.18f);
                }
                return s;
            };

            if (stepWave)
            {
                // Step wave: dùng nearest sample (không interpolate)
                int idx = int(fIdx);
                idx = std::clamp(idx, 0, n - 1);
                y = midY - drawSample(copy[c][idx]) * amp;

                if (i == 0) { path.moveTo(x, y); }
                else
                {
                    float prevFIdx = fStart + (float(i - 1) / float(drawPoints - 1)) * float(visibleSamples);
                    int prevIdx = std::clamp(int(prevFIdx), 0, n - 1);
                    float prevY = midY - drawSample(copy[c][prevIdx]) * amp;
                    path.lineTo(x, prevY);
                    path.lineTo(x, y);
                }
            }
            else
            {
                // Smooth wave: Catmull-Rom interpolation
                y = midY - drawSample(sampleInterp(copy[c], fIdx)) * amp;
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