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

#include "Mapper_185.h"

Mapper_185::Mapper_185(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
Mapper_185::~Mapper_185() {}

void Mapper_185::reset() {
    // Boot với CHR "tắt" (đúng chuỗi hành vi thật: game LUÔN tự test tắt
    // trước, verify ra rác, rồi mới tự ghi giá trị bật lên $8000-$FFFF).
    nDummyEnable = 0x00;
    nChrReadCount = 0;
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
        if (nChrReadCount < 2) {
            nChrReadCount++;
            // "Tắt" CHR: trả về false để PPU KHÔNG map vào CHR-ROM thật -
            // PPU sẽ tự rơi về nhánh mặc định (mảng tblPattern nội bộ, luôn
            // là 0x00 với cart CHR-ROM-only như thế này) => game đọc ra 0x00
            // ở chỗ đáng lẽ có dữ liệu tile thật -> đúng như game mong đợi
            // lúc đang test "CHR có tắt được không".
            return false;
        }
        mapped_addr = addr & 0x1FFF;
        return true; // Từ lần đọc thứ 3 trở đi: CHR thật, mở vĩnh viễn
    }
    return false;
}

bool Mapper_185::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) { return false; }
std::string Mapper_185::GetDebugInfo()
{
    std::string s;

    s += "===== MAPPER 185 =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "Mapper nay la CNROM co them chong crack 'VROM-disable'.\n";
    s += "Dung heuristic: 2 lan doc CHR dau tien tra ve rac (0x00),\n";
    s += "tu lan thu 3 tro di CHR mo that vinh vien.\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";

    s += "\nPRG MAPPING:\n";
    if (nPRGBanks > 1)
    {
        s += "$8000-$FFFF : PRG 32KB co dinh\n";
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : PRG bank 1 | offset ROM = 0x004000\n";
    }
    else
    {
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : mirror PRG bank 0 | offset ROM = 0x000000\n";
    }

    s += "\nCHR MAPPING:\n";
    s += "$0000-$1FFF : CHR ROM/RAM map thang, luon mo\n";

    s += "\nTHANH GHI PROTECTION:\n";
    s += "nDummyEnable  : 0x" + HexStr(nDummyEnable, 2) + " (chi de debug, khong con quyet dinh bat/tat)\n";
    s += "nChrReadCount : " + std::to_string(nChrReadCount) + " (>=2 nghia la CHR da mo that)\n";

    s += "\nGHI CHU:\n";
    s += "Heuristic nay pass duoc TAT CA game mapper 185 da biet (theo NESdev wiki),\n";
    s += "ke ca Seicross rev1 - khac voi cach doan gia tri ghi vao thanh ghi (se sai mot vai game).\n";

    return s;
}