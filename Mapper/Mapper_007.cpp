#include "Mapper_007.h"

// KHỞI TẠO & RESET
Mapper_007::Mapper_007(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    // AxROM sử dụng CHR-RAM thay vì CHR-ROM.
    // Nếu băng game không khai báo CHR-ROM (chrBanks == 0), hệ thống sẽ tự động cấp 8KB RAM.
}

Mapper_007::~Mapper_007() {}

void Mapper_007::reset() {
    // Khi reset, luôn chọn PRG Bank đầu tiên và Nametable 0
    nPRGBankSelect = 0x00;
}

// GIAO TIẾP VỚI CPU (Bank Switching & Mirroring)
bool Mapper_007::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    // CPU đọc dữ liệu ROM từ băng ($8000 - $FFFF)
    if (addr >= 0x8000) {
        // AxROM không chia nhỏ mà tráo nguyên một khối 32KB (0x8000 byte).
        // Công thức: (Bank_đang_chọn * 32KB) + Địa chỉ tương đối
        mapped_addr = (nPRGBankSelect * 0x8000) + (addr & 0x7FFF);
        return true;
    }
    return false;
}

bool Mapper_007::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    // Bất kỳ thao tác ghi nào vào vùng $8000 - $FFFF đều là ra lệnh cho Mapper
    if (addr >= 0x8000) {
        // 3 bit thấp (0, 1, 2) quyết định PRG Bank nào sẽ được nạp
        nPRGBankSelect = data & 0x07;

        // Bit 4 (0x10) quyết định Mirroring (Nametable 0 hay Nametable 1)
        if (data & 0x10) {
            mapper_mirror = MIRROR::ONESCREEN_HI; // Dùng Nametable 1
        }
        else {
            mapper_mirror = MIRROR::ONESCREEN_LO; // Dùng Nametable 0
        }
        return true;
    }
    return false;
}

// GIAO TIẾP VỚI PPU (Đồ họa sử dụng CHR-RAM)
bool Mapper_007::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    // PPU đọc Pattern Table ($0000 - $1FFF)
    if (addr < 0x2000) {
        // Vì là CHR-RAM 8KB nguyên khối, không cần Bank Switching.
        // Cứ ánh xạ thẳng địa chỉ 1-1.
        mapped_addr = addr;
        return true;
    }
    return false;
}

bool Mapper_007::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    // PPU (thực chất là CPU điều khiển) ghi dữ liệu đồ họa vào CHR-RAM
    if (addr < 0x2000) {
        // Ánh xạ thẳng giống hệt hàm Read để cho phép ghi vào RAM
        mapped_addr = addr;

        // CHÚ Ý: Nếu chrBanks == 0 (tức là băng dùng CHR-RAM), hệ thống Cartridge 
        // của bạn phải có mảng lưu trữ RAM này. Hàm này chỉ trả về địa chỉ.
        return true;
    }
    return false;
}

// OVERRIDE MIRRORING
MIRROR Mapper_007::mirror() {
    // Bắt buộc PPU phải lấy Mirroring do Mapper 7 chỉ định, bỏ qua thiết lập gốc của file iNES
    return mapper_mirror;
}