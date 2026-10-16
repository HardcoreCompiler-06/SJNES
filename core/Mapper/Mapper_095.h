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

// Mapper 95 = NAMCOT-3425 (Dragon Buster). Như 206 nhưng CHR A15 (bit 5 của bank) nối vào CIRAM A10:
//   bit 5 của R0 -> nametable $2000-$27FF, bit 5 của R1 -> nametable $2800-$2FFF.
//   (0,0) = one-screen thấp | (1,1) = one-screen cao | khác nhau = ngang (horizontal)
// CHR ROM 32KB của Dragon Buster tự bỏ bit 5 nhờ wrap theo kích thước CHR.
class Mapper_095 : public Mapper_206 {
public:
    Mapper_095(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_095() = default;

    MIRROR mirror() override { return currentMirror; }
    std::string GetDebugInfo() override;

protected:
    void updateBanks() override;

private:
    MIRROR currentMirror = MIRROR::ONESCREEN_LO;
};