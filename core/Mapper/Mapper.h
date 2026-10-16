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

/*
	Derived from olc::NES - Mapper Base Class (Abstract)
	by David Barr (javidx9), OneLoneCoder.com
	Source: https://github.com/OneLoneCoder/olcNES

	License (OLC-3)
	~~~~~~~~~~~~~~~

	Copyright 2018-2019 OneLoneCoder.com

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	1. Redistributions or derivations of source code must retain the above
	copyright notice, this list of conditions and the following disclaimer.

	2. Redistributions or derivative works in binary form must reproduce
	the above copyright notice. This list of conditions and the following
	disclaimer must be reproduced in the documentation and/or other
	materials provided with the distribution.

	3. Neither the name of the copyright holder nor the names of its
	contributors may be used to endorse or promote products derived
	from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

class BinaryWriter;
class BinaryReader;

enum MIRROR {
    HARDWARE,
    HORIZONTAL,
    VERTICAL,
    ONESCREEN_LO,
    ONESCREEN_HI,
};

inline std::string HexStr(uint32_t value, int width) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << value;
    return oss.str();
}

class Mapper {
public:
    Mapper(uint32_t prgBanks, uint32_t chrBanks);
    virtual ~Mapper();

    // === CÁC HÀM QUẢN LÝ IRQ MỚI (Cho Namco 163 / Mapper 019) ===
    virtual void clock() {}
    virtual bool irqState() { return false; }
    virtual void irqClear() {}

    // === CÁC HÀM CŨ PHẢI GIỮ LẠI (Cho MMC3, MMC5, VRC6...) ===
    virtual MIRROR mirror() { return HARDWARE; }
    virtual void ClockA12() {}
    virtual void irqStep() {}
    virtual float GetExpansionAudio() { return 0.0f; }

    virtual void GetExpansionAudioStereo(float& left, float& right) {
        float v = GetExpansionAudio();
        left = v;
        right = v;
    }

    virtual bool cpuReadRegister(uint16_t addr, uint8_t& data) { return false; }
    virtual void GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3) { ch1 = 0.0f; ch2 = 0.0f; ch3 = 0.0f; }

    virtual void ClockCpu(int cycles) {}
    virtual float GetExpansionAudioSample() const { return 0.0f; }

    virtual std::string GetDebugInfo() {
        std::string s = "===== DEBUG MAPPER =====\n";
        s += "PRG Banks: " + std::to_string(nPRGBanks) + "\n";
        s += "CHR Banks: " + std::to_string(nCHRBanks) + "\n";
        s += "Submapper: " + std::to_string(nSubmapper) + "\n";
        return s;
    }

    virtual bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) = 0;
    virtual bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) = 0;

    virtual void reset() = 0;

    // === SAVE/LOAD STATE (Default: empty, override nếu cần) ===
    virtual void SaveState(BinaryWriter& out) const {}
    virtual void LoadState(BinaryReader& in) {}

    uint32_t nPRGBanks = 0;
    uint32_t nCHRBanks = 0;
    uint8_t nSubmapper = 0;
};