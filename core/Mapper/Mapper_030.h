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
#include "Mapper_002.h" 
class Mapper_030 : public Mapper_002 {
public:
    Mapper_030(uint32_t prgBanks, uint32_t chrBanks);
    ~Mapper_030();

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;

    // Nạp đè hàm Mirror để lấy giá trị lật động
    MIRROR mirror() override;

    std::string GetDebugInfo() override;
    void SaveState(BinaryWriter& out) const override;
    void LoadState(BinaryReader& in) override;

private:
    // Thêm các biến mở rộng so với UXROM
    uint8_t nCHRBankSelect = 0x00;
    MIRROR mirroMode = MIRROR::VERTICAL;
};