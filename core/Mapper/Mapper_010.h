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

// Mapper 010 = MMC4 (Fire Emblem, Fire Emblem Gaiden, Famicom Wars - bản Nhật).
// Giống MMC2 (mapper 9) nhưng:
//   - PRG: 16KB đổi bank được ở $8000-$BFFF, 16KB cuối cố định ở $C000-$FFFF
//   - CHR latch kích hoạt khi PPU đọc dải 8 byte: $0FD8-$0FDF, $0FE8-$0FEF,
//     $1FD8-$1FDF, $1FE8-$1FEF
//   - Có PRG-RAM 8KB ở $6000-$7FFF (Cartridge xử lý)
//
// Thanh ghi:
//   $A000-$AFFF : PRG bank 16KB (4 bit)
//   $B000-$BFFF : CHR $0000-$0FFF khi latch 0 = $FD
//   $C000-$CFFF : CHR $0000-$0FFF khi latch 0 = $FE
//   $D000-$DFFF : CHR $1000-$1FFF khi latch 1 = $FD
//   $E000-$EFFF : CHR $1000-$1FFF khi latch 1 = $FE
//   $F000-$FFFF : mirroring (0 = vertical, 1 = horizontal)
class Mapper_010 : public Mapper {
public:
    Mapper_010(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_010() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;
    MIRROR mirror() override;
    std::string GetDebugInfo() override;

private:
    uint8_t nPRGBank;          // bank PRG 16KB tại $8000
    uint8_t nCHRBank[2][2];    // [nửa 0/1][latch 0 = FD, 1 = FE], đơn vị 4KB
    uint8_t nLatch[2];         // latch hiện tại của 2 nửa pattern table
    MIRROR  mirrormode;
};