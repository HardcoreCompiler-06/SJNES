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

// Mapper 034 gộp 2 board khác nhau:
//   BNROM   : CHR-RAM 8KB. Ghi vào $8000-$FFFF chọn bank PRG 32KB.
//             (Deadly Towers, ...)
//   NINA-001: CHR-ROM.     $7FFD chọn PRG 32KB, $7FFE/$7FFF chọn CHR 4KB
//             ($0000 và $1000).  (Impossible Mission II, ...)
// Phân biệt bằng cách có CHR-ROM hay không (nCHRBanks != 0 -> NINA-001).
// Mirroring theo header.
class Mapper_034 : public Mapper {
public:
    Mapper_034(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_034() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;
    std::string GetDebugInfo() override;

private:
    bool     bNINA;          // true = NINA-001 (có CHR-ROM), false = BNROM
    uint8_t  nPRGBank32;     // bank PRG 32KB đang chọn
    uint8_t  nCHRBank4[2];   // NINA-001: 2 bank CHR 4KB
};