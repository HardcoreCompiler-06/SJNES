#include "Mapper_009.h"

Mapper_009::Mapper_009(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
Mapper_009::~Mapper_009() {}

void Mapper_009::reset() {
    nPRGBank = 0;
    nCHRBank0_FD8 = 0;
    nCHRBank0_FE8 = 0;
    nCHRBank1_FD8 = 0;
    nCHRBank1_FE8 = 0;
    nLatch0 = 0;
    nLatch1 = 0;
    // Đã sửa lại thành chữ thay vì số 0
    mirromode = HORIZONTAL;
}

// Đã sửa lại thành MIRROR thay vì int
MIRROR Mapper_009::mirror() {
    return mirromode;
}

bool Mapper_009::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mapped_addr = addr & 0x1FFF;
        return true;
    }

    if (addr >= 0x8000 && addr <= 0x9FFF) {
        mapped_addr = (nPRGBank * 0x2000) + (addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xA000 && addr <= 0xBFFF) {
        mapped_addr = ((nPRGBanks * 2 - 3) * 0x2000) + (addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xC000 && addr <= 0xDFFF) {
        mapped_addr = ((nPRGBanks * 2 - 2) * 0x2000) + (addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xE000 && addr <= 0xFFFF) {
        mapped_addr = ((nPRGBanks * 2 - 1) * 0x2000) + (addr & 0x1FFF);
        return true;
    }
    return false;
}

bool Mapper_009::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    // 1. Chỉ ĐỘC NHẤT RAM phụ (6000-7FFF) là được trả về TRUE để lưu Save
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mapped_addr = addr & 0x1FFF;
        return true;
    }

    // 2. TẤT CẢ các lệnh cấu hình chip bên dưới PHẢI TRẢ VỀ FALSE
    // Nếu trả về true, game sẽ tự ghi đè làm nát bét ROM và Crash xanh màn hình!
    if (addr >= 0xA000 && addr <= 0xAFFF) {
        nPRGBank = data & 0x0F;
        return false; // <--- Đổi thành false
    }
    if (addr >= 0xB000 && addr <= 0xBFFF) {
        nCHRBank0_FD8 = data & 0x1F;
        return false; // <--- Đổi thành false
    }
    if (addr >= 0xC000 && addr <= 0xCFFF) {
        nCHRBank0_FE8 = data & 0x1F;
        return false; // <--- Đổi thành false
    }
    if (addr >= 0xD000 && addr <= 0xDFFF) {
        nCHRBank1_FD8 = data & 0x1F;
        return false; // <--- Đổi thành false
    }
    if (addr >= 0xE000 && addr <= 0xEFFF) {
        nCHRBank1_FE8 = data & 0x1F;
        return false; // <--- Đổi thành false
    }
    if (addr >= 0xF000 && addr <= 0xFFFF) {
        if (data & 0x01) mirromode = HORIZONTAL;
        else mirromode = VERTICAL;
        return false; // <--- Đổi thành false
    }
    return false;
}
bool Mapper_009::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        if (addr <= 0x0FFF) {
            if (nLatch0 == 0) mapped_addr = (nCHRBank0_FD8 * 0x1000) + (addr & 0x0FFF);
            else              mapped_addr = (nCHRBank0_FE8 * 0x1000) + (addr & 0x0FFF);
        }
        else {
            if (nLatch1 == 0) mapped_addr = (nCHRBank1_FD8 * 0x1000) + (addr & 0x0FFF);
            else              mapped_addr = (nCHRBank1_FE8 * 0x1000) + (addr & 0x0FFF);
        }

        if ((addr & 0x1FF8) == 0x0FD8) nLatch0 = 0;
        else if ((addr & 0x1FF8) == 0x0FE8) nLatch0 = 1;
        else if ((addr & 0x1FF8) == 0x1FD8) nLatch1 = 0;
        else if ((addr & 0x1FF8) == 0x1FE8) nLatch1 = 1;

        return true;
    }
    return false;
}

bool Mapper_009::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    return false;
}

