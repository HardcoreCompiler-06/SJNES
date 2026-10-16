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
#include "Mapper_206.h"

// Mapper 88 = mapper 206 nhưng PPU A12 nối vào CHR-ROM A16:
//   $0000-$0FFF chỉ lấy từ 64KB CHR đầu, $1000-$1FFF chỉ lấy từ 64KB CHR sau.
// Cài đặt: R2..R5 (cửa sổ $1000-$1FFF) OR thêm bit 6 (0x40); R0/R1 giữ trong 0..63.
// CHR <= 64KB thì y hệt 206.
class Mapper_088 : public Mapper_206 {
public:
    Mapper_088(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_088() = default;
    std::string GetDebugInfo() override;

protected:
    void updateChrBanks() override;
};