#include "Mapper_004.h"

Mapper_004::Mapper_004(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_004::~Mapper_004() {}

void Mapper_004::reset() {
    nTargetRegister = 0; bPRGBankMode = false; bCHRInversion = false; mirrormode = MIRROR::HORIZONTAL;
    bIRQActive = false; bIRQEnable = false; bIRQUpdate = false;
    nIRQCounter = 0; nIRQLatch = 0;
    bLastA12 = false;
    nA12Delay = 0;

    for (int i = 0; i < 8; i++) { pRegister[i] = 0; pCHRBank[i] = 0; }
    pPRGBank[0] = 0 * 0x2000;
    pPRGBank[1] = 1 * 0x2000;
    pPRGBank[2] = (nPRGBanks * 2 - 2) * 0x2000;
    pPRGBank[3] = (nPRGBanks * 2 - 1) * 0x2000;
}

bool Mapper_004::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) { mapped_addr = pPRGBank[0] + (addr & 0x1FFF); return true; }
    if (addr >= 0xA000 && addr <= 0xBFFF) { mapped_addr = pPRGBank[1] + (addr & 0x1FFF); return true; }
    if (addr >= 0xC000 && addr <= 0xDFFF) { mapped_addr = pPRGBank[2] + (addr & 0x1FFF); return true; }
    if (addr >= 0xE000 && addr <= 0xFFFF) { mapped_addr = pPRGBank[3] + (addr & 0x1FFF); return true; }
    return false;
}

