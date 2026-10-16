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

// Mapper 206 = Namco 108 / DxROM: MMC3 rút gọn.
//   $8000-$9FFF (chẵn): Bank select (3 bit thấp)
//   $8000-$9FFF (lẻ)  : Bank data
// Không có IRQ, không có điều khiển mirroring (dùng mirroring của header),
// không có PRG mode / CHR mode, không có PRG-RAM protect.
//   CHR: R0, R1 = 2KB ($0000, $0800); R2..R5 = 1KB ($1000..$1FFF)
//   PRG: R6 = $8000, R7 = $A000, $C000 = áp chót, $E000 = cuối
class Mapper_206 : public Mapper {
public:
    Mapper_206(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_206() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;
    std::string GetDebugInfo() override;

protected:
    // Tính lại toàn bộ bank. Mapper kế thừa (76/88/95/154) override updateBanks()
    // hoặc updateChrBanks() để đổi cách ghép bank / mirroring.
    virtual void updateBanks();
    void updatePrgBanks();
    virtual void updateChrBanks();
    // Gán 1 cửa sổ CHR 1KB (slot 0..7) = bank 1KB số bank1K (có wrap theo kích thước CHR-ROM)
    void setChr1K(int slot, uint32_t bank1K);

    uint8_t  nRegSelect;
    uint8_t  nReg[8];
    uint32_t pPRGBank[4];   // 4 x 8KB
    uint32_t pCHRBank[8];   // 8 x 1KB
};