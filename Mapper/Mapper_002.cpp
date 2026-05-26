#include "Mapper_002.h"

Mapper_002::Mapper_002(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset(); // Gọi reset ngay lúc khởi tạo
}

Mapper_002::~Mapper_002() {}

void Mapper_002::reset() {
    nPRGBankSelectLo = 0;
}

bool Mapper_002::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        mapped_addr = nPRGBankSelectLo * 16384 + (addr & 0x3FFF);
        return true;
    }
    else if (addr >= 0xC000 && addr <= 0xFFFF) {
        mapped_addr = (nPRGBanks - 1) * 16384 + (addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_002::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        nPRGBankSelectLo = data & 0xFF;
        if (nPRGBanks > 0) {
            nPRGBankSelectLo %= nPRGBanks;
        }
    }
    return false;
}

bool Mapper_002::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        return false; // Trả về false để PPU tự động dùng CHR-RAM nội bộ!
    }
    return false;
}

bool Mapper_002::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        return false; // Trả về false để PPU tự động ghi vào CHR-RAM nội bộ!
    }
    return false;
}

QString Mapper_002::GetDebugInfo()
{
    QString s;

    s += "===== MAPPER 002 - UXROM =====\n\n";

    s += "Mapper này dùng PRG bank switching.\n";
    s += "Vùng CPU $8000-$BFFF là bank có thể đổi.\n";
    s += "Vùng CPU $C000-$FFFF thường là bank cuối cố định.\n\n";

    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);

    uint32_t map8000 = 0;
    uint32_t mapC000 = 0;

    bool ok8000 = cpuMapRead(0x8000, map8000);
    bool okC000 = cpuMapRead(0xC000, mapC000);

    s += "\nPRG BANK HIỆN TẠI:\n";

    if (ok8000)
    {
        s += QString("$8000-$BFFF : đang trỏ tới PRG bank %1 | offset ROM = 0x%2\n")
            .arg(map8000 / 0x4000)
            .arg(map8000, 6, 16, QChar('0'))
            .toUpper();
    }
    else
    {
        s += "$8000-$BFFF : không map được\n";
    }

    if (okC000)
    {
        s += QString("$C000-$FFFF : đang trỏ tới PRG bank %1 | offset ROM = 0x%2\n")
            .arg(mapC000 / 0x4000)
            .arg(mapC000, 6, 16, QChar('0'))
            .toUpper();
    }
    else
    {
        s += "$C000-$FFFF : không map được\n";
    }

    s += "\nCHR:\n";

    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM\n";
    }
    else
    {
        uint32_t map0000 = 0;
        if (ppuMapRead(0x0000, map0000))
        {
            s += QString("$0000-$1FFF : CHR ROM | offset = 0x%1\n")
                .arg(map0000, 6, 16, QChar('0'))
                .toUpper();
        }
        else
        {
            s += "$0000-$1FFF : CHR ROM/RAM cố định\n";
        }
    }

    return s;
}