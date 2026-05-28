#pragma once
#include <cstdint>
#include <QString>

enum MIRROR {
    HARDWARE,
    HORIZONTAL,
    VERTICAL,
    ONESCREEN_LO,
    ONESCREEN_HI,
};
class Mapper {
public:
    Mapper(uint8_t prgBanks, uint8_t chrBanks);
    virtual ~Mapper();

    virtual bool cpuReadRegister(uint16_t addr, uint8_t& data)
    {
        return false;
    }
    virtual void GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3)
    {
        ch1 = 0.0f;
        ch2 = 0.0f;
        ch3 = 0.0f;
    }
    virtual QString GetDebugInfo()
    {
        QString s;
        s += "===== DEBUG MAPPER CƠ BẢN =====\n\n";
        s += "Mapper này chưa có phần debug chi tiết riêng.\n";
        s += "Muốn xem bank cụ thể thì cần viết GetDebugInfo() riêng cho mapper này.\n\n";
        s += QString("Số PRG banks: %1\n").arg(nPRGBanks);
        s += QString("Số CHR banks: %1\n").arg(nCHRBanks);
        s += QString("Submapper   : %1\n").arg(nSubmapper);
        return s;
    }
    virtual bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) = 0;
    virtual bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual void reset() = 0;
    virtual MIRROR mirror() { return MIRROR::HARDWARE; }
    virtual void irqStep() {}
    virtual bool irqState() { return false; }
    virtual void irqClear() {}
    virtual void scanline() {}
    virtual void ClockA12() {}
    virtual float GetExpansionAudio() { return 0.0f; }
    uint8_t nSubmapper = 0;
protected:
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;
};