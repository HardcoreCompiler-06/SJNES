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
#include "Mapper_004.h"

// Mapper 118 = TxSROM (MMC3 + mirroring điều khiển bằng bit 7 của thanh ghi CHR).
// Game: Armadillo, Pro Sport Hockey, NES Play Action Football...
//
// Mỗi nametable ($2000/$2400/$2800/$2C00) chọn trang CIRAM 0 hoặc 1 theo bit 7
// của thanh ghi CHR đang map vào vùng tương ứng:
//   CHR inversion = 0: NT0,NT1 <- R0 ; NT2,NT3 <- R1
//   CHR inversion = 1: NT0 <- R2 ; NT1 <- R3 ; NT2 <- R4 ; NT3 <- R5
// $A000 (mirroring của MMC3) bị bỏ qua.
//
// Emulator chỉ có 5 kiểu mirroring, nên chỉ biểu diễn được các tổ hợp
// AABB (horizontal), ABAB (vertical), AAAA (one-screen lo/hi). Tổ hợp khác
// (ví dụ ABBA) sẽ giữ nguyên kiểu mirroring trước đó.
class Mapper_118 : public Mapper_004 {
public:
    Mapper_118(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_118() override;

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    void reset() override;
    MIRROR mirror() override;
    std::string GetDebugInfo() override;

private:
    void updateMirroring();

    // Bản sao trạng thái thanh ghi MMC3 (chỉ để đọc bit 7 của R0..R5)
    uint8_t nTarget118;
    bool    bInvert118;
    uint8_t reg118[8];
    bool    ntPage[4];
    MIRROR  mirror118;
};