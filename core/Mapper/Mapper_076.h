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

// Mapper 76 = NAMCOT-3446 (Megami Tensei: Digital Devil Story).
// PRG giống 206. CHR: 4 bank 2KB, R2..R5 -> $0000/$0800/$1000/$1800; R0/R1 không dùng.
// Thanh ghi 6 bit -> tối đa 64 bank 2KB = 128KB CHR.
class Mapper_076 : public Mapper_206 {
public:
    Mapper_076(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_076() = default;
    std::string GetDebugInfo() override;

protected:
    void updateChrBanks() override;
};
