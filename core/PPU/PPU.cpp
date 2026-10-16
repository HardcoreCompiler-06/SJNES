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
	Derived from olc::NES - Picture Processing Unit (PPU) 2C02
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
#include "PPU.h"
#include <qDebug>
#include "Mapper_004.h"
#include "Mapper_005.h"
#include "Mapper_024.h"
#include "Mapper_476.h"
#include "BinaryIO.h"

// PPU open bus: giá trị "trôi nổi" trên bus dữ liệu PPU, dùng cho các bit/
// thanh ghi không thật sự trả dữ liệu (VD 5 bit thấp của $2002, hoặc đọc từ
// các thanh ghi write-only $2000/$2001/$2003/$2005/$2006).
// Dùng static file-scope vì codebase hiện chỉ có đúng 1 instance PPU.
static uint8_t g_ppu_open_bus = 0x00;

// ---- FIX test ppu_open_bus #3 "decay value should become zero by one
// second" VÀ #8 "high 2 bits from $2007 from palette should be from decay
// value": phần cứng thật KHÔNG giữ giá trị open bus vĩnh viễn - từng bit tự
// "rò điện" về 0 nếu không được refresh trong khoảng ~600ms. Model theo đúng
// cách nesalizer làm (emulator pass toàn bộ test blargg ppu_open_bus): CHIA
// LÀM 3 NHÓM bit với 3 mốc thời gian refresh riêng - không phải 2 (thử ban
// đầu chỉ tách 7-5/4-0, sai vì đọc $2002 refresh {7,6,5} còn đọc palette qua
// $2007 refresh {5,4,3,2,1,0} - 2 tập này GIAO NHAU ở bit 5, không thể gộp
// chung 2 nhóm được, phải tách riêng bit 5 thành nhóm thứ 3):
//   - nhóm {bit 7,6}: CHỈ refresh bởi ghi/đọc 8-bit đầy đủ.
//   - nhóm {bit 5}: refresh bởi CẢ ghi/đọc 8-bit đầy đủ, CẢ đọc $2002
//     (driven 3 bit cao), CẢ đọc palette qua $2007 (driven 6 bit thấp).
//   - nhóm {bit 4-0}: refresh bởi ghi/đọc 8-bit đầy đủ, VÀ đọc palette qua
//     $2007 (palette chỉ có 6 bit thật trong bộ nhớ - xem GetPaletteRAM/
//     ppuRead mask & 0x3F - nên bit 7,6 của kết quả đọc $2007 khi VRAM addr
//     trỏ vào palette KHÔNG được driven, phải lấy từ open bus cũ/đã decay).
// ppu_cycle_count đếm số dot PPU đã chạy (Step() gọi 1 lần/dot).
static uint64_t g_ppu_cycle_count = 0;
static uint64_t g_open_bus_76_wcycle = 0; // mốc refresh gần nhất, bit 7-6
static uint64_t g_open_bus_5_wcycle = 0;  // mốc refresh gần nhất, bit 5
static uint64_t g_open_bus_40_wcycle = 0; // mốc refresh gần nhất, bit 4-0
static constexpr double PPU_CLOCK_RATE_NTSC = 1789773.0 * 3.0;
static constexpr uint64_t OPEN_BUS_DECAY_CYCLES = (uint64_t)(0.6 * PPU_CLOCK_RATE_NTSC);

static uint8_t DecayedOpenBus()
{
    uint8_t v = g_ppu_open_bus;
    if (g_ppu_cycle_count - g_open_bus_76_wcycle > OPEN_BUS_DECAY_CYCLES) v &= 0x3F; // xoá bit 7-6
    if (g_ppu_cycle_count - g_open_bus_5_wcycle > OPEN_BUS_DECAY_CYCLES) v &= ~0x20; // xoá bit 5
    if (g_ppu_cycle_count - g_open_bus_40_wcycle > OPEN_BUS_DECAY_CYCLES) v &= 0xE0; // xoá bit 4-0
    return v;
}
// Ghi/đọc 8-bit đầy đủ (cpuWrite bất kỳ, đọc $2004, đọc $2007 vùng KHÔNG
// phải palette) -> refresh cả 3 nhóm.
static void RefreshOpenBusAll() { g_open_bus_76_wcycle = g_open_bus_5_wcycle = g_open_bus_40_wcycle = g_ppu_cycle_count; }
// Đọc $2002 (chỉ driven 3 bit cao 7,6,5) -> refresh nhóm {7,6} và {5}.
static void RefreshOpenBusTop3() { g_open_bus_76_wcycle = g_open_bus_5_wcycle = g_ppu_cycle_count; }
// Đọc palette qua $2007 (chỉ driven 6 bit thấp 5..0) -> refresh nhóm {5} và {4-0}.
static void RefreshOpenBusBottom6() { g_open_bus_5_wcycle = g_open_bus_40_wcycle = g_ppu_cycle_count; }
// KHỞI TẠO PPU & BẢNG MÀU
PPU::PPU() {
    palScreen[0x00] = { (uint8_t)102, (uint8_t)102, (uint8_t)102 };
    palScreen[0x01] = { (uint8_t)0, (uint8_t)42, (uint8_t)136 };
    palScreen[0x02] = { (uint8_t)20, (uint8_t)18, (uint8_t)167 };
    palScreen[0x03] = { (uint8_t)59, (uint8_t)0, (uint8_t)164 };
    palScreen[0x04] = { (uint8_t)92, (uint8_t)0, (uint8_t)126 };
    palScreen[0x05] = { (uint8_t)110, (uint8_t)0, (uint8_t)64 };
    palScreen[0x06] = { (uint8_t)108, (uint8_t)6, (uint8_t)0 };
    palScreen[0x07] = { (uint8_t)86, (uint8_t)29, (uint8_t)0 };
    palScreen[0x08] = { (uint8_t)51, (uint8_t)53, (uint8_t)0 };
    palScreen[0x09] = { (uint8_t)11, (uint8_t)72, (uint8_t)0 };
    palScreen[0x0A] = { (uint8_t)0, (uint8_t)82, (uint8_t)0 };
    palScreen[0x0B] = { (uint8_t)0, (uint8_t)79, (uint8_t)8 };
    palScreen[0x0C] = { (uint8_t)0, (uint8_t)64, (uint8_t)77 };
    palScreen[0x0D] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x0E] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x0F] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };

    palScreen[0x10] = { (uint8_t)173, (uint8_t)173, (uint8_t)173 };
    palScreen[0x11] = { (uint8_t)21, (uint8_t)95, (uint8_t)217 };
    palScreen[0x12] = { (uint8_t)66, (uint8_t)64, (uint8_t)255 };
    palScreen[0x13] = { (uint8_t)117, (uint8_t)39, (uint8_t)254 };
    palScreen[0x14] = { (uint8_t)160, (uint8_t)26, (uint8_t)204 };
    palScreen[0x15] = { (uint8_t)183, (uint8_t)30, (uint8_t)123 };
    palScreen[0x16] = { (uint8_t)181, (uint8_t)49, (uint8_t)32 };
    palScreen[0x17] = { (uint8_t)153, (uint8_t)78, (uint8_t)0 };
    palScreen[0x18] = { (uint8_t)107, (uint8_t)109, (uint8_t)0 };
    palScreen[0x19] = { (uint8_t)56, (uint8_t)135, (uint8_t)0 };
    palScreen[0x1A] = { (uint8_t)12, (uint8_t)147, (uint8_t)0 };
    palScreen[0x1B] = { (uint8_t)0, (uint8_t)143, (uint8_t)50 };
    palScreen[0x1C] = { (uint8_t)0, (uint8_t)124, (uint8_t)141 };
    palScreen[0x1D] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x1E] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x1F] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };

    palScreen[0x20] = { (uint8_t)255, (uint8_t)254, (uint8_t)255 };
    palScreen[0x21] = { (uint8_t)100, (uint8_t)176, (uint8_t)255 };
    palScreen[0x22] = { (uint8_t)146, (uint8_t)144, (uint8_t)255 };
    palScreen[0x23] = { (uint8_t)198, (uint8_t)118, (uint8_t)255 };
    palScreen[0x24] = { (uint8_t)243, (uint8_t)106, (uint8_t)255 };
    palScreen[0x25] = { (uint8_t)254, (uint8_t)110, (uint8_t)204 };
    palScreen[0x26] = { (uint8_t)254, (uint8_t)129, (uint8_t)112 };
    palScreen[0x27] = { (uint8_t)234, (uint8_t)158, (uint8_t)34 };
    palScreen[0x28] = { (uint8_t)188, (uint8_t)190, (uint8_t)0 };
    palScreen[0x29] = { (uint8_t)136, (uint8_t)216, (uint8_t)0 };
    palScreen[0x2A] = { (uint8_t)92, (uint8_t)228, (uint8_t)48 };
    palScreen[0x2B] = { (uint8_t)69, (uint8_t)224, (uint8_t)130 };
    palScreen[0x2C] = { (uint8_t)72, (uint8_t)205, (uint8_t)222 };
    palScreen[0x2D] = { (uint8_t)79, (uint8_t)79, (uint8_t)79 };
    palScreen[0x2E] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x2F] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };

    palScreen[0x30] = { (uint8_t)255, (uint8_t)254, (uint8_t)255 };
    palScreen[0x31] = { (uint8_t)192, (uint8_t)223, (uint8_t)255 };
    palScreen[0x32] = { (uint8_t)211, (uint8_t)210, (uint8_t)255 };
    palScreen[0x33] = { (uint8_t)232, (uint8_t)200, (uint8_t)255 };
    palScreen[0x34] = { (uint8_t)251, (uint8_t)194, (uint8_t)255 };
    palScreen[0x35] = { (uint8_t)254, (uint8_t)196, (uint8_t)234 };
    palScreen[0x36] = { (uint8_t)254, (uint8_t)204, (uint8_t)197 };
    palScreen[0x37] = { (uint8_t)247, (uint8_t)216, (uint8_t)165 };
    palScreen[0x38] = { (uint8_t)228, (uint8_t)229, (uint8_t)148 };
    palScreen[0x39] = { (uint8_t)207, (uint8_t)239, (uint8_t)150 };
    palScreen[0x3A] = { (uint8_t)189, (uint8_t)244, (uint8_t)171 };
    palScreen[0x3B] = { (uint8_t)179, (uint8_t)243, (uint8_t)204 };
    palScreen[0x3C] = { (uint8_t)181, (uint8_t)235, (uint8_t)242 };
    palScreen[0x3D] = { (uint8_t)184, (uint8_t)184, (uint8_t)184 };
    palScreen[0x3E] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x3F] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
}

