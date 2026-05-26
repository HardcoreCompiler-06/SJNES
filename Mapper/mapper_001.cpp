#include "Mapper_001.h"

Mapper_001::Mapper_001(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {

    reset(); 
}

Mapper_001::~Mapper_001() {}

void Mapper_001::reset() { 
    nControlRegister = 0x1C; // Mode mặc định: PRG 16KB cố định ở 0xC000
    nLoadRegister = 0x00;
    nLoadRegisterCount = 0x00;
    nCHRBankSelect4Lo = 0;
    nCHRBankSelect4Hi = 0;
    nCHRBankSelect8 = 0;
    nPRGBankSelect16Lo = 0;
    nPRGBankSelect16Hi = 0;
    nPRGBankSelect32 = 0;
    pPRGBank[0] = 0;
    pPRGBank[1] = (nPRGBanks - 1) * 0x4000; // Bank cuối cùng luôn nằm ở 0xC000
    pCHRBank[0] = 0;
    pCHRBank[1] = 0x1000;
}

bool Mapper_001::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000) {
        // NẾU BIT 7 LÀ 1 -> Reset bộ ghi dịch khẩn cấp!
        if (data & 0x80) {
            nLoadRegister = 0x00;
            nLoadRegisterCount = 0;
            nControlRegister = nControlRegister | 0x0C;
        }
        else {
            // Đút từng đồng xu (bit) vào túi
            nLoadRegister >>= 1;
            nLoadRegister |= (data & 0x01) << 4;
            nLoadRegisterCount++;

            // Đã đủ 5 đồng xu -> CHỐT ĐƠN VÀ LẬT BANK!
            if (nLoadRegisterCount == 5) {
                uint8_t nTargetRegister = (addr >> 13) & 0x03;

                if (nTargetRegister == 0) { // 0x8000 - 0x9FFF
                    nControlRegister = nLoadRegister & 0x1F;
                    switch (nControlRegister & 0x03) {
                    case 0: currentMirror = MIRROR::ONESCREEN_LO; break;
                    case 1: currentMirror = MIRROR::ONESCREEN_HI; break;
                    case 2: currentMirror = MIRROR::VERTICAL;     break;
                    case 3: currentMirror = MIRROR::HORIZONTAL;   break;
                    }
                }
                else if (nTargetRegister == 1) { // 0xA000 - 0xBFFF
                    nCHRBankSelect4Lo = nLoadRegister & 0x1F;
                }
                else if (nTargetRegister == 2) { // 0xC000 - 0xDFFF
                    nCHRBankSelect4Hi = nLoadRegister & 0x1F;
                }
                else if (nTargetRegister == 3) { // 0xE000 - 0xFFFF
                    nPRGBankSelect16Lo = nLoadRegister & 0x0F;
                }

                // ==========================================
                // THỰC THI LẬT BANK CHR (Hình ảnh)
                // ==========================================
                if (nControlRegister & 0x10) {
                    // Chế độ 4KB: Chia làm 2 Bank độc lập
                    pCHRBank[0] = nCHRBankSelect4Lo * 0x1000;
                    pCHRBank[1] = nCHRBankSelect4Hi * 0x1000;
                }
                else {
                    // Chế độ 8KB: Ghép 2 Bank làm 1
                    pCHRBank[0] = (nCHRBankSelect4Lo & 0xFE) * 0x1000;
                    pCHRBank[1] = pCHRBank[0] + 0x1000;
                }

                // ==========================================
                // THỰC THI LẬT BANK PRG (Code)
                // ==========================================
                uint8_t nPRGMode = (nControlRegister >> 2) & 0x03;
                if (nPRGMode == 0 || nPRGMode == 1) {
                    // Chế độ 32KB
                    pPRGBank[0] = (nPRGBankSelect16Lo & 0x0E) * 0x4000;
                    pPRGBank[1] = pPRGBank[0] + 0x4000;
                }
                else if (nPRGMode == 2) {
                    // Cố định 0x8000, lật 0xC000
                    pPRGBank[0] = 0;
                    pPRGBank[1] = nPRGBankSelect16Lo * 0x4000;
                }
                else if (nPRGMode == 3) {
                    // Lật 0x8000, cố định 0xC000 (Chế độ phổ biến nhất)
                    pPRGBank[0] = nPRGBankSelect16Lo * 0x4000;
                    pPRGBank[1] = (nPRGBanks - 1) * 0x4000;
                }

                // Đổ túi, chuẩn bị cho 5 đồng xu tiếp theo
                nLoadRegister = 0x00;
                nLoadRegisterCount = 0;
            }
        }
        return false;
    }
    return false;
}

