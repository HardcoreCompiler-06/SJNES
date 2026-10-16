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

#include "Mapper_064.h"

Mapper_064::Mapper_064(uint8_t prgBanks, uint8_t chrBanks) : Mapper_004(prgBanks, chrBanks) {
    // Lưu ý: constructor của Mapper_004 chỉ gọi được Mapper_004::reset(),
    // nên phải gọi lại reset() của lớp con ở đây.
    reset();
}

Mapper_064::~Mapper_064() {}

void Mapper_064::reset() {
    Mapper_004::reset();

    nSelect64 = 0;
    bCHRFullMode = false;
    bCycleMode = false;
    nPrescaler = 0;

    for (int i = 0; i < 16; i++) reg64[i] = 0;

    // Giá trị khởi tạo giống MMC3: CHR 0..7, PRG 0,1,2 + bank cuối cố định
    reg64[0] = 0; reg64[1] = 2;
    reg64[2] = 4; reg64[3] = 5; reg64[4] = 6; reg64[5] = 7;
    reg64[6] = 0; reg64[7] = 1;
    reg64[8] = 1; reg64[9] = 3;
    reg64[15] = 2;

    updateBanks();
}

void Mapper_064::updateBanks() {
    const uint32_t numPrg = nPRGBanks * 2;                          // số bank PRG 8KB
    const uint32_t numChr = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8); // số bank CHR 1KB

    auto wrapPRG = [&](uint32_t b) { return (b % numPrg) * 0x2000; };
    auto wrapCHR = [&](uint32_t b) { return (b % numChr) * 0x0400; };

    // ---- CHR: dựng bố cục "không đảo" rồi đảo nửa nếu cần ----
    uint32_t layout[8];
    if (!bCHRFullMode) {
        // K = 0: R0, R1 là bank 2KB (bỏ bit thấp)
        layout[0] = reg64[0] & 0xFE;  layout[1] = (reg64[0] & 0xFE) + 1;
        layout[2] = reg64[1] & 0xFE;  layout[3] = (reg64[1] & 0xFE) + 1;
    }
    else {
        // K = 1: tất cả là bank 1KB, R8/R9 chen vào giữa
        layout[0] = reg64[0];  layout[1] = reg64[8];
        layout[2] = reg64[1];  layout[3] = reg64[9];
    }
    layout[4] = reg64[2]; layout[5] = reg64[3];
    layout[6] = reg64[4]; layout[7] = reg64[5];

    for (int i = 0; i < 8; i++) {
        // CHR inversion = hoán đổi 2 nửa $0000-$0FFF <-> $1000-$1FFF
        pCHRBank[i] = wrapCHR(layout[bCHRInversion ? (i ^ 4) : i]);
    }

    // ---- PRG: 3 bank chuyển được R6, R7, R15 + bank cuối cố định ----
    if (bPRGBankMode) {
        pPRGBank[0] = wrapPRG(reg64[15]);
        pPRGBank[1] = wrapPRG(reg64[6]);
        pPRGBank[2] = wrapPRG(reg64[7]);
    }
    else {
        pPRGBank[0] = wrapPRG(reg64[6]);
        pPRGBank[1] = wrapPRG(reg64[7]);
        pPRGBank[2] = wrapPRG(reg64[15]);
    }
    pPRGBank[3] = wrapPRG(numPrg - 1);
}

bool Mapper_064::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    // $8000-$9FFF: Bank select / Bank data (khác MMC3 nên tự xử lý)
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if (!(addr & 0x0001)) {
            nSelect64 = data & 0x0F;
            bCHRFullMode = (data & 0x20) != 0;
            bPRGBankMode = (data & 0x40) != 0;
            bCHRInversion = (data & 0x80) != 0;
        }
        else {
            reg64[nSelect64] = data;   // R10-R14 ghi vào cũng vô hại
        }
        updateBanks();
        return false;
    }

    // $C001: IRQ reload + chọn chế độ đếm (khác MMC3)
    if (addr >= 0xC000 && addr <= 0xDFFF && (addr & 0x0001)) {
        bCycleMode = (data & 0x01) != 0;
        nIRQCounter = 0;
        bIRQUpdate = true;   // lần tick kế tiếp sẽ nạp lại từ latch
        nPrescaler = 0;
        return false;
    }

    // $A000 (mirroring), $C000 (latch), $E000/$E001 (IRQ ack/enable): giống MMC3
    return Mapper_004::cpuMapWrite(addr, mapped_addr, data);
}

