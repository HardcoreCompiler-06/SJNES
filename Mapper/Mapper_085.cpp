#include "Mapper_085.h"

Mapper_085::Mapper_085(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    opll = OPLL_new(3579545, 44100);

    if (opll)
    {
        OPLL_setChipType(opll, OPLL_VRC7_TONE);
        OPLL_resetPatch(opll, OPLL_VRC7_TONE);
    }
    reset();
}
Mapper_085::~Mapper_085()
{
    if (opll)
    {
        OPLL_delete(opll);
        opll = nullptr;
    }
}
void Mapper_085::reset()
{
    uint8_t total8K = nPRGBanks * 2;

    prgBank[0] = 0;
    prgBank[1] = 1;
    prgBank[2] = total8K >= 2 ? total8K - 2 : 0;

    for (int i = 0; i < 8; i++)
        chrBank[i] = i;

    mirrorMode = VERTICAL;

    irqEnabled = false;
    irqEnableAfterAck = false;
    irqPending = false;
    irqLatch = 0;
    irqCounter = 0;
    irqPrescaler = 341;

    audioAddr = 0;
    for (int i = 0; i < 0x40; i++)
        audioRegs[i] = 0;
    lastVrc7Sample = 0.0f;

    for (int i = 0; i < 6; i++)
        lastVrc7Debug[i] = 0.0f;

    if (opll)
    {
        OPLL_reset(opll);
        OPLL_setChipType(opll, OPLL_VRC7_TONE);
        OPLL_resetPatch(opll, OPLL_VRC7_TONE);
    }
}

bool Mapper_085::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x8000)
        return false;

    uint32_t total8KBanks = uint32_t(nPRGBanks) * 2;
    if (total8KBanks == 0)
        total8KBanks = 1;

    uint32_t bank = 0;

    if (addr >= 0x8000 && addr <= 0x9FFF)
    {
        bank = prgBank[0] % total8KBanks;
        mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xA000 && addr <= 0xBFFF)
    {
        bank = prgBank[1] % total8KBanks;
        mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xDFFF)
    {
        bank = prgBank[2] % total8KBanks;
        mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    // $E000-$FFFF fixed last 8KB
    bank = total8KBanks - 1;
    mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
    return true;
}

bool Mapper_085::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    if ((addr & 0xF030) == 0x9030)
    {
        audioRegs[audioAddr] = data;

        if (opll)
        {
            OPLL_writeIO(opll, 0, audioAddr);
            OPLL_writeIO(opll, 1, data);
        }

        return true;
    }

    uint16_t r = addr & 0xF010;
    
    switch (r)
    {
    case 0x8000:
        prgBank[0] = data & 0x3F;
        return true;

    case 0x8010:
        prgBank[1] = data & 0x3F;
        return true;

    case 0x9000:
        prgBank[2] = data & 0x3F;
        return true;

        // Audio VRC7: cần check bằng addr gốc vì $9010 trùng PRG/audio nếu chỉ mask 0xF010
    case 0x9010:
        audioAddr = data & 0x3F;
        return true;

        // CHR banks
    case 0xA000:
        chrBank[0] = data;
        return true;

    case 0xA010:
        chrBank[1] = data;
        return true;

    case 0xB000:
        chrBank[2] = data;
        return true;

    case 0xB010:
        chrBank[3] = data;
        return true;

    case 0xC000:
        chrBank[4] = data;
        return true;

    case 0xC010:
        chrBank[5] = data;
        return true;

    case 0xD000:
        chrBank[6] = data;
        return true;

    case 0xD010:
        chrBank[7] = data;
        return true;

    case 0xE000:
    {
        uint8_t m = data & 0x03;

        if (m == 0)
            mirrorMode = VERTICAL;
        else if (m == 1)
            mirrorMode = HORIZONTAL;
        else if (m == 2)
            mirrorMode = ONESCREEN_LO;
        else
            mirrorMode = ONESCREEN_HI;

        return true;
    }

    case 0xE010:
        irqLatch = data;
        return true;

    case 0xF000:
        irqEnabled = (data & 0x02) != 0;
        irqEnableAfterAck = (data & 0x01) != 0;
        irqPending = false;

        if (irqEnabled)
        {
            irqCounter = irqLatch;
            irqPrescaler = 341;
        }

        return true;

    case 0xF010:
        irqPending = false;
        irqEnabled = irqEnableAfterAck;

        return true;
    }

    return false;
}

