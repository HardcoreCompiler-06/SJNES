#include "Mapper_003.h"

Mapper_003::Mapper_003(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_003::~Mapper_003() {}

bool Mapper_003::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // Mapper 3 không tráo PRG. 
        // Nếu game nhẹ (1 cuộn PRG - 16KB) thì nó tự soi gương (mirror) lặp lại.
        // Nếu game nặng (2 cuộn PRG - 32KB) thì lấy nguyên dải 32KB.
        if (nPRGBanks == 1) {
            mapped_addr = addr & 0x3FFF;
        }
        else {
            mapped_addr = addr & 0x7FFF;
        }
        return true;
    }
    return false;
}

bool Mapper_003::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // CHÌA KHÓA CỦA MAPPER 3: Ghi vào CPU để đổi cuộn băng hình (CHR)
        // Thường CNROM chỉ dùng 2 bit cuối của data để chọn 4 bank CHR (0-3)
        nCHRBankSelect = data & 0x03;
        if (nCHRBanks > 0) {
            nCHRBankSelect %= nCHRBanks;
        }

        // Vẫn trả về false để báo là KHÔNG ghi đè lên ROM
        return false;
    }
    return false;
}

bool Mapper_003::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        // PPU đòi đọc hình, Mapper sẽ trỏ tới đúng cái Bank CHR đang được chọn
        mapped_addr = (nCHRBankSelect * 0x2000) + addr;
        return true;
    }
    return false;
}

bool Mapper_003::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    // Mapper 3 thường là băng ROM (không cho ghi đè hình)
    return false;
}

void Mapper_003::reset() {
    nCHRBankSelect = 0;
}
QString Mapper_003::GetDebugInfo()
{
    QString s;

    s += "===== MAPPER 003 - CNROM =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += "Mapper này không đổi bank PRG.\n";
    s += "Mapper này chỉ đổi bank CHR 8KB cho PPU.\n\n";

    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);

    s += "\nPRG BANK HIỆN TẠI:\n";

    if (nPRGBanks == 1)
    {
        s += "$8000-$BFFF : PRG bank 0\n";
        s += "$C000-$FFFF : mirror PRG bank 0\n";
    }
    else
    {
        s += "$8000-$FFFF : PRG 32KB cố định\n";
        s += "$8000-$BFFF : PRG bank 0\n";
        s += "$C000-$FFFF : PRG bank 1\n";
    }

    s += "\nCHR BANK HIỆN TẠI:\n";

    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : không có CHR ROM / có thể là CHR RAM\n";
    }
    else
    {
        s += QString("$0000-$1FFF : đang trỏ tới CHR bank 8KB số %1\n")
            .arg(nCHRBankSelect);

        s += QString("Offset CHR ROM     : 0x%1\n")
            .arg(nCHRBankSelect * 0x2000, 6, 16, QChar('0'))
            .toUpper();
    }

    s += "\nTHANH GHI MAPPER:\n";
    s += QString("nCHRBankSelect     : %1\n").arg(nCHRBankSelect);

    s += "\nGHI CHÚ:\n";
    s += "CPU ghi vào vùng $8000-$FFFF để chọn bank CHR.\n";
    s += "CNROM thường dùng 2 bit thấp của data để chọn tối đa 4 bank CHR.\n";
    s += "PPU đọc $0000-$1FFF sẽ lấy dữ liệu từ CHR bank đang chọn.\n";

    return s;
}