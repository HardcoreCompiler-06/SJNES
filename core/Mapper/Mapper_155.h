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
#include "Mapper_001.h"

// Mapper 155 = MMC1A (SxROM với chip MMC1A).
// Khác 001 (MMC1B) ở chỗ PRG-RAM luôn được bật, bỏ qua bit enable/disable WRAM của thanh ghi PRG.
// Mapper_001 của SJNES chưa mô phỏng việc tắt WRAM nên về hành vi hai mapper này giống nhau;
// class này tồn tại để mapper ID 155 có tên riêng (debug) và là chỗ gắn thêm nếu sau này 001 có tắt WRAM.
class Mapper_155 : public Mapper_001 {
public:
    Mapper_155(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_155() = default;
    std::string GetDebugInfo() override;
};