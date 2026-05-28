#include "Mapper_005.h"
#include <QString>
#include <QChar>

Mapper_005::Mapper_005(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_005::~Mapper_005()
{}

void Mapper_005::reset()
{
    prgMode = 3;
    chrMode = 3;
    extendedChrBank = 0;
    prgRamProtect1 = 0;
    prgRamProtect2 = 0;
    exRamMode = 0;
    ntMapping = 0x44; 
    fillTile = 0;
    fillAttr = 0;

    uint32_t prg8Count = GetPrg8Count();

    prgRamBank = 0;

    prgReg[0] = 0;
    prgReg[1] = 1;
    prgReg[2] = (prg8Count >= 2) ? uint8_t(prg8Count - 2) : 0;
    prgReg[3] = (prg8Count >= 1) ? uint8_t(prg8Count - 1) : 0;

    for (int i = 0; i < 8; i++)
        chrSpriteReg[i] = uint8_t(i);

    for (int i = 0; i < 4; i++)
        chrBgReg[i] = uint8_t(i);

    chrUpperBits = 0;

    for (int i = 0; i < 1024; i++)
        exRam[i] = 0;

    mulA = 0;
    mulB = 0;
    mulResult = 0;

    irqScanline = 0;
    irqStatus = 0;
    irqEnable = false;
    irqPending = false;
}

uint32_t Mapper_005::GetPrg8Count() const
{
    uint32_t count = nPRGBanks * 2;
    return count == 0 ? 1 : count;
}

uint32_t Mapper_005::GetChr1kCount() const
{
    uint32_t count = (nCHRBanks == 0) ? 8 : nCHRBanks * 8;
    return count == 0 ? 1 : count;
}

uint8_t Mapper_005::GetPrgBankReg(int index) const
{
    return prgReg[index & 3] & 0x7F;
}

uint8_t Mapper_005::GetNtSource(int ntIndex) const
{
    ntIndex &= 0x03;
    return (ntMapping >> (ntIndex * 2)) & 0x03;
}

uint8_t Mapper_005::ReadExRam(uint16_t offset) const
{
    return exRam[offset & 0x03FF];
}

void Mapper_005::WriteExRam(uint16_t offset, uint8_t data)
{
    exRam[offset & 0x03FF] = data;
}

uint8_t Mapper_005::GetFillTile() const
{
    return fillTile;
}

uint8_t Mapper_005::GetFillAttr() const
{
    // fillAttr chỉ có 2 bit, cần nhân ra 4 góc trong attribute byte
    return (fillAttr & 0x03) * 0x55;
}

uint8_t Mapper_005::GetExRamMode() const
{
    return exRamMode;
}

uint8_t Mapper_005::GetPrgRamBank() const
{
    return prgRamBank & 0x07;
}

void Mapper_005::SetExtendedChrBank(uint8_t bank)
{
    extendedChrBank = bank & 0x3F;
}

uint8_t Mapper_005::GetExtendedChrBank() const
{
    return extendedChrBank;
}
void Mapper_005::SetChrFetchModeBackground()
{
    chrFetchMode = ChrFetchMode::Background;
}

void Mapper_005::SetChrFetchModeSprite()
{
    chrFetchMode = ChrFetchMode::Sprite;
}
uint32_t Mapper_005::MapPrgBank8(uint8_t bank, uint16_t addr) const
{
    uint32_t prg8Count = GetPrg8Count();
    return (uint32_t(bank % prg8Count) * 0x2000) + (addr & 0x1FFF);
}

uint32_t Mapper_005::MapChrBank1K(uint32_t bank, uint16_t addr) const
{
    uint32_t chr1kCount = GetChr1kCount();
    return ((bank % chr1kCount) * 0x0400) + (addr & 0x03FF);
}

bool Mapper_005::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    uint32_t prg8Count = GetPrg8Count();

    // PRG RAM area. Tùy Cartridge xử lý RAM ra sao,
    // map 8KB để không crash.
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        return false;
    }

    if (addr < 0x8000)
        return false;

    // MMC5 PRG modes:
    // mode 0: 32KB
    // mode 1: 16KB + 16KB
    // mode 2: 16KB + 8KB + 8KB
    // mode 3: 8KB + 8KB + 8KB + 8KB

    switch (prgMode & 0x03)
    {
    case 0:
    {
        // 32KB at $8000-$FFFF, using $5117
        uint8_t bank = GetPrgBankReg(3) & 0x7C;
        mapped_addr = ((bank % prg8Count) * 0x2000) + (addr & 0x7FFF);
        return true;
    }

    case 1:
    {
        // $8000-$BFFF 16KB from $5115
        // $C000-$FFFF 16KB from $5117
        if (addr < 0xC000)
        {
            uint8_t bank = GetPrgBankReg(1) & 0x7E;
            mapped_addr = ((bank % prg8Count) * 0x2000) + (addr & 0x3FFF);
        }
        else
        {
            uint8_t bank = GetPrgBankReg(3) & 0x7E;
            mapped_addr = ((bank % prg8Count) * 0x2000) + (addr & 0x3FFF);
        }
        return true;
    }

    case 2:
    {
        // $8000-$BFFF 16KB from $5115
        // $C000-$DFFF 8KB from $5116
        // $E000-$FFFF 8KB from $5117
        if (addr < 0xC000)
        {
            uint8_t bank = GetPrgBankReg(1) & 0x7E;
            mapped_addr = ((bank % prg8Count) * 0x2000) + (addr & 0x3FFF);
        }
        else if (addr < 0xE000)
        {
            mapped_addr = MapPrgBank8(GetPrgBankReg(2), addr);
        }
        else
        {
            mapped_addr = MapPrgBank8(GetPrgBankReg(3), addr);
        }
        return true;
    }

    default:
    case 3:
    {
        // 4 x 8KB
        if (addr < 0xA000)
            mapped_addr = MapPrgBank8(GetPrgBankReg(0), addr);
        else if (addr < 0xC000)
            mapped_addr = MapPrgBank8(GetPrgBankReg(1), addr);
        else if (addr < 0xE000)
            mapped_addr = MapPrgBank8(GetPrgBankReg(2), addr);
        else
            mapped_addr = MapPrgBank8(GetPrgBankReg(3), addr);

        return true;
    }
    }
}

