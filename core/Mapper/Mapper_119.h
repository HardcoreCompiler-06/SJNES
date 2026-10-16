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

// Mapper 119 = TQROM (MMC3 + 64KB CHR-ROM + 8KB CHR-RAM).
// Game: High Speed, Pin Bot.
//
// Bit 6 của giá trị bank CHR chọn nguồn:
//   bit 6 = 0 -> CHR-ROM (bank 0..63, MMC3 xử lý như bình thường)
//   bit 6 = 1 -> CHR-RAM (trang 1KB số (bank & 7))
//
// Cartridge phải nối thêm 8KB CHR-RAM vào cuối vCHRMemory cho mapper này
// (giống cách đã làm với mapper 19), để mapper trỏ vào vùng đó.
class Mapper_119 : public Mapper_004 {
public:
    Mapper_119(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_119() override;

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;
    std::string GetDebugInfo() override;

private:
    // Trả về true nếu ô 1KB chứa addr đang trỏ vào CHR-RAM; page = trang RAM 0..7
    bool isRamSlot(uint16_t addr, uint32_t& page) const;

    // Bản sao trạng thái thanh ghi MMC3 để biết bank nào có bit 6
    uint8_t nTarget119;
    bool    bInvert119;
    uint8_t reg119[8];
};