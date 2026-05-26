#include "Mapper_004.h"
#include <Qdebug>
Mapper_004::Mapper_004(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_004::~Mapper_004() {}

void Mapper_004::reset() {
    nTargetRegister = 0; bPRGBankMode = false; bCHRInversion = false; mirrormode = MIRROR::HORIZONTAL;
    bIRQActive = false; bIRQEnable = false; bIRQUpdate = false;
    nIRQCounter = 0; nIRQLatch = 0;

    for (int i = 0; i < 8; i++)
    {
        pRegister[i] = 0;
        pCHRBank[i] = i * 0x0400;
    }
    pPRGBank[0] = 0 * 0x2000;
    pPRGBank[1] = 1 * 0x2000;
    pPRGBank[2] = (nPRGBanks * 2 - 2) * 0x2000;
    pPRGBank[3] = (nPRGBanks * 2 - 1) * 0x2000;
}

bool Mapper_004::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) { mapped_addr = pPRGBank[0] + (addr & 0x1FFF); return true; }
    if (addr >= 0xA000 && addr <= 0xBFFF) { mapped_addr = pPRGBank[1] + (addr & 0x1FFF); return true; }
    if (addr >= 0xC000 && addr <= 0xDFFF) { mapped_addr = pPRGBank[2] + (addr & 0x1FFF); return true; }
    if (addr >= 0xE000 && addr <= 0xFFFF) { mapped_addr = pPRGBank[3] + (addr & 0x1FFF); return true; }
    return false;
}

// 2. HÀM CHO PPU: BỘ LỌC A12 (TỈ LỆ 3) VÀ ĐỌC HÌNH ẢNH (CHR)
bool Mapper_004::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    // Chia Bank hình ảnh
    if (addr >= 0x0000 && addr <= 0x03FF) { mapped_addr = pCHRBank[0] + (addr & 0x03FF); return true; }
    if (addr >= 0x0400 && addr <= 0x07FF) { mapped_addr = pCHRBank[1] + (addr & 0x03FF); return true; }
    if (addr >= 0x0800 && addr <= 0x0BFF) { mapped_addr = pCHRBank[2] + (addr & 0x03FF); return true; }
    if (addr >= 0x0C00 && addr <= 0x0FFF) { mapped_addr = pCHRBank[3] + (addr & 0x03FF); return true; }
    if (addr >= 0x1000 && addr <= 0x13FF) { mapped_addr = pCHRBank[4] + (addr & 0x03FF); return true; }
    if (addr >= 0x1400 && addr <= 0x17FF) { mapped_addr = pCHRBank[5] + (addr & 0x03FF); return true; }
    if (addr >= 0x1800 && addr <= 0x1BFF) { mapped_addr = pCHRBank[6] + (addr & 0x03FF); return true; }
    if (addr >= 0x1C00 && addr <= 0x1FFF) { mapped_addr = pCHRBank[7] + (addr & 0x03FF); return true; }
    return false;
}
bool Mapper_004::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if (!(addr & 0x0001)) {
            nTargetRegister = data & 0x07;
            bPRGBankMode = (data & 0x40);
            bCHRInversion = (data & 0x80);
        }
        else {
            pRegister[nTargetRegister] = data;
        }
        
        uint32_t num_prg_banks = nPRGBanks * 2;
        uint32_t num_chr_banks = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

        auto wrapPRG = [&](uint32_t b) { return (b % num_prg_banks) * 0x2000; };
        auto wrapCHR = [&](uint32_t b) { return (b % num_chr_banks) * 0x0400; };

        // Xếp cửa sổ Hình Ảnh (CHR)
        if (bCHRInversion) {
            pCHRBank[0] = wrapCHR(pRegister[2]);
            pCHRBank[1] = wrapCHR(pRegister[3]);
            pCHRBank[2] = wrapCHR(pRegister[4]);
            pCHRBank[3] = wrapCHR(pRegister[5]);
            pCHRBank[4] = wrapCHR(pRegister[0] & 0xFE);
            pCHRBank[5] = wrapCHR((pRegister[0] & 0xFE) + 1);
            pCHRBank[6] = wrapCHR(pRegister[1] & 0xFE);
            pCHRBank[7] = wrapCHR((pRegister[1] & 0xFE) + 1);
        }
        else {
            pCHRBank[0] = wrapCHR(pRegister[0] & 0xFE);
            pCHRBank[1] = wrapCHR((pRegister[0] & 0xFE) + 1);
            pCHRBank[2] = wrapCHR(pRegister[1] & 0xFE);
            pCHRBank[3] = wrapCHR((pRegister[1] & 0xFE) + 1);
            pCHRBank[4] = wrapCHR(pRegister[2]);
            pCHRBank[5] = wrapCHR(pRegister[3]);
            pCHRBank[6] = wrapCHR(pRegister[4]);
            pCHRBank[7] = wrapCHR(pRegister[5]);
        }

        // Xếp cửa sổ Code (PRG)
        if (bPRGBankMode) {
            pPRGBank[2] = wrapPRG(pRegister[6] & 0x3F);
            pPRGBank[0] = wrapPRG(num_prg_banks - 2);
        }
        else {
            pPRGBank[0] = wrapPRG(pRegister[6] & 0x3F);
            pPRGBank[2] = wrapPRG(num_prg_banks - 2);
        }
        pPRGBank[1] = wrapPRG(pRegister[7] & 0x3F);
        pPRGBank[3] = wrapPRG(num_prg_banks - 1);

        return false;
    }
    else if (addr >= 0xA000 && addr <= 0xBFFF) {
        if (!(addr & 0x0001)) {
            if (data & 0x01) mirrormode = MIRROR::HORIZONTAL;
            else mirrormode = MIRROR::VERTICAL;
        }
        else {
        }
        return false;
    }
    else if (addr >= 0xC000 && addr <= 0xDFFF) {
        if (!(addr & 0x0001)) {
            nIRQLatch = data;
        }
        else {
            bIRQUpdate = true;
        }
        return false;
    }
    else if (addr >= 0xE000 && addr <= 0xFFFF) {
        if (!(addr & 0x0001)) {
            bIRQEnable = false;
            bIRQActive = false; 
        }
        else {
            bIRQEnable = true;
        }
        return false;
    }
    return false;
}