bool Mapper_001::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        mapped_addr = pPRGBank[0] + (addr & 0x3FFF);
        return true;
    }
    if (addr >= 0xC000 && addr <= 0xFFFF) {
        mapped_addr = pPRGBank[1] + (addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_001::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        if (nCHRBanks == 0) {
            // Băng game dùng RAM Đồ họa (Ví dụ: The Legend of Zelda)
            mapped_addr = addr;
            return true;
        }
        else {
            // Băng game dùng ROM Đồ họa
            if (addr >= 0x0000 && addr <= 0x0FFF) {
                mapped_addr = pCHRBank[0] + (addr & 0x0FFF);
                return true;
            }
            if (addr >= 0x1000 && addr <= 0x1FFF) {
                mapped_addr = pCHRBank[1] + (addr & 0x0FFF);
                return true;
            }
        }
    }
    return false;
}

bool Mapper_001::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    // CHỈ cho phép ghi hình ảnh nếu game dùng RAM đồ họa (CHR RAM)
    if (addr < 0x2000) {
        if (nCHRBanks == 0) {
            mapped_addr = addr;
            return true;
        }
    }
    return false;
}

QString Mapper_001::GetDebugInfo()
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

    uint8_t chrMode = (nControlRegister >> 4) & 0x01;
    uint8_t prgMode = (nControlRegister >> 2) & 0x03;
    uint8_t mirrorMode = nControlRegister & 0x03;

    s += "===== MAPPER 001 - MMC1 =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);
    s += QString("Kiểu mirroring    : %1\n").arg(mirrorToString(currentMirror));

    s += "\nTHANH GHI ĐIỀU KHIỂN MMC1:\n";
    s += QString("Control Register  : 0x%1\n")
        .arg(nControlRegister, 2, 16, QChar('0')).toUpper();
    s += QString("Mirror bits       : %1\n").arg(mirrorMode);
    s += QString("PRG mode          : %1 - ").arg(prgMode);

    if (prgMode == 0 || prgMode == 1)
        s += "32KB switch tại $8000-$FFFF\n";
    else if (prgMode == 2)
        s += "Cố định bank đầu tại $8000, đổi bank tại $C000\n";
    else
        s += "Đổi bank tại $8000, cố định bank cuối tại $C000\n";

    s += QString("CHR mode          : %1 - ").arg(chrMode);
    s += (chrMode ? "2 bank CHR 4KB riêng\n" : "1 bank CHR 8KB\n");

    s += "\nBỘ GHI DỊCH MMC1:\n";
    s += QString("Load Register     : 0x%1\n")
        .arg(nLoadRegister, 2, 16, QChar('0')).toUpper();
    s += QString("Số bit đã nạp     : %1 / 5\n").arg(nLoadRegisterCount);

    s += "\nTHANH GHI CHỌN BANK:\n";
    s += QString("CHR 4KB Low       : %1\n").arg(nCHRBankSelect4Lo);
    s += QString("CHR 4KB High      : %1\n").arg(nCHRBankSelect4Hi);
    s += QString("CHR 8KB           : %1\n").arg(nCHRBankSelect8);
    s += QString("PRG 16KB Low      : %1\n").arg(nPRGBankSelect16Lo);
    s += QString("PRG 16KB High     : %1\n").arg(nPRGBankSelect16Hi);
    s += QString("PRG 32KB          : %1\n").arg(nPRGBankSelect32);

    s += "\nPRG BANK HIỆN TẠI:\n";
    s += QString("$8000-$BFFF : offset ROM = 0x%1 | PRG bank 16KB = %2\n")
        .arg(pPRGBank[0], 6, 16, QChar('0'))
        .arg(pPRGBank[0] / 0x4000)
        .toUpper();

    s += QString("$C000-$FFFF : offset ROM = 0x%1 | PRG bank 16KB = %2\n")
        .arg(pPRGBank[1], 6, 16, QChar('0'))
        .arg(pPRGBank[1] / 0x4000)
        .toUpper();

    s += "\nCHR BANK HIỆN TẠI:\n";

    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM\n";
    }
    else
    {
        s += QString("$0000-$0FFF : offset CHR = 0x%1 | CHR bank 4KB = %2\n")
            .arg(pCHRBank[0], 6, 16, QChar('0'))
            .arg(pCHRBank[0] / 0x1000)
            .toUpper();

        s += QString("$1000-$1FFF : offset CHR = 0x%1 | CHR bank 4KB = %2\n")
            .arg(pCHRBank[1], 6, 16, QChar('0'))
            .arg(pCHRBank[1] / 0x1000)
            .toUpper();
    }

    s += "\nGHI CHÚ:\n";
    s += "MMC1 nạp từng bit vào shift register. Khi đủ 5 bit, mapper mới chốt vào thanh ghi đích.\n";
    s += "Vì vậy Load Register và số bit đã nạp có thể thay đổi rất nhanh khi game đang chạy.\n";

    return s;
}