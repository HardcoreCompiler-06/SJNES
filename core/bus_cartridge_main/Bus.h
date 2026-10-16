/*
 * SJNES - Nintendo Entertainment System Emulator
 * Copyright (C) 2026 Chiến
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/*
	Derived from olc::NES - System Bus
	by David Barr (javidx9), OneLoneCoder.com
	Source: https://github.com/OneLoneCoder/olcNES

	License (OLC-3)
	~~~~~~~~~~~~~~~

	Copyright 2018-2019 OneLoneCoder.com

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	1. Redistributions or derivations of source code must retain the above
	copyright notice, this list of conditions and the following disclaimer.

	2. Redistributions or derivative works in binary form must reproduce
	the above copyright notice. This list of conditions and the following
	disclaimer must be reproduced in the documentation and/or other
	materials provided with the distribution.

	3. Neither the name of the copyright holder nor the names of its
	contributors may be used to endorse or promote products derived
	from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <memory>
#include "PPU.h"
#include "APU.h"
#include "Cartridge.h"
#include "CPU6502.h"
#include <functional>
#include <QDebug>
class Bus {
public:
    Bus() {
        n_apu.bus = this;
        for (int i = 0; i < 2048; i++) {
            ram[i] = 0xFF;
        }
    }

    bool nsfMode = false;
    std::array<uint8_t, 65536> nsfMemory{};
    bool nsfBankMode = false;
    std::array<uint8_t, 8> nsfBanks{};
    std::vector<uint8_t> nsfBankData;
    std::function<bool(uint16_t, uint8_t)> nsfExpansionWrite;
    std::function<bool(uint16_t, uint8_t&)> nsfExpansionRead;

    void EnableNSFMode(
        uint16_t loadAddress,
        const std::vector<uint8_t>& data,
        const uint8_t* initialBanks = nullptr

    )
    {
        nsfMode = true;
        nsfExpansionWrite = nullptr;
        nsfMemory.fill(0x00);

        nsfBankMode = false;
        nsfBanks.fill(0x00);
        nsfBankData.clear();

        bool useBankSwitching = false;

        if (initialBanks)
        {
            for (int i = 0; i < 8; i++)
            {
                if (initialBanks[i] != 0)
                {
                    useBankSwitching = true;
                    break;
                }
            }
        }

        if (useBankSwitching)
        {
            nsfBankMode = true;

            int padding = loadAddress & 0x0FFF;

            nsfBankData.assign(padding, 0x00);
            nsfBankData.insert(nsfBankData.end(), data.begin(), data.end());

            for (int i = 0; i < 8; i++)
                nsfBanks[i] = initialBanks[i];
        }

        for (size_t i = 0; i < data.size(); i++)
        {
            uint32_t addr = static_cast<uint32_t>(loadAddress) + static_cast<uint32_t>(i);

            if (addr <= 0xFFFF)
                nsfMemory[addr] = data[i];
            else
                break;
        }
    }

    void DisableNSFMode()
    {
        nsfMode = false;
        nsfMemory.fill(0x00);

        nsfBankMode = false;
        nsfBanks.fill(0x00);
        nsfBankData.clear();
        nsfExpansionWrite = nullptr;
        nsfExpansionRead = nullptr;
    }

    uint8_t controller_state = 0x00;
    // true nếu CPU đã đọc $4016/$4017 (poll tay cầm) kể từ lần reset cuối.
    // EmuWorker::runFrame() dùng để phát hiện "lag frame" (game không chạy
    // logic trong frame đó -> slowdown) và reset về false cuối mỗi frame.
    bool inputPolledThisFrame = false;
    uint8_t controller_strobe = 0x00;
    uint8_t controller_shift = 0x00;
    uint8_t controller_state2 = 0x00;
    uint8_t controller_shift2 = 0x00;
    bool zapperConnected = false;
    bool zapperTrigger = false;
    int zapperX = -1;
    int zapperY = -1;


    // === CÁC BIẾN QUẢN LÝ DMA CHUẨN XÁC ===
    uint8_t dma_page = 0x00;
    uint8_t dma_addr = 0x00;
    uint8_t dma_data = 0x00;
    bool dma_dummy = true;
    bool dma_transfer = false;
    // Tổng số CPU cycle THẬT đã trôi qua kể từ power-on (tăng mỗi khi 1 CPU cycle
    // trôi qua, BẤT KỂ CPU đang chạy lệnh hay đang bị đóng băng vì DMA) - dùng để
    // biết parity chẵn/lẻ đúng lúc $4014 được ghi, theo đúng luật NESDev OAM DMA:
    // 513 cycle nếu DMA bắt đầu ở CPU cycle CHẴN, 514 cycle nếu bắt đầu ở cycle LẺ
    // (cần thêm 1 cycle "align" trước khi DMA controller thực sự chiếm bus).
    uint32_t total_cpu_cycles = 0;
    // Số cycle CPU phải đóng băng cho lần DMA hiện tại (513 hoặc 514), tính lại
    // mỗi khi $4014 được ghi (xem cpuWrite bên dưới).
    int dma_cycles_needed = 513;
    // Số cycle CPU còn phải đứng yên do DMC DMA (mỗi byte sample reader nạp ~4 cycle).
    // APU::SyncDmcToBus() cộng vào, EmuWorker::runFrame() trừ dần mỗi CPU cycle.
    int dmc_stall_cycles = 0;

    uint32_t system_clock_counter = 0; // Bộ đếm nhịp hệ thống

    // Giá trị "open bus" - byte cuối cùng thực sự nằm trên bus dữ liệu CPU.
    // Dùng để giả lập hành vi phần cứng thật khi đọc các port ghi-only
    // hoặc vùng địa chỉ chưa mapped ($4000-$4013, $4014, $4018-$40FF, ...).
    uint8_t open_bus = 0x00;

    // Địa chỉ CPU bus được truy cập gần nhất (cho overlay debug F10, UI đọc để hiển thị)
    uint16_t last_bus_addr = 0x0000;
    bool last_bus_write = false;

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
        last_bus_addr = addr;
        last_bus_write = true;
        // Mọi byte CPU ghi ra đều thực sự đi qua bus dữ liệu -> cập nhật open bus.
        open_bus = data;

        if (nsfMode && nsfBankMode && addr >= 0x5FF8 && addr <= 0x5FFF)
        {
            nsfBanks[addr - 0x5FF8] = data;
            return;
        }

        // Expansion audio NSF: VRC6 / VRC7 / S5B / MMC5
        if (nsfMode && nsfExpansionWrite)
        {
            if (nsfExpansionWrite(addr, data))
                return;
        }

        if (nsfMode && addr >= 0x4020)
        {
            if (!nsfBankMode || addr < 0x8000)
                nsfMemory[addr] = data;

            return;
        }


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
            // QUAN TRỌNG: copy OAM PHẢI làm ngay ở đây (tức thời), vì vòng lặp
            // chạy game THẬT (EmuWorker::runFrame) không hề gọi Bus::clock() -
            // nó tự đếm dma_dummy_counter riêng chỉ để giữ CPU đứng yên đúng số
            // cycle, KHÔNG tự copy dữ liệu. Nếu bỏ đoạn copy này, OAM sẽ không
            // bao giờ được cập nhật -> sprite biến mất/lem hình.
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
        last_bus_addr = addr;
        last_bus_write = false;
        // Mặc định = giá trị open bus hiện tại. Nếu không nhánh nào bên dưới
        // khớp với addr (vùng chưa mapped hoặc port chỉ-ghi như $4000-$4013,
        // $4014, $4018-$40FF), hàm sẽ tự nhiên trả về đúng giá trị open bus
        // thay vì 0x00 cố định - đúng hành vi phần cứng NES thật.
        uint8_t data = open_bus;

        if (nsfMode && nsfBankMode && addr >= 0x8000)
        {
            int slot = (addr - 0x8000) >> 12;
            int bank = nsfBanks[slot];

            uint32_t offset =
                static_cast<uint32_t>(bank) * 0x1000 +
                static_cast<uint32_t>(addr & 0x0FFF);

            if (offset < nsfBankData.size())
                return nsfBankData[offset];
            qDebug() << "OOB bank read! addr=" << Qt::hex << addr
                << "bank=" << (int)bank << "offset=" << offset
                << "size=" << nsfBankData.size();
            return 0x00;

            return 0x00;
        }

        if (nsfMode && nsfExpansionRead)
        {
            if (nsfExpansionRead(addr, data))
                return data;
        }

        if (nsfMode && addr >= 0x4020)
        {
            return nsfMemory[addr];
        }

        if (cart != nullptr && cart->cpuRead(addr, data)) {
            open_bus = data;
            return data;
        }

        if (addr >= 0x0000 && addr <= 0x1FFF) {
            data = ram[addr & 0x07FF];
        }
        else if (addr == 0x4016) {
            inputPolledThisFrame = true;
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
            inputPolledThisFrame = true;
            if (zapperConnected) {
                data = 0x40;
                data |= zapperTrigger ? 0x10 : 0x00;

                bool sensedLight = false;
                if (ppu != nullptr && zapperX >= 0 && zapperY >= 0) {
                    uint32_t pixel = ppu->GetPixelAt(zapperX, zapperY);
                    int r = (pixel >> 16) & 0xFF;
                    int g = (pixel >> 8) & 0xFF;
                    int b = pixel & 0xFF;
                    sensedLight = (r + g + b) > 300;
                }
                data |= sensedLight ? 0x00 : 0x08;
            }
            else {
                if (controller_strobe) {
                    data = (controller_state2 & 0x80) ? 1 : 0;
                }
                else {
                    data = (controller_shift2 & 0x80) ? 1 : 0;
                    controller_shift2 <<= 1;
                }
                data |= 0x40;
            }
        }
        else if (addr >= 0x2000 && addr <= 0x3FFF) {
            if (ppu != nullptr)
                data = ppu->cpuRead(addr & 0x0007);
        }
        else if (addr == 0x4015) {
            data = n_apu.readStatus();
        }

        // Byte vừa thực sự xuất hiện trên bus dữ liệu -> lưu lại làm open bus mới.
        open_bus = data;
        return data;
    }

    // !!! CẢNH BÁO: hàm này HIỆN KHÔNG được gọi ở đâu trong luồng chạy game
    // thật. Vòng lặp thật nằm ở EmuWorker::runFrame(), tự viết lại toàn bộ
    // logic bên dưới bằng tay (đếm dma_dummy_counter riêng, tự gọi nes_cpu.nmi()
    // trực tiếp). Nếu sau này có ý định gọi clock() này ở đâu đó, PHẢI xoá bỏ
    // logic tương ứng trong EmuWorker.cpp trước, kẻo NMI/DMA bị chạy đúp.
    // Giữ hàm này ở dạng đúng-về-mặt-logic (tham chiếu) nhưng không phải nơi
    // sửa nếu muốn thay đổi hành vi game thực tế - sửa EmuWorker.cpp thay vào đó.
    void clock() {
        // 1. CPU xử lý trước
        if (system_clock_counter % 3 == 0) {
            // Mỗi lần vào đây là đúng 1 "CPU cycle" trôi qua trên đồng hồ hệ
            // thống, BẤT KỂ CPU đang chạy lệnh thật hay đang bị đóng băng vì
            // DMA - cần đếm đủ để lần ghi $4014 KẾ TIẾP tính đúng parity chẵn/lẻ.
            total_cpu_cycles++;

            if (dma_transfer) {
                // ==== OAM DMA cycle-accurate thật (513/514 CPU cycle) ====
                // Dùng đúng parity của system_clock_counter để xen kẽ
                // read/write, y hệt luật NESDev: cycle "align" (nếu $4014 bị
                // ghi vào 1 CPU cycle lẻ) -> rồi 256 cặp (đọc CPU bus / ghi OAM).
                if (dma_dummy) {
                    // Cycle "align": chỉ tồn tại nếu DMA bắt đầu ở 1 system tick lẻ.
                    if (system_clock_counter % 2 == 1) {
                        dma_dummy = false;
                    }
                }
                else {
                    if (system_clock_counter % 2 == 0) {
                        // Cycle chẵn trong chuỗi DMA: ĐỌC 1 byte từ CPU bus
                        dma_data = cpuRead(((uint16_t)dma_page << 8) | dma_addr);
                    }
                    else {
                        // Cycle lẻ trong chuỗi DMA: GHI byte đó vào OAM
                        if (ppu != nullptr)
                            ppu->OAM[dma_addr] = dma_data;
                        dma_addr++;
                        if (dma_addr == 0x00) { // đã wrap sau đúng 256 byte
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

            // 2. Mapper đếm nhịp
            if (cart != nullptr && cart->pMapper != nullptr) {
                cart->pMapper->clock();
            }
        }

        // 3. PPU chạy sau cùng để đảm bảo đồng bộ trạng thái
        if (ppu != nullptr) ppu->Step();

        // [Fix NMI]: PPU chỉ ĐẶT cờ nmi_requested, Bus phải là nơi thực sự
        // "bấm nút" NMI trên CPU - nếu thiếu bước này NMI sẽ không bao giờ
        // được CPU nhận, dù cờ VBlank/status vẫn set bình thường.
        if (ppu != nullptr && ppu->nmi_requested) {
            ppu->nmi_requested = false;
            if (cpu != nullptr) cpu->nmi();
        }

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