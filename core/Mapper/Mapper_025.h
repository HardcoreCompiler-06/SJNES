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

// Mapper 025 gộp VRC4b, VRC4d (và VRC2c). Dùng lại toàn bộ logic thanh ghi
// của Mapper_021 (writeReg), chỉ khác cách decode latch từ CPU address:
//   VRC4b: A0 = CPU bit 1, A1 = CPU bit 0
//   VRC4d: A0 = CPU bit 3, A1 = CPU bit 2
class Mapper_025 : public Mapper_021 {
public:
    Mapper_025(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_025();

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    std::string GetDebugInfo() override;
};