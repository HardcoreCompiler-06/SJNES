#pragma once
#include <cstdint>
#include <memory>
#include "PPU.h"
#include "APU.h"
#include "Cartridge.h"
#include "CPU6502.h" // Nhớ include CPU để gọi hàm clock()

class Bus {
public:
    Bus() {
        n_apu.bus = this;
        for (int i = 0; i < 2048; i++) {
            ram[i] = 0xFF;
        }
    }

    uint8_t controller_state = 0x00;
    uint8_t controller_strobe = 0x00;
    uint8_t controller_shift = 0x00;

    // === CÁC BIẾN QUẢN LÝ DMA CHUẨN XÁC ===
    uint8_t dma_page = 0x00;
    uint8_t dma_addr = 0x00;
    uint8_t dma_data = 0x00;
    bool dma_dummy = true;
    bool dma_transfer = false;

    uint32_t system_clock_counter = 0; // Bộ đếm nhịp hệ thống

    PPU* ppu = nullptr;
    CPU6502* cpu = nullptr; // Con trỏ CPU
    APU n_apu;

    // Bộ nhớ RAM nội bộ (2KB)
    uint8_t ram[2048];

    // === KHE CẮM BĂNG GAME ===
    std::shared_ptr<Cartridge> cart;
    void insertCartridge(const std::shared_ptr<Cartridge>& cartridge) {
        this->cart = cartridge;
        if (ppu != nullptr) {
            ppu->ConnectCartridge(cartridge);
        }
    }

    void cpuWrite(uint16_t addr, uint8_t data) {
        if (cart != nullptr && cart->cpuWrite(addr, data)) {
            return;
        }

        if (addr >= 0x0000 && addr <= 0x1FFF) {
            ram[addr & 0x07FF] = data;
        }
        else if (addr >= 0x2000 && addr <= 0x3FFF) {
            if (ppu != nullptr)
                ppu->cpuWrite(addr & 0x0007, data);
        }
        // =========================================================
        // 2. SỬA DMA CÓ THỜI GIAN THẬT (REAL TIME OAM DMA)
        // =========================================================
        else if (addr == 0x4014) {
            dma_page = data;
            dma_addr = 0x00;
            dma_dummy = true;
            dma_transfer = true;
            for (int i = 0; i < 256; i++) {
                if (ppu != nullptr)
                    ppu->OAM[i] = cpuRead(((uint16_t)dma_page << 8) | i);
            }
        }
        else if (addr == 0x4016) {
            controller_strobe = data & 0x01;
            controller_shift = controller_state;
        }
        else if ((addr >= 0x4000 && addr <= 0x4013) || addr == 0x4015 || addr == 0x4017) {
            n_apu.cpuWrite(addr, data);
        }
    }

    uint8_t cpuRead(uint16_t addr) {
        uint8_t data = 0x00;
        if (cart != nullptr && cart->cpuRead(addr, data)) {
            return data;
        }

        if (addr >= 0x0000 && addr <= 0x1FFF) {
            data = ram[addr & 0x07FF];
        }
        else if (addr == 0x4016) {
            if (controller_strobe) {
                data = (controller_state & 0x80) ? 1 : 0;
            }
            else {
                data = (controller_shift & 0x80) ? 1 : 0;
                controller_shift <<= 1;
            }
            data |= 0x40;
        }
        else if (addr >= 0x2000 && addr <= 0x3FFF) {
            if (ppu != nullptr)
                data = ppu->cpuRead(addr & 0x0007);
        }
        else if (addr == 0x4015) {
            data = n_apu.readStatus();
        }
        return data;
    }

    // =========================================================
    // HÀM CLOCK ĐIỀU PHỐI NHỊP CPU/PPU VÀ DMA CHUẨN XÁC
    // =========================================================
    void clock() {
        if (ppu != nullptr) ppu->Step();

        if (system_clock_counter % 3 == 0) {
            if (dma_transfer) {
                if (dma_dummy) {
                    if (system_clock_counter % 2 == 1) dma_dummy = false;
                }
                else {
                    if (system_clock_counter % 2 == 0) {
                        dma_data = cpuRead(((uint16_t)dma_page << 8) | dma_addr);
                    }
                    else {
                        if (ppu != nullptr) ppu->OAM[dma_addr] = dma_data;
                        dma_addr++;
                        if (dma_addr == 0x00) {
                            dma_transfer = false;
                            dma_dummy = true;
                        }
                    }
                }
            }
            else {
                if (cpu != nullptr) {
                    cpu->clock();
                    n_apu.Step();
                }
            }
        }

        if (cart != nullptr && cart->pMapper != nullptr) {
            if (cart->pMapper->irqState()) {
                cart->pMapper->irqClear();
                if (cpu != nullptr) cpu->irq();
            }
        }

        system_clock_counter++;
    }
};