PPU::~PPU() {}

void PPU::ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge) {
    this->cart = cartridge;
}

void PPU::reset() {
    fine_x = 0x00;
    address_latch = 0x00;
    ppu_data_buffer = 0x00;

    scanline = 0;
    cycle = 0;

    bg_next_tile_id = 0x00;
    bg_next_tile_attrib = 0x00;
    bg_next_tile_lsb = 0x00;
    bg_next_tile_msb = 0x00;

    bg_shifter_pattern_lo = 0x0000;
    bg_shifter_pattern_hi = 0x0000;
    bg_shifter_attrib_lo = 0x0000;
    bg_shifter_attrib_hi = 0x0000;

    status = 0x00;
    ppu_mask = 0x00;
    ppu_ctrl = 0x00;

    vram_addr.reg = 0x0000;
    tram_addr.reg = 0x0000;

    nmi_requested = false;
    suppress_vbl_this_frame = false;
    cancel_pending_nmi = false;
    oam_addr = 0x00;

    // Reset open bus + 3 mốc thời gian decay - trước đây các biến này là
    // file-static và KHÔNG hề được reset giữa các lần mở ROM, khiến open bus
    // "rò rỉ" giá trị từ ROM trước sang ROM sau.
    g_ppu_open_bus = 0x00;
    g_ppu_cycle_count = 0;
    g_open_bus_76_wcycle = 0;
    g_open_bus_5_wcycle = 0;
    g_open_bus_40_wcycle = 0;

    mapper_a12 = false;
    mapper_a12_low_cycles = 255;

    odd_frame = false;
    odd_skip_render_enabled_latched = false;
    vbl_visible_to_write = false;

    sprite_count = 0;

    // DEBUG TẠM: reset lại counter/log mỗi lần mở ROM mới, để log không bị
    // "chết" từ ROM cũ (statics trước đây chỉ init 1 lần cho cả đời chương trình)
    debugM476LogCount = 0;
    debugVblSetCount = 0;
    debugLastVblCycle = -1;
    debugTotalPpuCycles = 0;

    for (int i = 0; i < 256; i++)
        OAM[i] = 0x00;

    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 1024; i++)
            tblName[t][i] = 0x00;

    for (int i = 0; i < 32; i++)
        tblPalette[i] = 0x00;

    for (int i = 0; i < 64; i++)
    {
        sprite_pattern_lo[i] = 0x00;
        sprite_pattern_hi[i] = 0x00;
        sprite_x[i] = 0x00;
        sprite_attribute[i] = 0x00;
        sprite_zero_being_rendered[i] = false;
    }

    for (int i = 0; i < 8; i++)
        sprite_pattern_addr[i] = 0x0000;

    for (int i = 0; i < 256 * 240; i++) {
        RGBColor c = palScreen[0x0F];
        frame_pixels[i] = 0xFF000000 | (c.r << 16) | (c.g << 8) | c.b;
    }
}

