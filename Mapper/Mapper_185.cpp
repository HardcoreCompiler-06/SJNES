#include "Mapper_185.h"

Mapper_185::Mapper_185(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
Mapper_185::~Mapper_185() {}

void Mapper_185::reset() {
    nDummyEnable = 0x21;
}

bool Mapper_185::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_185::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        nDummyEnable = data; // Ghi chép lại lệnh game vừa gửi
    }
    return false;
}

bool Mapper_185::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        mapped_addr = addr & 0x1FFF;
        return true;  // Luôn mở, bỏ qua copy protection
    }
    return false;
}

bool Mapper_185::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) { return false; }
QString Mapper_185::GetDebugInfo()
{
    QString s;

    s += "===== MAPPER 185 =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += "Mapper này thường liên quan tới CHR/copy protection.\n";
    s += "Bản hiện tại luôn mở CHR để game đọc CHR thật.\n\n";

    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);

    s += "\nPRG MAPPING:\n";
    if (nPRGBanks > 1)
    {
        s += "$8000-$FFFF : PRG 32KB cố định\n";
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : PRG bank 1 | offset ROM = 0x004000\n";
    }
    else
    {
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : mirror PRG bank 0 | offset ROM = 0x000000\n";
    }

    s += "\nCHR MAPPING:\n";
    s += "$0000-$1FFF : CHR ROM/RAM map thẳng, luôn mở\n";

    s += "\nTHANH GHI PROTECTION:\n";
    s += QString("nDummyEnable : 0x%1\n")
        .arg(nDummyEnable, 2, 16, QChar('0'))
        .toUpper();

    s += "\nGHI CHÚ:\n";
    s += "Một số game dùng mapper này để kiểm tra CHR. Nếu khóa CHR sai có thể màn hình đen hoặc crash.\n";
    s += "Code hiện tại bỏ qua khóa và luôn cho đọc CHR thật.\n";

    return s;
}