bool Mapper_005::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    // PRG RAM area
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        return false;
    }

    // MMC5 registers
    if (addr >= 0x5000 && addr <= 0x5FFF)
    {
        switch (addr)
        {
        case 0x5100:
            prgMode = data & 0x03;
            return false;

        case 0x5101:
            chrMode = data & 0x03;
            return false;

        case 0x5102:
            prgRamProtect1 = data;
            return false;

        case 0x5103:
            prgRamProtect2 = data;
            return false;

        case 0x5104:
            exRamMode = data & 0x03;
            return false;

        case 0x5105:
            ntMapping = data;
            return false;

        case 0x5106:
            fillTile = data;
            return false;

        case 0x5107:
            fillAttr = data & 0x03;
            return false;

        case 0x5113:
            prgRamBank = data & 0x07;
            return false;

        case 0x5114:
        case 0x5115:
        case 0x5116:
        case 0x5117:
            prgReg[addr - 0x5114] = data;
            return false;

        case 0x5120:
        case 0x5121:
        case 0x5122:
        case 0x5123:
        case 0x5124:
        case 0x5125:
        case 0x5126:
        case 0x5127:
            chrSpriteReg[addr - 0x5120] = data;
            return false;

        case 0x5128:
        case 0x5129:
        case 0x512A:
        case 0x512B:
            chrBgReg[addr - 0x5128] = data;
            return false;

        case 0x5130:
            chrUpperBits = data & 0x03;
            return false;

        case 0x5203:
            irqScanline = data;
            return false;

        case 0x5204:
            irqEnable = (data & 0x80) != 0;
            irqStatus &= ~0x80;
            irqPending = false;
            return false;

        case 0x5205:
            mulA = data;
            mulResult = uint16_t(mulA) * uint16_t(mulB);
            return false;

        case 0x5206:
            mulB = data;
            mulResult = uint16_t(mulA) * uint16_t(mulB);
            return false;

        default:
            break;
        }

        // ExRAM
        if (addr >= 0x5C00 && addr <= 0x5FFF)
        {
            exRam[addr & 0x03FF] = data;
            return false;
        }

        return false;
    }

    return false;
}

