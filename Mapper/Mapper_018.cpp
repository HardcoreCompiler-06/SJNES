#include "Mapper_018.h"
#include <QString>
#include <QChar>

Mapper_018::Mapper_018(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_018::~Mapper_018()
{}

void Mapper_018::SetLowNibble(uint8_t& reg, uint8_t data)
{
    reg = (reg & 0xF0) | (data & 0x0F);
}

void Mapper_018::SetHighNibble(uint8_t& reg, uint8_t data)
{
    reg = (reg & 0x0F) | ((data & 0x0F) << 4);
}

void Mapper_018::reset()
{
    prgBank[0] = 0;
    prgBank[1] = 1;

    uint32_t prg8Count = nPRGBanks * 2;
    if (prg8Count == 0)
        prg8Count = 1;

    prgBank[2] = (prg8Count >= 2) ? uint8_t(prg8Count - 2) : 0;

    for (int i = 0; i < 8; i++)
        chrBank[i] = uint8_t(i);

    mirrorMode = MIRROR::HORIZONTAL;

    irqReload = 0x0000;
    irqCounter = 0x0000;
    irqControl = 0x00;
    irqEnable = false;
    irqActive = false;
}

bool Mapper_018::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    uint32_t prg8Count = nPRGBanks * 2;
    if (prg8Count == 0)
        prg8Count = 1;

    if (addr >= 0x8000 && addr <= 0x9FFF)
    {
        mapped_addr = (prgBank[0] % prg8Count) * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xA000 && addr <= 0xBFFF)
    {
        mapped_addr = (prgBank[1] % prg8Count) * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xDFFF)
    {
        mapped_addr = (prgBank[2] % prg8Count) * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xE000 && addr <= 0xFFFF)
    {
        mapped_addr = (prg8Count - 1) * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    return false;
}

bool Mapper_018::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    // Mapper 18 decode mask: $F003
    uint16_t reg = addr & 0xF003;

    switch (reg)
    {
        // PRG bank 0: $8000-$9FFF
    case 0x8000: SetLowNibble(prgBank[0], data); break;
    case 0x8001: SetHighNibble(prgBank[0], data); break;

        // PRG bank 1: $A000-$BFFF
    case 0x8002: SetLowNibble(prgBank[1], data); break;
    case 0x8003: SetHighNibble(prgBank[1], data); break;

        // PRG bank 2: $C000-$DFFF
    case 0x9000: SetLowNibble(prgBank[2], data); break;
    case 0x9001: SetHighNibble(prgBank[2], data); break;

        // $9002 PRG RAM protect - tạm bỏ qua
    case 0x9002:
        break;

        // CHR bank 0: $0000-$03FF
    case 0xA000: SetLowNibble(chrBank[0], data); break;
    case 0xA001: SetHighNibble(chrBank[0], data); break;

        // CHR bank 1: $0400-$07FF
    case 0xA002: SetLowNibble(chrBank[1], data); break;
    case 0xA003: SetHighNibble(chrBank[1], data); break;

        // CHR bank 2: $0800-$0BFF
    case 0xB000: SetLowNibble(chrBank[2], data); break;
    case 0xB001: SetHighNibble(chrBank[2], data); break;

        // CHR bank 3: $0C00-$0FFF
    case 0xB002: SetLowNibble(chrBank[3], data); break;
    case 0xB003: SetHighNibble(chrBank[3], data); break;

        // CHR bank 4: $1000-$13FF
    case 0xC000: SetLowNibble(chrBank[4], data); break;
    case 0xC001: SetHighNibble(chrBank[4], data); break;

        // CHR bank 5: $1400-$17FF
    case 0xC002: SetLowNibble(chrBank[5], data); break;
    case 0xC003: SetHighNibble(chrBank[5], data); break;

        // CHR bank 6: $1800-$1BFF
    case 0xD000: SetLowNibble(chrBank[6], data); break;
    case 0xD001: SetHighNibble(chrBank[6], data); break;

        // CHR bank 7: $1C00-$1FFF
    case 0xD002: SetLowNibble(chrBank[7], data); break;
    case 0xD003: SetHighNibble(chrBank[7], data); break;

        // IRQ reload 16-bit, 4 nibble
    case 0xE000:
        irqReload = (irqReload & 0xFFF0) | (data & 0x0F);
        break;
    case 0xE001:
        irqReload = (irqReload & 0xFF0F) | ((data & 0x0F) << 4);
        break;
    case 0xE002:
        irqReload = (irqReload & 0xF0FF) | ((data & 0x0F) << 8);
        break;
    case 0xE003:
        irqReload = (irqReload & 0x0FFF) | ((data & 0x0F) << 12);
        break;

        // IRQ reload + acknowledge
    case 0xF000:
        irqCounter = irqReload;
        irqActive = false;
        break;

        // IRQ control/counter size + acknowledge
    case 0xF001:
        irqControl = data & 0x0F;
        irqEnable = (data & 0x01) != 0;
        irqActive = false;
        break;

        // Mirroring control
    case 0xF002:
        switch (data & 0x03)
        {
        case 0: mirrorMode = MIRROR::HORIZONTAL; break;
        case 1: mirrorMode = MIRROR::VERTICAL; break;
        case 2: mirrorMode = MIRROR::ONESCREEN_LO; break;
        case 3: mirrorMode = MIRROR::ONESCREEN_HI; break;
        }
        break;

        // Expansion sound register - tạm bỏ qua
    case 0xF003:
        break;

    default:
        break;
    }

    return false;
}

