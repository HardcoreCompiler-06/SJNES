#pragma once
#include <cstdint>

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

    virtual bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) = 0;
    virtual bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual void reset() = 0;

    // Hàm mặc định cho mọi Mapper
    virtual MIRROR mirror() { return MIRROR::HARDWARE; }

    virtual bool irqState() { return false; }
    virtual void irqClear() {}
    virtual void scanline() {}

    uint8_t nSubmapper = 0;

protected:
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;
};