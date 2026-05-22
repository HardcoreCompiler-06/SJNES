#include "Mapper_185.h"

Mapper_185::Mapper_185(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
Mapper_185::~Mapper_185() {}

void Mapper_185::reset() {
    // Boot với CHR MỞ — game đọc CHR thật để verify copy protection
    // B-Wings Japan dùng key 0x21 (low nibble != 0 → mở)
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