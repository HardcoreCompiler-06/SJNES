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

#include "Mapper_088.h"

Mapper_088::Mapper_088(uint8_t prgBanks, uint8_t chrBanks) : Mapper_206(prgBanks, chrBanks) {
    // Constructor của 206 chỉ gọi updateBanks() bản 206 -> gọi lại để bản 88 có hiệu lực
    reset();
}

void Mapper_088::updateChrBanks() {
    Mapper_206::updateChrBanks();   // cách ghép chuẩn của 206 (R0/R1 đã nằm trong 0..63)

    // CHR > 64KB: nửa phải ($1000-$1FFF) luôn lấy từ 64KB sau
    if (nCHRBanks > 8) {
        for (int i = 0; i < 4; i++)
            setChr1K(4 + i, (nReg[2 + i] & 0x3F) | 0x40);
    }
}

std::string Mapper_088::GetDebugInfo() {
    return "===== MAPPER 088 - Namco 118 (CHR A12 -> A16) =====\n"
        "Nua trai $0xxx = 64KB CHR dau, nua phai $1xxx = 64KB CHR sau.\n\n"
        + Mapper_206::GetDebugInfo();
}