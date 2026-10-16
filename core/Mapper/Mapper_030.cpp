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

#include "Mapper_030.h"
#include "BinaryIO.h"

Mapper_030::Mapper_030(uint32_t prgBanks, uint32_t chrBanks) : Mapper_002(prgBanks, chrBanks) {
    reset();
}

Mapper_030::~Mapper_030() {}

void Mapper_030::reset() {
    Mapper_002::reset(); // Đặt lại biến nPRGBankSelectLo của Mapper 2
    nCHRBankSelect = 0;
    mirroMode = MIRROR::VERTICAL;
}

MIRROR Mapper_030::mirror() {
    return mirroMode;
}

// Hàm Write này sẽ đè hàm của Mapper_002 để xử lý cấu trúc thanh ghi của UNROM 512
bool Mapper_030::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // --- 1. Xử lý PRG Bank (Mở rộng từ 3 bit -> 5 bit cho tối đa 32 bank) ---
        // Biến nPRGBankSelectLo nằm trong phạm vi protected hoặc public của class cha
        nPRGBankSelectLo = data & 0x1F;
        if (nPRGBanks > 0) {
            nPRGBankSelectLo %= nPRGBanks;
        }

        // --- 2. Xử lý CHR RAM Bank (Bit 5 và 6) ---
        nCHRBankSelect = (data >> 5) & 0x03;

        // --- 3. Xử lý Mirroring (Bit 7 - H/V) ---
        if (data & 0x80) {
            mirroMode = MIRROR::HORIZONTAL;
        }
        else {
            mirroMode = MIRROR::VERTICAL;
        }
    }
    return false;
}

// Do Mapper 30 hỗ trợ bank switching cho CHR RAM lên tới 32KB nên phải map lại
bool Mapper_030::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        mapped_addr = nCHRBankSelect * 8192 + addr;
        return true;
    }
    return false;
}

bool Mapper_030::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        mapped_addr = nCHRBankSelect * 8192 + addr;
        return true; // Cho phép ghi vì Mapper 30 dùng RAM, không phải ROM
    }
    return false;
}

void Mapper_030::SaveState(BinaryWriter& out) const {
    Mapper_002::SaveState(out);
    out << nCHRBankSelect;
    uint8_t m = (mirroMode == MIRROR::HORIZONTAL) ? 1 : 0;
    out << m;
}

void Mapper_030::LoadState(BinaryReader& in) {
    Mapper_002::LoadState(in);
    in >> nCHRBankSelect;
    uint8_t m = 0;
    in >> m;
    mirroMode = (m == 1) ? MIRROR::HORIZONTAL : MIRROR::VERTICAL;
}

std::string Mapper_030::GetDebugInfo() {
    std::string s = "===== MAPPER 030 - UNROM 512 =====\n\n";
    s += "Mo rong truc tiep tu Mapper 2 (UXROM)\n";
    s += "Hien co ho tro CHR RAM Bank Switching va chuyen doi Mirroring.\n\n";

    s += Mapper_002::GetDebugInfo(); // Gọi debug của Mapper 2 để hiển thị PRG Banks

    s += "\n--- BO SUNG CUA MAPPER 30 ---\n";
    s += "CHR RAM Bank hien tai: " + std::to_string(nCHRBankSelect) + "\n";
    s += "Che do Mirroring dang bat: ";
    s += (mirroMode == MIRROR::HORIZONTAL) ? "HORIZONTAL" : "VERTICAL";
    s += "\n";

    return s;
}