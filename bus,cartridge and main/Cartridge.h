#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
// Kéo Mapper vào để hiểu MIRROR là gì
#include "Mapper.h"

class Cartridge {
public:
    ~Cartridge();

    Cartridge(const std::string& sFileName);
    Cartridge(const std::vector<uint8_t>& romData);

    bool LoadFromFile(const std::string& sFileName);
    bool LoadFromData(const std::vector<uint8_t>& romData);
    bool ImageValid();
    bool ppuRead(uint16_t addr, uint8_t& data);
    bool ppuWrite(uint16_t addr, uint8_t data);
    bool cpuRead(uint16_t addr, uint8_t& data);
    bool cpuWrite(uint16_t addr, uint8_t data);

    // CHỈ CẦN KHAI BÁO BIẾN Ở ĐÂY, KHÔNG CÓ ENUM GÌ NỮA
    MIRROR mirror = MIRROR::HORIZONTAL;

    // Cục RAM 8KB cho Mapper của anh
    std::vector<uint8_t> PRGRAM;

    std::vector<uint8_t> vPRGMemory;
    std::vector<uint8_t> vCHRMemory;

    uint8_t nMapperID = 0;
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;

    std::shared_ptr<Mapper> pMapper;

private:
    bool bImageValid = false;
};