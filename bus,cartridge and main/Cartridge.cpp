#include "Cartridge.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <iterator>
#include "Mapper_000.h"
#include "Mapper_002.h"
#include "Mapper_003.h"
#include "Mapper_004.h"
#include "Mapper_005.h"
#include "Mapper_087.h"
#include "Mapper_185.h"
#include "Mapper_009.h"
#include "Mapper_001.h"
#include "Mapper_007.h"
#include "Mapper_018.h"
#include "Mapper_023.h"
#include "Mapper_021.h"
#include "Mapper_024.h"
#include "Mapper_066.h"
#include "Mapper_071.h"
#include "Mapper_221.h"

Cartridge::Cartridge(const std::string& sFileName)
{
    LoadFromFile(sFileName);
}

Cartridge::Cartridge(const std::vector<uint8_t>& romData)
{
    LoadFromData(romData);
}

bool Cartridge::LoadFromFile(const std::string& sFileName)
{
    std::ifstream ifs(sFileName, std::ios::binary);

    if (!ifs.is_open())
    {
        bImageValid = false;
        return false;
    }

    std::vector<uint8_t> romData(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );

    ifs.close();

    return LoadFromData(romData);
}

bool Cartridge::LoadFromData(const std::vector<uint8_t>& romData)
{
    bImageValid = false;

    vPRGMemory.clear();
    vCHRMemory.clear();
    PRGRAM.clear();
    pMapper.reset();

    if (romData.size() < 16)
        return false;

    struct sHeader {
        char name[4];
        uint8_t prg_rom_chunks;
        uint8_t chr_rom_chunks;
        uint8_t mapper1;
        uint8_t mapper2;
        uint8_t prg_ram_size;
        uint8_t tv_system1;
        uint8_t tv_system2;
        char unused[5];
    } header;

    std::memcpy(&header, romData.data(), sizeof(sHeader));

    if (header.name[0] != 'N' || header.name[1] != 'E' ||
        header.name[2] != 'S' || header.name[3] != 0x1A)
    {
        return false;
    }

    size_t offset = 16;

    nMapperID = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);

    if (header.mapper1 & 0x01)
        mirror = VERTICAL;
    else
        mirror = HORIZONTAL;

    if (header.mapper1 & 0x04)
        offset += 512;

    nPRGBanks = header.prg_rom_chunks;
    nCHRBanks = header.chr_rom_chunks;

    size_t prgSize = size_t(nPRGBanks) * 16384;
    size_t chrSize = size_t(nCHRBanks) * 8192;

    if (romData.size() < offset + prgSize)
        return false;

    vPRGMemory.resize(prgSize);
    std::memcpy(vPRGMemory.data(), romData.data() + offset, prgSize);
    offset += prgSize;

    if (nCHRBanks == 0)
    {
        vCHRMemory.resize(8192, 0x00);
    }
    else
    {
        if (romData.size() < offset + chrSize)
            return false;

        vCHRMemory.resize(chrSize);
        std::memcpy(vCHRMemory.data(), romData.data() + offset, chrSize);
        offset += chrSize;
    }

    PRGRAM.resize(64 * 1024, 0x00);

    switch (nMapperID) {
    case 0:  pMapper = std::make_shared<Mapper_000>(nPRGBanks, nCHRBanks); break;
    case 1:  pMapper = std::make_shared<Mapper_001>(nPRGBanks, nCHRBanks); break;
    case 2:  pMapper = std::make_shared<Mapper_002>(nPRGBanks, nCHRBanks); break;
    case 3:  pMapper = std::make_shared<Mapper_003>(nPRGBanks, nCHRBanks); break;
    case 4:  pMapper = std::make_shared<Mapper_004>(nPRGBanks, nCHRBanks); break;
    case 5:  pMapper = std::make_shared<Mapper_005>(nPRGBanks, nCHRBanks); break;
    case 7:  pMapper = std::make_shared<Mapper_007>(nPRGBanks, nCHRBanks); break;
    case 9:  pMapper = std::make_shared<Mapper_009>(nPRGBanks, nCHRBanks); break;
    case 18: pMapper = std::make_shared<Mapper_018>(nPRGBanks, nCHRBanks); break;
    case 21: pMapper = std::make_shared<Mapper_021>(nPRGBanks, nCHRBanks); break;
    case 23: pMapper = std::make_shared<Mapper_023>(nPRGBanks, nCHRBanks); break;
    case 24: pMapper = std::make_shared<Mapper_024>(nPRGBanks, nCHRBanks); break;
    case 66: pMapper = std::make_shared<Mapper_066>(nPRGBanks, nCHRBanks); break;
    case 71: pMapper = std::make_shared<Mapper_071>(nPRGBanks, nCHRBanks); break;
    case 87: pMapper = std::make_shared<Mapper_087>(nPRGBanks, nCHRBanks); break;
    case 185:pMapper = std::make_shared<Mapper_185>(nPRGBanks, nCHRBanks); break;
    case 221:pMapper = std::make_shared<Mapper_221>(nPRGBanks, nCHRBanks); break;
    default:
        std::cout << "CHƯA HỖ TRỢ MAPPER ID: " << (int)nMapperID << std::endl;
        break;
    }

    if (pMapper && (header.mapper2 & 0x0C) == 0x08)
    {
        pMapper->nSubmapper = (header.prg_ram_size >> 4);
    }

    std::cout << "Mapper ID: " << (int)nMapperID << std::endl;
    std::cout << "PRG Banks: " << (int)nPRGBanks << std::endl;
    std::cout << "CHR Banks: " << (int)nCHRBanks << std::endl;

    bImageValid = (pMapper != nullptr);
    return bImageValid;
}

