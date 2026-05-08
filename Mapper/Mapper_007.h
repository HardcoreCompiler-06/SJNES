#pragma once
#include "Mapper.h"

class Mapper_007 : public Mapper {
public:
    Mapper_007(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_007() override; // Nếu Mapper.h không có virtual ~Mapper(), hãy xóa chữ override đi.

    // Giao tiếp với CPU (Vùng $8000 - $FFFF)
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    // Giao tiếp với PPU (Vùng $0000 - $1FFF)
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override; // Nhắc lại: nếu class cha có thêm 'uint8_t data', hãy thêm vào.

    // Hàm thiết lập lại trạng thái ban đầu
    void reset() override;

    // QUAN TRỌNG: Mapper 7 ép hệ thống dùng Single-Screen Mirroring
    MIRROR mirror() override;

private:
    // Chỉ cần ĐÚNG 1 biến để lưu trạng thái Bank hiện tại của PRG
    uint8_t nPRGBankSelect = 0x00;

    // Lưu trạng thái Nametable
    MIRROR mapper_mirror = MIRROR::ONESCREEN_LO;

    // Không cần biến cho CHR Bank vì Mapper 7 không hỗ trợ đổi Bank hình ảnh.
    // Nó dùng nguyên một cục 8KB CHR-RAM.
};