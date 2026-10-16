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

#include "Mapper_021.h"
#include <cstdio>
#include "LogBuffer.h"

Mapper_021::Mapper_021(uint8_t prgBanks, uint8_t chrBanks, VRC4Variant variant)
    : Mapper(prgBanks, chrBanks), nVariant(variant) {
    reset();
}

Mapper_021::~Mapper_021() {}

void Mapper_021::reset() {
    nPRGSwapMode = 0;
    nPRGBankSelect[0] = 0;
    nPRGBankSelect[1] = 1;

    // Reset IRQ
    nIRQReload = 0;
    nIRQCounter = 0;
    nIRQPrescaler = 341;
    bIRQEnable = false;
    bIRQEnableAfterAck = false;
    bIRQCycleMode = false;
    bIRQActive = false;
    mirrormode = 0;

    // Khởi tạo CHR select
    for (int i = 0; i < 8; i++) {
        nCHRBankSelect[i] = 0;
    }

    // PRG: 8KB x 4. Bank cuối ($E000) luôn cố định, bank áp chót cố định ở
    // $C000 (swap mode 0) hoặc $8000 (swap mode 1).
    updateBanks();
}

void Mapper_021::updateBanks() {
    uint32_t nPrg = (uint32_t)nPRGBanks * 2;   // số bank 8KB
    if (nPrg < 2) nPrg = 2;

    uint32_t last2 = (nPrg - 2) * 0x2000;
    uint32_t last1 = (nPrg - 1) * 0x2000;
    uint32_t b0 = (nPRGBankSelect[0] % nPrg) * 0x2000;
    uint32_t b1 = (nPRGBankSelect[1] % nPrg) * 0x2000;

    if (nPRGSwapMode == 0) {
        pPRGBank[0] = b0;
        pPRGBank[1] = b1;
        pPRGBank[2] = last2;
    }
    else {
        pPRGBank[0] = last2;
        pPRGBank[1] = b1;
        pPRGBank[2] = b0;
    }
    pPRGBank[3] = last1;

    for (int i = 0; i < 8; i++) {
        // nCHRShift = 1 cho VRC2a (mapper 22): bit thấp của giá trị bank bị bỏ qua
        uint32_t sel = (uint32_t)nCHRBankSelect[i] >> nCHRShift;
        if (nCHRBanks != 0) sel %= ((uint32_t)nCHRBanks * 8);
        pCHRBank[i] = sel * 0x0400;
    }
}

bool Mapper_021::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) { mapped_addr = pPRGBank[0] + (addr & 0x1FFF); return true; }
    if (addr >= 0xA000 && addr <= 0xBFFF) { mapped_addr = pPRGBank[1] + (addr & 0x1FFF); return true; }
    if (addr >= 0xC000 && addr <= 0xDFFF) { mapped_addr = pPRGBank[2] + (addr & 0x1FFF); return true; }
    if (addr >= 0xE000 && addr <= 0xFFFF) { mapped_addr = pPRGBank[3] + (addr & 0x1FFF); return true; }
    return false;
}

bool Mapper_021::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr < 0x8000) return false;

    // Decode latch (0..3) trong block. Vì mapper 21 gộp cả VRC4a lẫn VRC4c
    // mà iNES 1.0 không phân biệt được, ta OR cả 2 kiểu wiring:
    //   VRC4a: A0 = bit1, A1 = bit2
    //   VRC4c: A0 = bit6, A1 = bit7
    uint8_t latch = ((addr >> 1) & 1) | (((addr >> 2) & 1) << 1)
        | ((addr >> 6) & 1) | (((addr >> 7) & 1) << 1);

    writeReg(addr & 0xF000, latch, data);
    return false;
}

