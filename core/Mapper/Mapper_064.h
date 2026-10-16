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

// Mapper 64 - Tengen RAMBO-1
// Kế thừa MMC3 (Mapper_004): dùng lại đọc PRG/CHR, mirroring, IRQ latch/enable.
// Khác MMC3:
//  - 3 bank PRG chuyển được (R6, R7, R15) thay vì 2
//  - 10 thanh ghi bank: R0-R9, R15 (R8/R9 là 2 bank CHR 1KB phụ)
//  - Bit 5 của $8000 (K): chế độ CHR 1KB toàn phần
//  - IRQ có 2 chế độ: A12 (như MMC3) hoặc đếm chu kỳ CPU (chia 4)
class Mapper_064 : public Mapper_004 {
public:
    Mapper_064(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_064();

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    void reset() override;
    void ClockA12() override;
    void irqStep() override;   // EmuWorker gọi hàm này mỗi 1 CPU cycle

    std::string GetDebugInfo() override;

private:
    void updateBanks();      // tính lại pPRGBank[] và pCHRBank[] từ thanh ghi
    void clockIRQCounter();  // một lần "tick" bộ đếm IRQ (dùng chung cho 2 chế độ)

    uint8_t nSelect64 = 0;       // chỉ số thanh ghi đang chọn (0-15)
    uint8_t reg64[16] = {};      // R0-R9, R15 (R10-R14 không dùng)
    bool bCHRFullMode = false;   // bit 5 của $8000 (K)

    bool bCycleMode = false;     // false: đếm theo A12, true: đếm theo CPU/4
    uint8_t nPrescaler = 0;      // bộ chia 4 chu kỳ CPU
};