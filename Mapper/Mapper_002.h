#pragma once
#include "Mapper.h"
#include <QString>

class Mapper_002 : public Mapper {
public:
    Mapper_002(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_002();
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    QString GetDebugInfo() override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;

private:
    uint8_t nPRGBankSelectLo = 0x00;
};