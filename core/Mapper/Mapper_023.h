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

#pragma once
#include "Mapper_021.h"

// Mapper 023 gộp VRC2b, VRC4e và VRC4f. Dùng lại toàn bộ logic thanh ghi
// (PRG/CHR/mirroring/swap mode/IRQ) của Mapper_021 (writeReg), chỉ khác cách
// decode latch từ CPU address:
//   VRC2b / VRC4f: A0 = CPU bit 0, A1 = CPU bit 1
//   VRC4e        : A0 = CPU bit 2, A1 = CPU bit 3
class Mapper_023 : public Mapper_021 {
public:
    Mapper_023(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_023() override;

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    std::string GetDebugInfo() override;
};