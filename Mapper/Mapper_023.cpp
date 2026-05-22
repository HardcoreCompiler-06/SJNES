#include "Mapper_023.h"

Mapper_023::Mapper_023(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_023::~Mapper_023() {}

void Mapper_023::reset() {
    mirrormode = MIRROR::VERTICAL;

    // Khởi tạo đồ họa sạch sẽ
    for (int i = 0; i < 8; i++) {
        nCHRBank_Lo[i] = 0;
        nCHRBank_Hi[i] = 0;
        pCHRBank[i] = 0;
    }

    // Khởi tạo Code: 2 Bank đầu tiên, và 2 Bank cuối bị KHÓA CỨNG 
    pPRGBank[0] = 0 * 0x2000;
    pPRGBank[1] = 1 * 0x2000;
    pPRGBank[2] = (nPRGBanks * 2 - 2) * 0x2000; // Bank áp chót
    pPRGBank[3] = (nPRGBanks * 2 - 1) * 0x2000; // Bank cuối cùng
}

bool Mapper_023::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) { mapped_addr = pPRGBank[0] + (addr & 0x1FFF); return true; }
    if (addr >= 0xA000 && addr <= 0xBFFF) { mapped_addr = pPRGBank[1] + (addr & 0x1FFF); return true; }
    if (addr >= 0xC000 && addr <= 0xDFFF) { mapped_addr = pPRGBank[2] + (addr & 0x1FFF); return true; }
    if (addr >= 0xE000 && addr <= 0xFFFF) { mapped_addr = pPRGBank[3] + (addr & 0x1FFF); return true; }
    return false;
}

bool Mapper_023::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000) {
        if (addr >= 0x8000 && addr <= 0x8FFF) {
            // Đổi PRG Bank 0
            pPRGBank[0] = (data & 0x1F) % (nPRGBanks * 2) * 0x2000;
        }
        else if (addr >= 0x9000 && addr <= 0x9FFF) {
            // Lật trang màn hình (Mirroring) trên không trung
            switch (data & 0x03) {
            case 0: mirrormode = MIRROR::VERTICAL; break;
            case 1: mirrormode = MIRROR::HORIZONTAL; break;
            case 2: mirrormode = MIRROR::ONESCREEN_LO; break;
            case 3: mirrormode = MIRROR::ONESCREEN_HI; break;
            }
        }
        else if (addr >= 0xA000 && addr <= 0xAFFF) {
            // Đổi PRG Bank 1
            pPRGBank[1] = (data & 0x1F) % (nPRGBanks * 2) * 0x2000;
        }
        else if (addr >= 0xB000 && addr <= 0xEFFF) {
            //  GHÉP MẢNH 4-BIT CỦA 

            // Tính xem game đang muốn đổi hình ảnh cho ô số mấy (0 đến 7)
            int bank_index = ((addr >> 12) - 0xB) * 2 + ((addr >> 1) & 0x01);

            // Phân loại xem mảnh vỡ này là nửa cao hay nửa thấp
            bool is_high = (addr & 0x01);

            if (is_high) {
                nCHRBank_Hi[bank_index] = data & 0x0F;
            }
            else {
                nCHRBank_Lo[bank_index] = data & 0x0F;
            }

            // Dùng keo 502 dán 2 nửa lại với nhau thành 1 số 8-bit hoàn chỉnh
            uint8_t full_bank = (nCHRBank_Hi[bank_index] << 4) | nCHRBank_Lo[bank_index];

            // Map địa chỉ ra thực tế
            uint32_t max_chr_banks = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);
            pCHRBank[bank_index] = (full_bank % max_chr_banks) * 0x0400;
        }

        // LUÔN TRẢ VỀ FALSE ĐỂ TRÁNH BUG GHI ĐÈ ROM GIỐNG TRẬN BATTLETOADS
        return false;
    }
    return false;
}

bool Mapper_023::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // Ánh xạ 8 ô cửa sổ 1KB cực kỳ nhanh gọn
        uint8_t bank_index = addr / 0x0400;
        mapped_addr = pCHRBank[bank_index] + (addr & 0x03FF);
        return true;
    }
    return false;
}

bool Mapper_023::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        if (nCHRBanks == 0) { // Nếu game xài CHR-RAM
            uint8_t bank_index = addr / 0x0400;
            mapped_addr = pCHRBank[bank_index] + (addr & 0x03FF);
            return true;
        }
    }
    return false;
}

MIRROR Mapper_023::mirror() {
    return mirrormode;
}