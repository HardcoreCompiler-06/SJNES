#include "Mapper_024.h"
#include <qdebug.h>
Mapper_024::Mapper_024(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_024::~Mapper_024() {}

void Mapper_024::reset()
{
    prg16Bank = 0;
    prg8Bank = 0;

    for (int i = 0; i < 8; i++)
        chrBank[i] = i;

    UpdateB003(0x20);

    irqActive = false;
    irqEnable = false;
    irqEnableAfterAck = false;
    irqModeCycle = false;
    irqLatch = 0;
    irqCounter = 0;
    irqPrescaler = 341;

    pulse1 = VRC6Pulse{};
    pulse2 = VRC6Pulse{};
    saw = VRC6Saw{};
}

void Mapper_024::UpdateB003(uint8_t data)
{
    b003 = data;
    ppuBankingMode = data & 0x03;
    useChrRomNametables = (data & 0x10) != 0;
    chrA10Control = (data & 0x20) != 0;
    prgRamEnable = (data & 0x80) != 0;

    auto SetNametable = [&](int index, uint8_t page)
    {
        ntSource[index & 0x03] = page & 0x01;
    };

    auto SetChrNametable = [&](int index, uint8_t page)
    {
        ntChrBank[index & 0x03] = page;
    };

    if (useChrRomNametables)
    {
        // Same nametable/CHR-ROM banking rules as Mesen's VRC6::UpdatePpuBanking().
        switch (b003 & 0x2F)
        {
        case 0x20:
        case 0x27:
            SetChrNametable(0, chrBank[6] & 0xFE);
            SetChrNametable(1, (chrBank[6] & 0xFE) | 1);
            SetChrNametable(2, chrBank[7] & 0xFE);
            SetChrNametable(3, (chrBank[7] & 0xFE) | 1);
            break;

        case 0x23:
        case 0x24:
            SetChrNametable(0, chrBank[6] & 0xFE);
            SetChrNametable(1, chrBank[7] & 0xFE);
            SetChrNametable(2, (chrBank[6] & 0xFE) | 1);
            SetChrNametable(3, (chrBank[7] & 0xFE) | 1);
            break;

        case 0x28:
        case 0x2F:
            SetChrNametable(0, chrBank[6] & 0xFE);
            SetChrNametable(1, chrBank[6] & 0xFE);
            SetChrNametable(2, chrBank[7] & 0xFE);
            SetChrNametable(3, chrBank[7] & 0xFE);
            break;

        case 0x2B:
        case 0x2C:
            SetChrNametable(0, (chrBank[6] & 0xFE) | 1);
            SetChrNametable(1, (chrBank[7] & 0xFE) | 1);
            SetChrNametable(2, (chrBank[6] & 0xFE) | 1);
            SetChrNametable(3, (chrBank[7] & 0xFE) | 1);
            break;

        default:
            switch (b003 & 0x07)
            {
            case 0:
            case 6:
            case 7:
                SetChrNametable(0, chrBank[6]);
                SetChrNametable(1, chrBank[6]);
                SetChrNametable(2, chrBank[7]);
                SetChrNametable(3, chrBank[7]);
                break;

            case 1:
            case 5:
                SetChrNametable(0, chrBank[4]);
                SetChrNametable(1, chrBank[5]);
                SetChrNametable(2, chrBank[6]);
                SetChrNametable(3, chrBank[7]);
                break;

            case 2:
            case 3:
            case 4:
                SetChrNametable(0, chrBank[6]);
                SetChrNametable(1, chrBank[7]);
                SetChrNametable(2, chrBank[6]);
                SetChrNametable(3, chrBank[7]);
                break;
            }
            break;
        }
        return;
    }

    // CIRAM nametable banking. mirrorMode is still maintained for the PPU's
    // simple mirroring path, while ntSource[] keeps the per-nametable mapping.
    switch (b003 & 0x2F)
    {
    case 0x20:
    case 0x27:
        mirrorMode = MIRROR::VERTICAL;
        SetNametable(0, 0); SetNametable(1, 1); SetNametable(2, 0); SetNametable(3, 1);
        break;

    case 0x23:
    case 0x24:
        mirrorMode = MIRROR::HORIZONTAL;
        SetNametable(0, 0); SetNametable(1, 0); SetNametable(2, 1); SetNametable(3, 1);
        break;

    case 0x28:
    case 0x2F:
        mirrorMode = MIRROR::ONESCREEN_LO;
        SetNametable(0, 0); SetNametable(1, 0); SetNametable(2, 0); SetNametable(3, 0);
        break;

    case 0x2B:
    case 0x2C:
        mirrorMode = MIRROR::ONESCREEN_HI;
        SetNametable(0, 1); SetNametable(1, 1); SetNametable(2, 1); SetNametable(3, 1);
        break;

    default:
        switch (b003 & 0x07)
        {
        case 0:
        case 6:
        case 7:
            SetNametable(0, chrBank[6]);
            SetNametable(1, chrBank[6]);
            SetNametable(2, chrBank[7]);
            SetNametable(3, chrBank[7]);
            // This can be more complex than a simple MIRROR enum, but keeping the
            // closest common value helps older PPU code paths.
            mirrorMode = ((chrBank[6] & 1) == 0 && (chrBank[7] & 1) == 1) ? MIRROR::VERTICAL :
                         ((chrBank[6] & 1) == 0 ? MIRROR::ONESCREEN_LO : MIRROR::ONESCREEN_HI);
            break;

        case 1:
        case 5:
            SetNametable(0, chrBank[4]);
            SetNametable(1, chrBank[5]);
            SetNametable(2, chrBank[6]);
            SetNametable(3, chrBank[7]);
            mirrorMode = MIRROR::VERTICAL;
            break;

        case 2:
        case 3:
        case 4:
            SetNametable(0, chrBank[6]);
            SetNametable(1, chrBank[7]);
            SetNametable(2, chrBank[6]);
            SetNametable(3, chrBank[7]);
            mirrorMode = MIRROR::VERTICAL;
            break;
        }
        break;
    }
}

uint8_t Mapper_024::GetChrRegisterForPpuAddress(uint16_t addr) const
{
    const uint8_t slot = (addr >> 10) & 0x07;

    switch (ppuBankingMode & 0x03)
    {
    default:
    case 0:
        // 8 x 1KB: R0 R1 R2 R3 R4 R5 R6 R7
        return slot;

    case 1:
        // 4 x 2KB: R0 R0 R1 R1 R2 R2 R3 R3
        return slot >> 1;

    case 2:
    case 3:
        // 4 x 1KB + 2 x 2KB:
        // R0 R1 R2 R3 R4 R4 R5 R5
        if (slot < 4)
            return slot;
        return 4 + ((slot - 4) >> 1);
    }
}

uint8_t Mapper_024::GetChrBankForPpuAddress(uint16_t addr) const
{
    const uint8_t slot = (addr >> 10) & 0x07;
    const uint8_t reg = GetChrRegisterForPpuAddress(addr) & 0x07;
    uint8_t bank = chrBank[reg];

    const bool uses2K =
        ((ppuBankingMode & 0x03) == 1) ||
        (((ppuBankingMode & 0x03) == 2 || (ppuBankingMode & 0x03) == 3) && slot >= 4);

    if (uses2K && chrA10Control)
        bank = (bank & 0xFE) | (slot & 0x01);

    return bank;
}

bool Mapper_024::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    const uint32_t prg8Count = nPRGBanks * 2;
    if (prg8Count == 0)
        return false;

    if (addr >= 0x8000 && addr <= 0xBFFF)
    {
        const uint32_t bank16Count = nPRGBanks;
        mapped_addr = ((prg16Bank % bank16Count) * 0x4000) + (addr & 0x3FFF);
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xDFFF)
    {
        mapped_addr = ((prg8Bank % prg8Count) * 0x2000) + (addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xE000 && addr <= 0xFFFF)
    {
        mapped_addr = ((prg8Count - 1) * 0x2000) + (addr & 0x1FFF);
        return true;
    }

    return false;
}

bool Mapper_024::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;
    const uint16_t reg = addr & 0xF003;

    switch (reg)
    {
    case 0x8000:
    case 0x8001:
    case 0x8002:
    case 0x8003:
        prg16Bank = data & 0x0F;
        return false;

    case 0x9000:
    case 0x9001:
    case 0x9002:
        WritePulse(pulse1, reg, data);
        return false;

    case 0x9003:
        return false;

    case 0xA000:
    case 0xA001:
    case 0xA002:
        WritePulse(pulse2, reg, data);
        return false;

    case 0xA003:
        return false;

    case 0xB000:
    case 0xB001:
    case 0xB002:
        WriteSaw(reg, data);
        return false;

    case 0xB003:
        UpdateB003(data);
        return false;

    case 0xC000:
    case 0xC001:
    case 0xC002:
    case 0xC003:
        prg8Bank = data & 0x1F;
        return false;

    case 0xD000: chrBank[0] = data; UpdateB003(b003); return false;
    case 0xD001: chrBank[1] = data; UpdateB003(b003); return false;
    case 0xD002: chrBank[2] = data; UpdateB003(b003); return false;
    case 0xD003: chrBank[3] = data; UpdateB003(b003); return false;

    case 0xE000: chrBank[4] = data; UpdateB003(b003); return false;
    case 0xE001: chrBank[5] = data; UpdateB003(b003); return false;
    case 0xE002: chrBank[6] = data; UpdateB003(b003); return false;
    case 0xE003: chrBank[7] = data; UpdateB003(b003); return false;

    case 0xF000:
        irqLatch = data;
        irqActive = false;
        return false;

    case 0xF001:
        irqEnableAfterAck = (data & 0x01) != 0;
        irqEnable = (data & 0x02) != 0;
        irqModeCycle = (data & 0x04) != 0;

        irqActive = false;
        irqPrescaler = 341;

        if (irqEnable)
            irqCounter = irqLatch;

        return false;

    case 0xF002:
        irqActive = false;
        irqEnable = irqEnableAfterAck;
        return false;
    }

    return false;
}