bool Mapper_085::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x2000)
    {
        // VRC7 vẫn bank CHR-RAM theo 1KB.
        // Nếu nCHRBanks == 0 thì coi như có 8 bank 1KB CHR-RAM.
        uint32_t total1KBanks = (nCHRBanks == 0)
            ? 8
            : uint32_t(nCHRBanks) * 8;

        uint8_t slot = addr / 0x0400;
        uint32_t bank = chrBank[slot] % total1KBanks;

        mapped_addr = bank * 0x0400 + (addr & 0x03FF);
        return true;
    }

    return false;
}

bool Mapper_085::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x2000)
    {
        // Chỉ cho ghi khi là CHR-RAM
        if (nCHRBanks == 0)
        {
            uint32_t total1KBanks = 8;

            uint8_t slot = addr / 0x0400;
            uint32_t bank = chrBank[slot] % total1KBanks;

            mapped_addr = bank * 0x0400 + (addr & 0x03FF);
            return true;
        }

        return false;
    }

    return false;
}
void Mapper_085::irqStep()
{
    if (!irqEnabled)
        return;

    irqPrescaler -= 3;

    while (irqPrescaler <= 0)
    {
        irqPrescaler += 341;

        if (irqCounter == 0xFF)
        {
            irqCounter = irqLatch;
            irqPending = true;
        }
        else
        {
            irqCounter++;
        }
    }
}

bool Mapper_085::irqState()
{
    return irqPending;
}

void Mapper_085::irqClear()
{
    irqPending = false;
}

float Mapper_085::GetExpansionAudio()
{
    if (!opll)
        return 0.0f;

    int sample = OPLL_calc(opll);

    float out = float(sample) / 32768.0f;
    out *= 1.0f;

    lastVrc7Sample = out;

    // 6 kênh VRC7 riêng
    for (int i = 0; i < 6; i++)
    {
        lastVrc7Debug[i] = float(opll->ch_out[i]) / 32768.0f;

        // scale debug riêng cho dễ nhìn, không ảnh hưởng âm thanh
        lastVrc7Debug[i] *= 2.5f;
    }

    return out;
}

void Mapper_085::GetVrc7DebugChannels(
    float& ch1, float& ch2, float& ch3,
    float& ch4, float& ch5, float& ch6
)
{
    ch1 = lastVrc7Debug[0];
    ch2 = lastVrc7Debug[1];
    ch3 = lastVrc7Debug[2];
    ch4 = lastVrc7Debug[3];
    ch5 = lastVrc7Debug[4];
    ch6 = lastVrc7Debug[5];
}

void Mapper_085::GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3)
{
    ch1 = lastVrc7Debug[0];
    ch2 = lastVrc7Debug[1];
    ch3 = lastVrc7Debug[2];
}

QString Mapper_085::GetDebugInfo()
{
    QString s;

    s += "===== Konami VRC7 / Mapper 85 =====\n";
    s += QString("PRG $8000: %1\n").arg(prgBank[0]);
    s += QString("PRG $A000: %1\n").arg(prgBank[1]);
    s += QString("PRG $C000: %1\n").arg(prgBank[2]);
    s += "$E000: fixed last\n\n";

    s += "CHR Banks 1KB:\n";
    for (int i = 0; i < 8; i++)
        s += QString("CHR[%1] = %2\n").arg(i).arg(chrBank[i]);

    s += "\n";
    s += QString("Mirror: %1\n").arg(int(mirrorMode));
    s += QString("IRQ Enabled: %1\n").arg(irqEnabled);
    s += QString("IRQ After Ack: %1\n").arg(irqEnableAfterAck);
    s += QString("IRQ Pending: %1\n").arg(irqPending);
    s += QString("IRQ Latch: %1\n").arg(irqLatch);
    s += QString("IRQ Counter: %1\n").arg(irqCounter);
    s += QString("Audio Addr: %1\n").arg(audioAddr);

    return s;
}