QString Mapper_009::GetDebugInfo()
{
    QString s;

    auto mirrorToString = [](MIRROR m) -> QString {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Dọc";
        case MIRROR::ONESCREEN_LO: return "One-screen thấp";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        default:                   return "Hardware / Không rõ";
        }
        };

    uint32_t prg8Count = nPRGBanks * 2;

    uint32_t prg8000 = nPRGBank * 0x2000;
    uint32_t prgA000 = (prg8Count - 3) * 0x2000;
    uint32_t prgC000 = (prg8Count - 2) * 0x2000;
    uint32_t prgE000 = (prg8Count - 1) * 0x2000;

    uint8_t activeChr0 = (nLatch0 == 0) ? nCHRBank0_FD8 : nCHRBank0_FE8;
    uint8_t activeChr1 = (nLatch1 == 0) ? nCHRBank1_FD8 : nCHRBank1_FE8;

    s += "===== MAPPER 009 - MMC2 =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += "Mapper MMC2 dùng CHR latch đặc biệt.\n";
    s += "Khi PPU đọc tile ở vùng $0FD8/$0FE8/$1FD8/$1FE8, latch sẽ đổi bank CHR.\n";
    s += "Mapper này nổi tiếng với Punch-Out!!.\n\n";

    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số PRG banks 8KB  : %1\n").arg(prg8Count);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);
    s += QString("Số CHR banks 4KB  : %1\n").arg(nCHRBanks * 2);
    s += QString("Mirroring         : %1\n").arg(mirrorToString(mirromode));

    s += "\nPRG BANK HIỆN TẠI:\n";
    s += QString("$8000-$9FFF : PRG bank 8KB %1 | offset ROM = 0x%2\n")
        .arg(nPRGBank)
        .arg(prg8000, 6, 16, QChar('0'))
        .toUpper();

    s += QString("$A000-$BFFF : PRG bank 8KB %1 | offset ROM = 0x%2 | cố định\n")
        .arg(prg8Count - 3)
        .arg(prgA000, 6, 16, QChar('0'))
        .toUpper();

    s += QString("$C000-$DFFF : PRG bank 8KB %1 | offset ROM = 0x%2 | cố định\n")
        .arg(prg8Count - 2)
        .arg(prgC000, 6, 16, QChar('0'))
        .toUpper();

    s += QString("$E000-$FFFF : PRG bank 8KB %1 | offset ROM = 0x%2 | cố định\n")
        .arg(prg8Count - 1)
        .arg(prgE000, 6, 16, QChar('0'))
        .toUpper();

    s += "\nCHR LATCH HIỆN TẠI:\n";
    s += QString("Latch 0 cho vùng PPU $0000-$0FFF : %1 (%2)\n")
        .arg(nLatch0)
        .arg(nLatch0 == 0 ? "FD8" : "FE8");

    s += QString("Latch 1 cho vùng PPU $1000-$1FFF : %1 (%2)\n")
        .arg(nLatch1)
        .arg(nLatch1 == 0 ? "FD8" : "FE8");

    s += "\nTHANH GHI CHR BANK:\n";
    s += QString("CHR Bank 0 FD8 : %1\n").arg(nCHRBank0_FD8);
    s += QString("CHR Bank 0 FE8 : %1\n").arg(nCHRBank0_FE8);
    s += QString("CHR Bank 1 FD8 : %1\n").arg(nCHRBank1_FD8);
    s += QString("CHR Bank 1 FE8 : %1\n").arg(nCHRBank1_FE8);

    s += "\nCHR BANK ĐANG ĐƯỢC DÙNG:\n";
    s += QString("$0000-$0FFF : CHR bank 4KB %1 | offset CHR = 0x%2\n")
        .arg(activeChr0)
        .arg(activeChr0 * 0x1000, 6, 16, QChar('0'))
        .toUpper();

    s += QString("$1000-$1FFF : CHR bank 4KB %1 | offset CHR = 0x%2\n")
        .arg(activeChr1)
        .arg(activeChr1 * 0x1000, 6, 16, QChar('0'))
        .toUpper();

    s += "\nCÁC ĐỊA CHỈ LATCH:\n";
    s += "$0FD8 : đổi Latch 0 sang FD8\n";
    s += "$0FE8 : đổi Latch 0 sang FE8\n";
    s += "$1FD8 : đổi Latch 1 sang FD8\n";
    s += "$1FE8 : đổi Latch 1 sang FE8\n";

    s += "\nGHI CHÚ:\n";
    s += "MMC2 không chỉ đổi CHR bằng CPU write, mà còn tự đổi theo địa chỉ PPU đọc.\n";
    s += "Vì vậy khi game vẽ sprite/background đặc biệt, latch có thể nhảy rất nhanh.\n";

    return s;
}