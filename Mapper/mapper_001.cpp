#include "Mapper_001.h"

Mapper_001::Mapper_001(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {

    reset(); // Khởi tạo trạng thái Bank lúc mới bật máy
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

    // Cài đặt PRG Bank mặc định
    pPRGBank[0] = 0;
    pPRGBank[1] = (nPRGBanks - 1) * 0x4000; // Bank cuối cùng luôn nằm ở 0xC000

    // Cài đặt CHR Bank mặc định
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