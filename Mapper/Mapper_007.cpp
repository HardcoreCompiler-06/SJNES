#include "Mapper_007.h"

Mapper_007::Mapper_007(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_007::~Mapper_007()
{}

void Mapper_007::reset()
{
    // Mapper 7 / AxROM reset mặc định chọn bank 0
    nPRGBankSelect = 0x00;

    // AxROM dùng one-screen mirroring
    mapper_mirror = MIRROR::ONESCREEN_LO;
}

bool Mapper_007::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000 && addr <= 0xFFFF)
    {
        // Mapper 7 switch PRG theo khối 32KB.
        // nPRGBanks trong emulator của bạn là số bank 16KB,
        // nên số bank 32KB = nPRGBanks / 2.
        uint8_t prg32Banks = nPRGBanks / 2;

        if (prg32Banks == 0)
            prg32Banks = 1;

        uint8_t bank = nPRGBankSelect % prg32Banks;

        mapped_addr = bank * 0x8000 + (addr & 0x7FFF);
        return true;
    }

    return false;
}

bool Mapper_007::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    if (addr >= 0x8000 && addr <= 0xFFFF)
    {
        // Bit 0-2: chọn PRG bank 32KB
        uint8_t prg32Banks = nPRGBanks / 2;

        if (prg32Banks == 0)
            prg32Banks = 1;

        nPRGBankSelect = (data & 0x07) % prg32Banks;

        // Bit 4: one-screen mirroring
        if (data & 0x10)
            mapper_mirror = MIRROR::ONESCREEN_HI;
        else
            mapper_mirror = MIRROR::ONESCREEN_LO;

        // Đây là ghi vào thanh ghi mapper, không phải ghi vào PRG ROM.
        // Vì vậy không cần Cartridge ghi data vào vPRGMemory.
        mapped_addr = 0;
        return false;
    }

    return false;
}

bool Mapper_007::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x0000 && addr <= 0x1FFF)
    {
        // Mapper 7 thường dùng CHR RAM 8KB.
        // Nếu ROM có CHR ROM thì vẫn map thẳng 8KB đầu.
        mapped_addr = addr;
        return true;
    }

    return false;
}

bool Mapper_007::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x0000 && addr <= 0x1FFF)
    {
        mapped_addr = addr;

        // Chỉ cho ghi nếu băng dùng CHR RAM.
        // CHR RAM khi header chrBanks == 0.
        return nCHRBanks == 0;
    }

    return false;
}

MIRROR Mapper_007::mirror()
{
    return mapper_mirror;
}