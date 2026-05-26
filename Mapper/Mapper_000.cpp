#include "Mapper_000.h"

Mapper_000::Mapper_000(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {}

Mapper_000::~Mapper_000() {}

bool Mapper_000::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_000::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_000::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        mapped_addr = addr;
        return true;
    }
    return false;
}

bool Mapper_000::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        if (nCHRBanks == 0) {
            mapped_addr = addr;
            return true;
        }
    }
    return false;
}

void Mapper_000::reset() {}

QString Mapper_000::GetDebugInfo()
{
    QString s;

    s += "===== MAPPER 000 - NROM =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += "Mapper này không có bank switching.\n";
    s += "PRG và CHR được map cố định.\n\n";

    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);

    s += "\nPRG BANK HIỆN TẠI:\n";

    if (nPRGBanks > 1)
    {
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : PRG bank 1 | offset ROM = 0x004000\n";
    }
    else
    {
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : mirror lại PRG bank 0 | offset ROM = 0x000000\n";
    }

    s += "\nCHR BANK HIỆN TẠI:\n";

    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM 8KB, PPU có thể ghi\n";
    }
    else
    {
        s += "$0000-$1FFF : CHR ROM bank 0, map thẳng\n";
    }

    s += "\nMAPPING CPU:\n";
    s += "CPU $8000-$FFFF được map bằng addr & 0x7FFF nếu có 32KB PRG.\n";
    s += "Nếu chỉ có 16KB PRG, dùng addr & 0x3FFF để mirror vùng $C000-$FFFF.\n";

    s += "\nMAPPING PPU:\n";
    s += "PPU $0000-$1FFF map thẳng vào CHR ROM/RAM.\n";

    return s;
}