// Xử lý ghi thanh ghi sau khi đã decode: block = $8000/$9000/.../$F000,
// latch = 0..3. Dùng chung cho Mapper_021 và các mapper con (Mapper_025).
void Mapper_021::writeReg(uint16_t block, uint8_t latch, uint8_t data) {
    switch (block) {
    case 0x8000:   // PRG bank 0 ($8000-$8003 đều như nhau)
        nPRGBankSelect[0] = data & 0x1F;
        break;

    case 0xA000:   // PRG bank 1 ($A000-$A003 đều như nhau)
        nPRGBankSelect[1] = data & 0x1F;
        break;

    case 0x9000:
        if (latch < 2) {
            // $9000/$9001: Mirroring
            switch (data & 0x03) {
            case 0: mirrormode = (int)MIRROR::VERTICAL;     break;
            case 1: mirrormode = (int)MIRROR::HORIZONTAL;   break;
            case 2: mirrormode = (int)MIRROR::ONESCREEN_LO; break;
            case 3: mirrormode = (int)MIRROR::ONESCREEN_HI; break;
            }
        }
        else {
            // $9002/$9003: bit1 = PRG swap mode (bit0 = WRAM enable, bỏ qua)
            nPRGSwapMode = (data >> 1) & 1;
        }
        break;

        // CHR: mỗi block chứa 2 bank 1KB, mỗi bank gồm 2 thanh ghi (nibble thấp/cao)
        //   latch 0: bank chẵn - nibble thấp (4 bit)
        //   latch 1: bank chẵn - nibble cao  (5 bit)
        //   latch 2: bank lẻ   - nibble thấp
        //   latch 3: bank lẻ   - nibble cao
    case 0xB000: case 0xC000: case 0xD000: case 0xE000: {
        int idx = ((block - 0xB000) >> 12) * 2 + (latch >> 1);
        if (latch & 1)
            nCHRBankSelect[idx] = (nCHRBankSelect[idx] & 0x00F) | ((uint16_t)(data & 0x1F) << 4);
        else
            nCHRBankSelect[idx] = (nCHRBankSelect[idx] & 0x1F0) | (data & 0x0F);
        break;
    }

               // IRQ
    case 0xF000:
        switch (latch) {
        case 0: // Latch low 4 bits
            nIRQReload = (nIRQReload & 0xF0) | (data & 0x0F);
            break;
        case 1: // Latch high 4 bits
            nIRQReload = (nIRQReload & 0x0F) | ((data & 0x0F) << 4);
            break;
        case 2: // Control
            bIRQEnableAfterAck = (data & 0x01) != 0;
            bIRQEnable = (data & 0x02) != 0;
            bIRQCycleMode = (data & 0x04) != 0;
            if (bIRQEnable) {
                nIRQCounter = nIRQReload;
                nIRQPrescaler = 341;
            }
            bIRQActive = false;
            break;
        case 3: // Acknowledge
            bIRQActive = false;
            bIRQEnable = bIRQEnableAfterAck;
            break;
        }
        break;
    }

    updateBanks();
}

void Mapper_021::irqStep() {
    if (!bIRQEnable) return;

    // VRC4: counter đếm LÊN, tràn từ 0xFF thì nạp lại latch và bắn IRQ
    auto clockCounter = [this]() {
        if (nIRQCounter == 0xFF) {
            nIRQCounter = nIRQReload;
            bIRQActive = true;
        }
        else {
            nIRQCounter++;
        }
        };

    if (bIRQCycleMode) {
        // Cycle mode: clock counter mỗi chu kỳ CPU
        clockCounter();
    }
    else {
        // Scanline mode: prescaler trừ 3 mỗi chu kỳ CPU, khi <= 0 thì cộng 341
        // và clock counter (341/3 ~ 113.67 chu kỳ CPU = 1 scanline)
        nIRQPrescaler -= 3;
        if (nIRQPrescaler <= 0) {
            nIRQPrescaler += 341;
            clockCounter();
        }
    }
}

bool Mapper_021::irqState() {
    return bIRQActive;
}

void Mapper_021::irqClear() {
    bIRQActive = false;
}

bool Mapper_021::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        uint8_t bank = addr / 0x0400;
        uint16_t offset = addr & 0x03FF;
        mapped_addr = pCHRBank[bank] + offset;
        return true;
    }
    return false;
}

