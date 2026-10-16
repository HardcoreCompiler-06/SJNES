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
#include <array>
#include <cstdint>
#include <algorithm>
#include <cmath>

// Namco 163 (mapper 19) expansion audio.
//
// Mô hình theo phần cứng thật:
//  - 128 byte RAM nội dùng chung cho wave data ($00-$3F) và thanh ghi kênh ($40-$7F).
//  - Kênh phần cứng 0..7 có thanh ghi tại $40 + ch*8. Kênh 7 ($78-$7F) luôn chạy trước;
//    $7F bit 4-6 = (số kênh - 1) và bit 0-3 = volume của kênh 7.
//  - Chỉ có MỘT DAC, ghép theo thời gian: mỗi 15 chu kỳ CPU chip cập nhật đúng 1 kênh
//    (7, 6, 5, ... xuống kênh thấp nhất đang bật, rồi quay lại 7). Giá trị DAC được giữ
//    cho tới lần cập nhật kế tiếp. Vì thế mỗi kênh được cập nhật sau 15 * N chu kỳ CPU.
//  - Cập nhật 1 kênh: phase(24-bit) += freq(18-bit), wrap theo (waveLength << 16),
//    ghi phase ngược lại vào RAM, rồi đọc mẫu 4-bit và nhân volume.
//  - Âm thanh thật là trung bình theo thời gian của DAC, nên khi nhiều kênh bật,
//    mỗi kênh đóng góp 1/N. Chế độ approx và chế độ accurate cho cùng thang âm lượng.
class Namco163Audio
{
public:
    // Reset kiểu bật nguồn: xóa RAM và trạng thái chip.
    // KHÔNG đụng tới cài đặt của UI (volume, chế độ ghép kênh, mute, gain)
    // để người dùng chỉnh xong rồi reset game không bị mất.
    void Reset()
    {
        ram.fill(0);
        address = 0;
        autoIncrement = false;

        // Thanh ghi bank $E000 lúc bật nguồn = 0 nên bit 6 = 0 (sound bật).
        // Game nào cũng ghi $E000 khi khởi tạo, nhưng mặc định bật thì an toàn hơn.
        soundDisabled = false;

        cpuCycleCounter = 0;
        currentChannel = 7;
        heldOutput = 0.0f;
        lpf1 = 0.0;
        lpf2 = 0.0;

        accSum = 0.0;
        accCount = 0;
        lastAvg = 0.0f;
    }

    // ---- Debug / oscilloscope ----------------------------------------------

    // out[0..7] = các kênh đang bật, dồn từ kênh thấp nhất về dòng 0
    // (view index 0 = kênh phần cứng "lowest", view index count-1 = kênh 7).
    void GetDebugChannels(float out[8]) const
    {
        for (int i = 0; i < 8; i++)
            out[i] = 0.0f;

        if (soundDisabled)
            return;

        const int lowest = GetLowestEnabledChannel();

        for (int ch = lowest; ch < 8; ch++)
        {
            const int viewIndex = ch - lowest;
            out[viewIndex] = NormalizeOutput(ReadChannelOutput(ch)) * volume;
        }
    }

    // Chu kỳ sóng của từng kênh, tính bằng "mẫu xuất ra" ở sample rate thật.
    void GetDebugPeriods(float out[8]) const
    {
        for (int i = 0; i < 8; i++)
            out[i] = 0.0f;

        if (soundDisabled)
            return;

        const float sampleRate = float(debugSampleRate);

        const int count = GetActiveChannelCount();
        const int lowest = GetLowestEnabledChannel();

        for (int ch = lowest; ch < 8; ch++)
        {
            const int viewIndex = ch - lowest;
            if (muteChannel[viewIndex])
                continue;

            const uint32_t freq = ReadFrequency(ch);
            if (freq == 0)
                continue;

            const uint32_t waveLength = ReadWaveLength(ch);

            // Một vòng sóng cần (waveLength << 16) / freq lần cập nhật,
            // mỗi lần cách nhau 15 * count chu kỳ CPU.
            const float updatesPerWave = (float(waveLength) * 65536.0f) / float(freq);
            const float cpuCyclesPerWave = updatesPerWave * (15.0f * float(count));
            const float periodSamples = cpuCyclesPerWave * sampleRate / CPU_RATE;

            out[viewIndex] = std::clamp(periodSamples, 2.0f, 8192.0f);
        }
    }

