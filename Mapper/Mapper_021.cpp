#include "Mapper_021.h"
#include <cstdio>
#include "LogBuffer.h"
Mapper_021::Mapper_021(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_021::~Mapper_021() {}

void Mapper_021::reset() {
    nPRGSwapMode = 0;

    // Reset IRQ
    nIRQReload = 0;
    nIRQCounter = 0;
    nIRQPrescaler = 342;
    bIRQEnable = false;
    bIRQEnableAfterAck = false;
    bIRQActive = false;
    mirrormode = 0;
    // Khởi tạo PRG (Chia làm 4 cục, mỗi cục 8KB)
    pPRGBank[0] = 0;
    pPRGBank[1] = 0x2000; // 8KB
    // Hai bank cuối luôn cố định ở cuối ROM lúc khởi động để game không bị crash
    pPRGBank[2] = (nPRGBanks * 2 - 2) * 0x2000;
    pPRGBank[3] = (nPRGBanks * 2 - 1) * 0x2000;

    // Khởi tạo CHR (Chia làm 8 cục, mỗi cục 1KB)
    for (int i = 0; i < 8; i++) {
        nCHRBankSelect[i] = 0;
        pCHRBank[i] = 0;
    }
}

bool Mapper_021::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) { mapped_addr = pPRGBank[0] + (addr & 0x1FFF); return true; }
    if (addr >= 0xA000 && addr <= 0xBFFF) { mapped_addr = pPRGBank[1] + (addr & 0x1FFF); return true; }
    if (addr >= 0xC000 && addr <= 0xDFFF) { mapped_addr = pPRGBank[2] + (addr & 0x1FFF); return true; }
    if (addr >= 0xE000 && addr <= 0xFFFF) { mapped_addr = pPRGBank[3] + (addr & 0x1FFF); return true; }
    return false;
}

