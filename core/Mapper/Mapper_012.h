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
#include "Mapper_004.h"

class Mapper_012 : public Mapper_004 {
public:
    Mapper_012(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_012();

    void reset() override;
    void ClockA12() override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;

private:
    uint8_t chr_low_outer = 0;  // Cờ 256KB cho vùng $0000-$0FFF
    uint8_t chr_high_outer = 0; // Cờ 256KB cho vùng $1000-$1FFF
};