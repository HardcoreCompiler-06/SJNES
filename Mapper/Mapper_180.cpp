#include "Mapper_180.h"

Mapper_180::Mapper_180(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
}

void Mapper_180::reset()
{
    nPRGBank = 0;
}

bool Mapper_180::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000 && addr <= 0xBFFF)
    {
        mapped_addr = addr & 0x3FFF;
        return true;
    }

    if (addr >= 0xC000)
    {
        mapped_addr =
            (uint32_t)nPRGBank * 0x4000 +
            (addr & 0x3FFF);

        return true;
    }

    return false;
}

bool Mapper_180::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    if (addr >= 0x8000)
    {
        nPRGBank = data & 0x07;

        if (nPRGBanks)
            nPRGBank %= nPRGBanks;
    }

    return false;
}

bool Mapper_180::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        mapped_addr = addr;
        return true;
    }

    return false;
}

bool Mapper_180::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF && nCHRBanks == 0)
    {
        mapped_addr = addr;
        return true;
    }

    return false;
}