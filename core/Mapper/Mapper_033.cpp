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

#include "Mapper_033.h"

Mapper_033::Mapper_033(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

void Mapper_033::reset() {
    nPrgReg[0] = 0; nPrgReg[1] = 1;
    nChrReg[0] = 0; nChrReg[1] = 1;                       // 2KB: bank 0, 1
    nChrReg[2] = 4; nChrReg[3] = 5; nChrReg[4] = 6; nChrReg[5] = 7;
    currentMirror = MIRROR::HARDWARE;
    updateBanks();
}

void Mapper_033::updateBanks() {
    // ---- PRG (8KB x 4) ----
    uint32_t nPrg = (uint32_t)nPRGBanks * 2;              // số bank 8KB
    if (nPrg < 2) nPrg = 2;
    pPRGBank[0] = (nPrgReg[0] % nPrg) * 0x2000;
    pPRGBank[1] = (nPrgReg[1] % nPrg) * 0x2000;
    pPRGBank[2] = (nPrg - 2) * 0x2000;
    pPRGBank[3] = (nPrg - 1) * 0x2000;

    // ---- CHR ----
    if (nCHRBanks == 0) {                                 // CHR-RAM: identity
        for (int i = 0; i < 8; i++) pCHRBank[i] = i * 0x0400;
        return;
    }
    uint32_t n2k = (uint32_t)nCHRBanks * 4;               // số bank 2KB
    uint32_t n1k = (uint32_t)nCHRBanks * 8;               // số bank 1KB
    // R0/R1: giá trị ghi vào = số bank 2KB (không bỏ bit thấp)
    uint32_t b0 = (nChrReg[0] % n2k) * 0x0800;
    uint32_t b1 = (nChrReg[1] % n2k) * 0x0800;
    pCHRBank[0] = b0; pCHRBank[1] = b0 + 0x0400;
    pCHRBank[2] = b1; pCHRBank[3] = b1 + 0x0400;
    for (int i = 0; i < 4; i++)
        pCHRBank[4 + i] = (nChrReg[2 + i] % n1k) * 0x0400;
}

void Mapper_033::writeBankRegister(uint16_t r, uint8_t data) {
    switch (r) {
    case 0x8000:
        nPrgReg[0] = data & 0x3F;
        if (bMirrorAtReg0)
            currentMirror = (data & 0x40) ? MIRROR::HORIZONTAL : MIRROR::VERTICAL;
        break;
    case 0x8001: nPrgReg[1] = data & 0x3F; break;
    case 0x8002: nChrReg[0] = data; break;
    case 0x8003: nChrReg[1] = data; break;
    case 0xA000: case 0xA001: case 0xA002: case 0xA003:
        nChrReg[2 + (r & 3)] = data;
        break;
    default: return;
    }
    updateBanks();
}

bool Mapper_033::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    // Chỉ $8000-$BFFF, mask $A003
    if (addr >= 0x8000 && addr <= 0xBFFF)
        writeBankRegister(addr & 0xA003, data);
    return false;
}

bool Mapper_033::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) { mapped_addr = pPRGBank[0] + (addr & 0x1FFF); return true; }
    if (addr >= 0xA000 && addr <= 0xBFFF) { mapped_addr = pPRGBank[1] + (addr & 0x1FFF); return true; }
    if (addr >= 0xC000 && addr <= 0xDFFF) { mapped_addr = pPRGBank[2] + (addr & 0x1FFF); return true; }
    if (addr >= 0xE000 && addr <= 0xFFFF) { mapped_addr = pPRGBank[3] + (addr & 0x1FFF); return true; }
    return false;
}

bool Mapper_033::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        mapped_addr = pCHRBank[addr / 0x0400] + (addr & 0x03FF);
        return true;
    }
    return false;
}

bool Mapper_033::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000 && nCHRBanks == 0) {   // CHR-RAM
        mapped_addr = addr;
        return true;
    }
    return false;
}

std::string Mapper_033::GetDebugInfo() {
    std::string s;
    s += "===== MAPPER 033 - Taito TC0190 =====\n\n";
    s += "So PRG banks 8KB  : " + std::to_string(nPRGBanks * 2) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";

    const char* m = "Theo header";
    if (currentMirror == MIRROR::HORIZONTAL) m = "Horizontal / Ngang";
    else if (currentMirror == MIRROR::VERTICAL) m = "Vertical / Doc";
    s += std::string("Mirroring         : ") + m + "\n";

    s += "\nTHANH GHI:\n";
    s += "PRG0 / PRG1       : " + std::to_string(nPrgReg[0]) + " / " + std::to_string(nPrgReg[1]) + "\n";
    s += "CHR0 / CHR1 (2KB) : " + std::to_string(nChrReg[0]) + " / " + std::to_string(nChrReg[1]) + "\n";
    s += "CHR2..CHR5 (1KB)  : " + std::to_string(nChrReg[2]) + " " + std::to_string(nChrReg[3]) + " " +
        std::to_string(nChrReg[4]) + " " + std::to_string(nChrReg[5]) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";
    const char* prgRange[4] = { "$8000-$9FFF", "$A000-$BFFF", "$C000-$DFFF", "$E000-$FFFF" };
    for (int i = 0; i < 4; i++)
        s += std::string(prgRange[i]) + " : offset ROM = 0x" + HexStr(pPRGBank[i], 6) +
        " | PRG bank 8KB = " + std::to_string(pPRGBank[i] / 0x2000) + "\n";

    s += "\nCHR BANK HIEN TAI:\n";
    for (int i = 0; i < 8; i++) {
        uint16_t start = i * 0x0400;
        s += "$" + HexStr(start, 4) + "-$" + HexStr(start + 0x03FF, 4) +
            " : offset CHR = 0x" + HexStr(pCHRBank[i], 6) +
            " | CHR bank 1KB = " + std::to_string(pCHRBank[i] / 0x0400) + "\n";
    }
    s += "\nGHI CHU: mapper 33 khong co IRQ. Nhieu ROM co IRQ bi gan nham la 33 (that ra la 48).\n";
    return s;
}