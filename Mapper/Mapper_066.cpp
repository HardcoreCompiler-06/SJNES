#include "Mapper_066.h"

Mapper_066::Mapper_066(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_066::~Mapper_066()
{}

void Mapper_066::reset()
{
    prgBankSelect = 0;
    chrBankSelect = 0;
}

bool Mapper_066::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000 && addr <= 0xFFFF)
    {
        uint32_t prg32Count = nPRGBanks / 2;

        if (prg32Count == 0)
            prg32Count = 1;

        mapped_addr = (prgBankSelect % prg32Count) * 0x8000 + (addr & 0x7FFF);
        return true;
    }

    return false;
}

bool Mapper_066::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    if (addr >= 0x8000 && addr <= 0xFFFF)
    {
        // Mapper 66 / GxROM:
        // bit 4-5: chọn PRG bank 32KB
        // bit 0-1: chọn CHR bank 8KB
        prgBankSelect = (data >> 4) & 0x03;
        chrBankSelect = data & 0x03;

        return false;
    }

    return false;
}

bool Mapper_066::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        if (nCHRBanks == 0)
        {
            // Trường hợp CHR RAM hiếm hoặc homebrew
            mapped_addr = addr & 0x1FFF;
            return true;
        }

        mapped_addr = (chrBankSelect % nCHRBanks) * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    return false;
}

bool Mapper_066::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        if (nCHRBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }
    }

    return false;
}

QString Mapper_066::GetDebugInfo()
{
    QString s;

    uint32_t prg32Count = nPRGBanks / 2;
    if (prg32Count == 0)
        prg32Count = 1;

    s += "===== MAPPER 066 - GXROM / GNROM =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += "Mapper này dùng PRG bank 32KB và CHR bank 8KB.\n";
    s += "CPU ghi vào $8000-$FFFF để chọn cả PRG và CHR bank.\n\n";

    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số PRG banks 32KB : %1\n").arg(prg32Count);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);

    s += "\nTHANH GHI MAPPER:\n";
    s += QString("PRG bank select   : %1\n").arg(prgBankSelect);
    s += QString("CHR bank select   : %1\n").arg(chrBankSelect);
    s += "Bit 4-5 của data  : chọn PRG bank 32KB\n";
    s += "Bit 0-1 của data  : chọn CHR bank 8KB\n";

    s += "\nPRG BANK HIỆN TẠI:\n";
    s += QString("$8000-$FFFF : PRG bank 32KB = %1 | offset ROM = 0x%2\n")
        .arg(prgBankSelect % prg32Count)
        .arg((prgBankSelect % prg32Count) * 0x8000, 6, 16, QChar('0'))
        .toUpper();

    s += "\nCHR BANK HIỆN TẠI:\n";
    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM 8KB\n";
    }
    else
    {
        s += QString("$0000-$1FFF : CHR bank 8KB = %1 | offset CHR = 0x%2\n")
            .arg(chrBankSelect % nCHRBanks)
            .arg((chrBankSelect % nCHRBanks) * 0x2000, 6, 16, QChar('0'))
            .toUpper();
    }

    s += "\nGHI CHÚ:\n";
    s += "Offset PRG nhảy theo 0x8000 vì mỗi PRG bank là 32KB.\n";
    s += "Offset CHR nhảy theo 0x2000 vì mỗi CHR bank là 8KB.\n";

    return s;
}