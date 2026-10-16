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

#include "Mapper_034.h"
#include "LogBuffer.h"

Mapper_034::Mapper_034(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_034::~Mapper_034() {}

void Mapper_034::reset() {
    bNINA = (nCHRBanks != 0);
    nPRGBank32 = 0;
    nCHRBank4[0] = 0;
    nCHRBank4[1] = 1;
}

bool Mapper_034::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000) {
        mapped_addr = (uint32_t)nPRGBank32 * 0x8000 + (addr & 0x7FFF);
        return true;
    }
    return false;
}

bool Mapper_034::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    uint32_t nPrg32 = nPRGBanks / 2;   // số bank 32KB
    if (nPrg32 == 0) nPrg32 = 1;

    if (!bNINA) {
        // BNROM: mọi ghi vào $8000-$FFFF chọn bank PRG 32KB
        if (addr >= 0x8000) {
            nPRGBank32 = data % nPrg32;
        }
    }
    else {
        // NINA-001: thanh ghi nằm ở $7FFD-$7FFF
        if (addr == 0x7FFD) {
            nPRGBank32 = data % nPrg32;
        }
        else if (addr == 0x7FFE) {
            nCHRBank4[0] = data;
        }
        else if (addr == 0x7FFF) {
            nCHRBank4[1] = data;
        }
    }
    return false;
}

bool Mapper_034::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        if (!bNINA) {               // BNROM: CHR-RAM
            mapped_addr = addr;
            return true;
        }
        uint32_t nChr4 = (uint32_t)nCHRBanks * 2;   // số bank 4KB
        uint32_t bank = nCHRBank4[(addr >> 12) & 1] % nChr4;
        mapped_addr = bank * 0x1000 + (addr & 0x0FFF);
        return true;
    }
    return false;
}

bool Mapper_034::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000 && !bNINA) {   // CHR-RAM (BNROM)
        mapped_addr = addr;
        return true;
    }
    return false;
}

std::string Mapper_034::GetDebugInfo()
{
    std::string s;
    s += "===== MAPPER 034 - ";
    s += bNINA ? "NINA-001" : "BNROM";
    s += " =====\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So PRG banks 32KB : " + std::to_string(nPRGBanks / 2) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "PRG bank 32KB     : " + std::to_string(nPRGBank32) + "\n";
    s += "  offset ROM      : 0x" + HexStr((uint32_t)nPRGBank32 * 0x8000, 6) + "\n";

    if (bNINA) {
        s += "CHR $0000 (4KB)   : " + std::to_string(nCHRBank4[0]) + "\n";
        s += "CHR $1000 (4KB)   : " + std::to_string(nCHRBank4[1]) + "\n";
    }
    else {
        s += "CHR               : RAM 8KB\n";
    }

    s += "\nGHI CHU:\n";
    s += "Mirroring theo header. NINA-001 ghi thanh ghi o $7FFD-$7FFF.\n";
    return s;
}