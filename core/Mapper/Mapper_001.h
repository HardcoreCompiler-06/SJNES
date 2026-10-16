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
#include <cstdint>

class Mapper_001 : public Mapper {
public:
    Mapper_001(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_001();

    // 4 hàm cơ bản để giao tiếp với CPU và PPU
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    MIRROR mirror() override { return currentMirror; }
    // Bắt buộc phải có hàm reset cho MMC1
    void reset() override;
    std::string GetDebugInfo() override;
protected:
    // Tính lại pPRGBank[] / pCHRBank[] từ control register + các thanh ghi bank.
    // Gọi sau mỗi lần chốt 5 bit VÀ sau khi ghi reset (bit 7).
    virtual void updateBanks();
    // Đặt currentMirror theo 2 bit thấp của control register.
    void updateMirror();
    // Hook cho mapper kế thừa (105...): gọi ngay sau khi 5 bit được chốt vào thanh ghi
    // đích (0=control, 1=CHR0, 2=CHR1, 3=PRG), TRƯỚC khi updateBanks().
    virtual void onRegisterCommit(uint8_t targetReg) { (void)targetReg; }

    MIRROR currentMirror = MIRROR::ONESCREEN_LO;
    // Bank hình ảnh (CHR)
    uint8_t nCHRBankSelect4Lo = 0x00;
    uint8_t nCHRBankSelect4Hi = 0x00;
    // Bank code game (PRG)
    uint8_t nPRGBankSelect16Lo = 0x00;
    // Cái túi để nhét từng bit một (đủ 5 bit thì chốt đơn)
    uint8_t nLoadRegister = 0x00;
    // Đếm xem CPU đã nhét được mấy bit vào túi rồi
    uint8_t nLoadRegisterCount = 0x00;
    // Mặc định 0x1C (0b11100): PRG mode 3 (bank cuối cố định ở $C000)
    uint8_t nControlRegister = 0x1C;
    uint32_t pPRGBank[2] = { 0 };
    uint32_t pCHRBank[2] = { 0 };

    // Chu kỳ CPU (g_cpuCycle) của lần ghi $8000-$FFFF gần nhất.
    // MMC1 thật bỏ qua lần ghi nằm ngay chu kỳ sau một lần ghi khác
    // (lệnh read-modify-write như INC ghi 2 lần liên tiếp).
    uint64_t lastWriteCycle = 0;
};