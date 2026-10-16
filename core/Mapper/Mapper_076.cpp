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

#include "Mapper_076.h"

Mapper_076::Mapper_076(uint8_t prgBanks, uint8_t chrBanks) : Mapper_206(prgBanks, chrBanks) {
    reset();   // áp dụng updateChrBanks() của 76
}

void Mapper_076::updateChrBanks() {
    if (nCHRBanks == 0) {                 // CHR-RAM: giữ identity như 206
        Mapper_206::updateChrBanks();
        return;
    }

    uint32_t n2k = nCHRBanks * 4;         // số bank 2KB (nCHRBanks tính theo 8KB)
    for (int i = 0; i < 4; i++) {
        uint32_t off = ((nReg[2 + i] & 0x3F) % n2k) * 0x0800;
        pCHRBank[i * 2] = off;
        pCHRBank[i * 2 + 1] = off + 0x0400;
    }
}

std::string Mapper_076::GetDebugInfo() {
    return "===== MAPPER 076 - NAMCOT-3446 (Namco 109) =====\n"
        "CHR: 4 bank 2KB, R2..R5 -> $0000/$0800/$1000/$1800 (R0, R1 khong dung).\n\n"
        + Mapper_206::GetDebugInfo();
}