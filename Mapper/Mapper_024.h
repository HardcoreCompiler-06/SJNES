#pragma once
#include "Mapper.h"
#include <cstdint>
#include <QString>
class Mapper_024 : public Mapper {
public:
    Mapper_024(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_024();
    bool muteVRC6 = false;
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;
    MIRROR mirror() override;
    void GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3) override;
    void irqStep() override;
    bool irqState() override;
    void irqClear() override;
    QString GetDebugInfo() override;
    float GetExpansionAudio() override;
    uint8_t GetNtSource(int index) const;
private:
    // VRC6 PRG
    uint8_t prg16Bank = 0; // $8000-$BFFF, 16KB
    uint8_t prg8Bank  = 0; // $C000-$DFFF, 8KB

    // VRC6 CHR registers R0-R7, 1KB each
    uint8_t chrBank[8] = {};

    // $B003 PPU control
    uint8_t b003 = 0x20;
    uint8_t ppuBankingMode = 0;       // bits 0-1
    bool useChrRomNametables = false; // bit 4
    bool chrA10Control = true;        // bit 5
    bool prgRamEnable = false;        // bit 7
    MIRROR mirrorMode = MIRROR::VERTICAL;

    // VRC6 nametable banking
    // ntSource[] is used for CIRAM A/B selection when CHR-ROM nametables are off.
    // ntChrBank[] is used when $B003 bit 4 enables CHR-ROM nametables.
    uint8_t ntSource[4] = { 0, 1, 0, 1 };
    uint8_t ntChrBank[4] = { 0, 0, 0, 0 };

    // IRQ
    bool irqActive = false;
    bool irqEnable = false;
    bool irqEnableAfterAck = false;
    bool irqModeCycle = false;
    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    int irqPrescaler = 341;

    struct VRC6Pulse
    {
        uint8_t volume = 0;
        uint8_t duty = 0;
        bool mode = false;
        bool enable = false;

        uint16_t period = 0;
        uint16_t timer = 0;
        uint8_t dutyPos = 0;

        float output = 0.0f;
    };

    struct VRC6Saw
    {
        uint8_t rate = 0;
        bool enable = false;

        uint16_t period = 0;
        uint16_t timer = 0;

        uint8_t step = 0;
        uint8_t accumulator = 0;

        float output = 0.0f;
    };

    VRC6Pulse pulse1;
    VRC6Pulse pulse2;
    VRC6Saw saw;

    void UpdateB003(uint8_t data);

    uint8_t GetChrRegisterForPpuAddress(uint16_t addr) const;
    uint8_t GetChrBankForPpuAddress(uint16_t addr) const;

    void ClockVRC6Audio();
    void WritePulse(VRC6Pulse& p, uint16_t reg, uint8_t data);
    void WriteSaw(uint16_t reg, uint8_t data);
};
