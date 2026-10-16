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

#include "Mapper_118.h"

Mapper_118::Mapper_118(uint8_t prgBanks, uint8_t chrBanks) : Mapper_004(prgBanks, chrBanks) {
    reset();
}

Mapper_118::~Mapper_118() {}

void Mapper_118::reset() {
    Mapper_004::reset();

    nTarget118 = 0;
    bInvert118 = false;
    for (int i = 0; i < 8; i++) reg118[i] = 0;
    for (int i = 0; i < 4; i++) ntPage[i] = false;
    mirror118 = MIRROR::ONESCREEN_LO;
}

bool Mapper_118::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    // Theo dõi thanh ghi bank select / bank data để biết bit 7 của R0..R5
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if ((addr & 1) == 0) {
            nTarget118 = data & 0x07;
            bInvert118 = (data & 0x80) != 0;
        }
        else {
            reg118[nTarget118] = data;
        }
        updateMirroring();
    }

    // Phần còn lại (PRG/CHR/IRQ) là MMC3 nguyên bản
    return Mapper_004::cpuMapWrite(addr, mapped_addr, data);
}

void Mapper_118::updateMirroring() {
    if (!bInvert118) {
        ntPage[0] = ntPage[1] = (reg118[0] & 0x80) != 0;
        ntPage[2] = ntPage[3] = (reg118[1] & 0x80) != 0;
    }
    else {
        ntPage[0] = (reg118[2] & 0x80) != 0;
        ntPage[1] = (reg118[3] & 0x80) != 0;
        ntPage[2] = (reg118[4] & 0x80) != 0;
        ntPage[3] = (reg118[5] & 0x80) != 0;
    }

    if (ntPage[0] == ntPage[1] && ntPage[2] == ntPage[3]) {
        if (ntPage[0] == ntPage[2])
            mirror118 = ntPage[0] ? MIRROR::ONESCREEN_HI : MIRROR::ONESCREEN_LO;   // AAAA
        else
            mirror118 = MIRROR::HORIZONTAL;                                        // AABB
    }
    else if (ntPage[0] == ntPage[2] && ntPage[1] == ntPage[3]) {
        mirror118 = MIRROR::VERTICAL;                                              // ABAB
    }
    // Tổ hợp khác: không biểu diễn được -> giữ nguyên
}

MIRROR Mapper_118::mirror() {
    return mirror118;
}

std::string Mapper_118::GetDebugInfo()
{
    std::string s = Mapper_004::GetDebugInfo();

    size_t pos = s.find("MAPPER 004 - MMC3");
    if (pos != std::string::npos) {
        s.replace(pos, 17, "MAPPER 118 - TxSROM (MMC3)");
    }

    s += "\nTXSROM - NAMETABLE THEO BIT 7 CUA THANH GHI CHR:\n";
    for (int i = 0; i < 4; i++) {
        s += "NT" + std::to_string(i) + " -> CIRAM trang " + std::to_string(ntPage[i] ? 1 : 0) + "\n";
    }
    s += "Kieu mirroring suy ra: ";
    switch (mirror118) {
    case MIRROR::HORIZONTAL:   s += "Horizontal\n"; break;
    case MIRROR::VERTICAL:     s += "Vertical\n"; break;
    case MIRROR::ONESCREEN_LO: s += "One-screen thap\n"; break;
    case MIRROR::ONESCREEN_HI: s += "One-screen cao\n"; break;
    default:                   s += "Khong ro\n"; break;
    }
    return s;
}