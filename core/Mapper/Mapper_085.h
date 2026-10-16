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
#include <array>
#include <QString>

extern "C" {
#include "emu2413.h"
}

class Mapper_085 : public Mapper
{
public:
    Mapper_085(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_085();

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void irqStep() override;
    bool irqState() override;
    void irqClear() override;

    std::string GetDebugInfo() override;

    MIRROR mirror() override { return mirrorMode; }
    void GetVrc7DebugChannels(
        float& ch1, float& ch2, float& ch3,
        float& ch4, float& ch5, float& ch6
    );
    void GetVrc7DebugPeriods(
        float& ch1, float& ch2, float& ch3,
        float& ch4, float& ch5, float& ch6
    );
    void GetVrc7DebugPhases(
        float& ch1, float& ch2, float& ch3,
        float& ch4, float& ch5, float& ch6
    );
    float GetExpansionAudio() override;
    void GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3) override;

    // Mute riêng từng kênh FM (0..5). Ép volume nibble của thanh ghi
    // Instrument/Volume ($30+ch) về im lặng khi mute, khôi phục giá trị
    // gốc (lưu trong audioRegs) khi unmute.
    void SetVrc7ChannelMuted(int ch, bool muted);

    // Gain riêng từng kênh FM (0.0 - 1.0, mặc định 1.0). VRC7 dùng chip FM
    // thật (emu2413) tự mix 6 kênh analog BÊN TRONG chip, không tách riêng
    // được sau khi mix để khuếch đại > 100% như các chip khác — chỉ có thể
    // GIẢM âm lượng bằng cách cộng thêm attenuation vào thanh ghi Volume
    // (giống cơ chế mute ở trên, nhưng liên tục 0-15 step thay vì on/off).
    // Vì vậy gain VRC7 luôn bị clamp trong [0.0, 1.0], không boost được 200%.
    // channelId dạng "vrc7.ch1".."vrc7.ch6".
    void SetChannelGain(const QString& channelId, float gain);

    // Dong bo sample rate xuat ra (44.1/48/96kHz...) cho emu2413. Idempotent.
    void SetSampleRate(double hz);

private:
    std::array<uint8_t, 3> prgBank{};
    std::array<uint8_t, 8> chrBank{};

    MIRROR mirrorMode = VERTICAL;

    bool irqEnabled = false;
    bool irqEnableAfterAck = false;
    bool irqPending = false;
    bool irqCycleMode = false;
    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    int irqPrescaler = 341;
    uint32_t totalIrqStepCalls = 0;
    uint32_t lastFireCycle = 0;
    OPLL* opll = nullptr;
    double opllSampleRate = 44100.0;

    uint8_t audioAddr = 0;
    bool audioDisabled = false; // $E000 bit 6: tắt + xóa âm thanh VRC7
    uint8_t audioRegs[0x40]{};

    float lastVrc7Sample = 0.0f;
    float lastVrc7Debug[6]{};
    float lastVrc7Phase[6]{};
    float lastVrc7Period[6]{};
    bool lastVrc7PhaseValid[6]{};
    bool muteChannel[6]{};
    float gainVrc7[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
};