bool Mapper_024::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        const uint8_t bank = GetChrBankForPpuAddress(addr);
        const uint32_t chr1kCount = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

        if (chr1kCount == 0)
            return false;

        mapped_addr = ((bank % chr1kCount) * 0x0400) + (addr & 0x03FF);
        return true;
    }

    // VRC6 can map nametable space ($2000-$2FFF, mirrored at $3000-$3EFF)
    // to CHR ROM/RAM when $B003 bit 4 is set. This is required by games that
    // use CHR-ROM nametables for split backgrounds.
    if (addr >= 0x2000 && addr <= 0x3EFF && useChrRomNametables)
    {
        const uint16_t ntAddr = (addr - 0x2000) & 0x0FFF;
        const uint8_t ntIndex = (ntAddr >> 10) & 0x03;
        const uint16_t offset = ntAddr & 0x03FF;

        const uint32_t chr1kCount = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);
        if (chr1kCount == 0)
            return false;

        mapped_addr = ((ntChrBank[ntIndex] % chr1kCount) * 0x0400) + offset;
        return true;
    }

    return false;
}

bool Mapper_024::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        const uint8_t bank = GetChrBankForPpuAddress(addr);
        const uint32_t chr1kCount = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

        if (chr1kCount == 0)
            return false;

        mapped_addr = ((bank % chr1kCount) * 0x0400) + (addr & 0x03FF);
        return nCHRBanks == 0;
    }

    if (addr >= 0x2000 && addr <= 0x3EFF && useChrRomNametables)
    {
        const uint16_t ntAddr = (addr - 0x2000) & 0x0FFF;
        const uint8_t ntIndex = (ntAddr >> 10) & 0x03;
        const uint16_t offset = ntAddr & 0x03FF;

        const uint32_t chr1kCount = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);
        if (chr1kCount == 0)
            return false;

        mapped_addr = ((ntChrBank[ntIndex] % chr1kCount) * 0x0400) + offset;
        return nCHRBanks == 0;
    }

    return false;
}

