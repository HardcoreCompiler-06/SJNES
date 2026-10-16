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

#include "Mapper_095.h"

Mapper_095::Mapper_095(uint8_t prgBanks, uint8_t chrBanks) : Mapper_206(prgBanks, chrBanks) {
    reset();   // áp dụng updateBanks() của 95 (tính mirroring)
}

// Được gọi sau mỗi lần ghi bank data và khi reset -> tính lại mirroring từ bit 5 của R0/R1
void Mapper_095::updateBanks() {
    Mapper_206::updateBanks();

    bool nt01 = (nReg[0] & 0x20) != 0;   // CIRAM A10 cho $2000-$27FF
    bool nt23 = (nReg[1] & 0x20) != 0;   // CIRAM A10 cho $2800-$2FFF

    if (!nt01 && !nt23)      currentMirror = MIRROR::ONESCREEN_LO;
    else if (nt01 && nt23)   currentMirror = MIRROR::ONESCREEN_HI;
    else                     currentMirror = MIRROR::HORIZONTAL;   // (0,1) hoặc (1,0): hai trang hoán đổi vẫn là ngang
}

std::string Mapper_095::GetDebugInfo() {
    std::string m;
    switch (currentMirror) {
    case MIRROR::ONESCREEN_LO: m = "One-screen thap"; break;
    case MIRROR::ONESCREEN_HI: m = "One-screen cao";  break;
    case MIRROR::HORIZONTAL:   m = "Horizontal / Ngang"; break;
    default:                   m = "Khac"; break;
    }
    return "===== MAPPER 095 - NAMCOT-3425 (CHR A15 -> CIRAM A10) =====\n"
        "Mirroring (tu bit 5 cua R0/R1): " + m + "\n\n"
        + Mapper_206::GetDebugInfo();
}