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
#include "Mapper_088.h"

// Mapper 154 = NAMCOT-3453 (Devil Man) = mapper 88 + 1 bit điều khiển one-screen mirroring:
//   $8000-$FFFF (toàn bộ 32KB, kể cả địa chỉ chẵn/lẻ): bit 6 = 0 -> 1ScA (thấp), 1 -> 1ScB (cao)
class Mapper_154 : public Mapper_088 {
public:
    Mapper_154(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_154() = default;

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    MIRROR mirror() override { return currentMirror; }
    void reset() override;
    std::string GetDebugInfo() override;

private:
    MIRROR currentMirror = MIRROR::ONESCREEN_LO;
};