// GIAO TIẾP VỚI CPU
uint8_t PPU::cpuRead(uint16_t addr, bool rdonly) {
    uint8_t data = 0x00;
    if (rdonly) {
        switch (addr) {
        case 0x0000: data = ppu_ctrl; break;
        case 0x0001: data = ppu_mask; break;
        case 0x0002: data = status; break;
        case 0x0003: data = 0; break;
        case 0x0004: data = 0; break;
        case 0x0005: data = 0; break;
        case 0x0006: data = 0; break;
        case 0x0007: data = 0; break;
        }
    }
    else {
        switch (addr) {
        case 0x0000: data = DecayedOpenBus(); break; // write-only -> trả open bus, không phải 0
        case 0x0001: data = DecayedOpenBus(); break;
        case 0x0002:
            data = (status & 0xE0) | (DecayedOpenBus() & 0x1F);

            // NMI suppression race condition (hardware NES thật) - NESdev
            // "PPU frame timing": có ĐỦ 3 dot đặc biệt quanh dot set VBL
            // (scanline==241, dot 1), không phải chỉ 1. Do Step() luôn ++cycle
            // trước khi return, dot thật D được quan sát từ ngoài (đúng lúc
            // CPU gọi cpuRead() này) là cycle == D+1:
            //   - 1 dot TRƯỚC lúc set (dot 0, quan sát cycle==1): đọc thấy
            //     flag=0 NHƯ BÌNH THƯỜNG (chưa set), nhưng quan trọng hơn:
            //     frame này VĨNH VIỄN không được set flag/bắn NMI nữa dù
            //     Step() sắp chạy tới đúng dot set ngay sau đó - cần báo cho
            //     Step() biết qua suppress_vbl_this_frame.
            //   - CÙNG dot (dot 1, quan sát cycle==2): đọc thấy flag=1 NHƯNG
            //     huỷ NMI của frame này.
            //   - 1 dot SAU (dot 2, quan sát cycle==3): đọc thấy flag=1 NHƯNG
            //     VẪN huỷ NMI của frame này (case này trước đây bị thiếu -
            //     đúng nguyên nhân treo máy dạng "100% đúng 1 chỗ" ở
            //     Battletoads Wookie Hole, vì code game luôn đọc $2002 tại
            //     đúng dot này mỗi lần chạy tới đoạn đó).
            if (scanline == 241 && cycle == 1) {
                // Chưa tới dot set nên data đã tự nhiên có bit7=0, không cần
                // sửa data. Chỉ cần chặn lần set SẮP TỚI.
                suppress_vbl_this_frame = true;
            }
            else if (scanline == 241 && (cycle == 2 || cycle == 3)) {
                // NESdev "PPU frame timing": đọc ĐÚNG lúc set hoặc 1 dot SAU
                // đều phải đọc ra flag ĐANG SET (data đã đúng = 1 rồi vì
                // Step() đã set status trước đó) - KHÔNG được xoá bit của
                // data ở đây (fix test 02-vbl_set_time). Chỉ xoá status nội
                // bộ (hành vi đọc-thì-xoá $2002 bình thường) và huỷ NMI.
                //
                // QUAN TRỌNG: tới thời điểm đọc này, cpu.nmi() gần như chắc
                // chắn ĐÃ được EmuWorker gọi rồi (dispatch ngay khi Step()
                // set VBL, cùng tick hoặc tick trước) - xoá nmi_requested ở
                // đây không còn tác dụng vì nó đã bị tiêu thụ. Phải bật thêm
                // cancel_pending_nmi để EmuWorker gọi cpu.CancelPendingNmi(),
                // huỷ đúng nmi_pending BÊN TRONG CPU (fix test 06-suppression).
                status &= ~0x80;
                nmi_requested = false;
                cancel_pending_nmi = true;
                vbl_visible_to_write = false;
            }
            else {
                status &= ~0x80;
                vbl_visible_to_write = false;
            }
            address_latch = 0;

            // Đọc $2002 chỉ "đẩy" lên bus đúng 3 bit thật (VBL/spr0/overflow),
            // 5 bit thấp giữ nguyên giá trị open bus cũ (không bị driven).
            // Chỉ refresh 2 nhóm {7,6} và {5} - 5 bit thấp không được driven
            // nên không được "hồi sinh" decay timer của chúng.
            g_ppu_open_bus = (g_ppu_open_bus & 0x1F) | (data & 0xE0);
            RefreshOpenBusTop3();
            break;
        case 0x0003: data = DecayedOpenBus(); break;
        case 0x0004:
            data = OAM[oam_addr];
            // Byte thuộc tính sprite (offset thứ 3 trong mỗi block 4-byte,
            // tức oam_addr % 4 == 2) chỉ có 5 bit thật tồn tại trên phần cứng
            // (bit 7=vflip, 6=hflip, 5=priority, 1-0=palette) - bit 2,3,4
            // KHÔNG có mạch vật lý, luôn đọc ra 0 dù ghi gì vào cũng vậy (ghi
            // thì vẫn LƯU nguyên trong OAM[], chỉ khi ĐỌC RA mới bị che).
            if ((oam_addr & 0x03) == 0x02)
                data &= 0xE3; // xoá bit 2,3,4 (mask 0b1110_0011)
            // OAMDATA đọc ra đủ 8 bit thật -> refresh cả 3 nhóm
            g_ppu_open_bus = data;
            RefreshOpenBusAll();
            break;
        case 0x0005: data = DecayedOpenBus(); break;
        case 0x0006: data = DecayedOpenBus(); break;
        case 0x0007:
        {
            uint16_t addr = vram_addr.reg & 0x3FFF;
            NotifyMapperA12(addr);
            if (addr >= 0x3F00)
            {
                // Palette RAM chỉ có 6 bit thật (xem ppuRead() mask & 0x3F) -
                // 2 bit cao KHÔNG được driven bởi palette, phải lấy từ open
                // bus CŨ (đã decay) thay vì bị ép về 0 hay bị refresh mới.
                uint8_t paletteByte = ppuRead(addr) & 0x3F;
                data = (DecayedOpenBus() & 0xC0) | paletteByte;
                ppu_data_buffer = ppuRead(addr - 0x1000);

                g_ppu_open_bus = (g_ppu_open_bus & 0xC0) | paletteByte;
                RefreshOpenBusBottom6();
            }
            else
            {
                data = ppu_data_buffer;
                ppu_data_buffer = ppuRead(addr);

                // Đọc VRAM thường (không phải palette) qua buffer $2007 trả
                // ra đủ 8 bit thật -> refresh cả 3 nhóm.
                g_ppu_open_bus = data;
                RefreshOpenBusAll();
            }

            if ((ppu_mask & 0x18) && (scanline >= -1 && scanline < 240)) {
                // Đọc/ghi $2007 lúc đang render: tăng đồng thời cả coarse_x lẫn fine_y/coarse_y
                if (vram_addr.coarse_x == 31) { vram_addr.coarse_x = 0; vram_addr.nametable_x = ~vram_addr.nametable_x; }
                else { vram_addr.coarse_x++; }

                if (vram_addr.fine_y < 7) { vram_addr.fine_y++; }
                else {
                    vram_addr.fine_y = 0;
                    if (vram_addr.coarse_y == 29) { vram_addr.coarse_y = 0; vram_addr.nametable_y = ~vram_addr.nametable_y; }
                    else if (vram_addr.coarse_y == 31) { vram_addr.coarse_y = 0; }
                    else { vram_addr.coarse_y++; }
                }
            }
            else {
                vram_addr.reg += (ppu_ctrl & 0x04) ? 32 : 1;
            }
        }
        break;
        }
    }
    return data;
}

void PPU::cpuWrite(uint16_t addr, uint8_t data) {
    static int scrollLogCount = 0;

    // Mọi lần CPU ghi vào $2000-$2007 đều đẩy đủ 8 bit lên bus dữ liệu PPU,
    // bất kể thanh ghi đó có dùng hết 8 bit hay không (VD PPUMASK chỉ dùng
    // vài bit thấp, nhưng phần "thừa" ghi vào vẫn refresh open bus thật).
    g_ppu_open_bus = data;
    RefreshOpenBusAll();

    auto logScrollWrite = [&](const char* regName, uint8_t value)
        {
            if (scrollLogCount < 300)
            {
                scrollLogCount++;
            }
        };
    switch (addr) {
    case 0x0000:
    {
        bool nmi_was_enabled = (ppu_ctrl & 0x80) > 0;

        ppu_ctrl = data;
        tram_addr.nametable_x = ppu_ctrl & 0x01;
        tram_addr.nametable_y = (ppu_ctrl & 0x02) >> 1;
        if (!nmi_was_enabled && (ppu_ctrl & 0x80) && vbl_visible_to_write) {
            // KHÔNG set nmi_requested trực tiếp ở đây: NMI do CHÍNH lệnh ghi
            // $2000 này gây ra (lúc VBL đã set sẵn) phải trễ thêm ĐÚNG 1 lệnh
            // CPU nữa mới được CPU nhận ra (test 04-nmi_control #11) - xem
            // giải thích đầy đủ ở khai báo nmi_write_pending trong PPU.h.
            // EmuWorker.cpp sẽ tự trễ đúng 1 CPU cycle rồi mới gọi CPU::nmi().
            nmi_write_pending = true;
        }
        else if (nmi_was_enabled && !(ppu_ctrl & 0x80))
        {
            // TẮT NMI generate (bit7 1->0). Nếu việc tắt này rơi đúng vào cửa
            // sổ race ngay sát lúc VBL tự nhiên vừa set (scanline==241) - xem
            // NESdev/Mesen: ghi ($2000) bị trễ 1 master-clock tick so với đọc
            // ($2002), nên cửa sổ race của WRITE lệch pha so với READ - việc
            // tắt vẫn phải HUỶ ĐƯỢC NMI vừa bắn, dùng lại đúng cơ chế
            // cancel_pending_nmi đã dùng cho race condition phía đọc $2002
            // (test 06-suppression) - fix test 08-nmi_off_timing.
            if (scanline == 241 && (cycle == 2 || cycle == 3)) {
                nmi_requested = false;
                cancel_pending_nmi = true;
            }
        }
    }
    break;
    case 0x0001:
    {
        static uint8_t lastMask = 0xFF;
        if (data != lastMask)
        {
            lastMask = data;
        }
    }
    ppu_mask = data;
    break;
    case 0x0002: break;
    case 0x0003: oam_addr = data; break;
    case 0x0004: OAM[oam_addr] = data; oam_addr++; break;
    case 0x0005:
    {
        if (address_latch == 0)
        {
            fine_x = data & 0x07;
            tram_addr.coarse_x = data >> 3;
            address_latch = 1;
        }
        else
        {
            tram_addr.fine_y = data & 0x07;
            tram_addr.coarse_y = data >> 3;
            address_latch = 0;
        }
    }
    break;
    case 0x0006:
    {
        if (address_latch == 0) {
            tram_addr.reg = (uint16_t)((data & 0x3F) << 8) | (tram_addr.reg & 0x00FF);
            address_latch = 1;
        }
        else {
            tram_addr.reg = (tram_addr.reg & 0xFF00) | data;
            vram_addr = tram_addr;
            address_latch = 0;
            NotifyMapperA12(vram_addr.reg);
        }
    }
    break;
    case 0x0007:
    {
        NotifyMapperA12(vram_addr.reg & 0x3FFF);

        ppuWrite(vram_addr.reg, data);
        if ((ppu_mask & 0x18) && (scanline >= -1 && scanline < 240)) {
            if (vram_addr.coarse_x == 31) { vram_addr.coarse_x = 0; vram_addr.nametable_x = ~vram_addr.nametable_x; }
            else { vram_addr.coarse_x++; }

            if (vram_addr.fine_y < 7) { vram_addr.fine_y++; }
            else {
                vram_addr.fine_y = 0;
                if (vram_addr.coarse_y == 29) { vram_addr.coarse_y = 0; vram_addr.nametable_y = ~vram_addr.nametable_y; }
                else if (vram_addr.coarse_y == 31) { vram_addr.coarse_y = 0; }
                else { vram_addr.coarse_y++; }
            }
        }
        else {
            vram_addr.reg += (ppu_ctrl & 0x04) ? 32 : 1;
        }
    }
    break;
    }
}

