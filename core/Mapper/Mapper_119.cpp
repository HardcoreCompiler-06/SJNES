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

#include "Mapper_119.h"
#include "LogBuffer.h"

Mapper_119::Mapper_119(uint8_t prgBanks, uint8_t chrBanks) : Mapper_004(prgBanks, chrBanks) {
    reset();
}

Mapper_119::~Mapper_119() {}

void Mapper_119::reset() {
    Mapper_004::reset();

    nTarget119 = 0;
    bInvert119 = false;
    for (int i = 0; i < 8; i++) reg119[i] = 0;
}

bool Mapper_119::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    // Theo dõi thanh ghi bank select / bank data
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if ((addr & 1) == 0) {
            nTarget119 = data & 0x07;
            bInvert119 = (data & 0x80) != 0;
        }
        else {
            reg119[nTarget119] = data;
        }
    }

    // PRG/IRQ/mirroring là MMC3 nguyên bản. Phần CHR-ROM base tự lo
    // (modulo 64 bank nên bit 6/7 bị cắt), phần CHR-RAM xử lý ở dưới.
    return Mapper_004::cpuMapWrite(addr, mapped_addr, data);
}

bool Mapper_119::isRamSlot(uint16_t addr, uint32_t& page) const {
    // Ô 1KB số slot (0..7) và thanh ghi cấp cho ô đó
    int slot = (addr >> 10) & 7;
    uint8_t v;

    if (!bInvert119) {
        switch (slot) {
        case 0: v = reg119[0] & 0xFE;       break;
        case 1: v = (reg119[0] & 0xFE) + 1; break;
        case 2: v = reg119[1] & 0xFE;       break;
        case 3: v = (reg119[1] & 0xFE) + 1; break;
        default: v = reg119[slot - 2];      break;   // slot 4..7 -> R2..R5
        }
    }
    else {
        switch (slot) {
        case 0: case 1: case 2: case 3: v = reg119[slot + 2]; break;   // R2..R5
        case 4: v = reg119[0] & 0xFE;       break;
        case 5: v = (reg119[0] & 0xFE) + 1; break;
        case 6: v = reg119[1] & 0xFE;       break;
        default: v = (reg119[1] & 0xFE) + 1; break;   // slot 7
        }
    }

    if (v & 0x40) {
        page = v & 0x07;
        return true;
    }
    return false;
}

bool Mapper_119::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x2000) return false;

    uint32_t page;
    if (isRamSlot(addr, page)) {
        // CHR-RAM nằm ngay sau CHR-ROM trong vCHRMemory
        mapped_addr = (uint32_t)nCHRBanks * 0x2000 + page * 0x0400 + (addr & 0x03FF);
        return true;
    }
    return Mapper_004::ppuMapRead(addr, mapped_addr);
}

bool Mapper_119::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x2000) return false;

    uint32_t page;
    if (isRamSlot(addr, page)) {
        mapped_addr = (uint32_t)nCHRBanks * 0x2000 + page * 0x0400 + (addr & 0x03FF);
        return true;
    }
    return false;   // CHR-ROM không ghi được
}

std::string Mapper_119::GetDebugInfo()
{
    std::string s = Mapper_004::GetDebugInfo();

    size_t pos = s.find("MAPPER 004 - MMC3");
    if (pos != std::string::npos) {
        s.replace(pos, 17, "MAPPER 119 - TQROM (MMC3)");
    }

    s += "\nTQROM - NGUON CHR TUNG O 1KB:\n";
    for (int i = 0; i < 8; i++) {
        uint16_t addr = i * 0x0400;
        uint32_t page = 0;
        bool ram = isRamSlot(addr, page);
        s += "$" + HexStr(addr, 4) + " : " + (ram ? "CHR-RAM trang " + std::to_string(page) : std::string("CHR-ROM")) + "\n";
    }
    return s;
}