bool Mapper_021::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        if (nCHRBanks == 0) { // Nếu dùng CHR RAM
            mapped_addr = addr;
            return true;
        }
    }
    return false;
}

MIRROR Mapper_021::mirror() {
    return (MIRROR)mirrormode;
}

std::string Mapper_021::GetDebugInfo()
{
    std::string s;

    auto mirrorToString = [](MIRROR m) -> std::string {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Doc";
        case MIRROR::ONESCREEN_LO: return "One-screen thap";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        default:                   return "Khong ro";
        }
        };

    s += "===== MAPPER 021 - VRC4 (decode OR ca 4a + 4c; variant tham so = ";
    s += (nVariant == VRC4Variant::VRC4a ? "VRC4a" : "VRC4c");
    s += ") =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So PRG banks 8KB  : " + std::to_string(nPRGBanks * 2) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "So CHR banks 1KB  : " + std::to_string(nCHRBanks == 0 ? 8 : nCHRBanks * 8) + "\n";
    s += "Mirroring         : " + mirrorToString((MIRROR)mirrormode) + "\n";

    s += "\nCHE DO PRG:\n";
    s += "PRG Swap Mode     : " + std::to_string(nPRGSwapMode) + "\n";
    if (nPRGSwapMode == 0)
    {
        s += "$8000-$9FFF doi bang PRG select 0\n";
        s += "$A000-$BFFF doi bang PRG select 1\n";
        s += "$C000-$DFFF co dinh bank ap chot\n";
    }
    else
    {
        s += "$8000-$9FFF co dinh bank ap chot\n";
        s += "$A000-$BFFF doi bang PRG select 1\n";
        s += "$C000-$DFFF doi bang PRG select 0\n";
    }

    s += "\nTHANH GHI CHON PRG:\n";
    s += "PRG Select 0      : " + std::to_string(nPRGBankSelect[0]) + "\n";
    s += "PRG Select 1      : " + std::to_string(nPRGBankSelect[1]) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";
    const char* prgRange[4] = {
        "$8000-$9FFF",
        "$A000-$BFFF",
        "$C000-$DFFF",
        "$E000-$FFFF"
    };

    for (int i = 0; i < 4; i++)
    {
        s += std::string(prgRange[i]) + " : offset ROM = 0x" + HexStr(pPRGBank[i], 6) +
            " | PRG bank 8KB = " + std::to_string(pPRGBank[i] / 0x2000) + "\n";
    }

    s += "\nCHR BANK HIEN TAI:\n";
    for (int i = 0; i < 8; i++)
    {
        uint16_t start = i * 0x0400;
        s += "$" + HexStr(start, 4) + "-$" + HexStr(start + 0x03FF, 4) +
            " : select=" + std::to_string(nCHRBankSelect[i]) +
            " | offset CHR=0x" + HexStr(pCHRBank[i], 6) +
            " | CHR bank 1KB=" + std::to_string(pCHRBank[i] / 0x0400) + "\n";
    }

    s += "\nTHONG TIN IRQ VRC4:\n";
    s += std::string("IRQ Enable          : ") + (bIRQEnable ? "BAT" : "TAT") + "\n";
    s += std::string("IRQ Enable After ACK: ") + (bIRQEnableAfterAck ? "CO" : "KHONG") + "\n";
    s += std::string("IRQ Mode            : ") + (bIRQCycleMode ? "Cycle" : "Scanline") + "\n";
    s += std::string("IRQ Active          : ") + (bIRQActive ? "CO" : "KHONG") + "\n";
    s += "IRQ Reload          : " + std::to_string(nIRQReload) + "\n";
    s += "IRQ Counter         : " + std::to_string(nIRQCounter) + "\n";
    s += "IRQ Prescaler       : " + std::to_string(nIRQPrescaler) + "\n";

    s += "\nGHI CHU:\n";
    s += "Mapper 021 la VRC4. PRG bank 8KB, CHR bank 1KB.\n";
    s += "IRQ: counter 8-bit dem LEN, scanline mode dung prescaler tru 3/chu ky CPU.\n";

    return s;
}   