bool Mapper_005::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        if (nCHRBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }

        uint32_t upper = uint32_t(chrUpperBits) << 8;
        uint8_t slot = (addr >> 10) & 0x07;      // sprite dùng 0-7
        uint8_t bgSlot = (addr >> 10) & 0x03;    // background dùng 0-3
        bool bgFetch = (chrFetchMode == ChrFetchMode::Background);
        // MMC5 ExRAM mode 1: Extended Attribute
        // Mỗi tile BG có thể chọn CHR 4KB page riêng bằng ExRAM.
        // bit 0-5 của ExRAM byte = CHR 4KB page
        // bit 6-7 của ExRAM byte = palette
        if (bgFetch && exRamMode == 1)
        {
            uint32_t bank = (uint32_t(extendedChrBank & 0x3F) << 2) + bgSlot;
            mapped_addr = MapChrBank1K(bank, addr);
            return true;
        }
        switch (chrMode & 0x03)
        {
        case 0:
        {
            // 8KB mode
            uint32_t bank;

            if (bgFetch)
            {
                // BG chỉ cần 4KB, dùng $512B làm base 4KB
                uint32_t base = upper | (chrBgReg[3] & 0xFC);
                bank = base + bgSlot;
            }
            else
            {
                uint32_t base = upper | (chrSpriteReg[7] & 0xF8);
                bank = base + slot;
            }

            mapped_addr = MapChrBank1K(bank, addr);
            return true;
        }

        case 1:
        {
            // 4KB mode
            uint32_t bank;

            if (bgFetch)
            {
                uint32_t base = upper | (chrBgReg[3] & 0xFC);
                bank = base + bgSlot;
            }
            else
            {
                uint32_t base = (slot < 4)
                    ? (upper | (chrSpriteReg[3] & 0xFC))
                    : (upper | (chrSpriteReg[7] & 0xFC));

                bank = base + (slot & 0x03);
            }

            mapped_addr = MapChrBank1K(bank, addr);
            return true;
        }

        case 2:
        {
            // 2KB mode
            uint32_t bank;

            if (bgFetch)
            {
                int pair = bgSlot >> 1;
                uint32_t base = upper | (chrBgReg[pair * 2 + 1] & 0xFE);
                bank = base + (bgSlot & 0x01);
            }
            else
            {
                int pair = slot >> 1;
                int regIndex = pair * 2 + 1;
                uint32_t base = upper | (chrSpriteReg[regIndex] & 0xFE);
                bank = base + (slot & 0x01);
            }

            mapped_addr = MapChrBank1K(bank, addr);
            return true;
        }

        default:
        case 3:
        {
            // 1KB mode
            uint32_t bank;

            if (bgFetch)
            {
                // Background dùng 4 reg: $5128-$512B
                bank = upper | chrBgReg[bgSlot];
            }
            else
            {
                // Sprite dùng 8 reg: $5120-$5127
                bank = upper | chrSpriteReg[slot];
            }

            mapped_addr = MapChrBank1K(bank, addr);
            return true;
        }
        }
    }

    // Nametable / ExRAM mapping của MMC5 phức tạp
    return false;
}

bool Mapper_005::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        if (nCHRBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }

        return false;
    }

    return false;
}

bool Mapper_005::cpuReadRegister(uint16_t addr, uint8_t& data)
{
    switch (addr)
    {
    case 0x5204:
        data = irqStatus;
        irqStatus &= ~0x80;
        irqPending = false;
        return true;

    case 0x5205:
        // Multiplier result low byte
        data = mulResult & 0xFF;
        return true;

    case 0x5206:
        // Multiplier result high byte
        data = (mulResult >> 8) & 0xFF;
        return true;
    }

    return false;
}
void Mapper_005::NotifyScanline(int scanline)
{
    // Bit 6: PPU đang trong vùng render frame
    if (scanline >= 0 && scanline < 240)
        irqStatus |= 0x40;
    else
        irqStatus &= ~0x40;

    // Nếu IRQ enable và scanline khớp thanh ghi $5203
    if (irqEnable && scanline == irqScanline)
    {
        irqStatus |= 0x80;
        irqPending = true;
    }
}

bool Mapper_005::irqState()
{
    return irqPending;
}

