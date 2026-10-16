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

#include "Mapper_002.h"
#include "BinaryIO.h"

Mapper_002::Mapper_002(uint32_t prgBanks, uint32_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset(); // Gọi reset ngay lúc khởi tạo
}

Mapper_002::~Mapper_002() {}

void Mapper_002::reset() {
    nPRGBankSelectLo = 0;
}

bool Mapper_002::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        mapped_addr = nPRGBankSelectLo * 16384 + (addr & 0x3FFF);
        return true;
    }
    else if (addr >= 0xC000 && addr <= 0xFFFF) {
        mapped_addr = (nPRGBanks - 1) * 16384 + (addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_002::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        nPRGBankSelectLo = data & 0xFF;
        if (nPRGBanks > 0) {
            nPRGBankSelectLo %= nPRGBanks;
        }
    }
    return false;
}

bool Mapper_002::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        return false; // Trả về false để PPU tự động dùng CHR-RAM nội bộ!
    }
    return false;
}

bool Mapper_002::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        return false; // Trả về false để PPU tự động ghi vào CHR-RAM nội bộ!
    }
    return false;
}

void Mapper_002::SaveState(BinaryWriter& out) const {
    out << nPRGBankSelectLo;
}

void Mapper_002::LoadState(BinaryReader& in) {
    in >> nPRGBankSelectLo;
}

std::string Mapper_002::GetDebugInfo()
{
    std::string s;

    s += "===== MAPPER 002 - UXROM =====\n\n";

    s += "Mapper nay dung PRG bank switching.\n";
    s += "Vung CPU $8000-$BFFF la bank co the doi.\n";
    s += "Vung CPU $C000-$FFFF thuong la bank cuoi co dinh.\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";

    uint32_t map8000 = 0;
    uint32_t mapC000 = 0;

    bool ok8000 = cpuMapRead(0x8000, map8000);
    bool okC000 = cpuMapRead(0xC000, mapC000);

    s += "\nPRG BANK HIEN TAI:\n";

    if (ok8000)
    {
        s += "$8000-$BFFF : dang tro toi PRG bank " + std::to_string(map8000 / 0x4000) +
            " | offset ROM = 0x" + HexStr(map8000, 6) + "\n";
    }
    else
    {
        s += "$8000-$BFFF : khong map duoc\n";
    }

    if (okC000)
    {
        s += "$C000-$FFFF : dang tro toi PRG bank " + std::to_string(mapC000 / 0x4000) +
            " | offset ROM = 0x" + HexStr(mapC000, 6) + "\n";
    }
    else
    {
        s += "$C000-$FFFF : khong map duoc\n";
    }

    s += "\nCHR:\n";

    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM\n";
    }
    else
    {
        uint32_t map0000 = 0;
        if (ppuMapRead(0x0000, map0000))
        {
            s += "$0000-$1FFF : CHR ROM | offset = 0x" + HexStr(map0000, 6) + "\n";
        }
        else
        {
            s += "$0000-$1FFF : CHR ROM/RAM co dinh\n";
        }
    }

    return s;
}