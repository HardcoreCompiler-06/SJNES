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
class Mapper_185 : public Mapper {
public:
    Mapper_185(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_185();
    std::string GetDebugInfo() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;

private:
    uint8_t nDummyEnable = 0x00; // Lưu lại lệnh của game (giữ để debug, không còn dùng để quyết định bật/tắt)

    // Heuristic "2 lần đọc CHR đầu tiên luôn là rác" (theo NESdev wiki, mục
    // INES Mapper 185): KHÔNG dựa vào giá trị game ghi vào $8000-$FFFF (mỗi
    // game dùng 1 giá trị bật/tắt riêng, không có pattern chung - dựa vào giá
    // trị ghi sẽ sai với 1 số game như Seicross rev2). Thay vào đó: PPU đọc
    // pattern table ($0000-$1FFF) - qua $2007 hoặc qua render - 2 LẦN ĐẦU
    // TIÊN kể từ lúc reset luôn coi là CHR "đang tắt" (trả về rác/0), từ lần
    // thứ 3 trở đi CHR "mở" vĩnh viễn và giữ nguyên (đúng chuỗi hành vi mọi
    // game mapper 185 mong đợi: tắt -> verify rác -> bật -> chạy bình thường).
    uint32_t nChrReadCount = 0;
};