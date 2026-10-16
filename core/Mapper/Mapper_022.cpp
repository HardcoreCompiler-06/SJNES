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

#include "Mapper_022.h"

Mapper_022::Mapper_022(uint8_t prgBanks, uint8_t chrBanks) : Mapper_021(prgBanks, chrBanks) {
    nCHRShift = 1;   // VRC2a: bỏ bit thấp của giá trị bank CHR
    reset();         // tính lại pCHRBank với nCHRShift mới
}

Mapper_022::~Mapper_022() {}

bool Mapper_022::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr < 0x8000) return false;

    // VRC2a: A0 = CPU bit 1, A1 = CPU bit 0
    uint8_t latch = ((addr >> 1) & 1) | ((addr & 1) << 1);
    uint16_t block = addr & 0xF000;

    switch (block) {
    case 0x9000:
        // VRC2: chỉ có mirroring (bit 0), không có swap mode
        writeReg(0x9000, 0, data & 0x01);
        break;

    case 0xB000: case 0xC000: case 0xD000: case 0xE000:
        // VRC2: nibble cao của CHR chỉ 4 bit (bỏ bit 4 ở thanh ghi lẻ)
        if (latch & 1) data &= 0x0F;
        writeReg(block, latch, data);
        break;

    case 0xF000:
        // VRC2 không có IRQ
        break;

    default:
        writeReg(block, latch, data);
        break;
    }

    // LUÔN TRẢ VỀ FALSE ĐỂ TRÁNH GHI ĐÈ ROM
    return false;
}

std::string Mapper_022::GetDebugInfo()
{
    std::string s = Mapper_021::GetDebugInfo();

    // "MAPPER 021" -> "MAPPER 022"
    size_t pos = s.find("MAPPER 021");
    if (pos != std::string::npos) {
        s.replace(pos, 10, "MAPPER 022");
    }

    // Đổi phần mô tả trong tiêu đề: "VRC4 (...) ====="
    size_t a = s.find("VRC4 (");
    if (a != std::string::npos) {
        size_t b = s.find(") =====", a);
        if (b != std::string::npos) {
            s.replace(a, (b + 1) - a, "VRC2a (A0=bit1, A1=bit0, CHR >> 1)");
        }
    }

    // Thêm note
    size_t notePos = s.find("GHI CHU:");
    if (notePos != std::string::npos) {
        size_t endPos = s.find("\n", notePos);
        if (endPos != std::string::npos) {
            s.insert(endPos, "\nVRC2a: khong co IRQ, khong co PRG swap mode, mirroring chi bit 0.");
        }
    }

    return s;
}