//BỘ NHỚ BUS GIAO TIẾP VỚI PPU
uint8_t PPU::ppuRead(uint16_t addr, bool rdonly) {
    uint8_t data = 0x00;
    addr &= 0x3FFF;
    if (addr <= 0x1FFF)
        NotifyMapperA12(addr);
    if (cart && cart->ppuRead(addr, data)) return data;
    else if (addr <= 0x1FFF)
        data = tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF];
    else if (addr >= 0x2000 && addr <= 0x3EFF)
    {
        uint16_t ntAddr = addr & 0x0FFF;
        int ntIndex = ntAddr / 0x0400;
        uint16_t offset = ntAddr & 0x03FF;
        if (cart && cart->pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
            {
                uint8_t source = mmc5->GetNtSource(ntIndex);

                switch (source)
                {
                case 0:
                    data = tblName[0][offset];
                    return data;

                case 1:
                    data = tblName[1][offset];
                    return data;

                case 2:
                    data = mmc5->ReadExRam(offset);
                    return data;

                case 3:
                    if (offset < 0x03C0)
                        data = mmc5->GetFillTile();
                    else
                        data = mmc5->GetFillAttr();

                    return data;
                }
            }
        }
        // VRC6 nametable banking
        if (cart && cart->pMapper)
        {
            if (auto* vrc6 = dynamic_cast<Mapper_024*>(cart->pMapper.get()))
            {
                uint8_t source = vrc6->GetNtSource(ntIndex);
                data = tblName[source & 0x01][offset];
                return data;
            }
        }
        if (cart && cart->pMapper)
        {
            if (auto* m476 = dynamic_cast<Mapper_476*>(cart->pMapper.get()))
            {
                if (debugM476LogCount < 5) {
                    qDebug() << "PPU dang doc nametable tu Mapper476, offset=" << offset;
                    debugM476LogCount++;
                }
                data = m476->ReadFrameNametable(offset);
                return data;
            }
        }
        // Mirroring cũ cho mapper thường
        addr = ntAddr;
        MIRROR m = cart->mirror;
        if (cart->pMapper != nullptr) {
            MIRROR mapper_mirror = cart->pMapper->mirror();
            if (mapper_mirror != MIRROR::HARDWARE) {
                m = mapper_mirror;
            }
        }
        if (m == MIRROR::VERTICAL) {
            if (addr <= 0x03FF) data = tblName[0][addr & 0x03FF];
            else if (addr <= 0x07FF) data = tblName[1][addr & 0x03FF];
            else if (addr <= 0x0BFF) data = tblName[0][addr & 0x03FF];
            else data = tblName[1][addr & 0x03FF];
        }
        else if (m == MIRROR::HORIZONTAL) {
            if (addr <= 0x03FF) data = tblName[0][addr & 0x03FF];
            else if (addr <= 0x07FF) data = tblName[0][addr & 0x03FF];
            else if (addr <= 0x0BFF) data = tblName[1][addr & 0x03FF];
            else data = tblName[1][addr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_LO) {
            data = tblName[0][addr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_HI) {
            data = tblName[1][addr & 0x03FF];
        }
    }
    else if (addr >= 0x3F00) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000; if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008; if (addr == 0x001C) addr = 0x000C;
        data = tblPalette[addr] & 0x3F;
    }

    return data;
}

void PPU::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        if (cart && cart->ppuWrite(addr, data))
            return;

        tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF] = data;
    }
    else if (addr >= 0x2000 && addr <= 0x3EFF)
    {
        uint16_t ntAddr = addr & 0x0FFF;
        int ntIndex = ntAddr / 0x0400;
        uint16_t offset = ntAddr & 0x03FF;

        // MMC5 nametable mapping
        if (cart && cart->pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
            {
                uint8_t source = mmc5->GetNtSource(ntIndex);

                switch (source)
                {
                case 0:
                    tblName[0][offset] = data;
                    return;

                case 1:
                    tblName[1][offset] = data;
                    return;

                case 2:
                    // ExRAM làm nametable
                    mmc5->WriteExRam(offset, data);
                    return;

                case 3:
                    // Fill mode không ghi trực tiếp vào nametable
                    return;
                }
            }
        }
        // VRC6 nametable banking
        if (cart && cart->pMapper)
        {
            if (auto* vrc6 = dynamic_cast<Mapper_024*>(cart->pMapper.get()))
            {
                uint8_t source = vrc6->GetNtSource(ntIndex);
                tblName[source & 0x01][offset] = data;
                return;
            }
        }
        // Mirroring cũ cho mapper thường
        addr = ntAddr;
        MIRROR m = cart->mirror;
        if (cart->pMapper != nullptr) {
            MIRROR mapper_mirror = cart->pMapper->mirror();
            if (mapper_mirror != MIRROR::HARDWARE) {
                m = mapper_mirror;
            }
        }
        if (m == MIRROR::VERTICAL) {
            if (addr <= 0x03FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x07FF) tblName[1][addr & 0x03FF] = data;
            else if (addr <= 0x0BFF) tblName[0][addr & 0x03FF] = data;
            else tblName[1][addr & 0x03FF] = data;
        }
        else if (m == MIRROR::HORIZONTAL) {
            if (addr <= 0x03FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x07FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x0BFF) tblName[1][addr & 0x03FF] = data;
            else tblName[1][addr & 0x03FF] = data;
        }
        else if (m == MIRROR::ONESCREEN_LO) {
            tblName[0][addr & 0x03FF] = data;
        }
        else if (m == MIRROR::ONESCREEN_HI) {
            tblName[1][addr & 0x03FF] = data;
        }

    }
    else if (addr >= 0x3F00) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000; if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008; if (addr == 0x001C) addr = 0x000C;
        tblPalette[addr] = data & 0x3F;
    }
}