bool Mapper_018::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        if (nCHRBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }

        uint32_t chr1kCount = nCHRBanks * 8;
        if (chr1kCount == 0)
            chr1kCount = 1;

        uint8_t slot = (addr >> 10) & 0x07;
        mapped_addr = (chrBank[slot] % chr1kCount) * 0x0400 + (addr & 0x03FF);
        return true;
    }

    return false;
}

bool Mapper_018::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        if (nCHRBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }
    }

    return false;
}

MIRROR Mapper_018::mirror()
{
    return mirrorMode;
}

uint16_t Mapper_018::GetIRQMask() const
{
    // NESdev: bit F overrides E overrides T.
    // bit 3 -> 4-bit counter
    // bit 2 -> 8-bit counter
    // bit 1 -> 12-bit counter
    // none  -> 16-bit counter
    if (irqControl & 0x08)
        return 0x000F;

    if (irqControl & 0x04)
        return 0x00FF;

    if (irqControl & 0x02)
        return 0x0FFF;

    return 0xFFFF;
}

void Mapper_018::irqStep()
{
    if (!irqEnable)
        return;

    uint16_t mask = GetIRQMask();

    uint16_t before = irqCounter & mask;

    irqCounter = (irqCounter - 1) & 0xFFFF;

    // Khi counter wrap theo kích thước đang chọn thì assert IRQ.
    if (before == 0)
    {
        irqActive = true;
    }
}

bool Mapper_018::irqState()
{
    return irqActive;
}

void Mapper_018::irqClear()
{
    irqActive = false;
}

QString Mapper_018::GetDebugInfo()
{
    QString s;

    auto mirrorToString = [](MIRROR m) -> QString {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Dọc";
        case MIRROR::ONESCREEN_LO: return "One-screen thấp";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        default:                   return "Không rõ";
        }
        };

    uint32_t prg8Count = nPRGBanks * 2;
    if (prg8Count == 0)
        prg8Count = 1;

    uint32_t chr1kCount = (nCHRBanks == 0) ? 8 : nCHRBanks * 8;

    s += "===== MAPPER 018 - JALECO SS88006 =====\n\n";

    s += "THÔNG TIN CHUNG:\n";
    s += "Mapper này dùng PRG bank 8KB, CHR bank 1KB và IRQ.\n";
    s += "PRG/CHR bank number bị tách thành low nibble và high nibble.\n\n";

    s += QString("Số PRG banks 16KB : %1\n").arg(nPRGBanks);
    s += QString("Số PRG banks 8KB  : %1\n").arg(prg8Count);
    s += QString("Số CHR banks 8KB  : %1\n").arg(nCHRBanks);
    s += QString("Số CHR banks 1KB  : %1\n").arg(chr1kCount);
    s += QString("Mirroring         : %1\n").arg(mirrorToString(mirrorMode));

    s += "\nPRG BANK HIỆN TẠI:\n";
    const char* prgRange[4] = {
        "$8000-$9FFF",
        "$A000-$BFFF",
        "$C000-$DFFF",
        "$E000-$FFFF"
    };

    uint8_t prgMap[4] = {
        uint8_t(prgBank[0] % prg8Count),
        uint8_t(prgBank[1] % prg8Count),
        uint8_t(prgBank[2] % prg8Count),
        uint8_t(prg8Count - 1)
    };

    for (int i = 0; i < 4; i++)
    {
        s += QString("%1 : PRG bank 8KB = %2 | offset ROM = 0x%3\n")
            .arg(prgRange[i])
            .arg(prgMap[i])
            .arg(prgMap[i] * 0x2000, 6, 16, QChar('0'))
            .toUpper();
    }

    s += "\nCHR BANK HIỆN TẠI:\n";
    for (int i = 0; i < 8; i++)
    {
        uint16_t start = i * 0x0400;
        uint32_t realBank = chrBank[i] % chr1kCount;

        s += QString("$%1-$%2 : CHR bank 1KB = %3 | offset CHR = 0x%4\n")
            .arg(start, 4, 16, QChar('0'))
            .arg(start + 0x03FF, 4, 16, QChar('0'))
            .arg(realBank)
            .arg(realBank * 0x0400, 6, 16, QChar('0'))
            .toUpper();
    }

    s += "\nTHÔNG TIN IRQ:\n";
    s += QString("IRQ Enable   : %1\n").arg(irqEnable ? "BẬT" : "TẮT");
    s += QString("IRQ Active   : %1\n").arg(irqActive ? "CÓ" : "KHÔNG");
    s += QString("IRQ Reload   : %1\n").arg(irqReload);
    s += QString("IRQ Counter  : %1\n").arg(irqCounter);
    s += QString("IRQ Control  : 0x%1\n").arg(irqControl, 2, 16, QChar('0')).toUpper();
    s += QString("IRQ Mask     : 0x%1\n").arg(GetIRQMask(), 4, 16, QChar('0')).toUpper();

    s += "\nGHI CHÚ:\n";
    s += "$8000/$8001: PRG0 low/high nibble\n";
    s += "$8002/$8003: PRG1 low/high nibble\n";
    s += "$9000/$9001: PRG2 low/high nibble\n";
    s += "$A000-$D003: 8 thanh ghi CHR, mỗi bank 1KB\n";
    s += "$E000-$E003: IRQ reload 16-bit theo 4 nibble\n";
    s += "$F000: reload IRQ counter\n";
    s += "$F001: IRQ enable/control\n";
    s += "$F002: mirroring\n";

    return s;
}