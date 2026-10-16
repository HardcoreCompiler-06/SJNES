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

#include "Mapper_044.h"

Mapper_044::Mapper_044(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper_004(prgBanks, chrBanks) {
    // Gọi thẳng constructor của lớp cha để khởi tạo lõi MMC3
    reset();
}

Mapper_044::~Mapper_044() {}

void Mapper_044::reset() {
    Mapper_004::reset(); // Reset lõi MMC3 gốc
    prg_bank_offset = 0;
    chr_bank_offset = 0;
}

bool Mapper_044::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr == 0xA001) {
        // Dùng đúng 3 bit (0-7) để gom chung cả PRG và CHR vào 1 block
        uint8_t block = data & 0x07;
        prg_bank_offset = block;
        chr_bank_offset = block;
    }
    // Vẫn trả về cho MMC3 xử lý lật trang Mirroring
    return Mapper_004::cpuMapWrite(addr, mapped_addr, data);
}

bool Mapper_044::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (Mapper_004::cpuMapRead(addr, mapped_addr)) {
        // Khối 6 có dung lượng 256KB (mask 0x3FFFF), các khối còn lại 128KB (mask 0x1FFFF)
        uint32_t mask = (prg_bank_offset == 6) ? 0x3FFFF : 0x1FFFF;
        mapped_addr = (mapped_addr & mask) | (prg_bank_offset << 17);
        return true;
    }
    return false;
}

bool Mapper_044::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (Mapper_004::ppuMapRead(addr, mapped_addr)) {
        // Tương tự cho phần đọc đồ họa
        uint32_t mask = (chr_bank_offset == 6) ? 0x3FFFF : 0x1FFFF;
        mapped_addr = (mapped_addr & mask) | (chr_bank_offset << 17);
        return true;
    }
    return false;
}