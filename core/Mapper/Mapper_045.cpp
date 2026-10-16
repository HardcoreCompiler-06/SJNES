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

#include "Mapper_045.h"

Mapper_045::Mapper_045(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper_004(prgBanks, chrBanks) {
    reset();
}

Mapper_045::~Mapper_045() {}

void Mapper_045::reset() {
    Mapper_004::reset();
    regs[0] = regs[1] = regs[2] = regs[3] = 0;
    regIndex = 0;
}


bool Mapper_045::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (!(regs[3] & 0x40)) {
            regs[regIndex] = data;
            regIndex = (regIndex + 1) & 3;
        }
        return false;
    }
    return Mapper_004::cpuMapWrite(addr, mapped_addr, data);
}

bool Mapper_045::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (Mapper_004::cpuMapRead(addr, mapped_addr)) {
        uint32_t bank = (mapped_addr / 0x2000) & (0x3F ^ (regs[3] & 0x3F));
        bank |= regs[1];
        mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }
    return false;
}

bool Mapper_045::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (Mapper_004::ppuMapRead(addr, mapped_addr)) {
        uint32_t chrOr = regs[0] | ((regs[2] & 0xF0) << 4);
        uint32_t chrAnd = 0xFF >> (~regs[2] & 0x0F);
        uint32_t bank = ((mapped_addr / 0x400) & chrAnd) | chrOr;
        mapped_addr = bank * 0x400 + (addr & 0x3FF);
        return true;
    }
    return false;
}