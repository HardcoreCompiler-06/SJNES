#pragma once
#include "Mapper.h"
#include <QString>
class Mapper_004 : public Mapper {
public:
    MIRROR mirror() override;

    Mapper_004(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_004();
    QString GetDebugInfo() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    void ClockA12() override;
    // Còi báo IRQ chuẩn
    bool irqState() override;
    void irqClear() override;

private:

    MIRROR mirrormode = MIRROR::HORIZONTAL;
    uint8_t nTargetRegister = 0x00;
    bool bPRGBankMode = false;
    bool bCHRInversion = false;

    uint32_t pRegister[8]; // 8 Thanh ghi R0 - R7 chứa số thứ tự Bank
    uint32_t pCHRBank[8];  // 8 Cửa sổ 1KB cho hình ảnh
    uint32_t pPRGBank[4];  // 4 Cửa sổ 8KB cho code CPU

    
    bool bIRQActive = false;
    bool bIRQEnable = false;
    bool bIRQUpdate = false;
    uint16_t nIRQCounter = 0x0000;
    uint16_t nIRQLatch = 0x0000;
};