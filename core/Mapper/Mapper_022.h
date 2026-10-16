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

// Mapper 022 = VRC2a (Ganbare Pennant Race, TwinBee 3).
// Dùng lại logic thanh ghi của Mapper_021 (writeReg), khác ở:
//   - Wiring: A0 = CPU bit 1, A1 = CPU bit 0
//   - Giá trị bank CHR bị dịch phải 1 bit (bit thấp bị bỏ qua)
//   - VRC2 không có IRQ, không có PRG swap mode
//   - Mirroring chỉ dùng bit 0 (0 = vertical, 1 = horizontal)
//   - Nibble cao của CHR chỉ 4 bit
class Mapper_022 : public Mapper_021 {
public:
    Mapper_022(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_022() override;

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    std::string GetDebugInfo() override;
};