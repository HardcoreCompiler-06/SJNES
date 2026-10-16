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

class Mapper_069 : public Mapper
{
public:
    Mapper_069(uint8_t prgBanks, uint8_t chrBanks);

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void irqStep() override;
    bool irqState() override;
    void irqClear() override;

    std::string GetDebugInfo() override;

    float GetExpansionAudio() override;
    void GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3) override;
    void GetS5BDebugPeriods(float& ch1, float& ch2, float& ch3) const;
    void GetS5BDebugDuty(float& ch1, float& ch2, float& ch3) const;
    void GetExpansionAudioStereo(float& expL, float& expR) override;
    int GetMirrorMode() const { return mirrorMode; }
    // Sample rate xuat ra hien tai (Hz), dung de quy doi chu ky debug cho scope.
    void SetSampleRate(double hz) { if (hz >= 8000.0) debugSampleRate = hz; }
    bool IsPrg6000RamSelected() const { return prg6000RamSelected; }
    bool IsPrg6000RamEnabled() const { return prg6000RamEnabled; }
private:
    uint8_t command = 0x00;

    std::array<uint8_t, 8> chrBank{};
    std::array<uint8_t, 4> prgBank{};

    int mirrorMode = 0;

    bool irqEnabled = false;
    bool irqCounterEnabled = false;
    bool irqPending = false;
    uint16_t irqCounter = 0;

    // Sunsoft 5B audio
    uint8_t audioRegSelect = 0;
    uint8_t audioRegs[16]{};

    int toneCounter[3]{};
    bool toneOutput[3]{};
    float lastToneSample[3]{};

    int audioDivider = 0;
    double debugSampleRate = 44100.0;
    bool prg6000RamSelected = false;
    bool prg6000RamEnabled = false;

public:
    // Mute riêng từng kênh tone A/B/C
    bool muteToneA = false;
    bool muteToneB = false;
    bool muteToneC = false;

    // Gain riêng từng kênh (0.0 - 2.0, mặc định 1.0), đi cùng mute ở trên.
    // Set qua SettingsDialog slider, channelId dạng "s5b.toneA"/"s5b.toneB"/"s5b.toneC".
    float gainToneA = 1.0f;
    float gainToneB = 1.0f;
    float gainToneC = 1.0f;
    void SetChannelGain(const QString& channelId, float gain);
};