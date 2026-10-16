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

class Mapper_015 : public Mapper
{
public:
    Mapper_015(uint8_t prgBanks, uint8_t chrBanks);
    void reset() override;
public:
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    bool IsHorizontalMirror() const { return mirrorHorizontal; }

    // Cartridge::cpuWrite() đọc mirror() của mapper sau mỗi lần ghi để cập nhật
    // mirroring cho PPU. Không override thì lấy mặc định (HARDWARE) -> bit M
    // (D6) của thanh ghi bị bỏ qua, nametable dùng mirroring của header.
    MIRROR mirror() override
    {
        return mirrorHorizontal ? HORIZONTAL : VERTICAL;
    }

private:
    uint8_t prgBank = 0;
    uint8_t mode = 0;
    bool prgA13 = false;
    bool mirrorHorizontal = false;

private:
    uint32_t Map16K(uint32_t bank, uint16_t addr) const;
    uint32_t Map8K(uint32_t bank, uint16_t addr) const;
};