void PPU::LoadBackgroundShifters() {
    bg_shifter_pattern_lo = (bg_shifter_pattern_lo & 0xFF00) | bg_next_tile_lsb;
    bg_shifter_pattern_hi = (bg_shifter_pattern_hi & 0xFF00) | bg_next_tile_msb;
    bg_shifter_attrib_lo = (bg_shifter_attrib_lo & 0xFF00) | ((bg_next_tile_attrib & 0b01) ? 0xFF : 0x00);
    bg_shifter_attrib_hi = (bg_shifter_attrib_hi & 0xFF00) | ((bg_next_tile_attrib & 0b10) ? 0xFF : 0x00);
}

void PPU::UpdateShifters() {
    if (ppu_mask & 0x18) {
        bg_shifter_pattern_lo <<= 1; bg_shifter_pattern_hi <<= 1;
        bg_shifter_attrib_lo <<= 1; bg_shifter_attrib_hi <<= 1;
    }
}

//PHẦN CHÍNH
void PPU::Step() {
    debugTotalPpuCycles++;
    g_ppu_cycle_count++;

    if (!mapper_a12 && mapper_a12_low_cycles < 255)
    {
        mapper_a12_low_cycles++;
    }
    if (cycle == 0)
    {
        if (cart && cart->pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
            {
                mmc5->NotifyScanline(scanline);
            }
        }
    }
    auto lam_IncScrollX = [&]() {
        if (ppu_mask & 0x18) {
            if (vram_addr.coarse_x == 31) { vram_addr.coarse_x = 0; vram_addr.nametable_x = ~vram_addr.nametable_x; }
            else { vram_addr.coarse_x++; }
        }
        };
    auto lam_IncScrollY = [&]() {
        if (ppu_mask & 0x18) {
            if (vram_addr.fine_y < 7) { vram_addr.fine_y++; }
            else {
                vram_addr.fine_y = 0;
                if (vram_addr.coarse_y == 29) { vram_addr.coarse_y = 0; vram_addr.nametable_y = ~vram_addr.nametable_y; }
                else if (vram_addr.coarse_y == 31) { vram_addr.coarse_y = 0; }
                else { vram_addr.coarse_y++; }
            }
        }
        };
    auto lam_TransAddrX = [&]() { if (ppu_mask & 0x18) { vram_addr.nametable_x = tram_addr.nametable_x; vram_addr.coarse_x = tram_addr.coarse_x; } };
    auto lam_TransAddrY = [&]() { if (ppu_mask & 0x18) { vram_addr.fine_y = tram_addr.fine_y; vram_addr.nametable_y = tram_addr.nametable_y; vram_addr.coarse_y = tram_addr.coarse_y; } };

    if (scanline >= -1 && scanline < 240) {
        // vbl_visible_to_write clear SỚM HƠN 1 dot so với status thật - xem
        // giải thích ở khai báo trong PPU.h (fix test 07-nmi_on_timing).
        if (scanline == -1 && cycle == 0) vbl_visible_to_write = false;
        if (scanline == -1 && cycle == 1) status &= ~0xE0;

        // --- Sprite overflow timing (gần đúng) ---
        // Trên phần cứng thật, cờ overflow được set ở giữa scanline, đúng lúc
        // sprite evaluation (cycle 65-256) tìm ra sprite thứ 9 hợp lệ - KHÔNG
        // phải luôn luôn ở cuối (cycle 257) như trước. Thời điểm chính xác lệ
        // thuộc sprite nào (OAM index n, 0-63) gây ra overflow: càng nằm sau
        // trong OAM thì càng set trễ. Việc set cờ luôn ở cycle 257 (tức luôn
        // "trễ nhất có thể") là lý do fail 3.Timing #5 (quá trễ cho scanline
        // đầu) và khiến Battletoads màn 2 bị treo (game poll cờ bằng vòng lặp
        // chờ ở đúng thời điểm phần cứng thật set nó, sớm hơn nhiều so với 257).
        if (cycle == 65) {
            pending_overflow_cycle = -1;
            if (ppu_mask & 0x18) {
                int target_scanline = scanline + 1;
                // Lưu ý: <= 240 (KHÔNG phải < 240). Trong lúc quét scanline 239
                // (dòng hiển thị cuối cùng), phần cứng vẫn evaluate sprite cho
                // "dòng 240 ảo" (post-render, không bao giờ được vẽ) và VẪN set
                // cờ overflow dựa trên kết quả đó - đây là lý do sprite Y=239
                // (hiển thị ở hàng 240) phải set cờ, còn Y=240/255 thì không.
                if (target_scanline >= 0 && target_scanline <= 240) {
                    int spriteHeight = (ppu_ctrl & 0x20) ? 16 : 8;

                    auto inRangeAt = [&](int oamByteIndex) -> bool {
                        // Đọc đúng byte tại offset này của OAM và coi nó như thể
                        // là Y-coordinate (kể cả khi thực chất đó là tile/attr/X -
                        // đây chính là bug của phần cứng thật ở giai đoạn 2 bên dưới).
                        uint8_t y = OAM[oamByteIndex & 0xFF];
                        int diffY = target_scanline - (y + 1);
                        return (diffY >= 0 && diffY < spriteHeight);
                        };

                    // Bắt đầu ở 64 (không phải 65): cycle 65 là cycle ĐỌC (lẻ),
                    // quyết định in-range/ghi secondary OAM diễn ra ở cycle CHẴN
                    // kế tiếp (66). Mỗi cặp đọc+quyết định = +2, nên t=64+2=66
                    // đúng là cycle quyết định đầu tiên.
                    int t = 64;
                    int n = 0;
                    int count = 0;

                    // Giai đoạn 1 (n, m luôn đồng bộ - KHÔNG có bug): gom tối đa
                    // 8 sprite hợp lệ đầu tiên vào secondary OAM. Sprite không hợp
                    // lệ chỉ tốn 2 cycle (đọc Y + quyết định); sprite hợp lệ tốn
                    // thêm 6 cycle để copy 3 byte còn lại.
                    while (n < 64 && count < 8) {
                        bool inRange = inRangeAt(n * 4 + 0);
                        t += 2;
                        if (inRange) {
                            count++;
                            t += 6;
                        }
                        n++;
                    }

                    // Giai đoạn 2: đã đủ 8 sprite -> chuyển sang dò overflow. Từ
                    // đây trở đi, nếu sprite ĐANG XÉT không hợp lệ, phần cứng tăng
                    // CẢ n LẪN m (bug thật, không "carry" bình thường), nên các lần
                    // kiểm tra tiếp theo có thể đọc nhầm byte khác làm Y. Cờ overflow
                    // được set ngay khi tìm ra sprite (thật hoặc "giả" do đọc nhầm)
                    // rơi vào phạm vi scanline tiếp theo.
                    if (count == 8 && n < 64) {
                        int m = 0;
                        while (n < 64) {
                            bool inRange = inRangeAt(n * 4 + m);
                            t += 2;
                            if (inRange) {
                                pending_overflow_cycle = (int16_t)((t > 256) ? 256 : t);
                                break;
                            }
                            n++;
                            m = (m + 1) & 3;
                        }
                    }
                }
            }
        }
        if (cycle == pending_overflow_cycle) {
            status |= 0x20;
            pending_overflow_cycle = -1;
        }

        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338)) {
            UpdateShifters();

            if (ppu_mask & 0x18) {
                switch ((cycle - 1) % 8) {
                case 0:
                    LoadBackgroundShifters();
                    NotifyMapperA12(0x0000);
                    bg_next_tile_id = ppuRead(0x2000 | (vram_addr.reg & 0x0FFF));
                    break;
                case 2:
                {
                    NotifyMapperA12(0x0000);

                    bool usedMmc5ExtendedAttr = false;

                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                        {
                            if (mmc5->GetExRamMode() == 1)
                            {
                                uint16_t exOffset =
                                    (uint16_t(vram_addr.coarse_y) * 32)
                                    + vram_addr.coarse_x;

                                uint8_t ex = mmc5->ReadExRam(exOffset);

                                // bit 6-7: palette cho tile hiện tại
                                bg_next_tile_attrib = (ex >> 6) & 0x03;

                                // bit 0-5: CHR 4KB page cho tile hiện tại
                                mmc5->SetExtendedChrBank(ex & 0x3F);

                                usedMmc5ExtendedAttr = true;
                            }
                        }
                    }

                    if (!usedMmc5ExtendedAttr)
                    {
                        bg_next_tile_attrib = ppuRead(
                            0x23C0
                            | (vram_addr.nametable_y << 11)
                            | (vram_addr.nametable_x << 10)
                            | ((vram_addr.coarse_y >> 2) << 3)
                            | (vram_addr.coarse_x >> 2)
                        );

                        if (vram_addr.coarse_y & 0x02)
                            bg_next_tile_attrib >>= 4;

                        if (vram_addr.coarse_x & 0x02)
                            bg_next_tile_attrib >>= 2;

                        bg_next_tile_attrib &= 0x03;
                    }
                }
                break;
                case 4:
                {
                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                            mmc5->SetChrFetchModeBackground();
                    }

                    bg_next_tile_lsb = ppuRead(
                        ((ppu_ctrl & 0x10) ? 0x1000 : 0x0000)
                        + ((uint16_t)bg_next_tile_id << 4)
                        + vram_addr.fine_y
                        + 0
                    );
                }
                break;

                case 6:
                {
                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                            mmc5->SetChrFetchModeBackground();
                    }

                    bg_next_tile_msb = ppuRead(
                        ((ppu_ctrl & 0x10) ? 0x1000 : 0x0000)
                        + ((uint16_t)bg_next_tile_id << 4)
                        + vram_addr.fine_y
                        + 8
                    );
                }
                break;
                case 7: lam_IncScrollX(); break;
                }
            }
        }
        if (cycle == 256) lam_IncScrollY();
        if (cycle == 257) {
            LoadBackgroundShifters();
            lam_TransAddrX();

            if (ppu_mask & 0x18) {
                sprite_count = 0;

                int target_scanline = scanline + 1;

                // CHỈ đánh giá (evaluation) ở đây: tìm sprite khớp scanline kế
                // tiếp và tính sẵn pattern_addr/x/attribute/cờ sprite-0. KHÔNG
                // gọi ppuRead() ở đây cho 8 sprite đầu - việc đó rải ra đúng
                // dot thật (257-320, 8 dot/sprite) ở khối ngay dưới, để A12
                // toggle đúng lúc phần cứng thật toggle. Đây là fix cho hiện
                // tượng HUD rung ở các game canh IRQ khít theo cycle như
                // Wolverine / Summer Carnival '92 - Recca (toàn bộ 8 sprite
                // trước đây bị dồn vào đúng 1 dot giả lập).
                if (target_scanline >= 0 && target_scanline < 240) {
                    int spriteLimit = bRemoveSpriteLimit ? 64 : 8;
                    int foundSprites = 0;
                    for (int i = 0; i < 64; i++)
                    {
                        uint8_t sprite_y = OAM[i * 4 + 0];
                        uint8_t sprite_id = OAM[i * 4 + 1];
                        uint8_t sprite_attr = OAM[i * 4 + 2];
                        uint8_t sprite_x_pos = OAM[i * 4 + 3];

                        int spriteHeight = (ppu_ctrl & 0x20) ? 16 : 8;
                        int diffY = target_scanline - (sprite_y + 1);

                        if (diffY >= 0 && diffY < spriteHeight)
                        {
                            foundSprites++;
                            // Cờ overflow ($2002 bit 5) đã được set ở đúng cycle (65-256)
                            // tại khối tính pending_overflow_cycle phía trên - không set ở
                            // đây nữa để tránh luôn set trễ vào đúng cycle 257.

                            if (sprite_count >= spriteLimit)
                                continue;

                            bool flipY = (sprite_attr & 0x80) != 0;
                            int row = flipY ? (spriteHeight - 1 - diffY) : diffY;

                            uint16_t pattern_addr = 0;

                            if (spriteHeight == 8)
                            {
                                pattern_addr =
                                    ((ppu_ctrl & 0x08) ? 0x1000 : 0x0000) |
                                    ((uint16_t)sprite_id << 4) |
                                    row;
                            }
                            else
                            {
                                uint16_t table = (sprite_id & 0x01) ? 0x1000 : 0x0000;
                                uint16_t tile = (sprite_id & 0xFE);

                                if (row < 8)
                                    pattern_addr = table | (tile << 4) | row;
                                else
                                    pattern_addr = table | ((tile + 1) << 4) | (row - 8);
                            }

                            sprite_x[sprite_count] = sprite_x_pos;
                            sprite_attribute[sprite_count] = sprite_attr;
                            sprite_zero_being_rendered[sprite_count] = (i == 0);

                            if (sprite_count < 8)
                            {
                                // 8 sprite đầu tiên: chỉ lưu địa chỉ, việc gọi
                                // ppuRead() thật sự diễn ra đúng dot của nó ở
                                // khối 257-320 bên dưới.
                                sprite_pattern_addr[sprite_count] = pattern_addr;
                            }
                            else
                            {
                                // Chỉ xảy ra khi bRemoveSpriteLimit bật (>8
                                // sprite/dòng - vốn dĩ đã không có timing thật
                                // trên phần cứng NES để mô phỏng cho trường
                                // hợp này), nên đọc ngay tại đây như cũ.
                                if (cart && cart->pMapper)
                                {
                                    if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                                        mmc5->SetChrFetchModeSprite();
                                }
                                sprite_pattern_lo[sprite_count] = ppuRead(pattern_addr);
                                sprite_pattern_hi[sprite_count] = ppuRead(pattern_addr + 8);
                            }

                            sprite_count++;
                        }
                    }
                }
            }
        }

        // Fetch pattern sprite rải đúng theo dot thật của phần cứng NES:
        // dot 257-264 = sprite 0, 265-272 = sprite 1, ..., 313-320 = sprite 7
        // (8 dot/sprite). Byte thấp đọc ở dot con thứ 5, byte cao ở dot con
        // thứ 7 trong nhóm 8 dot đó (khớp cấu trúc timing PPU chuẩn NESdev).
        // Rải fetch ra đúng dot khiến A12 (dùng để clock bộ đếm IRQ kiểu
        // MMC3) toggle đúng lúc phần cứng thật toggle - sửa hiện tượng HUD
        // rung ở Wolverine / Summer Carnival '92 - Recca, và các game MMC3
        // khác canh IRQ khít theo từng cycle.
        if (cycle >= 257 && cycle <= 320 && (ppu_mask & 0x18)) {
            int idx = cycle - 257;
            int spriteSlot = idx / 8;
            int subCycle = idx % 8;

            // Mỗi slot 8 dot của phần cứng bắt đầu bằng 2 lần fetch nametable/attribute
            // "rác" (dot 1 và 3 của slot, địa chỉ $2xxx -> A12 = 0) rồi mới tới pattern
            // lo/hi. Bắt buộc mô phỏng để A12 hạ xuống giữa 2 sprite khác bảng pattern
            // (8x16 trộn bảng): khoảng thấp thật là ~12 dot, vượt ngưỡng lọc 10 nên vẫn
            // phải clock IRQ như phần cứng thật - thiếu 2 lần gọi này thì khoảng thấp
            // trong mô phỏng chỉ còn 8 dot và bị lọc mất.
            if (subCycle == 0 || subCycle == 2)
                NotifyMapperA12(0x0000);

            if (subCycle == 5 || subCycle == 7) {
                if (cart && cart->pMapper)
                {
                    if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                        mmc5->SetChrFetchModeSprite();
                }

                if (spriteSlot < sprite_count && spriteSlot < 8)
                {
                    uint16_t pattern_addr = sprite_pattern_addr[spriteSlot];
                    if (subCycle == 5)
                        sprite_pattern_lo[spriteSlot] = ppuRead(pattern_addr);
                    else
                        sprite_pattern_hi[spriteSlot] = ppuRead(pattern_addr + 8);
                }
                else
                {
                    // Slot trống (dòng có ít hơn 8 sprite): phần cứng thật
                    // vẫn fetch "rác" bằng tile $FF ở đúng dot này - không
                    // được bỏ qua, nếu không A12 sẽ thiếu đúng số lần toggle
                    // so với thật.
                    uint16_t dummy_addr = ((ppu_ctrl & 0x08) ? 0x1000 : 0x0000) | (0xFF << 4);
                    if (subCycle == 5)
                        ppuRead(dummy_addr);
                    else
                        ppuRead(dummy_addr + 8);
                }
            }
        }
        // Dummy nametable fetch: phần cứng NES thật fetch 2 lần "thừa" tại đúng
        // dot 337 và 339 (không phải 338/340). Lệch 1 cycle ở đây từng gây A12
        // toggle sai nhịp -> bộ đếm IRQ của mapper kiểu MMC3 (dùng để chia màn
        // hình / status bar) bị trôi pha dần theo thời gian, biểu hiện thành
        // hiện tượng mép trên cuộn trước - mép dưới cuộn theo sau khi scroll.
        if (cycle == 337 || cycle == 339) {
            if (ppu_mask & 0x18) {
                NotifyMapperA12(0x0000);
                bg_next_tile_id = ppuRead(0x2000 | (vram_addr.reg & 0x0FFF));
            }
        }
        if (scanline == -1 && cycle >= 280 && cycle < 305) lam_TransAddrY();
    }

    if (scanline >= 0 && scanline < 240 && cycle >= 1 && cycle <= 256) {
        uint8_t bg_pixel = 0x00; uint8_t bg_palette = 0x00;
        if (ppu_mask & 0x08) {
            uint16_t bit_mux = 0x8000 >> fine_x;
            uint8_t p0_pixel = (bg_shifter_pattern_lo & bit_mux) > 0; uint8_t p1_pixel = (bg_shifter_pattern_hi & bit_mux) > 0;
            bg_pixel = (p1_pixel << 1) | p0_pixel;
            uint8_t bg_pal0 = (bg_shifter_attrib_lo & bit_mux) > 0; uint8_t bg_pal1 = (bg_shifter_attrib_hi & bit_mux) > 0;
            bg_palette = (bg_pal1 << 1) | bg_pal0;
            if (!(ppu_mask & 0x02) && (cycle >= 1 && cycle <= 8)) bg_pixel = 0;
        }

        uint8_t fg_pixel = 0x00; uint8_t fg_palette = 0x00; uint8_t fg_priority = 0; bool bSpriteZeroBeingRendered = false;
        if (ppu_mask & 0x10) {
            for (int i = 0; i < sprite_count; i++) {
                int diffX = (cycle - 1) - sprite_x[i];
                if (diffX >= 0 && diffX < 8) {
                    uint8_t flipX = (sprite_attribute[i] & 0x40) > 0;
                    int col = flipX ? (7 - diffX) : diffX;
                    uint8_t bit_mux = 0x80 >> col;

                    if ((sprite_pattern_lo[i] & bit_mux) || (sprite_pattern_hi[i] & bit_mux)) {
                        uint8_t p0_pixel = (sprite_pattern_lo[i] & bit_mux) > 0;
                        uint8_t p1_pixel = (sprite_pattern_hi[i] & bit_mux) > 0;
                        fg_pixel = (p1_pixel << 1) | p0_pixel;
                        fg_palette = (sprite_attribute[i] & 0x03) + 0x04;
                        fg_priority = (sprite_attribute[i] & 0x20) == 0;
                        bSpriteZeroBeingRendered = sprite_zero_being_rendered[i];
                        break;
                    }
                }
            }
            
            if (!(ppu_mask & 0x04) && (cycle >= 1 && cycle <= 8)) fg_pixel = 0;
        }

        if (bSpriteZeroBeingRendered && (ppu_mask & 0x18) == 0x18) {
            if (cycle >= 1 && cycle < 256) {
                if (bg_pixel > 0 && fg_pixel > 0) {
                    status |= 0x40;
                }
            }
        }
        uint8_t final_pixel = 0;
        uint8_t final_palette = 0;
        if (bg_pixel == 0 && fg_pixel == 0) {
            final_pixel = 0; final_palette = 0;
        }
        else
            if (bg_pixel == 0 && fg_pixel > 0) {
                final_pixel = fg_pixel; final_palette = fg_palette;
            }
            else
                if (bg_pixel > 0 && fg_pixel == 0) {
                    final_pixel = bg_pixel; final_palette = bg_palette;
                }
                else {
                    if (fg_priority) {
                        final_pixel = fg_pixel;
                        final_palette = fg_palette;
                    }
                    else {
                        final_pixel = bg_pixel;
                        final_palette = bg_palette;
                    }
                }

        uint16_t backdrop_addr = ((ppu_mask & 0x18) == 0 && (vram_addr.reg & 0x3FFF) >= 0x3F00)
            ? (vram_addr.reg & 0x3FFF)
            : 0x3F00;
        uint8_t pal_idx = (final_pixel != 0)
            ? (ppuRead(0x3F00 + (final_palette << 2) + final_pixel) & 0x3F)
            : (ppuRead(backdrop_addr) & 0x3F);

        if (ppu_mask & 0x01)
            pal_idx &= 0x30;

        {
            RGBColor c = palScreen[pal_idx];
            frame_pixels[scanline * 256 + (cycle - 1)] = 0xFF000000 | (c.r << 16) | (c.g << 8) | c.b;
        }
    }

    if (scanline == 241 && cycle == 1) {
        if (!suppress_vbl_this_frame) {
            status |= 0x80;
            vbl_visible_to_write = true;
            if (ppu_ctrl & 0x80) nmi_requested = true;
        }
        suppress_vbl_this_frame = false; // chỉ áp dụng cho đúng frame vừa bị đọc lệch pha

        if (debugLastVblCycle >= 0)
        {
            long delta = debugTotalPpuCycles - debugLastVblCycle;
            if (debugVblSetCount < 200)
                qDebug("[PPU-DEBUG] VBL set #%d, cach lan truoc %ld PPU cycle (odd_frame=%d, mask=0x%02X)",
                    debugVblSetCount, delta, odd_frame ? 1 : 0, ppu_mask);
        }
        debugLastVblCycle = debugTotalPpuCycles;
        debugVblSetCount++;
    }

    // Chốt (latch) trạng thái rendering ĐÚNG lúc dot 338 - xem giải thích ở
    // khai báo odd_skip_render_enabled_latched trong PPU.h. Phải đặt TRƯỚC
    // khối skip bên dưới vì cùng nằm cuối Step() của dot 338 (chạy trước dot
    // 339 đúng 1 lần gọi Step() sau).
    if (scanline == -1 && cycle == 338)
    {
        odd_skip_render_enabled_latched = (ppu_mask & 0x18) != 0;
    }

    // 262 scanline * 341 cycle = 89.342 dot/frame, KHÔNG chia hết cho 3 (89.342 mod 3 = 2).
    // Nếu mọi frame đều dài 89.342 dot như nhau, quan hệ pha giữa "PPU đang ở
    // scanline/cycle nào" và "CPU đã chạy bao nhiêu cycle" (Bus::clock() dùng
    // system_clock_counter % 3) sẽ TRÔI DẦN 2 dot mỗi frame, không bao giờ tự
    // đồng bộ lại -> biểu hiện thành hiện tượng điểm chia màn hình/HUD trôi
    // xuống dần rồi giật bù lại theo chu kỳ. Bỏ đúng 1 dot ở cycle 339 của dòng
    // pre-render vào frame LẺ (khi rendering đang bật) làm cho tổng 2 frame liền
    // nhau = 178.683 dot, chia hết cho 3 -> tự resync mỗi 2 frame, đúng NTSC.
    //
    // Dùng odd_skip_render_enabled_latched (chốt ở dot 338) thay vì đọc
    // ppu_mask sống ở đây - fix test 10-even_odd_timing #3.
    if (scanline == -1 && cycle == 339 && odd_frame && odd_skip_render_enabled_latched)
    {
        cycle = 0;
        scanline = 0;
    }
    else
    {
        cycle++;
        if (cycle >= 341) {
            cycle = 0;
            scanline++;
            if (scanline >= 261) {
                scanline = -1;
                odd_frame = !odd_frame; // đảo cờ mỗi khi bắt đầu frame mới
            }
        }
    }

}

