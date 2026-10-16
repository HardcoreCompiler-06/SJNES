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
#include "Mapper_033.h"

// Mapper 48 = Taito TC0690 = mapper 33 + IRQ kiểu MMC3.
//   Range $8000-$FFFF, mask $E003:
//   $8000-$8003, $A000-$A003 : như mapper 33, NHƯNG $8000 không còn bit mirroring
//   $C000 : IRQ latch (giá trị ghi bị XOR $FF - chip Taito đếm lên, MMC3 đếm xuống)
//   $C001 : IRQ reload (counter = 0 -> nạp lại ở lần clock kế tiếp), xóa IRQ đang chờ
//   $C002 : IRQ enable
//   $C003 : IRQ disable + acknowledge
//   $E000 [.M.. ....] : mirroring (0 = dọc, 1 = ngang)
// IRQ đếm theo cạnh lên A12 (ClockA12) như Mapper_004, nhưng bắn trễ vài chu kỳ CPU so với MMC3.
class Mapper_048 : public Mapper_033 {
public:
    Mapper_048(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_048() = default;

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    void reset() override;

    void ClockA12() override;
    void irqStep() override;      // EmuWorker gọi mỗi chu kỳ CPU -> dùng để đếm độ trễ IRQ
    bool irqState() override;
    void irqClear() override;

    std::string GetDebugInfo() override;

    // Độ trễ IRQ so với MMC3 (chu kỳ CPU). Wiki ghi ~4; một số người thử ghi nhận >= 6.
    // Nếu thanh trạng thái Flintstones / Jetsons bị lệch 1 scanline hoặc rung thì thử 6.
    static constexpr int kIrqDelayCycles = 4;

private:
    uint8_t  nIRQLatch = 0;
    uint8_t  nIRQCounter = 0;
    bool     bIRQReload = false;
    bool     bIRQEnable = false;
    bool     bIRQActive = false;
    int      nIRQDelay = 0;       // > 0: đang chờ tới lúc kéo dòng IRQ
};