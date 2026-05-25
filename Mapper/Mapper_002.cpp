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