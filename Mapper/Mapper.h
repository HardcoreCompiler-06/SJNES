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

    // === CÁC HÀM QUẢN LÝ IRQ MỚI (Cho Namco 163 / Mapper 019) ===
    virtual void clock() {}
    virtual bool irqState() { return false; }
    virtual void irqClear() {}

    // === CÁC HÀM CŨ PHẢI GIỮ LẠI (Cho MMC3, MMC5, VRC6...) ===
    virtual MIRROR mirror() { return HARDWARE; }
    virtual void ClockA12() {}
    virtual void irqStep() {}
    virtual float GetExpansionAudio() { return 0.0f; }

    // === CÁC HÀM ĐỌC/GHI CƠ BẢN ===
    virtual bool cpuReadRegister(uint16_t addr, uint8_t& data) { return false; }
    virtual void GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3) { ch1 = 0.0f; ch2 = 0.0f; ch3 = 0.0f; }

    virtual QString GetDebugInfo() {
        QString s = "===== DEBUG MAPPER =====\n";
        s += QString("PRG Banks: %1\n").arg(nPRGBanks);
        s += QString("CHR Banks: %1\n").arg(nCHRBanks);
        s += QString("Submapper: %1\n").arg(nSubmapper);
        return s;
    }

    virtual bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) = 0;
    virtual bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) = 0;

    virtual void reset() = 0;

    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;
    uint8_t nSubmapper = 0;
};