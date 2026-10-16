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
#include <cstdint>
#include <string>

// Mapper 105 - NES-EVENT (Nintendo World Championships 1990).
// MMC1 đấu dây khác + bộ đếm giờ 30 bit đếm theo CPU cycle.
//
//  $8000 control : như MMC1 (bit CHR mode không dùng)
//  $A000 CHR0    : [...I OAA.]  I = reset/chạy timer + bit "mở khóa" PRG
//                               O = chọn chip PRG (0: 128K đầu, 1: 128K sau)
//                               A = PRG reg A (32KB swap, chỉ khi O=0)
//  $E000 PRG     : [...W BBBB]  B = PRG reg B (kiểu MMC1, chỉ khi O=1)
//
// CHR: 8KB CHR-RAM cố định.
class Mapper_105 : public Mapper_001 {
public:
    Mapper_105(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_105() = default;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;

    // Gọi 1 lần mỗi CPU cycle (EmuWorker đã gọi mapper->irqStep() như vậy).
    void irqStep() override;
    bool irqState() override;

    std::string GetDebugInfo() override;

    // 4 DIP switch D C B A (bit3..bit0) -> bit 28..25 của ngưỡng đếm.
    // Mặc định 0b0100 (chỉ C đóng) = cấu hình giải đấu chính thức (~6 phút 15 giây).
    void setDipSwitches(uint8_t dip);
    uint8_t getDipSwitches() const { return dipSwitches; }

protected:
    void updateBanks() override;
    void onRegisterCommit(uint8_t targetReg) override;

private:
    // 0 = chưa động gì, 1 = đã thấy I=0, 2 = đã thấy I=1 sau đó (mở khóa PRG)
    uint8_t initState = 0;

    uint8_t  dipSwitches = 0x04;
    uint32_t irqTarget = 0x28000000;
    uint32_t irqCounter = 0;
    bool     irqPending = false;
};