bool Mapper_004::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x03FF) { mapped_addr = pCHRBank[0] + (addr & 0x03FF); return true; }
    if (addr >= 0x0400 && addr <= 0x07FF) { mapped_addr = pCHRBank[1] + (addr & 0x03FF); return true; }
    if (addr >= 0x0800 && addr <= 0x0BFF) { mapped_addr = pCHRBank[2] + (addr & 0x03FF); return true; }
    if (addr >= 0x0C00 && addr <= 0x0FFF) { mapped_addr = pCHRBank[3] + (addr & 0x03FF); return true; }
    if (addr >= 0x1000 && addr <= 0x13FF) { mapped_addr = pCHRBank[4] + (addr & 0x03FF); return true; }
    if (addr >= 0x1400 && addr <= 0x17FF) { mapped_addr = pCHRBank[5] + (addr & 0x03FF); return true; }
    if (addr >= 0x1800 && addr <= 0x1BFF) { mapped_addr = pCHRBank[6] + (addr & 0x03FF); return true; }
    if (addr >= 0x1C00 && addr <= 0x1FFF) { mapped_addr = pCHRBank[7] + (addr & 0x03FF); return true; }
    return false;
}
// MMC3 IRQ must be held as an IRQ source/line.
// Do not discard IRQ just because I flag is currently set,
// otherwise split-screen HUD may jitter in games like "Contra Force".
// lỗi hud có thể do CPU sai IRQ (mất 7 tháng để tìm ra lỗi)
void Mapper_004::ClockA12()
{
    if (nIRQCounter == 0 || bIRQUpdate)
    {
        nIRQCounter = nIRQLatch;
        bIRQUpdate = false;
    }
    else
    {
        nIRQCounter--;
    }

    if (nIRQCounter == 0 && bIRQEnable)
    {
        bIRQActive = true;
    }
}
bool Mapper_004::irqState() { return bIRQActive; }
void Mapper_004::irqClear() { bIRQActive = false; }

