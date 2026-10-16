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

#include "Mapper_206.h"
#include "LogBuffer.h"

Mapper_206::Mapper_206(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_206::~Mapper_206() {}

void Mapper_206::reset() {
    nRegSelect = 0;
    // Giá trị khởi tạo an toàn: R0/R1 = 0/2, R2..R5 = 4..7, R6/R7 = 0/1
    nReg[0] = 0; nReg[1] = 2;
    nReg[2] = 4; nReg[3] = 5; nReg[4] = 6; nReg[5] = 7;
    nReg[6] = 0; nReg[7] = 1;
    updateBanks();
}

void Mapper_206::updateBanks() {
    updatePrgBanks();
    updateChrBanks();
}

void Mapper_206::updatePrgBanks() {
    uint32_t nPrg = (uint32_t)nPRGBanks * 2;   // số bank 8KB
    if (nPrg < 2) nPrg = 2;

    pPRGBank[0] = ((nReg[6] & 0x0F) % nPrg) * 0x2000;
    pPRGBank[1] = ((nReg[7] & 0x0F) % nPrg) * 0x2000;
    pPRGBank[2] = (nPrg - 2) * 0x2000;
    pPRGBank[3] = (nPrg - 1) * 0x2000;
}

void Mapper_206::setChr1K(int slot, uint32_t bank1K) {
    uint32_t nChr = (uint32_t)nCHRBanks * 8;   // số bank 1KB
    if (nChr == 0) nChr = 8;
    pCHRBank[slot] = (bank1K % nChr) * 0x0400;
}

void Mapper_206::updateChrBanks() {
    // CHR-ROM: R0/R1 là 2KB (bỏ bit thấp), R2..R5 là 1KB. Mask 6 bit như Namco 108.
    if (nCHRBanks != 0) {
        uint32_t r0 = nReg[0] & 0x3E;
        uint32_t r1 = nReg[1] & 0x3E;
        setChr1K(0, r0);
        setChr1K(1, r0 + 1);
        setChr1K(2, r1);
        setChr1K(3, r1 + 1);
        setChr1K(4, nReg[2] & 0x3F);
        setChr1K(5, nReg[3] & 0x3F);
        setChr1K(6, nReg[4] & 0x3F);
        setChr1K(7, nReg[5] & 0x3F);
    }
    else {
        for (int i = 0; i < 8; i++) pCHRBank[i] = i * 0x0400;
    }
}

bool Mapper_206::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) { mapped_addr = pPRGBank[0] + (addr & 0x1FFF); return true; }
    if (addr >= 0xA000 && addr <= 0xBFFF) { mapped_addr = pPRGBank[1] + (addr & 0x1FFF); return true; }
    if (addr >= 0xC000 && addr <= 0xDFFF) { mapped_addr = pPRGBank[2] + (addr & 0x1FFF); return true; }
    if (addr >= 0xE000 && addr <= 0xFFFF) { mapped_addr = pPRGBank[3] + (addr & 0x1FFF); return true; }
    return false;
}

bool Mapper_206::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    // Chỉ có 2 thanh ghi ở $8000-$9FFF, phân biệt bằng bit 0 của địa chỉ
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if ((addr & 1) == 0) {
            nRegSelect = data & 0x07;
        }
        else {
            nReg[nRegSelect] = data;
            updateBanks();
        }
    }
    return false;
}

bool Mapper_206::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        mapped_addr = pCHRBank[addr / 0x0400] + (addr & 0x03FF);
        return true;
    }
    return false;
}

bool Mapper_206::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000 && nCHRBanks == 0) {   // CHR-RAM
        mapped_addr = addr;
        return true;
    }
    return false;
}

std::string Mapper_206::GetDebugInfo()
{
    std::string s;
    s += "===== MAPPER 206 - Namco 108 / DxROM =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So PRG banks 8KB  : " + std::to_string(nPRGBanks * 2) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "Bank select       : " + std::to_string(nRegSelect) + "\n";

    s += "\nTHANH GHI R0..R7:\n";
    for (int i = 0; i < 8; i++)
        s += "R" + std::to_string(i) + " = " + std::to_string(nReg[i]) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";
    const char* prgRange[4] = { "$8000-$9FFF", "$A000-$BFFF", "$C000-$DFFF", "$E000-$FFFF" };
    for (int i = 0; i < 4; i++)
        s += std::string(prgRange[i]) + " : offset ROM = 0x" + HexStr(pPRGBank[i], 6) +
        " | PRG bank 8KB = " + std::to_string(pPRGBank[i] / 0x2000) + "\n";

    s += "\nCHR BANK HIEN TAI:\n";
    for (int i = 0; i < 8; i++) {
        uint16_t start = i * 0x0400;
        s += "$" + HexStr(start, 4) + "-$" + HexStr(start + 0x03FF, 4) +
            " : offset CHR=0x" + HexStr(pCHRBank[i], 6) +
            " | CHR bank 1KB=" + std::to_string(pCHRBank[i] / 0x0400) + "\n";
    }

    s += "\nGHI CHU:\n";
    s += "Mapper 206: MMC3 rut gon, khong IRQ, mirroring theo header.\n";
    return s;
}