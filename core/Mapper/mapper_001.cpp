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

#include "Mapper_001.h"

// Bộ đếm CPU cycle toàn cục, định nghĩa trong EmuWorker.cpp
extern uint64_t g_cpuCycle;

Mapper_001::Mapper_001(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_001::~Mapper_001() {}

void Mapper_001::updateMirror() {
    switch (nControlRegister & 0x03) {
    case 0: currentMirror = MIRROR::ONESCREEN_LO; break;
    case 1: currentMirror = MIRROR::ONESCREEN_HI; break;
    case 2: currentMirror = MIRROR::VERTICAL;     break;
    case 3: currentMirror = MIRROR::HORIZONTAL;   break;
    }
}

void Mapper_001::updateBanks() {
    // ---- CHR (chỉ khi là CHR ROM; CHR RAM không đổi bank) ----
    if (nCHRBanks > 0) {
        uint32_t total4k = (uint32_t)nCHRBanks * 2;   // nCHRBanks tính theo 8KB
        if (nControlRegister & 0x10) {                // 2 bank 4KB riêng
            pCHRBank[0] = (nCHRBankSelect4Lo % total4k) * 0x1000;
            pCHRBank[1] = (nCHRBankSelect4Hi % total4k) * 0x1000;
        }
        else {                                        // 1 bank 8KB (bỏ bit thấp)
            uint32_t b = (nCHRBankSelect4Lo & 0x1E) % total4k;
            pCHRBank[0] = b * 0x1000;
            pCHRBank[1] = pCHRBank[0] + 0x1000;
        }
    }
    else {
        pCHRBank[0] = 0;
        pCHRBank[1] = 0x1000;
    }

    // ---- PRG ----
    uint32_t n = nPRGBanks ? nPRGBanks : 1;           // tránh chia cho 0
    uint8_t mode = (nControlRegister >> 2) & 0x03;
    if (mode <= 1) {                                  // 32KB switch
        uint32_t b = nPRGBankSelect16Lo & 0x0E;
        pPRGBank[0] = (b % n) * 0x4000;
        pPRGBank[1] = ((b + 1) % n) * 0x4000;
    }
    else if (mode == 2) {                             // bank đầu cố định ở $8000
        pPRGBank[0] = 0;
        pPRGBank[1] = (nPRGBankSelect16Lo % n) * 0x4000;
    }
    else {                                            // bank cuối cố định ở $C000
        pPRGBank[0] = (nPRGBankSelect16Lo % n) * 0x4000;
        pPRGBank[1] = (n - 1) * 0x4000;
    }
}

void Mapper_001::reset() {
    nControlRegister = 0x1C; // Mode mặc định: PRG 16KB cố định ở 0xC000
    nLoadRegister = 0x00;
    nLoadRegisterCount = 0x00;
    nCHRBankSelect4Lo = 0;
    nCHRBankSelect4Hi = 0;
    nPRGBankSelect16Lo = 0;
    lastWriteCycle = 0;
    updateMirror();
    updateBanks();
}

bool Mapper_001::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000) {
        // Bỏ qua lần ghi ở chu kỳ ngay sau một lần ghi khác
        // (lần ghi thứ 2 của lệnh INC/DEC/ASL... vào ROM).
        uint64_t now = g_cpuCycle;
        bool consecutive = (now >= lastWriteCycle) && ((now - lastWriteCycle) < 2);
        lastWriteCycle = now;
        if (consecutive) return false;

        // BIT 7 = 1 -> reset bộ ghi dịch, ép PRG mode 3
        if (data & 0x80) {
            nLoadRegister = 0x00;
            nLoadRegisterCount = 0;
            nControlRegister |= 0x0C;
            updateBanks();   // phải áp dụng ngay, không đợi lần chốt kế tiếp
        }
        else {
            // Đút từng bit vào shift register
            nLoadRegister >>= 1;
            nLoadRegister |= (data & 0x01) << 4;
            nLoadRegisterCount++;

            // Đủ 5 bit -> chốt vào thanh ghi đích
            if (nLoadRegisterCount == 5) {
                uint8_t nTargetRegister = (addr >> 13) & 0x03;

                if (nTargetRegister == 0) {          // $8000-$9FFF: control
                    nControlRegister = nLoadRegister & 0x1F;
                    updateMirror();
                }
                else if (nTargetRegister == 1) {     // $A000-$BFFF: CHR bank 0
                    nCHRBankSelect4Lo = nLoadRegister & 0x1F;
                }
                else if (nTargetRegister == 2) {     // $C000-$DFFF: CHR bank 1
                    nCHRBankSelect4Hi = nLoadRegister & 0x1F;
                }
                else {                               // $E000-$FFFF: PRG bank
                    nPRGBankSelect16Lo = nLoadRegister & 0x0F;
                }

                onRegisterCommit(nTargetRegister);
                updateBanks();
                nLoadRegister = 0x00;
                nLoadRegisterCount = 0;
            }
        }
        return false;
    }
    return false;
}