// Chế độ A12: chỉ đếm khi không ở chế độ cycle
void Mapper_064::ClockA12() {
    if (bCycleMode) return;
    clockIRQCounter();
}

// Chế độ cycle: EmuWorker gọi irqStep() đúng 1 lần mỗi CPU cycle
// (xem vòng lặp chính, trong khối system_clock_counter % 3 == 0),
// nên cứ 4 lần gọi thì tick bộ đếm 1 lần.
void Mapper_064::irqStep() {
    if (!bCycleMode) return;
    if (++nPrescaler >= 4) {
        nPrescaler = 0;
        clockIRQCounter();
    }
}

void Mapper_064::clockIRQCounter() {
    if (bIRQUpdate) {
        // Nạp lại cưỡng bức: RAMBO-1 nạp latch + 1 (theo nesdev/Mesen).
        // Nếu game lệch 1 scanline thì thử đổi thành "= nIRQLatch".
        nIRQCounter = nIRQLatch + 1;
        bIRQUpdate = false;
    }
    else if (nIRQCounter == 0) {
        nIRQCounter = nIRQLatch;
    }
    else {
        nIRQCounter--;
    }

    if (nIRQCounter == 0 && bIRQEnable) {
        bIRQActive = true;
    }
}

std::string Mapper_064::GetDebugInfo() {
    std::string s;
    s += "===== MAPPER 064 - TENGEN RAMBO-1 =====\n\n";
    s += "PRG banks 8KB : " + std::to_string(nPRGBanks * 2) + "\n";
    s += "CHR banks 1KB : " + std::to_string(nCHRBanks == 0 ? 8 : nCHRBanks * 8) + "\n";
    s += "Select        : R" + std::to_string(nSelect64) + "\n";
    s += std::string("PRG mode      : ") + (bPRGBankMode ? "1 ($8000=R15,$A000=R6,$C000=R7)" : "0 ($8000=R6,$A000=R7,$C000=R15)") + "\n";
    s += std::string("CHR invert    : ") + (bCHRInversion ? "BAT" : "TAT") + "\n";
    s += std::string("CHR 1KB (K)   : ") + (bCHRFullMode ? "BAT" : "TAT") + "\n";

    s += "\nTHANH GHI:\n";
    for (int i = 0; i < 10; i++) s += "R" + std::to_string(i) + " = " + std::to_string(reg64[i]) + "\n";
    s += "R15 = " + std::to_string(reg64[15]) + "\n";

    s += "\nPRG:\n";
    for (int i = 0; i < 4; i++)
        s += "$" + HexStr(0x8000 + i * 0x2000, 4) + " : bank 8KB = " + std::to_string(pPRGBank[i] / 0x2000) + "\n";

    s += "\nCHR:\n";
    for (int i = 0; i < 8; i++)
        s += "$" + HexStr(i * 0x400, 4) + " : bank 1KB = " + std::to_string(pCHRBank[i] / 0x0400) + "\n";

    s += "\nIRQ:\n";
    s += std::string("Che do        : ") + (bCycleMode ? "CPU cycle (/4)" : "A12 scanline") + "\n";
    s += std::string("Enable        : ") + (bIRQEnable ? "BAT" : "TAT") + "\n";
    s += std::string("Active        : ") + (bIRQActive ? "CO" : "KHONG") + "\n";
    s += "Counter       : " + std::to_string(nIRQCounter) + "\n";
    s += "Latch         : " + std::to_string(nIRQLatch) + "\n";
    return s;
}