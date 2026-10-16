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

#include "Mapper_070.h"
Mapper_070::Mapper_070(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
}

void Mapper_070::reset()
{
    nPRGBank = 0;
    nCHRBank = 0;
    mirrormode = HORIZONTAL;
}

MIRROR Mapper_070::mirror()
{
    return mirrormode;
}

bool Mapper_070::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000 && addr <= 0xBFFF)
    {
        mapped_addr =
            (uint32_t)nPRGBank * 0x4000 +
            (addr & 0x3FFF);

        return true;
    }

    if (addr >= 0xC000)
    {
        mapped_addr =
            ((uint32_t)nPRGBanks - 1) * 0x4000 +
            (addr & 0x3FFF);

        return true;
    }

    return false;
}

bool Mapper_070::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    if (addr >= 0x8000)
    {
        nCHRBank = data & 0x0F;
        nPRGBank = (data >> 4) & 0x07;

        nPRGBank %= std::max(1, (int)nPRGBanks);
        nCHRBank %= std::max(1, (int)nCHRBanks);

        mirrormode =
            (data & 0x80)
            ? ONESCREEN_HI
            : ONESCREEN_LO;

        return true;
    }
    return false;
}

bool Mapper_070::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        mapped_addr =
            (uint32_t)nCHRBank * 0x2000 +
            addr;

        static uint8_t lastBank = 255;

        if (lastBank != nCHRBank)
        {
            lastBank = nCHRBank;
        }

        return true;
    }

    return false;
}

bool Mapper_070::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
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