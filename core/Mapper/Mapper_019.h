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

#include "Mapper.h"
#include "Namco163Audio.h"
#include <QString>

class Mapper_019 : public Mapper
{
public:
    Mapper_019(uint8_t prgBanks, uint8_t chrBanks);

    bool cpuMapRead(uint16_t addr, uint32_t& mapped) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override;

    bool cpuReadRegister(uint16_t addr, uint8_t& data) override;

    void clock() override;
    bool irqState() override;
    void irqClear() override;
    void reset() override;

    MIRROR mirror() override
    {
        return mirroring_mode;
    }

    // Âm thanh Namco 163
    void ClockCpu(int cycles) override;
    float GetExpansionAudio() override;

    void irqStep() override;

    void GetN163DebugChannels(
        float& ch1, float& ch2, float& ch3, float& ch4,
        float& ch5, float& ch6, float& ch7, float& ch8
    );

    void GetN163DebugPeriods(
        float& ch1, float& ch2, float& ch3, float& ch4,
        float& ch5, float& ch6, float& ch7, float& ch8
    );

    // Phase thật 0..1 của từng kênh (cùng view index), dùng cho khóa pha oscilloscope.
    void GetN163DebugPhases(
        float& ch1, float& ch2, float& ch3, float& ch4,
        float& ch5, float& ch6, float& ch7, float& ch8
    );

    // Mute riêng từng kênh N163 (viewIndex 0..7, dồn từ lowest..7 giống
    // thứ tự hiển thị debug). Forward thẳng xuống Namco163Audio::SetChannelMuted.
    void SetN163ChannelMuted(int viewIndex, bool muted)
    {
        n163.SetChannelMuted(viewIndex, muted);
    }

    // channelId dạng "n163.ch1".."n163.ch8" (1-indexed cho dễ đọc, tự trừ 1
    // ra viewIndex 0-indexed). Forward xuống Namco163Audio::SetChannelGain.
    void SetChannelGain(const QString& channelId, float gain);

    // Đồng bộ sample rate xuất ra cho phần debug chu kỳ N163 (oscilloscope).
    void SetSampleRate(double hz) { n163.SetSampleRate(hz); }

    // Chế độ âm thanh N163:
    //   false = chia trung bình 8 kênh (sạch, trong)
    //   true  = 8 kênh ghép qua 1 DAC như chip thật (có độ ù/hạt đặc trưng)
    // accurateAudioDefault giữ lựa chọn của Setting qua các lần nạp ROM
    // (reset() đọc giá trị này); SetAccurateAudio() đổi ngay trên game đang chạy.
    static inline bool accurateAudioDefault = false;
    void SetAccurateAudio(bool accurate) { n163.SetAccurateMultiplex(accurate); }
private:

    void UpdateNametableMirror();
    bool enableChrRomNametable = true;
    uint8_t prg[4];
    uint8_t chr[12];

    uint8_t reg_E800 = 0;

    MIRROR mirroring_mode = HARDWARE;

    // Namco 163 audio thật
    Namco163Audio n163;

    uint16_t irq_counter = 0;
    bool irq_enable = false;
    bool irq_active = false;

    static constexpr uint32_t WRITE_SINK = 0x7FFFFFFF;

    uint32_t GetCiramBase() const;
    bool BankUsesCiram(int slot, uint8_t bank) const;
    bool MapPpuAddress(uint16_t addr, uint32_t& mapped, bool forWrite) const;

};