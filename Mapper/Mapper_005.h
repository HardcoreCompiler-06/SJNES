#pragma once
#include "Mapper.h"
#include <cstdint>
#include <QString>

class Mapper_005 : public Mapper
{
public:
    Mapper_005(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_005();
    void NotifyScanline(int scanline);
    bool irqState() override;
    void irqClear() override;
    uint8_t GetPrgRamBank() const;
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    uint8_t GetNtSource(int ntIndex) const;
    uint8_t ReadExRam(uint16_t offset) const;
    void WriteExRam(uint16_t offset, uint8_t data);
    uint8_t GetFillTile() const;
    uint8_t GetFillAttr() const;
    uint8_t GetExRamMode() const;
    void reset() override;
    MIRROR mirror() override;
    enum class ChrFetchMode
    {
        Background,
        Sprite
    };
    bool cpuReadRegister(uint16_t addr, uint8_t& data) override;
    void SetChrFetchModeBackground();
    void SetChrFetchModeSprite();
    QString GetDebugInfo() override;
    void SetExtendedChrBank(uint8_t bank);
    uint8_t GetExtendedChrBank() const;
private:
    ChrFetchMode chrFetchMode = ChrFetchMode::Background;
    // MMC5 control registers
    uint8_t prgMode = 3;     // $5100
    uint8_t chrMode = 3;     // $5101
    uint8_t prgRamProtect1 = 0;
    uint8_t prgRamProtect2 = 0;
    uint8_t exRamMode = 0;   // $5104
    uint8_t ntMapping = 0;   // $5105
    uint8_t fillTile = 0;    // $5106
    uint8_t fillAttr = 0;    // $5107

    // PRG regs
    uint8_t prgRamBank = 0;  // $5113
    uint8_t prgReg[4] = { 0, 0, 0, 0 }; // $5114-$5117

    // CHR regs
    uint8_t chrSpriteReg[8] = { 0,1,2,3,4,5,6,7 }; // $5120-$5127
    uint8_t chrBgReg[4] = { 0,1,2,3 };             // $5128-$512B
    uint8_t chrUpperBits = 0;                       // $5130

    // MMC5 extra RAM
    uint8_t exRam[1024] = {};

    // Multiplier
    uint8_t mulA = 0;
    uint8_t mulB = 0;
    uint16_t mulResult = 0;

    // IRQ placeholder
    uint8_t irqScanline = 0;
    uint8_t irqStatus = 0;
    bool irqEnable = false;
    bool irqPending = false;
    uint8_t extendedChrBank = 0;
private:
    uint32_t GetPrg8Count() const;
    uint32_t GetChr1kCount() const;
    uint8_t GetPrgBankReg(int index) const;
    uint32_t MapPrgBank8(uint8_t bank, uint16_t addr) const;
    uint32_t MapChrBank1K(uint32_t bank, uint16_t addr) const;

};