    // Phase THẬT của từng kênh trong vòng wavetable, chuẩn hóa 0..1 (= phase24 / (waveLength << 16)).
    // Cùng view index và cùng thời điểm lấy mẫu với GetDebugChannels(), nên mẫu nào có phase nào
    // là chính xác; phase wrap 1 -> 0 = đầu wavetable. Oscilloscope dùng làm mốc khóa pha
    // (giống Restart() của DMC / phase-wrap của VRC7). Kênh không bật trả 0.
    void GetDebugPhases(float out[8]) const
    {
        for (int i = 0; i < 8; i++)
            out[i] = 0.0f;

        if (soundDisabled)
            return;

        const int lowest = GetLowestEnabledChannel();

        for (int ch = lowest; ch < 8; ch++)
        {
            const uint32_t modulo = ReadWaveLength(ch) << 16;
            if (modulo == 0)
                continue;

            const uint32_t phase = ReadPhase(ch) % modulo;
            out[ch - lowest] = float(double(phase) / double(modulo));
        }
    }

    // ---- Cài đặt ----------------------------------------------------------

    void SetVolume(float v)
    {
        volume = v;
    }

    // Sample rate xuất ra hiện tại (Hz), dùng để quy đổi chu kỳ debug ra "sample".
    void SetSampleRate(double hz)
    {
        if (hz >= 8000.0)
            debugSampleRate = hz;
    }

    void SetAccurateMultiplex(bool enabled)
    {
        accurateMultiplex = enabled;

        // Đổi chế độ -> xóa bộ tích lũy trung bình
        accSum = 0.0;
        accCount = 0;
        lastAvg = 0.0f;
        cpuCycleCounter = 0;
        lpf1 = 0.0;
        lpf2 = 0.0;
    }

