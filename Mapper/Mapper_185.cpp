#include "Mapper_185.h"

Mapper_185::Mapper_185(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
Mapper_185::~Mapper_185() {}

void Mapper_185::reset() {
    nDummyEnable = 0x00; // Bật máy là phải khóa chặt cửa!
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
        
        bool bEnable = ((nDummyEnable & 0x0F) != 0 && nDummyEnable != 0x13);

        if (bEnable) {
            mapped_addr = addr & 0x1FFF; // Đọc thẳng vào 8KB hình ảnh
        }
        else {
            mapped_addr = 0xFFFFFFFF; // Trả về địa chỉ ma để lừa bài test bảo mật
        }
        return true;
    }
    return false;
}

bool Mapper_185::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) { return false; }