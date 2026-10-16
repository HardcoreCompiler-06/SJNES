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

#include "Mapper_010.h"
#include "LogBuffer.h"

Mapper_010::Mapper_010(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_010::~Mapper_010() {}

void Mapper_010::reset() {
    nPRGBank = 0;
    for (int h = 0; h < 2; h++) {
        nCHRBank[h][0] = 0;
        nCHRBank[h][1] = 0;
        nLatch[h] = 1;   // latch khởi tạo = $FE
    }
    mirrormode = MIRROR::HORIZONTAL;
}

MIRROR Mapper_010::mirror() {
    return mirrormode;
}

bool Mapper_010::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    uint32_t nPrg16 = nPRGBanks;   // số bank 16KB
    if (nPrg16 == 0) nPrg16 = 1;

    if (addr >= 0x8000 && addr <= 0xBFFF) {
        mapped_addr = ((uint32_t)(nPRGBank % nPrg16)) * 0x4000 + (addr & 0x3FFF);
        return true;
    }
    if (addr >= 0xC000) {
        mapped_addr = (nPrg16 - 1) * 0x4000 + (addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_010::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    // $6000-$7FFF (PRG-RAM) do Cartridge xử lý trước khi tới mapper
    if (addr < 0x8000) return false;

    switch (addr & 0xF000) {
    case 0xA000: nPRGBank = data & 0x0F;      break;
    case 0xB000: nCHRBank[0][0] = data & 0x1F; break;
    case 0xC000: nCHRBank[0][1] = data & 0x1F; break;
    case 0xD000: nCHRBank[1][0] = data & 0x1F; break;
    case 0xE000: nCHRBank[1][1] = data & 0x1F; break;
    case 0xF000:
        mirrormode = (data & 0x01) ? MIRROR::HORIZONTAL : MIRROR::VERTICAL;
        break;
    }

    // Luôn false để không ghi đè PRG-ROM
    return false;
}

bool Mapper_010::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x2000) return false;

    if (nCHRBanks == 0) {   // CHR-RAM (không có trên game thật, chỉ để an toàn)
        mapped_addr = addr;
        return true;
    }

    int half = (addr >> 12) & 1;
    uint32_t nChr4 = (uint32_t)nCHRBanks * 2;   // số bank 4KB
    uint32_t bank = nCHRBank[half][nLatch[half]] % nChr4;
    mapped_addr = bank * 0x1000 + (addr & 0x0FFF);

    // Latch đổi SAU khi fetch (byte vừa đọc vẫn dùng bank cũ)
    // MMC4: cả hai nửa đều kích hoạt theo dải 8 byte
    uint16_t a = addr & 0x1FF8;
    if (a == 0x0FD8)      nLatch[0] = 0;
    else if (a == 0x0FE8) nLatch[0] = 1;
    else if (a == 0x1FD8) nLatch[1] = 0;
    else if (a == 0x1FE8) nLatch[1] = 1;

    return true;
}

bool Mapper_010::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000 && nCHRBanks == 0) {
        mapped_addr = addr;
        return true;
    }
    return false;
}

std::string Mapper_010::GetDebugInfo()
{
    std::string s;

    auto mirrorToString = [](MIRROR m) -> std::string {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Doc";
        case MIRROR::ONESCREEN_LO: return "One-screen thap";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        default:                   return "Hardware / Khong ro";
        }
        };

    s += "===== MAPPER 010 - MMC4 =====\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "So CHR banks 4KB  : " + std::to_string(nCHRBanks * 2) + "\n";
    s += "Mirroring         : " + mirrorToString(mirrormode) + "\n";

    s += "\nPRG:\n";
    s += "$8000-$BFFF : PRG bank 16KB " + std::to_string(nPRGBank) +
        " | offset ROM = 0x" + HexStr((uint32_t)nPRGBank * 0x4000, 6) + "\n";
    s += "$C000-$FFFF : PRG bank 16KB " + std::to_string(nPRGBanks == 0 ? 0 : nPRGBanks - 1) + " | co dinh\n";

    s += "\nCHR LATCH:\n";
    for (int h = 0; h < 2; h++) {
        s += "Latch " + std::to_string(h) + " ($" + (h == 0 ? "0000" : "1000") + ") : " +
            (nLatch[h] == 0 ? "FD" : "FE") +
            " | FD bank = " + std::to_string(nCHRBank[h][0]) +
            " | FE bank = " + std::to_string(nCHRBank[h][1]) + "\n";
    }

    s += "\nGHI CHU:\n";
    s += "MMC4: giong MMC2 nhung PRG 16KB, latch kich hoat theo dai 8 byte.\n";

    return s;
}