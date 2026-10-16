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

// Mapper 33 = Taito TC0190 (không có IRQ).
//   Range $8000-$BFFF, mask $A003:
//   $8000 [.MPP PPPP] M = mirroring (0 = dọc, 1 = ngang), P = PRG reg 0 (8KB @ $8000)
//   $8001 [..PP PPPP] PRG reg 1 (8KB @ $A000)
//   $8002 [CCCC CCCC] CHR reg 0 (2KB @ $0000)   <- KHÔNG bỏ bit thấp như MMC3: giá trị = số bank 2KB
//   $8003 [CCCC CCCC] CHR reg 1 (2KB @ $0800)
//   $A000..$A003      CHR reg 2..5 (1KB @ $1000/$1400/$1800/$1C00)
//   $C000 = bank áp chót, $E000 = bank cuối (cố định)
class Mapper_033 : public Mapper {
public:
    Mapper_033(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_033() override = default;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    MIRROR mirror() override { return currentMirror; }
    void reset() override;
    std::string GetDebugInfo() override;

protected:
    virtual void updateBanks();
    // r = địa chỉ đã mask ($8000-$8003 hoặc $A000-$A003)
    void writeBankRegister(uint16_t r, uint8_t data);

    // 33: bit 6 của $8000 điều khiển mirroring. 48 (TC0690) chuyển bit này sang $E000 nên tắt cờ này.
    bool bMirrorAtReg0 = true;

    uint8_t  nPrgReg[2] = { 0, 1 };
    uint8_t  nChrReg[6] = { 0, 1, 4, 5, 6, 7 };   // R0/R1 theo đơn vị 2KB, R2..R5 theo đơn vị 1KB
    uint32_t pPRGBank[4] = { 0 };                 // 4 x 8KB
    uint32_t pCHRBank[8] = { 0 };                 // 8 x 1KB
    MIRROR   currentMirror = MIRROR::HARDWARE;    // theo header cho tới khi game ghi
};