    // Tần số cắt (-3 dB) của mạch lọc thông thấp analog mô phỏng ở chế độ 1 DAC.
    // Nesdev không nêu cutoff nào: qua ngõ A/V không có lọc cao tần (tiếng rít do
    // chuyển kênh còn nguyên), qua RF thì mạch giải điều chế mới lọc bớt.
    // Mặc định 20 kHz = chỉ để chống alias khi hạ xuống 44.1/48 kHz, giữ nguyên
    // tiếng rít 14.9 kHz (8 kênh), 17 kHz (7 kênh), 19.9 kHz (6 kênh) như ngõ A/V.
    // Hạ xuống ~10-14 kHz nếu muốn nghe giống đường RF (êm hơn).
    // Lọc chạy ở tốc độ CPU (1.79 MHz) TRƯỚC khi lấy trung bình ra sample rate.
    void SetLowpassCutoff(double hz)
    {
        hz = std::clamp(hz, 1000.0, 20000.0);
        // 2 tầng RC một cực nối tiếp: -3 dB tổng = 0.6436 * cutoff mỗi tầng.
        const double stageHz = hz / 0.6436;
        lpfAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * stageHz / double(CPU_RATE));
    }

    bool IsSoundEnabled() const
    {
        return !soundDisabled;
    }

    // ---- Thanh ghi CPU ------------------------------------------------------

    // $E000-$E7FF: bit 6 = 1 thì tắt sound.
    void WriteSoundEnable(uint8_t data)
    {
        soundDisabled = (data & 0x40) != 0;
    }

    // $F800-$FFFF: bit 7 = auto increment, bit 0-6 = địa chỉ RAM trong chip.
    void WriteAddressPort(uint8_t data)
    {
        address = data & 0x7F;
        autoIncrement = (data & 0x80) != 0;
    }

    // $4800-$4FFF write
    void WriteDataPort(uint8_t data)
    {
        ram[address] = data;

        // Theo nesdev: auto-increment KHÔNG quấn vòng, dừng lại ở $7F.
        if (autoIncrement && address < 0x7F)
            address++;
    }

    // $4800-$4FFF read
    uint8_t ReadDataPort()
    {
        const uint8_t value = ram[address];

        // Theo nesdev: auto-increment KHÔNG quấn vòng, dừng lại ở $7F.
        if (autoIncrement && address < 0x7F)
            address++;

        return value;
    }

    // Gọi theo CPU cycle. Ví dụ CPU chạy xong opcode mất 4 cycle thì gọi ClockCpu(4).
    void ClockCpu(int cpuCycles)
    {
        if (soundDisabled)
            return;

        if (!accurateMultiplex)
        {
            // Chỉ cần giữ đúng nhịp 15 chu kỳ CPU / lần cập nhật kênh.
            cpuCycleCounter += cpuCycles;

            while (cpuCycleCounter >= 15)
            {
                cpuCycleCounter -= 15;
                ClockOneChannel();
            }
            return;
        }

        // Chế độ 1 DAC: chạy từng chu kỳ CPU, tích lũy mức DAC đang giữ rồi lấy
        // trung bình khi xuất mẫu (lọc box) để tránh aliasing do DAC đổi giá trị ~119kHz.
        for (int i = 0; i < cpuCycles; i++)
        {
            if (++cpuCycleCounter >= 15)
            {
                cpuCycleCounter = 0;
                ClockOneChannel();
            }

            // DAC (đổi giá trị mỗi 15 chu kỳ) -> lọc thông thấp analog ở tốc độ CPU
            lpf1 += lpfAlpha * (double(heldOutput) - lpf1);
            lpf2 += lpfAlpha * (lpf1 - lpf2);

            accSum += lpf2;
            accCount++;
        }
    }

    float GetSample() const
    {
        if (soundDisabled)
            return 0.0f;

        if (accurateMultiplex)
        {
            if (accCount > 0)
            {
                lastAvg = float(accSum / double(accCount));
                accSum = 0.0;
                accCount = 0;
            }
            return NormalizeOutput(lastAvg) * volume;
        }

        return GetMixedSampleApprox();
    }

    uint8_t PeekRam(uint8_t addr) const
    {
        return ram[addr & 0x7F];
    }

    // $7F bit 4-6: 0 => 1 kênh ... 7 => 8 kênh.
    int GetActiveChannelCount() const
    {
        return ((ram[0x7F] >> 4) & 0x07) + 1;
    }

    // Mute / gain theo "view index" (0..7, dồn từ lowest..7 giống GetDebugChannels).
    void SetChannelMuted(int viewIndex, bool muted)
    {
        if (viewIndex < 0 || viewIndex >= 8)
            return;
        muteChannel[viewIndex] = muted;
    }

    bool IsChannelMuted(int viewIndex) const
    {
        if (viewIndex < 0 || viewIndex >= 8)
            return false;
        return muteChannel[viewIndex];
    }

    void SetChannelGain(int viewIndex, float gain)
    {
        if (viewIndex < 0 || viewIndex >= 8)
            return;
        gainChannel[viewIndex] = std::clamp(gain, 0.0f, 2.0f);
    }

    float GetChannelGainByView(int viewIndex) const
    {
        if (viewIndex < 0 || viewIndex >= 8)
            return 1.0f;
        return gainChannel[viewIndex];
    }