MIRROR Mapper_004::mirror() {
    return mirrormode;
}
QString Mapper_004::GetDebugInfo()
{
    QString s;

    auto mirrorToString = [](MIRROR m) -> QString {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Dọc";
        case MIRROR::ONESCREEN_LO: return "One-screen thấp";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        default:                   return "Hardware / Không rõ";
        }
        };

    s += "===== MAPPER 004 - MMC3 =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số PRG banks 8KB  : %1\n").arg(nPRGBanks * 2);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);
    s += QString("Số CHR banks 1KB  : %1\n").arg(nCHRBanks == 0 ? 8 : nCHRBanks * 8);
    s += QString("Mirroring         : %1\n").arg(mirrorToString(mirrormode));

    s += "\nTHANH GHI BANK SELECT:\n";
    s += QString("Target Register R : %1\n").arg(nTargetRegister);
    s += QString("PRG Bank Mode     : %1\n").arg(bPRGBankMode ? "1 - cố định bank áp chót tại $8000" : "0 - cố định bank áp chót tại $C000");
    s += QString("CHR Inversion     : %1\n").arg(bCHRInversion ? "BẬT - đảo vùng CHR $0000/$1000" : "TẮT");

    s += "\n8 THANH GHI R0-R7:\n";
    for (int i = 0; i < 8; i++)
    {
        s += QString("R%1 = %2\n").arg(i).arg(pRegister[i]);
    }

    s += "\nPRG BANK HIỆN TẠI:\n";
    s += QString("$8000-$9FFF : offset ROM = 0x%1 | PRG bank 8KB = %2\n")
        .arg(pPRGBank[0], 6, 16, QChar('0'))
        .arg(pPRGBank[0] / 0x2000)
        .toUpper();

    s += QString("$A000-$BFFF : offset ROM = 0x%1 | PRG bank 8KB = %2\n")
        .arg(pPRGBank[1], 6, 16, QChar('0'))
        .arg(pPRGBank[1] / 0x2000)
        .toUpper();

    s += QString("$C000-$DFFF : offset ROM = 0x%1 | PRG bank 8KB = %2\n")
        .arg(pPRGBank[2], 6, 16, QChar('0'))
        .arg(pPRGBank[2] / 0x2000)
        .toUpper();

    s += QString("$E000-$FFFF : offset ROM = 0x%1 | PRG bank 8KB = %2\n")
        .arg(pPRGBank[3], 6, 16, QChar('0'))
        .arg(pPRGBank[3] / 0x2000)
        .toUpper();

    s += "\nCHR BANK HIỆN TẠI:\n";

    if (nCHRBanks == 0)
    {
        s += "Game dùng CHR RAM. Các offset dưới đây là offset trong CHR RAM.\n";
    }

    const char* chrRanges[8] = {
        "$0000-$03FF",
        "$0400-$07FF",
        "$0800-$0BFF",
        "$0C00-$0FFF",
        "$1000-$13FF",
        "$1400-$17FF",
        "$1800-$1BFF",
        "$1C00-$1FFF"
    };

    for (int i = 0; i < 8; i++)
    {
        s += QString("%1 : offset CHR = 0x%2 | CHR bank 1KB = %3\n")
            .arg(chrRanges[i])
            .arg(pCHRBank[i], 6, 16, QChar('0'))
            .arg(pCHRBank[i] / 0x0400)
            .toUpper();
    }

    s += "\nTHÔNG TIN IRQ MMC3:\n";
    s += QString("IRQ Enable        : %1\n").arg(bIRQEnable ? "BẬT" : "TẮT");
    s += QString("IRQ Active        : %1\n").arg(bIRQActive ? "CÓ" : "KHÔNG");
    s += QString("IRQ Reload/Update : %1\n").arg(bIRQUpdate ? "CÓ" : "KHÔNG");
    s += QString("IRQ Counter       : %1\n").arg(nIRQCounter);
    s += QString("IRQ Latch         : %1\n").arg(nIRQLatch);

    s += "\nGIẢI THÍCH NHANH:\n";
    s += "MMC3 dùng R0-R5 để chọn CHR bank và R6-R7 để chọn PRG bank.\n";
    s += "PRG bank có kích thước 8KB, nên offset ROM thường nhảy theo 0x2000.\n";
    s += "CHR bank có kích thước 1KB, nên offset CHR thường nhảy theo 0x0400.\n";
    s += "IRQ MMC3 thường dùng để chia màn hình, ví dụ HUD cố định và nền cuộn riêng.\n";

    return s;
}