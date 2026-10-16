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

#include "Mapper_011.h"

Mapper_011::Mapper_011(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
}

void Mapper_011::reset()
{
    nPRGBank = 0;
    nCHRBank = 0;
}

bool Mapper_011::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000)
    {
        mapped_addr =
            (uint32_t)nPRGBank * 0x8000 +
            (addr & 0x7FFF);

        return true;
    }

    return false;
}

bool Mapper_011::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    if (addr >= 0x8000)
    {
        nPRGBank = data & 0x03;
        nCHRBank = (data >> 4) & 0x0F;

        return false;
    }

    return false;
}

bool Mapper_011::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        mapped_addr =
            (uint32_t)nCHRBank * 0x2000 +
            addr;

        return true;
    }

    return false;
}

bool Mapper_011::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (nCHRBanks == 0)
    {
        mapped_addr =
            (uint32_t)nCHRBank * 0x2000 +
            addr;

        return true;
    }

    return false;
}