private:
    static constexpr float CPU_RATE = 1789773.0f;

    std::array<uint8_t, 0x80> ram{};
    double debugSampleRate = 44100.0;

    uint8_t address = 0;
    bool autoIncrement = false;
    bool soundDisabled = false;

    int cpuCycleCounter = 0;
    int currentChannel = 7;
    float heldOutput = 0.0f;

    // Bộ tích lũy trung bình mức DAC giữa hai lần GetSample() (chế độ 1 DAC)
    mutable double accSum = 0.0;
    mutable int accCount = 0;
    mutable float lastAvg = 0.0f;

    float volume = 2.0f;
    bool accurateMultiplex = false;

    // Bộ lọc thông thấp 2 tầng chạy ở tốc độ CPU (chế độ 1 DAC)
    double lpf1 = 0.0;
    double lpf2 = 0.0;
    double lpfAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * (20000.0 / 0.6436) / 1789773.0);

    std::array<bool, 8> muteChannel{};
    std::array<float, 8> gainChannel{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

private:
    // Mẫu 4-bit (0..15) trừ 8 rồi nhân volume (0..15): khoảng -120..+105.
    static float NormalizeOutput(float value)
    {
        return std::clamp(value / 120.0f, -1.0f, 1.0f);
    }

    int GetLowestEnabledChannel() const
    {
        // 1 kênh: ch 7 | 2 kênh: ch 7,6 | ... | 8 kênh: ch 7..0
        return 8 - GetActiveChannelCount();
    }

    // ---- Truy cập thanh ghi kênh trong RAM ---------------------------------

    static int ChannelBase(int ch)
    {
        return 0x40 + ch * 8;
    }

    // Tần số 18-bit: byte 0 (thấp), byte 2 (giữa), byte 4 bit 0-1 (cao).
    uint32_t ReadFrequency(int ch) const
    {
        const int base = ChannelBase(ch);
        return (uint32_t(ram[base + 4] & 0x03) << 16) |
            (uint32_t(ram[base + 2]) << 8) |
            uint32_t(ram[base + 0]);
    }

    // Độ dài sóng = 256 - (byte4 & 0xFC), tức 4..256 mẫu (bội số của 4).
    uint32_t ReadWaveLength(int ch) const
    {
        return 256u - uint32_t(ram[ChannelBase(ch) + 4] & 0xFC);
    }

    // Phase 24-bit: byte 1, 3, 5.
    uint32_t ReadPhase(int ch) const
    {
        const int base = ChannelBase(ch);
        return (uint32_t(ram[base + 5]) << 16) |
            (uint32_t(ram[base + 3]) << 8) |
            uint32_t(ram[base + 1]);
    }

    void WritePhase(int ch, uint32_t phase)
    {
        const int base = ChannelBase(ch);
        ram[base + 1] = uint8_t(phase & 0xFF);
        ram[base + 3] = uint8_t((phase >> 8) & 0xFF);
        ram[base + 5] = uint8_t((phase >> 16) & 0xFF);
    }

    // N163 lưu 2 mẫu 4-bit trong 1 byte: mẫu chẵn = nibble thấp, mẫu lẻ = nibble cao.
    // Chỉ số mẫu 8-bit nên quấn vòng trong 256 nibble = 128 byte RAM.
    uint8_t ReadWaveSample(uint8_t sampleIndex) const
    {
        const uint8_t byte = ram[sampleIndex >> 1];
        return (sampleIndex & 1) ? ((byte >> 4) & 0x0F) : (byte & 0x0F);
    }

    // Đầu ra DAC của kênh phần cứng ch, theo phase hiện tại trong RAM.
    float ReadChannelOutput(int ch) const
    {
        const int viewIndex = ch - GetLowestEnabledChannel();
        if (viewIndex >= 0 && viewIndex < 8 && muteChannel[viewIndex])
            return 0.0f;

        const int base = ChannelBase(ch);

        const uint32_t phase = ReadPhase(ch);
        const uint8_t waveAddress = ram[base + 6];
        const uint8_t volume4 = ram[base + 7] & 0x0F;

        const uint8_t sampleIndex = uint8_t((phase >> 16) + waveAddress);
        const uint8_t sample = ReadWaveSample(sampleIndex);

        const float raw = float((int(sample) - 8) * int(volume4));
        const float gain = (viewIndex >= 0 && viewIndex < 8) ? gainChannel[viewIndex] : 1.0f;

        return raw * gain;
    }

    // Một lần cập nhật của chip: xử lý đúng 1 kênh rồi chuyển sang kênh kế tiếp.
    void ClockOneChannel()
    {
        const int lowest = GetLowestEnabledChannel();

        // Game đổi số kênh giữa chừng: kênh hiện tại nằm ngoài vùng bật -> về kênh 7.
        if (currentChannel < lowest || currentChannel > 7)
            currentChannel = 7;

        const int ch = currentChannel;

        const uint32_t freq = ReadFrequency(ch);
        const uint32_t modulo = ReadWaveLength(ch) << 16;

        // 24-bit phase, wrap theo độ dài sóng. Dùng % vì game có thể ghi phase
        // lớn hơn modulo mới khi đổi độ dài sóng.
        uint32_t phase = ReadPhase(ch) + freq;
        phase %= modulo;
        WritePhase(ch, phase);

        // Phase đã cập nhật -> đọc mẫu và giữ ở DAC cho tới lần cập nhật sau.
        heldOutput = ReadChannelOutput(ch);

        if (--currentChannel < lowest)
            currentChannel = 7;
    }

    // Xấp xỉ: lấy trung bình tức thời các kênh đang bật.
    // Cùng thang âm lượng với chế độ accurate (trung bình theo thời gian của 1 DAC).
    float GetMixedSampleApprox() const
    {
        const int count = GetActiveChannelCount();
        const int lowest = GetLowestEnabledChannel();

        float sum = 0.0f;
        for (int ch = lowest; ch < 8; ch++)
            sum += ReadChannelOutput(ch);

        sum /= float(count);

        return NormalizeOutput(sum) * volume;
    }
};