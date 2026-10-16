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
class Mapper_004 : public Mapper {
public:
    MIRROR mirror() override;

    Mapper_004(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_004();
    std::string GetDebugInfo() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    void ClockA12() override;
    bool irqState() override;
    void irqClear() override;

protected:

    MIRROR mirrormode = MIRROR::HORIZONTAL;
    uint8_t nTargetRegister = 0x00;
    bool bPRGBankMode = false;
    bool bCHRInversion = false;
    uint32_t pRegister[8];
    uint32_t pCHRBank[8];
    uint32_t pPRGBank[4];


    bool bIRQActive = false;
    bool bIRQEnable = false;
    bool bIRQUpdate = false;
    uint16_t nIRQCounter = 0x0000;
    uint16_t nIRQLatch = 0x0000;
};