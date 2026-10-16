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
	Derived from olc::NES - Mapper 000 (NROM)
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
#include "Mapper_000.h"
#include <string>

Mapper_000::Mapper_000(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {}

Mapper_000::~Mapper_000() {}

bool Mapper_000::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_000::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_000::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        mapped_addr = addr;
        return true;
    }
    return false;
}

bool Mapper_000::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        if (nCHRBanks == 0) {
            mapped_addr = addr;
            return true;
        }
    }
    return false;
}

void Mapper_000::reset() {}

std::string Mapper_000::GetDebugInfo()
{
    std::string s;

    s += "===== MAPPER 000 - NROM =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "Mapper nay khong co bank switching.\n";
    s += "PRG va CHR duoc map co dinh.\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";

    if (nPRGBanks > 1)
    {
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : PRG bank 1 | offset ROM = 0x004000\n";
    }
    else
    {
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : mirror lai PRG bank 0 | offset ROM = 0x000000\n";
    }

    s += "\nCHR BANK HIEN TAI:\n";

    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM 8KB, PPU co the ghi\n";
    }
    else
    {
        s += "$0000-$1FFF : CHR ROM bank 0, map thang\n";
    }

    s += "\nMAPPING CPU:\n";
    s += "CPU $8000-$FFFF duoc map bang addr & 0x7FFF neu co 32KB PRG.\n";
    s += "Neu chi co 16KB PRG, dung addr & 0x3FFF de mirror vung $C000-$FFFF.\n";

    s += "\nMAPPING PPU:\n";
    s += "PPU $0000-$1FFF map thang vao CHR ROM/RAM.\n";

    return s;
}