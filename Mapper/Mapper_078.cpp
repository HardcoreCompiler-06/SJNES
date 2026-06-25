#include "Mapper_078.h"

Mapper_078::Mapper_078(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
}

void Mapper_078::reset()
{
    nPRGBank = 0;
    nCHRBank = 0;
}

bool Mapper_078::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
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

bool Mapper_078::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    if (addr >= 0x8000)
    {
        nPRGBank = data & 0x0F;
        nCHRBank = (data >> 4) & 0x0F;

        if (nPRGBanks)
            nPRGBank %= nPRGBanks;

        if (nCHRBanks)
            nCHRBank %= nCHRBanks;
    }

    return false;
}

bool Mapper_078::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
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

bool Mapper_078::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF && nCHRBanks == 0)
    {
        mapped_addr =
            (uint32_t)nCHRBank * 0x2000 +
            addr;

        return true;
    }

    return false;
}