void Mapper_024::irqStep()
{
    ClockVRC6Audio();

    if (!irqEnable)
        return;

    auto clockIRQCounter = [&]()
    {
        if (irqCounter == 0xFF)
        {
            irqCounter = irqLatch;
            irqActive = true;
        }
        else
        {
            irqCounter++;
        }
    };

    if (irqModeCycle)
    {
        clockIRQCounter();
    }
    else
    {
        irqPrescaler -= 3;

        if (irqPrescaler <= 0)
        {
            irqPrescaler += 341;
            clockIRQCounter();
        }
    }
}

bool Mapper_024::irqState()
{
    return irqActive;
}

void Mapper_024::irqClear()
{
    irqActive = false;
}

MIRROR Mapper_024::mirror()
{
    return mirrorMode;
}

void Mapper_024::WritePulse(VRC6Pulse& p, uint16_t reg, uint8_t data)
{
    switch (reg & 0x0003)
    {
    case 0:
        p.volume = data & 0x0F;
        p.duty = (data >> 4) & 0x07;
        p.mode = (data & 0x80) != 0;
        break;

    case 1:
        p.period = (p.period & 0x0F00) | data;
        break;

    case 2:
        p.period = (p.period & 0x00FF) | ((data & 0x0F) << 8);
        p.enable = (data & 0x80) != 0;

        if (!p.enable)
        {
            p.output = 0.0f;
            p.dutyPos = 0;
        }
        break;
    }
}

