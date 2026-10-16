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

#include "Mapper_023.h"

Mapper_023::Mapper_023(uint8_t prgBanks, uint8_t chrBanks) : Mapper_021(prgBanks, chrBanks) {}

Mapper_023::~Mapper_023() {}

bool Mapper_023::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr < 0x8000) return false;

    // Mapper 23 gộp VRC2b, VRC4e, VRC4f, khác nhau ở wiring chân A0/A1 của chip:
    //   VRC2b / VRC4f: A0 = CPU bit 0, A1 = CPU bit 1
    //   VRC4e        : A0 = CPU bit 2, A1 = CPU bit 3
    // iNES 1.0 không phân biệt được nên OR cả 2 kiểu.
    uint8_t latch = (addr & 1) | (((addr >> 1) & 1) << 1)          // VRC2b / VRC4f
        | ((addr >> 2) & 1) | (((addr >> 3) & 1) << 1);  // VRC4e

    writeReg(addr & 0xF000, latch, data);

    // LUÔN TRẢ VỀ FALSE ĐỂ TRÁNH GHI ĐÈ ROM
    return false;
}

std::string Mapper_023::GetDebugInfo()
{
    // Lấy debug info từ parent (Mapper_021), chỉ đổi title
    std::string s = Mapper_021::GetDebugInfo();

    // "MAPPER 021" -> "MAPPER 023"
    size_t pos = s.find("MAPPER 021");
    if (pos != std::string::npos) {
        s.replace(pos, 10, "MAPPER 023");
    }

    // Đổi phần mô tả variant trong tiêu đề: "VRC4 (...) ====="
    size_t a = s.find("VRC4 (");
    if (a != std::string::npos) {
        size_t b = s.find(") =====", a);
        if (b != std::string::npos) {
            s.replace(a, (b + 1) - a, "VRC2b/VRC4 (decode OR ca 2b/4f + 4e)");
        }
    }

    // Thêm note về wiring
    size_t notePos = s.find("GHI CHU:");
    if (notePos != std::string::npos) {
        size_t endPos = s.find("\n", notePos);
        if (endPos != std::string::npos) {
            s.insert(endPos, "\nWiring: 2b/4f (A0=bit0, A1=bit1), 4e (A0=bit2, A1=bit3).");
        }
    }

    return s;
}