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

#include "Mapper_012.h"

Mapper_012::Mapper_012(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper_004(prgBanks, chrBanks) {
    reset();
}

Mapper_012::~Mapper_012() {}

void Mapper_012::reset() {
    Mapper_004::reset();
    chr_low_outer = 0;
    chr_high_outer = 0;
}

bool Mapper_012::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    // Bắt tín hiệu ghi vào thanh ghi mở rộng tại dải $4100 - $5FFF
    if (addr >= 0x4100 && addr <= 0x5FFF) {
        chr_low_outer = data & 0x01;
        chr_high_outer = (data & 0x10) >> 4;
        return false;
    }

    // Toàn bộ dải $8000 - $FFFF đẩy thẳng về cho lõi MMC3 xử lý
    return Mapper_004::cpuMapWrite(addr, mapped_addr, data);
}

bool Mapper_012::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (Mapper_004::ppuMapRead(addr, mapped_addr)) {
        // Giữ nguyên địa chỉ trong phạm vi 256KB (0x3FFFF) của MMC3
        // Sau đó dịch bit 18 (tương đương +256KB) tùy theo nửa bảng Pattern
        uint32_t outer = (addr < 0x1000) ? chr_low_outer : chr_high_outer;
        mapped_addr = (mapped_addr & 0x3FFFF) | (outer << 18);

        // Chặn tràn bộ nhớ nếu file ROM thực tế nhỏ hơn 512KB CHR
        if (nCHRBanks > 0) {
            uint32_t total_chr_size = nCHRBanks * 0x2000;
            mapped_addr %= total_chr_size;
        }
        return true;
    }
    return false;
}

void Mapper_012::ClockA12() {
    uint16_t before = nIRQCounter;
    bool reloadFlag = bIRQUpdate;

    if (nIRQCounter == 0 || bIRQUpdate)
        nIRQCounter = nIRQLatch;
    else
        nIRQCounter--;
    bIRQUpdate = false;

    // MMC3A: chỉ báo IRQ khi counter vừa đếm xuống 0 (trước đó > 0),
    // hoặc khi có cờ reload; reload từ latch = 0 mà không có cờ thì không báo.
    if ((before > 0 || reloadFlag) && nIRQCounter == 0 && bIRQEnable)
        bIRQActive = true;
}