const uint32_t* PPU::GetScreenBuffer() const {
    return frame_pixels;
}
void PPU::NotifyMapperA12(uint16_t addr)
{
    bool new_a12 = (addr & 0x1000) != 0;

    if (!mapper_a12 && new_a12)
    {
        // Bộ lọc A12 của MMC3: chỉ tính cạnh lên nếu A12 đã THẤP đủ lâu. Ngưỡng
        // phải >= 10 dot (Mesen: cyclesDown > 10, tính cả dot đầu = 10 dot trôi qua).
        // Với ngưỡng 8 như trước, game để BG ở $1000 + sprite ở $0000 (như Wario's
        // Woods) bị đếm THỪA 1 lần mỗi scanline: A12 hạ xuống ở dot 337 (fetch
        // nametable thừa) rồi lên lại ở dot 5 của dòng kế = 9 dot -> lọt lưới 8, nên
        // bộ đếm IRQ chạy gấp đôi tốc độ và ngắt bắn ở NỬA vị trí thật (scanline
        // ~108 thay vì ~216). Phần cứng thật/Mesen lọc cạnh này (9 < 10).
        if (mapper_a12_low_cycles >= 10)
        {
            if (cart && cart->pMapper)
            {
                cart->pMapper->ClockA12();
            }
        }

        mapper_a12_low_cycles = 0;
    }

    if (mapper_a12 && !new_a12)
    {
        mapper_a12_low_cycles = 0;
    }

    mapper_a12 = new_a12;
}
uint8_t PPU::GetOAMByte(uint8_t index) const
{
    return OAM[index];
}

