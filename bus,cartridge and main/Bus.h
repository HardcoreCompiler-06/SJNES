#pragma once
#include <cstdint>
#include <memory>
#include "PPU.h"
#include "APU.h"
#include "Cartridge.h"
#include "CPU6502.h"

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
    uint8_t controller_state2 = 0x00;
    uint8_t controller_shift2 = 0x00;


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

            if (controller_strobe) {
                controller_shift = controller_state;    // tay 1
                controller_shift2 = controller_state2;  // tay 2
            }
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
            // tay 1
            if (controller_strobe) {
                data = (controller_state & 0x80) ? 1 : 0;
            }
            else {
                data = (controller_shift & 0x80) ? 1 : 0;
                controller_shift <<= 1;
            }

            data |= 0x40;
        }
        else if (addr == 0x4017) {
            // tay 2
            if (controller_strobe) {
                data = (controller_state2 & 0x80) ? 1 : 0;
            }
            else {
                data = (controller_shift2 & 0x80) ? 1 : 0;
                controller_shift2 <<= 1;
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
    // Trong file Bus.h, sửa lại hàm clock()
    void clock() {
        // 1. CPU xử lý trước
        if (system_clock_counter % 3 == 0) {
            if (dma_transfer) {
                // ... (Giữ nguyên logic DMA cũ của bạn)
            }
            else {
                if (cpu != nullptr) {
                    cpu->clock();
                    n_apu.Step();
                }
            }

            // 2. Mapper đếm nhịp
            if (cart != nullptr && cart->pMapper != nullptr) {
                cart->pMapper->clock();
            }
        }

        // 3. PPU chạy sau cùng để đảm bảo đồng bộ trạng thái
        if (ppu != nullptr) ppu->Step();

        // [Fix IRQ]: Đưa về dạng Level-Triggered chuẩn
        // Xử lý IRQ Level-Triggered:
        if (cart != nullptr && cart->pMapper != nullptr) {
            if (cart->pMapper->irqState()) {
                if (cpu != nullptr) {
                    cpu->SetIrqSource(CPU6502::IRQ_EXTERNAL);

                    // [THÊM MỚI]: Báo cho PPU biết Mapper vừa yêu cầu ngắt.
                    // Nếu PPU của bạn có hàm đặt cờ update VRAM/Nametable, hãy gọi nó ở đây.
                    // Ví dụ: if (ppu != nullptr) ppu->ForceUpdate(); 
                }
            }
            else {
                if (cpu != nullptr) cpu->ClearIrqSource(CPU6502::IRQ_EXTERNAL);
            }
        }

        system_clock_counter++;
        }
    };