// =========================================================
// 2. HÀM CHO PPU: BỘ LỌC A12 (TỈ LỆ 3) VÀ ĐỌC HÌNH ẢNH (CHR)
// =========================================================
bool Mapper_004::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    bool A12 = (addr & 0x1000) != 0;

    if (A12) {
        if (!bLastA12 && nA12Delay >= 3) {
            if (nIRQCounter == 0 || bIRQUpdate) {
                nIRQCounter = nIRQLatch;
                bIRQUpdate = false;
            }
            else {
                nIRQCounter--;
            }
            if (nIRQCounter == 0 && bIRQEnable) bIRQActive = true;
        }
        bLastA12 = true;
        nA12Delay = 0; 
    }
    else {
        bLastA12 = false;
        if (nA12Delay < 255) nA12Delay++;
    }
    // Chia Bank hình ảnh
    if (addr >= 0x0000 && addr <= 0x03FF) { mapped_addr = pCHRBank[0] + (addr & 0x03FF); return true; }
    if (addr >= 0x0400 && addr <= 0x07FF) { mapped_addr = pCHRBank[1] + (addr & 0x03FF); return true; }
    if (addr >= 0x0800 && addr <= 0x0BFF) { mapped_addr = pCHRBank[2] + (addr & 0x03FF); return true; }
    if (addr >= 0x0C00 && addr <= 0x0FFF) { mapped_addr = pCHRBank[3] + (addr & 0x03FF); return true; }
    if (addr >= 0x1000 && addr <= 0x13FF) { mapped_addr = pCHRBank[4] + (addr & 0x03FF); return true; }
    if (addr >= 0x1400 && addr <= 0x17FF) { mapped_addr = pCHRBank[5] + (addr & 0x03FF); return true; }
    if (addr >= 0x1800 && addr <= 0x1BFF) { mapped_addr = pCHRBank[6] + (addr & 0x03FF); return true; }
    if (addr >= 0x1C00 && addr <= 0x1FFF) { mapped_addr = pCHRBank[7] + (addr & 0x03FF); return true; }
    return false;
}
bool Mapper_004::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if (!(addr & 0x0001)) {
            nTargetRegister = data & 0x07;
            bPRGBankMode = (data & 0x40);
            bCHRInversion = (data & 0x80);
        }
        else {
            pRegister[nTargetRegister] = data;
        }
        
        uint32_t num_prg_banks = nPRGBanks * 2;
        uint32_t num_chr_banks = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

        auto wrapPRG = [&](uint32_t b) { return (b % num_prg_banks) * 0x2000; };
        auto wrapCHR = [&](uint32_t b) { return (b % num_chr_banks) * 0x0400; };

        // Xếp cửa sổ Hình Ảnh (CHR)
        if (bCHRInversion) {
            pCHRBank[0] = wrapCHR(pRegister[2]);
            pCHRBank[1] = wrapCHR(pRegister[3]);
            pCHRBank[2] = wrapCHR(pRegister[4]);
            pCHRBank[3] = wrapCHR(pRegister[5]);
            pCHRBank[4] = wrapCHR(pRegister[0] & 0xFE);
            pCHRBank[5] = wrapCHR((pRegister[0] & 0xFE) + 1);
            pCHRBank[6] = wrapCHR(pRegister[1] & 0xFE);
            pCHRBank[7] = wrapCHR((pRegister[1] & 0xFE) + 1);
        }
        else {
            pCHRBank[0] = wrapCHR(pRegister[0] & 0xFE);
            pCHRBank[1] = wrapCHR((pRegister[0] & 0xFE) + 1);
            pCHRBank[2] = wrapCHR(pRegister[1] & 0xFE);
            pCHRBank[3] = wrapCHR((pRegister[1] & 0xFE) + 1);
            pCHRBank[4] = wrapCHR(pRegister[2]);
            pCHRBank[5] = wrapCHR(pRegister[3]);
            pCHRBank[6] = wrapCHR(pRegister[4]);
            pCHRBank[7] = wrapCHR(pRegister[5]);
        }

        // Xếp cửa sổ Code (PRG)
        if (bPRGBankMode) {
            pPRGBank[2] = wrapPRG(pRegister[6] & 0x3F);
            pPRGBank[0] = wrapPRG(num_prg_banks - 2);
        }
        else {
            pPRGBank[0] = wrapPRG(pRegister[6] & 0x3F);
            pPRGBank[2] = wrapPRG(num_prg_banks - 2);
        }
        pPRGBank[1] = wrapPRG(pRegister[7] & 0x3F);
        pPRGBank[3] = wrapPRG(num_prg_banks - 1);

        return false; // Lưu ý: Chỗ này phải trả về false nhé!
    }
    else if (addr >= 0xA000 && addr <= 0xBFFF) {
        if (!(addr & 0x0001)) {
            if (data & 0x01) mirrormode = MIRROR::HORIZONTAL;
            else mirrormode = MIRROR::VERTICAL;
        }
        else {
            // Chỗ này để trống cho PRG RAM Protect sau này
        }
        return false;
    }
    else if (addr >= 0xC000 && addr <= 0xDFFF) {
        if (!(addr & 0x0001)) {
            nIRQLatch = data;
        }
        else {
            bIRQUpdate = true;
        }
        return false;
    }
    else if (addr >= 0xE000 && addr <= 0xFFFF) {
        if (!(addr & 0x0001)) {
            bIRQEnable = false;
            bIRQActive = false; // Dập chuông
        }
        else {
            bIRQEnable = true;
        }
        return false;
    }
    return false;
}

bool Mapper_004::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x03FF) { mapped_addr = pCHRBank[0] + (addr & 0x03FF); return true; }
    if (addr >= 0x0400 && addr <= 0x07FF) { mapped_addr = pCHRBank[1] + (addr & 0x03FF); return true; }
    if (addr >= 0x0800 && addr <= 0x0BFF) { mapped_addr = pCHRBank[2] + (addr & 0x03FF); return true; }
    if (addr >= 0x0C00 && addr <= 0x0FFF) { mapped_addr = pCHRBank[3] + (addr & 0x03FF); return true; }
    if (addr >= 0x1000 && addr <= 0x13FF) { mapped_addr = pCHRBank[4] + (addr & 0x03FF); return true; }
    if (addr >= 0x1400 && addr <= 0x17FF) { mapped_addr = pCHRBank[5] + (addr & 0x03FF); return true; }
    if (addr >= 0x1800 && addr <= 0x1BFF) { mapped_addr = pCHRBank[6] + (addr & 0x03FF); return true; }
    if (addr >= 0x1C00 && addr <= 0x1FFF) { mapped_addr = pCHRBank[7] + (addr & 0x03FF); return true; }
    return false;
}

bool Mapper_004::irqState() { return bIRQActive; }
void Mapper_004::irqClear() { bIRQActive = false; }

MIRROR Mapper_004::mirror() {
    return mirrormode;
}