Cartridge::~Cartridge() {}

bool Cartridge::ImageValid()
{
    return bImageValid;
}
// 4 CỔNG GIAO TIẾP VỚI CPU VÀ PPU (Nhờ Mapper phiên dịch địa chỉ)

bool Cartridge::cpuRead(uint16_t addr, uint8_t& data)
{
    if (pMapper && pMapper->cpuReadRegister(addr, data))
    {
        return true;
    }

    // PRG RAM / WRAM $6000-$7FFF
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        uint32_t ramOffset = addr & 0x1FFF;

        if (pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(pMapper.get()))
            {
                ramOffset += uint32_t(mmc5->GetPrgRamBank()) * 0x2000;
            }
        }

        if (ramOffset < PRGRAM.size())
            data = PRGRAM[ramOffset];
        else
            data = 0x00;

        return true;
    }

    uint32_t mapped_addr = 0;

    if (pMapper && pMapper->cpuMapRead(addr, mapped_addr))
    {
        if (mapped_addr < vPRGMemory.size())
        {
            data = vPRGMemory[mapped_addr];
            return true;
        }

        data = 0x00;
        return true;
    }

    return false;
}

bool Cartridge::cpuWrite(uint16_t addr, uint8_t data) {
    if (pMapper == nullptr) return false;

    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        uint32_t ramOffset = addr & 0x1FFF;

        if (pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(pMapper.get()))
            {
                ramOffset += uint32_t(mmc5->GetPrgRamBank()) * 0x2000;
            }
        }

        if (ramOffset < PRGRAM.size())
            PRGRAM[ramOffset] = data;

        return true;
    }

    uint32_t mapped_addr = 0;
    if (pMapper->cpuMapWrite(addr, mapped_addr, data)) {
        
        return true;
    }
    return false;
}

bool Cartridge::ppuRead(uint16_t addr, uint8_t& data) {
    uint32_t mapped_addr = 0;

    if (pMapper && pMapper->ppuMapRead(addr, mapped_addr)) {
        // Special marker: mapper wants PPU internal nametable, not CHR ROM
        if (mapped_addr & 0x80000000) {
            return false;
        }

        if (mapped_addr < vCHRMemory.size()) {
            data = vCHRMemory[mapped_addr];
            return true;
        }

        data = 0x00;
        return true;
    }

    return false;
}

bool Cartridge::ppuWrite(uint16_t addr, uint8_t data) {
    uint32_t mapped_addr = 0;
    if (pMapper == nullptr) return false;

    if (pMapper->ppuMapWrite(addr, mapped_addr)) {
        if (mapped_addr & 0x80000000) {
            return false;
        }

        if (mapped_addr < vCHRMemory.size()) {
            vCHRMemory[mapped_addr] = data;
        }

        return true;
    }

    return false;
}