void Mapper_024::WriteSaw(uint16_t reg, uint8_t data)
{
    switch (reg & 0x0003)
    {
    case 0:
        saw.rate = data & 0x3F;
        break;

    case 1:
        saw.period = (saw.period & 0x0F00) | data;
        break;

    case 2:
        saw.period = (saw.period & 0x00FF) | ((data & 0x0F) << 8);
        saw.enable = (data & 0x80) != 0;

        if (!saw.enable)
        {
            saw.output = 0.0f;
            saw.step = 0;
            saw.accumulator = 0;
        }
        break;
    }
}

void Mapper_024::ClockVRC6Audio()
{
    auto clockPulse = [](VRC6Pulse& p)
    {
        if (!p.enable || p.period < 8)
        {
            p.output = 0.0f;
            return;
        }

        if (p.timer == 0)
        {
            p.timer = p.period;
            p.dutyPos = (p.dutyPos + 1) & 0x0F;
        }
        else
        {
            p.timer--;
        }

        if (p.mode)
        {
            p.output = static_cast<float>(p.volume) / 15.0f;
        }
        else
        {
            const bool high = p.dutyPos <= p.duty;
            p.output = high ? (static_cast<float>(p.volume) / 15.0f) : 0.0f;
        }
    };

    clockPulse(pulse1);
    clockPulse(pulse2);

    if (!saw.enable || saw.period < 8)
    {
        saw.output = 0.0f;
        return;
    }

    if (saw.timer == 0)
    {
        saw.timer = saw.period;

        if (saw.step == 0)
            saw.accumulator = 0;

        if (saw.step & 1)
            saw.accumulator += saw.rate;

        saw.step++;
        if (saw.step >= 14)
        {
            saw.step = 0;
            saw.accumulator = 0;
        }

        saw.output = static_cast<float>((saw.accumulator >> 3) & 0x1F) / 31.0f;
    }
    else
    {
        saw.timer--;
    }
}

float Mapper_024::GetExpansionAudio()
{
    if (muteVRC6)
        return 0.0f;

    float mix = 0.0f;

    mix += pulse1.output * 0.35f;
    mix += pulse2.output * 0.35f;
    mix += saw.output * 0.45f;

    return mix * 0.35f;
}
