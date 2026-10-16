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

#include "Mapper_105.h"

Mapper_105::Mapper_105(uint8_t prgBanks, uint8_t chrBanks) : Mapper_001(prgBanks, chrBanks) {
    // Constructor của Mapper_001 gọi reset() của CHÍNH NÓ (chưa có phần 105),
    // nên phải reset lại ở đây để các biến của 105 + updateBanks() của 105 có hiệu lực.
    setDipSwitches(dipSwitches);
    reset();
}

void Mapper_105::setDipSwitches(uint8_t dip) {
    dipSwitches = dip & 0x0F;
    // [1 D C B A x x ...] -> bit 29 luôn = 1, DIP D..A là bit 28..25
    irqTarget = 0x20000000u | ((uint32_t)dipSwitches << 25);
}

void Mapper_105::reset() {
    initState = 0;
    irqCounter = 0;
    irqPending = false;
    Mapper_001::reset();   // đặt control/CHR0/PRG reg về mặc định rồi gọi updateBanks() (bản 105)
}

// Lọc theo đúng thanh ghi vừa được chốt: "mở khóa" cần ghi bit I = 0 rồi I = 1 vào $A000.
void Mapper_105::onRegisterCommit(uint8_t targetReg) {
    if (targetReg != 1) return;                 // chỉ quan tâm $A000
    bool I = (nCHRBankSelect4Lo & 0x10) != 0;
    if (initState == 0 && !I) initState = 1;
    else if (initState == 1 && I) initState = 2;
}

void Mapper_105::updateBanks() {
    // CHR-RAM 8KB cố định
    pCHRBank[0] = 0;
    pCHRBank[1] = 0x1000;

    // Chưa mở khóa: luôn là 32KB đầu của chip đầu, bất kể thanh ghi nào
    if (initState != 2) {
        pPRGBank[0] = 0x0000;
        pPRGBank[1] = 0x4000;
        return;
    }

    const bool O = (nCHRBankSelect4Lo & 0x08) != 0;

    if (!O) {
        // Chip đầu (128KB đầu), PRG reg A (2 bit), swap 32KB
        uint32_t a = (nCHRBankSelect4Lo >> 1) & 0x03;
        pPRGBank[0] = a * 0x8000;
        pPRGBank[1] = pPRGBank[0] + 0x4000;
    }
    else {
        // Chip sau (128KB sau), PRG reg B, banking kiểu MMC1 theo control register
        const uint32_t chip = 0x20000;
        uint32_t b = nPRGBankSelect16Lo & 0x07;           // chip chỉ có 8 bank 16KB
        uint8_t mode = (nControlRegister >> 2) & 0x03;
        if (mode <= 1) {                                  // 32KB swap
            uint32_t b2 = b & 0x06;
            pPRGBank[0] = chip + b2 * 0x4000;
            pPRGBank[1] = chip + (b2 + 1) * 0x4000;
        }
        else if (mode == 2) {                             // bank đầu cố định ở $8000
            pPRGBank[0] = chip;
            pPRGBank[1] = chip + b * 0x4000;
        }
        else {                                            // bank cuối (7) cố định ở $C000
            pPRGBank[0] = chip + b * 0x4000;
            pPRGBank[1] = chip + 7 * 0x4000;
        }
    }

    // Phòng ROM nhỏ hơn 256KB (header sai): không cho vượt kích thước thật
    if (nPRGBanks) {
        uint32_t total = (uint32_t)nPRGBanks * 0x4000;
        pPRGBank[0] %= total;
        pPRGBank[1] %= total;
    }
}

bool Mapper_105::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) { mapped_addr = addr; return true; }   // CHR-RAM
    return false;
}

bool Mapper_105::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) { mapped_addr = addr; return true; }   // CHR-RAM
    return false;
}

void Mapper_105::irqStep() {
    // I = 1: reset counter về 0, đứng yên, và ack IRQ đang chờ
    if (nCHRBankSelect4Lo & 0x10) {
        irqCounter = 0;
        irqPending = false;
        return;
    }
    // I = 0: đếm mỗi CPU cycle, chạm ngưỡng thì giữ IRQ (level) tới khi I = 1
    if (irqCounter < irqTarget) {
        if (++irqCounter == irqTarget) irqPending = true;
    }
}

bool Mapper_105::irqState() {
    return irqPending;
}

std::string Mapper_105::GetDebugInfo() {
    std::string s;
    s += "===== MAPPER 105 - NES-EVENT =====\n\n";
    s += "Trang thai khoa PRG : ";
    s += (initState == 2) ? "da mo khoa\n" : "CHUA mo khoa (32KB dau co dinh), initState = " + std::to_string(initState) + "\n";
    s += "Bit I (timer)       : " + std::string((nCHRBankSelect4Lo & 0x10) ? "1 - reset/dung" : "0 - dang dem") + "\n";
    s += "Bit O (chip PRG)    : " + std::to_string((nCHRBankSelect4Lo >> 3) & 1) + "\n";
    s += "PRG reg A / B       : " + std::to_string((nCHRBankSelect4Lo >> 1) & 3) + " / " + std::to_string(nPRGBankSelect16Lo & 7) + "\n";
    s += "DIP (D C B A)       : " + std::to_string((dipSwitches >> 3) & 1) + std::to_string((dipSwitches >> 2) & 1)
        + std::to_string((dipSwitches >> 1) & 1) + std::to_string(dipSwitches & 1) + "\n";
    s += "Nguong dem          : " + std::to_string(irqTarget) + " cycle (~" + std::to_string((int)(irqTarget / 1789773.0)) + " giay)\n";
    s += "Bo dem hien tai     : " + std::to_string(irqCounter) + "\n";
    s += "IRQ dang cho        : " + std::string(irqPending ? "co" : "khong") + "\n\n";
    s += "---- MMC1 goc ----\n";
    s += Mapper_001::GetDebugInfo();
    return s;
}