uint8_t PPU::GetPPUCtrl() const
{
    return ppu_ctrl;
}

PPU::RGBColor PPU::GetNESColor(uint8_t index) const {
    return palScreen[index & 0x3F];
}
uint8_t PPU::DebugPPURead(uint16_t addr)
{
    uint8_t data = 0x00;
    addr &= 0x3FFF;

    if (cart && cart->ppuRead(addr, data))
    {
        return data;
    }
    else if (addr <= 0x1FFF)
    {
        data = tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF];
    }
    else if (addr >= 0x2000 && addr <= 0x3EFF)
    {
        uint16_t ntAddr = addr & 0x0FFF;

        MIRROR m = cart->mirror;

        if (cart->pMapper != nullptr)
        {
            MIRROR mapper_mirror = cart->pMapper->mirror();
            if (mapper_mirror != MIRROR::HARDWARE)
            {
                m = mapper_mirror;
            }
        }

        if (m == MIRROR::VERTICAL)
        {
            if (ntAddr <= 0x03FF) data = tblName[0][ntAddr & 0x03FF];
            else if (ntAddr <= 0x07FF) data = tblName[1][ntAddr & 0x03FF];
            else if (ntAddr <= 0x0BFF) data = tblName[0][ntAddr & 0x03FF];
            else data = tblName[1][ntAddr & 0x03FF];
        }
        else if (m == MIRROR::HORIZONTAL)
        {
            if (ntAddr <= 0x03FF) data = tblName[0][ntAddr & 0x03FF];
            else if (ntAddr <= 0x07FF) data = tblName[0][ntAddr & 0x03FF];
            else if (ntAddr <= 0x0BFF) data = tblName[1][ntAddr & 0x03FF];
            else data = tblName[1][ntAddr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_LO)
        {
            data = tblName[0][ntAddr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_HI)
        {
            data = tblName[1][ntAddr & 0x03FF];
        }
    }
    else if (addr >= 0x3F00)
    {
        addr &= 0x001F;

        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;

        data = tblPalette[addr] & 0x3F;
    }

    return data;
}
void PPU::SetExtraScanlinesBeforeNMI(int value)
{
    if (value < 0) value = 0;
    if (value > 300) value = 300;
    extraScanlinesBeforeNMI = value;
}

int PPU::GetExtraScanlinesBeforeNMI() const
{
    return extraScanlinesBeforeNMI;
}
void PPU::SaveState(BinaryWriter& out) const
{
    out << nmi_requested;
    out << oam_addr;

    for (int i = 0; i < 256; i++)
        out << OAM[i];

    out << mapper_a12;
    out << mapper_a12_low_cycles;

    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 1024; i++)
            out << tblName[t][i];

    for (int i = 0; i < 32; i++)
        out << tblPalette[i];

    out << ppu_ctrl;
    out << ppu_mask;
    out << status;
    out << ppu_data_buffer;
    out << address_latch;

    out << vram_addr.reg;
    out << tram_addr.reg;
    out << fine_x;

    out << scanline;
    out << cycle;

    out << bg_next_tile_id;
    out << bg_next_tile_attrib;
    out << bg_next_tile_lsb;
    out << bg_next_tile_msb;

    out << bg_shifter_pattern_lo;
    out << bg_shifter_pattern_hi;
    out << bg_shifter_attrib_lo;
    out << bg_shifter_attrib_hi;

    out << extraScanlinesBeforeNMI;
    out << bRemoveSpriteLimit;
    out << sprite_count;
    out << odd_frame;

    for (int i = 0; i < 64; i++)
    {
        out << sprite_pattern_lo[i];
        out << sprite_pattern_hi[i];
        out << sprite_x[i];
        out << sprite_attribute[i];
        out << sprite_zero_being_rendered[i];
    }
}

