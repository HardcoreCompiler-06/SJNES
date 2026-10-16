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

#include "Mapper_048.h"

Mapper_048::Mapper_048(uint8_t prgBanks, uint8_t chrBanks) : Mapper_033(prgBanks, chrBanks) {
    bMirrorAtReg0 = false;   // 48: mirroring nằm ở $E000, bit 6 của $8000 bị bỏ qua
    reset();                 // constructor của 033 chỉ gọi reset() bản 033
}

void Mapper_048::reset() {
    nIRQLatch = 0;
    nIRQCounter = 0;
    bIRQReload = false;
    bIRQEnable = false;
    bIRQActive = false;
    nIRQDelay = 0;
    Mapper_033::reset();
}

bool Mapper_048::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr < 0x8000) return false;

    uint16_t r = addr & 0xE003;
    switch (r) {
    case 0x8000: case 0x8001: case 0x8002: case 0x8003:
    case 0xA000: case 0xA001: case 0xA002: case 0xA003:
        writeBankRegister(r, data);
        break;
    case 0xC000:                       // IRQ latch, XOR $FF so với MMC3
        nIRQLatch = data ^ 0xFF;
        break;
    case 0xC001:                       // IRQ reload + xóa IRQ đang chờ
        nIRQCounter = 0;
        bIRQReload = true;
        bIRQActive = false;
        nIRQDelay = 0;
        break;
    case 0xC002:                       // IRQ enable
        bIRQEnable = true;
        break;
    case 0xC003:                       // IRQ disable + acknowledge
        bIRQEnable = false;
        bIRQActive = false;
        nIRQDelay = 0;
        break;
    case 0xE000:                       // mirroring
        currentMirror = (data & 0x40) ? MIRROR::HORIZONTAL : MIRROR::VERTICAL;
        break;
    default:
        break;
    }
    return false;
}

// Cùng cách đếm với Mapper_004 (counter về 0 hoặc có reload -> nạp latch, ngược lại đếm xuống)
void Mapper_048::ClockA12() {
    if (nIRQCounter == 0 || bIRQReload) {
        nIRQCounter = nIRQLatch;
        bIRQReload = false;
    }
    else {
        nIRQCounter--;
    }

    if (nIRQCounter == 0 && bIRQEnable)
        nIRQDelay = kIrqDelayCycles;   // TC0690 bắn trễ hơn MMC3 vài chu kỳ CPU
}

void Mapper_048::irqStep() {
    if (nIRQDelay > 0 && --nIRQDelay == 0)
        bIRQActive = true;
}

bool Mapper_048::irqState() { return bIRQActive; }
void Mapper_048::irqClear() { bIRQActive = false; }

std::string Mapper_048::GetDebugInfo() {
    std::string s = Mapper_033::GetDebugInfo();
    // Đổi tiêu đề và bỏ ghi chú của 33
    size_t p = s.find("===== MAPPER 033 - Taito TC0190 =====");
    if (p != std::string::npos) s.replace(p, 36, "===== MAPPER 048 - Taito TC0690 =====");
    size_t n = s.find("\nGHI CHU:");
    if (n != std::string::npos) s.erase(n);

    s += "\nTHONG TIN IRQ (kieu MMC3, latch XOR $FF):\n";
    s += std::string("IRQ Enable        : ") + (bIRQEnable ? "BAT" : "TAT") + "\n";
    s += std::string("IRQ Active        : ") + (bIRQActive ? "CO" : "KHONG") + "\n";
    s += std::string("IRQ Reload        : ") + (bIRQReload ? "CO" : "KHONG") + "\n";
    s += "IRQ Counter       : " + std::to_string(nIRQCounter) + "\n";
    s += "IRQ Latch         : " + std::to_string(nIRQLatch) + "\n";
    s += "Cho tre con lai   : " + std::to_string(nIRQDelay) + " chu ky CPU (tong " + std::to_string(kIrqDelayCycles) + ")\n";
    return s;
}