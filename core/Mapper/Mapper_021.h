/*
 * SJNES - Nintendo Entertainment System Emulator
 * Copyright (C) 2026 Chiến
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include "Mapper.h"
#include <QString>

// Mapper 021 gộp chung 2 board Konami khác nhau, khác nhau ở việc
// CPU address bit nào nối vào chân A0/A1 của chip (quyết định latch nào
// được chọn trong mỗi block $8000/$9000/$A000/...):
//
//   VRC4a : A0 = CPU bit 1, A1 = CPU bit 2   (vd: Wai Wai World 2)
//   VRC4c : A0 = CPU bit 6, A1 = CPU bit 7   (vd: Ganbare Goemon Gaiden 2)
//
// iNES 1.0 header không phân biệt được 2 variant này, nên cpuMapWrite()
// decode bằng cách OR cả 2 kiểu wiring (giống Mesen khi không có submapper).
// Tham số variant vẫn được giữ lại để tương thích với code khởi tạo hiện có
// và để hiển thị trong debug info.
enum class VRC4Variant {
    VRC4a, // Wai Wai World 2
    VRC4c  // Ganbare Goemon Gaiden 2
};

class Mapper_021 : public Mapper {
public:
    Mapper_021(uint8_t prgBanks, uint8_t chrBanks, VRC4Variant variant = VRC4Variant::VRC4a);
    ~Mapper_021();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    MIRROR mirror() override;
    void reset() override;
    std::string GetDebugInfo() override;
    bool irqState() override;
    void irqClear() override;
    void irqStep();

protected:
    // Ghi thanh ghi đã decode: block = $8000..$F000, latch = 0..3.
    // Mapper con (Mapper_025) chỉ cần tự tính latch theo wiring riêng rồi gọi hàm này.
    void writeReg(uint16_t block, uint8_t latch, uint8_t data);

    // Số bit dịch phải áp cho giá trị bank CHR (0 = VRC4/VRC2b/c, 1 = VRC2a / mapper 22)
    uint8_t nCHRShift = 0;

private:
    // Tính lại pPRGBank[] / pCHRBank[] từ các thanh ghi select
    void updateBanks();

    VRC4Variant nVariant;

    // Physical addresses (đã tính sẵn, PPU/CPU dùng trực tiếp)
    uint32_t pPRGBank[4];    // 4 x 8KB = 32KB PRG
    uint32_t pCHRBank[8];    // 8 x 1KB = 8KB CHR

    // Bank select registers
    uint8_t  nPRGBankSelect[2];   // Chọn bank cho slot 0 và 1
    uint16_t nCHRBankSelect[8];   // 9-bit
    int      mirrormode;
    uint8_t  nPRGSwapMode;

    // IRQ
    int     nIRQPrescaler;
    uint8_t nIRQReload;
    uint8_t nIRQCounter;
    bool    bIRQEnable;
    bool    bIRQEnableAfterAck;
    bool    bIRQCycleMode;   // $F002 bit 2: 1 = cycle mode, 0 = scanline mode
    bool    bIRQActive;
};