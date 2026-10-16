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

class Mapper_044 : public Mapper_004 {
public:
    Mapper_044(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_044();

    void reset() override;

    // Chỉ cần chặn các hàm Map Read/Write, toàn bộ IRQ để nguyên cho Mapper_004 lo!
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;

private:
    uint8_t prg_bank_offset = 0;
    uint8_t chr_bank_offset = 0;
};