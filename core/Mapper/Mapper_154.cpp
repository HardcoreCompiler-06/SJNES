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

#include "Mapper_154.h"

Mapper_154::Mapper_154(uint8_t prgBanks, uint8_t chrBanks) : Mapper_088(prgBanks, chrBanks) {
    reset();
}

void Mapper_154::reset() {
    currentMirror = MIRROR::ONESCREEN_LO;   // power-on: 1ScA
    Mapper_088::reset();
}

bool Mapper_154::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    // Bit mirroring có mặt trên cả $8000-$FFFF, không chỉ vùng thanh ghi bank
    if (addr >= 0x8000)
        currentMirror = (data & 0x40) ? MIRROR::ONESCREEN_HI : MIRROR::ONESCREEN_LO;

    // Phần bank giữ nguyên như 88/206 (chỉ $8000-$9FFF; bit 6 bị 206 bỏ qua nên không ảnh hưởng PRG/CHR)
    return Mapper_088::cpuMapWrite(addr, mapped_addr, data);
}

std::string Mapper_154::GetDebugInfo() {
    return "===== MAPPER 154 - NAMCOT-3453 (Devil Man) =====\n"
        "Mirroring one-screen (bit 6 moi lan ghi $8000-$FFFF): " +
        std::string(currentMirror == MIRROR::ONESCREEN_HI ? "cao (1ScB)" : "thap (1ScA)") + "\n\n"
        + Mapper_088::GetDebugInfo();
}