void PPU::LoadState(BinaryReader& in)
{
    in >> nmi_requested;
    in >> oam_addr;

    for (int i = 0; i < 256; i++)
        in >> OAM[i];

    in >> mapper_a12;
    in >> mapper_a12_low_cycles;

    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 1024; i++)
            in >> tblName[t][i];

    for (int i = 0; i < 32; i++)
        in >> tblPalette[i];

    in >> ppu_ctrl;
    in >> ppu_mask;
    in >> status;
    in >> ppu_data_buffer;
    in >> address_latch;

    in >> vram_addr.reg;
    in >> tram_addr.reg;
    in >> fine_x;

    in >> scanline;
    in >> cycle;

    in >> bg_next_tile_id;
    in >> bg_next_tile_attrib;
    in >> bg_next_tile_lsb;
    in >> bg_next_tile_msb;

    in >> bg_shifter_pattern_lo;
    in >> bg_shifter_pattern_hi;
    in >> bg_shifter_attrib_lo;
    in >> bg_shifter_attrib_hi;

    in >> extraScanlinesBeforeNMI;
    in >> bRemoveSpriteLimit;
    in >> sprite_count;
    in >> odd_frame;

    for (int i = 0; i < 64; i++)
    {
        in >> sprite_pattern_lo[i];
        in >> sprite_pattern_hi[i];
        in >> sprite_x[i];
        in >> sprite_attribute[i];
        in >> sprite_zero_being_rendered[i];
    }
}