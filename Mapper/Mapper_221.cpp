#include "Mapper_221.h"

Mapper_221::Mapper_221(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_221::~Mapper_221() {}

void Mapper_221::reset()
{
    cmd = 0x0000;
    bank = 0x00;
    Sync();
}

void Mapper_221::Sync()
{
    // Mapper 221 / NTDEC N625092:
    // cmd bit 0 = mirroring control
    // FCEUX dùng setmirror((cmd & 1) ^ 1)
    // Trong enum của bạn: HORIZONTAL / VERTICAL
    mirrorMode = (cmd & 0x0001) ? MIRROR::HORIZONTAL : MIRROR::VERTICAL;

    const uint32_t prg16Count = nPRGBanks;
    if (prg16Count == 0)
    {
        prgBank8000 = 0;
        prgBankC000 = 0;
        return;
    }

    // Base chọn nhóm PRG lớn
    uint32_t base = (cmd & 0x00FC) >> 2;

    if (cmd & 0x0002)
    {
        // Mode 16KB switch
        if (cmd & 0x0100)
        {
            // $8000 = base | bank
            // $C000 = base | 7
            prgBank8000 = (base | bank) % prg16Count;
            prgBankC000 = (base | 0x07) % prg16Count;
        }
        else
        {
            // $8000 = base | even bank
            // $C000 = base | odd bank
            prgBank8000 = (base | (bank & 0x06)) % prg16Count;
            prgBankC000 = (base | ((bank & 0x06) | 0x01)) % prg16Count;
        }
    }
    else
    {
        // Mode 32KB nhưng map bằng 2 nửa 16KB giống nhau theo công thức board
        prgBank8000 = (base | bank) % prg16Count;
        prgBankC000 = (base | bank) % prg16Count;
    }
}

bool Mapper_221::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000 && addr <= 0xBFFF)
    {
        mapped_addr = prgBank8000 * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xFFFF)
    {
        mapped_addr = prgBankC000 * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    return false;
}

bool Mapper_221::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    // Mapper này chọn bank bằng ĐỊA CHỈ ghi, data thường không quan trọng
    if (addr >= 0x8000 && addr <= 0xBFFF)
    {
        cmd = addr;
        Sync();
        return false;
    }

    if (addr >= 0xC000 && addr <= 0xFFFF)
    {
        bank = addr & 0x0007;
        Sync();
        return false;
    }

    return false;
}

bool Mapper_221::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        // ROM này header CHR = 0, tức dùng CHR RAM 8KB
        mapped_addr = addr & 0x1FFF;
        return true;
    }

    return false;
}

bool Mapper_221::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        mapped_addr = addr & 0x1FFF;

        // Chỉ cho ghi nếu là CHR RAM
        return nCHRBanks == 0;
    }

    return false;
}

MIRROR Mapper_221::mirror()
{
    return mirrorMode;
}

QString Mapper_221::GetDebugInfo()
{
    QString s;

    auto mirrorToString = [](MIRROR m) -> QString {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Dọc";
        case MIRROR::ONESCREEN_LO: return "One-screen thấp";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        default:                   return "Không rõ";
        }
        };

    uint32_t base = (cmd & 0x00FC) >> 2;

    s += "===== MAPPER 221 - NTDEC N625092 / MULTICART =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += "Mapper này thường gặp ở băng multicart.\n";
    s += "Bank được chọn chủ yếu bằng địa chỉ ghi, data thường không quan trọng.\n\n";

    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);
    s += QString("Mirroring         : %1\n").arg(mirrorToString(mirrorMode));

    s += "\nTHANH GHI MULTICART:\n";
    s += QString("cmd  : 0x%1\n").arg(cmd, 4, 16, QChar('0')).toUpper();
    s += QString("bank : %1\n").arg(bank);
    s += QString("base : %1\n").arg(base);

    s += "\nGIẢI MÃ CMD:\n";
    s += QString("cmd bit 0 - Mirroring      : %1\n").arg(cmd & 0x0001 ? "Horizontal" : "Vertical");
    s += QString("cmd bit 1 - Mode           : %1\n").arg(cmd & 0x0002 ? "16KB switch" : "32KB style");
    s += QString("cmd bit 8 - 16KB sub-mode  : %1\n").arg(cmd & 0x0100 ? "$8000=base|bank, $C000=base|7" : "$8000 even, $C000 odd");

    s += "\nPRG BANK HIỆN TẠI:\n";
    s += QString("$8000-$BFFF : PRG bank 16KB = %1 | offset ROM = 0x%2\n")
        .arg(prgBank8000)
        .arg(prgBank8000 * 0x4000, 6, 16, QChar('0'))
        .toUpper();

    s += QString("$C000-$FFFF : PRG bank 16KB = %1 | offset ROM = 0x%2\n")
        .arg(prgBankC000)
        .arg(prgBankC000 * 0x4000, 6, 16, QChar('0'))
        .toUpper();

    s += "\nCHR MAPPING:\n";
    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM 8KB, PPU có thể ghi\n";
    }
    else
    {
        s += "$0000-$1FFF : CHR ROM/RAM map thẳng\n";
    }

    s += "\nGHI CHÚ:\n";
    s += "Ghi $8000-$BFFF cập nhật cmd.\n";
    s += "Ghi $C000-$FFFF cập nhật bank = addr & 7.\n";
    s += "Sau mỗi lần ghi, Sync() sẽ tính lại PRG bank và mirroring.\n";

    return s;
}