void Mapper_005::irqClear()
{
    irqPending = false;
    irqStatus &= ~0x80;
}
MIRROR Mapper_005::mirror()
{
    // MMC5 thật dùng $5105 map từng nametable.
    // PPU hiện tại chỉ hỗ trợ MIRROR enum đơn giản.
    // Tạm suy ra kiểu gần nhất.
    uint8_t nt0 = ntMapping & 0x03;
    uint8_t nt1 = (ntMapping >> 2) & 0x03;
    uint8_t nt2 = (ntMapping >> 4) & 0x03;
    uint8_t nt3 = (ntMapping >> 6) & 0x03;

    if (nt0 == 0 && nt1 == 1 && nt2 == 0 && nt3 == 1)
        return MIRROR::VERTICAL;

    if (nt0 == 0 && nt1 == 0 && nt2 == 1 && nt3 == 1)
        return MIRROR::HORIZONTAL;

    if (nt0 == 0 && nt1 == 0 && nt2 == 0 && nt3 == 0)
        return MIRROR::ONESCREEN_LO;

    if (nt0 == 1 && nt1 == 1 && nt2 == 1 && nt3 == 1)
        return MIRROR::ONESCREEN_HI;

    return MIRROR::HARDWARE;
}

QString Mapper_005::GetDebugInfo()
{
    QString s;

    auto mirrorToString = [](MIRROR m) -> QString {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Dọc";
        case MIRROR::ONESCREEN_LO: return "One-screen thấp";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        case MIRROR::HARDWARE:     return "MMC5 nametable mapping phức tạp";
        default:                   return "Không rõ";
        }
        };

    uint32_t prg8Count = GetPrg8Count();
    uint32_t chr1kCount = GetChr1kCount();

    s += "===== MAPPER 005 - MMC5 / EXROM =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += "MMC5 là mapper nâng cao của Nintendo, có PRG/CHR banking nhiều mode, ExRAM, IRQ, multiplier và audio mở rộng.\n";
    s += "Bản hiện tại là phase 1, chưa phải MMC5 full chính xác.\n\n";

    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số PRG banks 8KB  : %1\n").arg(prg8Count);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);
    s += QString("Số CHR banks 1KB  : %1\n").arg(chr1kCount);
    s += QString("Mirroring gần đúng: %1\n").arg(mirrorToString(mirror()));

    s += "\nTHANH GHI CONTROL:\n";
    s += QString("$5100 PRG Mode     : %1\n").arg(prgMode);
    s += QString("$5101 CHR Mode     : %1\n").arg(chrMode);
    s += QString("$5104 ExRAM Mode   : %1\n").arg(exRamMode);
    s += QString("$5105 NT Mapping   : 0x%1\n").arg(ntMapping, 2, 16, QChar('0')).toUpper();
    s += QString("$5106 Fill Tile    : %1\n").arg(fillTile);
    s += QString("$5107 Fill Attr    : %1\n").arg(fillAttr);

    s += "\nPRG REGISTERS:\n";
    for (int i = 0; i < 4; i++)
    {
        s += QString("$511%1 = 0x%2 | bank=%3 | ROM/RAM bit=%4\n")
            .arg(i + 4)
            .arg(prgReg[i], 2, 16, QChar('0'))
            .arg(prgReg[i] & 0x7F)
            .arg((prgReg[i] & 0x80) ? "ROM" : "RAM")
            .toUpper();
    }

    s += "\nCHR SPRITE REGISTERS $5120-$5127:\n";
    for (int i = 0; i < 8; i++)
    {
        s += QString("CHR sprite[%1] = %2\n").arg(i).arg(chrSpriteReg[i]);
    }

    s += "\nCHR BG REGISTERS $5128-$512B:\n";
    for (int i = 0; i < 4; i++)
    {
        s += QString("CHR bg[%1] = %2\n").arg(i).arg(chrBgReg[i]);
    }

    s += QString("CHR upper bits $5130: %1\n").arg(chrUpperBits);

    s += "\nIRQ / MULTIPLIER:\n";
    s += QString("IRQ scanline : %1\n").arg(irqScanline);
    s += QString("IRQ enable   : %1\n").arg(irqEnable ? "BẬT" : "TẮT");
    s += QString("IRQ status   : 0x%1\n").arg(irqStatus, 2, 16, QChar('0')).toUpper();
    s += QString("Multiplier   : %1 x %2 = %3\n").arg(mulA).arg(mulB).arg(mulResult);

    s += "\nGHI CHÚ:\n";
    s += "MMC5 full cần sửa thêm PPU để phân biệt background/sprite CHR fetch, ExRAM nametable, fill mode, vertical split và IRQ scanline.\n";
    s += "Bản này dùng để bắt đầu boot/test game Mapper 5 trước.\n";

    return s;
}