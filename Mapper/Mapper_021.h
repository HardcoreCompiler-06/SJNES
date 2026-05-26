#pragma once
#include "Mapper.h"
#include <QString>
class Mapper_021 : public Mapper {
public:
    Mapper_021(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_021();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    MIRROR mirror() override;
    void reset() override;
    QString GetDebugInfo() override;
    bool irqState() override;
    void irqClear() override;
    void irqStep();

private:
    // Physical addresses (đã tính sẵn, PPU/CPU dùng trực tiếp)
    uint32_t pPRGBank[4];    // 4 x 8KB = 32KB PRG
    uint32_t pCHRBank[8];    // 8 x 1KB = 8KB CHR  ← thêm dòng này

    // Bank select registers
    uint8_t  nPRGBankSelect[2];   // Chọn bank cho slot 0 và 1
    uint16_t nCHRBankSelect[8];   // 9-bit, chỉ khai báo 1 lần
    int     mirrormode;
    uint8_t nPRGSwapMode;

    // IRQ
    int     nIRQPrescaler;
    uint8_t nIRQReload;
    uint8_t nIRQCounter;
    bool    bIRQEnable;
    bool    bIRQEnableAfterAck;
    bool    bIRQActive;
};