bool Mapper_001::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        mapped_addr = pPRGBank[0] + (addr & 0x3FFF);
        return true;
    }
    if (addr >= 0xC000 && addr <= 0xFFFF) {
        mapped_addr = pPRGBank[1] + (addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_001::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        if (nCHRBanks == 0) {
            mapped_addr = addr;
            return true;
        }
        else {
            if (addr <= 0x0FFF) {
                mapped_addr = pCHRBank[0] + (addr & 0x0FFF);
                return true;
            }
            if (addr >= 0x1000 && addr <= 0x1FFF) {
                mapped_addr = pCHRBank[1] + (addr & 0x0FFF);
                return true;
            }
        }
    }
    return false;
}

bool Mapper_001::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        if (nCHRBanks == 0) {
            mapped_addr = addr;
            return true;
        }
    }
    return false;
}

std::string Mapper_001::GetDebugInfo()
{
    std::string s;

    auto mirrorToString = [](MIRROR m) -> std::string {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Doc";
        case MIRROR::ONESCREEN_LO: return "One-screen thap";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        default:                   return "Hardware / Khong ro";
        }
        };

    uint8_t chrMode = (nControlRegister >> 4) & 0x01;
    uint8_t prgMode = (nControlRegister >> 2) & 0x03;
    uint8_t mirrorMode = nControlRegister & 0x03;

    s += "===== MAPPER 001 - MMC1 =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "Kieu mirroring    : " + mirrorToString(currentMirror) + "\n";

    s += "\nTHANH GHI DIEU KHIEN MMC1:\n";
    s += "Control Register  : 0x" + HexStr(nControlRegister, 2) + "\n";
    s += "Mirror bits       : " + std::to_string(mirrorMode) + "\n";
    s += "PRG mode          : " + std::to_string(prgMode) + " - ";

    if (prgMode == 0 || prgMode == 1)
        s += "32KB switch tai $8000-$FFFF\n";
    else if (prgMode == 2)
        s += "Co dinh bank dau tai $8000, doi bank tai $C000\n";
    else
        s += "Doi bank tai $8000, co dinh bank cuoi tai $C000\n";

    s += "CHR mode          : " + std::to_string(chrMode) + " - ";
    s += (chrMode ? "2 bank CHR 4KB rieng\n" : "1 bank CHR 8KB\n");

    s += "\nBO GHI DICH MMC1:\n";
    s += "Load Register     : 0x" + HexStr(nLoadRegister, 2) + "\n";
    s += "So bit da nap     : " + std::to_string(nLoadRegisterCount) + " / 5\n";

    s += "\nTHANH GHI CHON BANK:\n";
    s += "CHR 4KB Low       : " + std::to_string(nCHRBankSelect4Lo) + "\n";
    s += "CHR 4KB High      : " + std::to_string(nCHRBankSelect4Hi) + "\n";
    s += "PRG 16KB          : " + std::to_string(nPRGBankSelect16Lo) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";
    s += "$8000-$BFFF : offset ROM = 0x" + HexStr(pPRGBank[0], 6) +
        " | PRG bank 16KB = " + std::to_string(pPRGBank[0] / 0x4000) + "\n";

    s += "$C000-$FFFF : offset ROM = 0x" + HexStr(pPRGBank[1], 6) +
        " | PRG bank 16KB = " + std::to_string(pPRGBank[1] / 0x4000) + "\n";

    s += "\nCHR BANK HIEN TAI:\n";

    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM\n";
    }
    else
    {
        s += "$0000-$0FFF : offset CHR = 0x" + HexStr(pCHRBank[0], 6) +
            " | CHR bank 4KB = " + std::to_string(pCHRBank[0] / 0x1000) + "\n";

        s += "$1000-$1FFF : offset CHR = 0x" + HexStr(pCHRBank[1], 6) +
            " | CHR bank 4KB = " + std::to_string(pCHRBank[1] / 0x1000) + "\n";
    }

    s += "\nGHI CHU:\n";
    s += "MMC1 nap tung bit vao shift register. Khi du 5 bit, mapper moi chot vao thanh ghi dich.\n";
    s += "Vi vay Load Register va so bit da nap co the thay doi rat nhanh khi game dang chay.\n";

    return s;
}