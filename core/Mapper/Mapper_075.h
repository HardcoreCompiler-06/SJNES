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
#include "Mapper.h"
#include <QString>

class Mapper_075 : public Mapper {
private:
    uint8_t prg_bank[4];
    uint8_t chr_bank[2];
    uint8_t chr_bit_high_0;
    uint8_t chr_bit_high_1;
    uint8_t mirroring_mode;

public:
    Mapper_075(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_075();

    // Khớp chuẩn xác từng tham số theo đúng khuôn Mapper.h của nhóm
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    MIRROR mirror() override; // Ghi đè hàm mirror để xử lý cuộn màn hình tự động
    std::string GetDebugInfo() override;
};