bool Mapper_021::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000) {
        // Phân rã địa chỉ theo chuẩn Mapper 21 (VRC4a/c)
        // VRC4 dùng 2 bit địa chỉ A1 và A2 để phân biệt các thanh ghi con
        uint8_t a0 = ((addr & 0x02) ? 1 : 0) | ((addr & 0x40) ? 1 : 0);
        uint8_t a1 = ((addr & 0x04) ? 1 : 0) | ((addr & 0x80) ? 1 : 0);
        uint16_t reg = (addr & 0xF000) | (a1 << 1) | a0;
        
        switch (reg) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003: // Thanh ghi Bank 0
            nPRGBankSelect[0] = data & 0x1F;
            break;
        case 0xA000: case 0xA001: case 0xA002: case 0xA003: // Thanh ghi Bank 1
            nPRGBankSelect[1] = data & 0x1F;
            break;
        case 0x9000: // Chế độ lật Bank
            nPRGSwapMode = (data & 0x02) >> 1;
            break;
        case 0x9001: // 9001 LÀ CỦA MIRRORING
            switch (data & 0x03) {
            case 0: mirrormode = MIRROR::VERTICAL; break;
            case 1: mirrormode = MIRROR::HORIZONTAL; break;
            case 2: mirrormode = MIRROR::ONESCREEN_LO; break;
            case 3: mirrormode = MIRROR::ONESCREEN_HI; break;
            }
            break;

            // =====================================
            // LẬT BANK CHR (HÌNH ẢNH)
            // =====================================
            // (VRC4 lật CHR hơi cồng kềnh, mỗi Bank cần 2 lần ghi: nửa thấp và nửa cao)
        // Bank CHR 0
        case 0xB000: nCHRBankSelect[0] = (nCHRBankSelect[0] & 0xF0) | (data & 0x0F); break;
        case 0xB001: nCHRBankSelect[0] = (nCHRBankSelect[0] & 0x0F) | ((data & 0x1F) << 4); break;
            // Bank CHR 1
        case 0xB002: nCHRBankSelect[1] = (nCHRBankSelect[1] & 0xF0) | (data & 0x0F); break;
        case 0xB003: nCHRBankSelect[1] = (nCHRBankSelect[1] & 0x0F) | ((data & 0x1F) << 4); break;
            // Bank CHR 2
        case 0xC000: nCHRBankSelect[2] = (nCHRBankSelect[2] & 0xF0) | (data & 0x0F); break;
        case 0xC001: nCHRBankSelect[2] = (nCHRBankSelect[2] & 0x0F) | ((data & 0x1F) << 4); break;
            // Bank CHR 3
        case 0xC002: nCHRBankSelect[3] = (nCHRBankSelect[3] & 0xF0) | (data & 0x0F); break;
        case 0xC003: nCHRBankSelect[3] = (nCHRBankSelect[3] & 0x0F) | ((data & 0x1F) << 4); break;
            // Bank CHR 4
        case 0xD000: nCHRBankSelect[4] = (nCHRBankSelect[4] & 0xF0) | (data & 0x0F); break;
        case 0xD001: nCHRBankSelect[4] = (nCHRBankSelect[4] & 0x0F) | ((data & 0x1F) << 4); break;
            // Bank CHR 5
        case 0xD002: nCHRBankSelect[5] = (nCHRBankSelect[5] & 0xF0) | (data & 0x0F); break;
        case 0xD003: nCHRBankSelect[5] = (nCHRBankSelect[5] & 0x0F) | ((data & 0x1F) << 4); break;
            // Bank CHR 6
        case 0xE000: nCHRBankSelect[6] = (nCHRBankSelect[6] & 0xF0) | (data & 0x0F); break;
        case 0xE001: nCHRBankSelect[6] = (nCHRBankSelect[6] & 0x0F) | ((data & 0x1F) << 4); break;
            // Bank CHR 7
        case 0xE002: nCHRBankSelect[7] = (nCHRBankSelect[7] & 0xF0) | (data & 0x0F); break;
        case 0xE003: nCHRBankSelect[7] = (nCHRBankSelect[7] & 0x0F) | ((data & 0x1F) << 4); break;
            // =====================================
            // BỘ ĐIỀU KHIỂN NGẮT (IRQ)
            // =====================================
        case 0xF000:   // Low 4 bits
            nIRQReload = (nIRQReload & 0xF0) | (data & 0x0F);
            break;
        case 0xF001:   // High 4 bits
            nIRQReload = (nIRQReload & 0x0F) | ((data & 0x0F) << 4);
            break;
        case 0xF002:
            bIRQEnableAfterAck = (data & 0x01) != 0;  // bit A
            bIRQEnable = (data & 0x02) != 0;  // bit E
            if (bIRQEnable) {
                nIRQCounter = nIRQReload;
            }
            nIRQPrescaler = 342;   // ← thêm: reset prescaler mỗi lần ghi
            bIRQActive = false;
            break;
        case 0xF003: // Xác nhận IRQ (Acknowledge)
            bIRQActive = false;
            bIRQEnable = bIRQEnableAfterAck;
            break;
        }

        // --- CHỐT ĐỊA CHỈ PRG SAU KHI LẬT ---
        if (nPRGSwapMode == 0) {
            pPRGBank[0] = nPRGBankSelect[0] * 0x2000;
            pPRGBank[1] = nPRGBankSelect[1] * 0x2000;
            pPRGBank[2] = (nPRGBanks * 2 - 2) * 0x2000;
        }
        else {
            pPRGBank[0] = (nPRGBanks * 2 - 2) * 0x2000;
            pPRGBank[1] = nPRGBankSelect[1] * 0x2000;
            pPRGBank[2] = nPRGBankSelect[0] * 0x2000;
        }

        // --- CHỐT ĐỊA CHỈ CHR SAU KHI LẬT ---
        for (int i = 0; i < 8; i++) {
            pCHRBank[i] = nCHRBankSelect[i] * 0x0400;
        }
        return false; 
    }
    return false;
}

bool Mapper_021::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        // Chia addr cho 0x0400 (1024) để biết nó đang chọt vào Bank nào trong 8 Bank CHR
        uint8_t bank = addr / 0x0400;
        uint16_t offset = addr & 0x03FF;
        mapped_addr = pCHRBank[bank] + offset;
        return true;
    }
    return false;
}

bool Mapper_021::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    // Đa số game VRC4 dùng ROM đồ họa, nên cấm ghi
    if (addr < 0x2000) {
        if (nCHRBanks == 0) {
            mapped_addr = addr;
            return true;
        }
    }
    return false;
}

// ========================================================
// HỆ THỐNG IRQ (VŨ KHÍ TỐI THƯỢNG CỦA VRC4)
// ========================================================
bool Mapper_021::irqState() {
    return bIRQActive;
}

void Mapper_021::irqClear() {
    bIRQActive = false;
}

void Mapper_021::irqStep() {
    if (!bIRQEnable) return;

    nIRQPrescaler -= 3;          // Trừ 3 mỗi CPU cycle
    if (nIRQPrescaler <= 0) {
        nIRQPrescaler += 342;    // Cộng thêm (giữ phần dư âm)

        if (nIRQCounter == 0xFF) {
            nIRQCounter = nIRQReload;
            bIRQActive = true;
            static int irqLog = 0;
            if (irqLog++ < 10) {
                std::string s = "IRQ FIRED! reload=" + std::to_string(nIRQReload) + " CHR: ";
                for (int i = 0; i < 8; i++)
                    s += std::to_string(nCHRBankSelect[i]) + " ";
                LogPrint(s);
            }

        }
        else {
            nIRQCounter++;
        }
    }
}
MIRROR Mapper_021::mirror() {
    return (MIRROR)mirrormode;
}