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

QString Mapper_007::GetDebugInfo()
{
    QString s;

    auto mirrorToString = [](MIRROR m) -> QString {
        switch (m)
        {
        case MIRROR::ONESCREEN_LO: return "One-screen thấp";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Dọc";
        default:                   return "Hardware / Không rõ";
        }
        };

    uint8_t prg32Banks = nPRGBanks / 2;
    if (prg32Banks == 0)
        prg32Banks = 1;

    uint8_t currentBank = nPRGBankSelect % prg32Banks;
    uint32_t prgOffset = currentBank * 0x8000;

    s += "===== MAPPER 007 - AXROM =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += "Mapper này đổi PRG theo bank 32KB.\n";
    s += "AxROM thường dùng CHR RAM 8KB.\n";
    s += "Mirroring là one-screen, chọn bằng bit 4 khi CPU ghi vào mapper.\n\n";

    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số PRG banks 32KB : %1\n").arg(prg32Banks);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);

    s += "\nPRG BANK HIỆN TẠI:\n";
    s += QString("$8000-$FFFF : đang trỏ tới PRG bank 32KB số %1\n")
        .arg(currentBank);
    s += QString("Offset ROM  : 0x%1\n")
        .arg(prgOffset, 6, 16, QChar('0'))
        .toUpper();

    s += "\nCHR:\n";
    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM 8KB, cho phép PPU ghi\n";
    }
    else
    {
        s += "$0000-$1FFF : CHR ROM 8KB đầu, map thẳng\n";
    }

    s += "\nMIRRORING:\n";
    s += QString("Kiểu mirroring hiện tại : %1\n").arg(mirrorToString(mapper_mirror));

    s += "\nTHANH GHI MAPPER:\n";
    s += QString("nPRGBankSelect : %1\n").arg(nPRGBankSelect);
    s += "Bit 0-2: chọn PRG bank 32KB\n";
    s += "Bit 4  : chọn one-screen mirroring thấp/cao\n";

    s += "\nGHI CHÚ:\n";
    s += "Offset ROM nhảy theo 0x8000 vì mỗi bank PRG của AxROM là 32KB.\n";
    s += "Một số game AxROM dùng mirroring một màn hình để đổi trang nametable.\n";

    return s;
}