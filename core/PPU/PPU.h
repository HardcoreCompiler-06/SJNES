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
#pragma once
#include <cstdint>
#include <memory>
#include "Cartridge.h"
#include "BinaryIO.h"
class PPU {
public:
    PPU();
    ~PPU();
    void SetRemoveSpriteLimit(bool enable) { bRemoveSpriteLimit = enable; }
    //ép xung =))
    void SetExtraScanlinesBeforeNMI(int value);
    int GetExtraScanlinesBeforeNMI() const;
    void ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge);
    void reset();
    void Step(); // Chạy 1 chu kỳ PPU
    uint8_t getPPUMask() { return ppu_mask; }
    uint8_t getPPUCtrl() { return ppu_ctrl; }
    struct RGBColor { uint8_t r, g, b; };
    RGBColor palScreen[0x40];
    RGBColor GetNESColor(uint8_t index) const;
    // GIAO TIẾP VỚI CPU (Đọc/Ghi các thanh ghi $2000 - $2007)
    uint8_t cpuRead(uint16_t addr, bool rdonly = false);
    void cpuWrite(uint16_t addr, uint8_t data);

    // GIAO TIẾP VỚI BĂNG GAME VÀ BỘ NHỚ PPU ($0000 - $3FFF)
    uint8_t ppuRead(uint16_t addr, bool rdonly = false);
    void ppuWrite(uint16_t addr, uint8_t data);

    // XUẤT HÌNH ẢNH RA QT CỦA ANH
    const uint32_t* GetScreenBuffer() const;
    uint32_t GetPixelAt(int x, int y) const {
        if (x < 0 || x >= 256 || y < 0 || y >= 240) return 0x00000000;
        return frame_pixels[y * 256 + x];
    }

    // HÀM PHỤ TRỢ CHO DEBUG / SPRITE VIEWER
    uint8_t DebugPPURead(uint16_t addr);
    uint8_t GetOAMByte(uint8_t index) const;
    uint8_t GetPPUCtrl() const;
    // Cho overlay debug (F10): vị trí PPU đang quét và địa chỉ VRAM hiện tại (thanh ghi v)
    int GetScanline() const { return scanline; }
    int GetCycle() const { return cycle; }
    uint16_t GetVramAddr() const { return vram_addr.reg; }

    // Báo cho vòng lặp BÊN NGOÀI (EmuWorker::runFrame) biết CHÍNH XÁC lúc nào
    // 1 frame mới bắt đầu (vừa wrap từ scanline 260 sang -1, cycle 0) - thay
    // vì đếm cố định 89342 dot/frame. Cần thiết vì odd-frame cycle skip khiến
    // frame LẺ chỉ có 89341 dot, không phải con số cố định.
    bool IsFrameBoundary() const { return scanline == -1 && cycle == 0; }


    // CÁC BIẾN TRẠNG THÁI QUAN TRỌNG ĐỂ BUS.CPP VÀ UI GỌI
    bool nmi_requested = false;

    // NMI do CHÍNH lệnh CPU ghi $2000 (bật NMI enable) gây ra, TRONG LÚC VBL
    // flag đã set sẵn từ trước - xem NESdev "NMI"/test 04-nmi_control #11
    // ("Immediate occurence should be after NEXT instruction"). Khác với
    // nmi_requested (NMI "tự nhiên" do chính PPU set VBL ở (241,1), phải bắn
    // NGAY không trễ - xem test 05/07), trường hợp này phải trễ ĐÚNG 1 lệnh
    // CPU nữa mới được nhận ra, vì CPU poll ngắt ở cycle áp chót của mỗi lệnh:
    // ghi $2000 thường rơi vào cycle CUỐI của chính lệnh đó nên lần poll của
    // NGAY lệnh này đã lỡ mất, phải đợi lần poll của lệnh KẾ TIẾP. Cờ này chỉ
    // được PPU set lên true; việc "trễ 1 CPU cycle" trước khi thực sự gọi
    // CPU::nmi() do EmuWorker.cpp xử lý (biến đếm nmi_write_delay).
    bool nmi_write_pending = false;

    // NMI suppression race (case CÙNG dot hoặc 1 dot SAU lúc VBL set - xem
    // suppress_vbl_this_frame bên dưới cho case 1 dot TRƯỚC): với 2 case
    // này, cpu.nmi() ĐÃ được EmuWorker gọi (dispatch ngay khi Step() set VBL,
    // để tránh trễ 1 CPU cycle cho case bình thường) TRƯỚC KHI CPU kịp chạy
    // tới lệnh đọc $2002 của chính nó trong cùng tick. Chỉ xoá nmi_requested
    // (biến PPU) lúc đó không còn tác dụng gì nữa vì đã tiêu thụ rồi - cần
    // báo ngược cho EmuWorker gọi cpu.CancelPendingNmi() để huỷ đúng
    // nmi_pending BÊN TRONG CPU. Cờ này do cpuRead($2002) bật lên; EmuWorker
    // đọc và tự reset về false sau khi xử lý.
    bool cancel_pending_nmi = false;
    uint8_t oam_addr = 0x00;
    uint8_t OAM[256];
    void SaveState(BinaryWriter& out) const;
    void LoadState(BinaryReader& in);

    uint8_t GetPaletteRAM(uint8_t index) const { return tblPalette[index & 0x1F]; }
private:
    bool bRemoveSpriteLimit = false;
    int sprite_count = 0;
    uint8_t sprite_pattern_lo[64];
    uint8_t sprite_pattern_hi[64];
    uint8_t sprite_x[64];
    uint8_t sprite_attribute[64];
    bool sprite_zero_being_rendered[64];
    // Địa chỉ pattern đã tính sẵn cho 8 slot sprite đầu tiên (dòng kế tiếp),
    // dùng bởi khối fetch rải-theo-dot ở cycle 257-320 trong Step(). Chỉ cần
    // 8 phần tử vì phần cứng thật chỉ có đúng 8 slot sprite/dòng quét; sprite
    // thứ 9 trở đi (chỉ có khi bRemoveSpriteLimit bật) không có timing thật
    // để mô phỏng nên vẫn đọc ngay tại cycle 257 như cũ.
    uint16_t sprite_pattern_addr[8];
    std::shared_ptr<Cartridge> cart;
    bool mapper_a12 = false;
    uint8_t mapper_a12_low_cycles = 0;
    // NMI race condition ($2002 read đúng lúc VBL sắp/vừa set - xem NESdev
    // "PPU frame timing"). Đọc $2002 ĐÚNG 1 dot TRƯỚC lúc VBL set nghĩa là
    // frame này KHÔNG BAO GIỜ được set flag/bắn NMI nữa, dù Step() sắp chạy
    // tới đúng dot set (scanline==241, cycle==1). Cờ này báo cho Step() biết
    // để bỏ qua việc set status|=0x80 / nmi_requested đúng 1 lần đó.
    bool suppress_vbl_this_frame = false;

    // Odd-frame cycle skip (xem chỗ dùng trong Step()): phần cứng thật CHỐT
    // (latch) trạng thái "rendering có đang bật hay không" ở ĐÚNG dot 338 của
    // dòng pre-render, KHÔNG đọc ppu_mask sống ngay tại dot 339 (nơi việc
    // skip thực sự xảy ra) - xem NESdev forum, người dùng report "passed"
    // chỉ khi lấy mẫu ở cycle 338, không phải 328/339. Nếu CPU ghi $2001 bật
    // BG đúng lúc dot 338->339, đọc ppu_mask sống ở 339 sẽ thấy giá trị MỚI
    // (quá trễ so với hardware) -> gây lỗi test 10-even_odd_timing #3
    // ("Clock is skipped too late, relative to enabling BG").
    bool odd_skip_render_enabled_latched = false;

    // "Bản sao" của bit VBL ($2002 bit 7), dùng RIÊNG cho cpuWrite($2000) khi
    // quyết định có bắn NMI hay không (thay vì đọc status&0x80 sống). Lý do:
    // NESdev forum xác nhận READ ($2002) và WRITE ($2000) "nhìn thấy" thời
    // điểm VBL tự động CLEAR ở đầu dòng pre-render KHÁC NHAU đúng 1 dot - fix
    // test 07-nmi_on_timing (enable NMI generate gần lúc VBL tự clear). Được
    // set/clear song song với status bit7 ở MỌI chỗ, TRỪ auto-clear tự nhiên
    // (scanline==-1) thì clear SỚM HƠN 1 dot (cycle==0 thay vì cycle==1).
    bool vbl_visible_to_write = false;
    void NotifyMapperA12(uint16_t addr);
    // BỘ NHỚ NỘI BỘ CỦA PPU
    uint8_t tblName[2][1024];    // 2 Nametables (2KB)
    uint8_t tblPattern[2][4096]; // 2 Pattern Tables (8KB - Thường do Cartridge đè lên)
    uint8_t tblPalette[32];      // Bảng màu Palette (32 bytes)

    // Khung hình để vẽ lên (256x240 pixels)
    uint32_t frame_pixels[256 * 240];

    // CÁC THANH GHI ĐIỀU KHIỂN CHÍNH
    uint8_t ppu_ctrl = 0x00;   // $2000
    uint8_t ppu_mask = 0x00;   // $2001
    uint8_t status = 0x00;     // $2002
    uint8_t ppu_data_buffer = 0x00; // Bộ đệm cho lệnh đọc $2007 trễ 1 nhịp
    uint8_t address_latch = 0x00;   // Latch cho $2005 và $2006 (w)

    // THANH GHI CUỘN MÀN HÌNH "LOOPY REGISTER" TRỨ DANH (Siêu quan trọng)
    union loopy_register {
        struct {
            uint16_t coarse_x : 5;
            uint16_t coarse_y : 5;
            uint16_t nametable_x : 1;
            uint16_t nametable_y : 1;
            uint16_t fine_y : 3;
            uint16_t unused : 1;
        };
        uint16_t reg = 0x0000;
    };

    loopy_register vram_addr; // Địa chỉ PPU hiện tại (v)
    loopy_register tram_addr; // Địa chỉ tạm thời lưu tọa độ (t)
    uint8_t fine_x = 0x00;    // Cuộn ngang pixel lẻ (x)

    int16_t scanline = 0; // Dòng đang quét (0-261)
    int16_t cycle = 0;    // Chu kỳ hiện tại trên dòng (0-340)
    bool odd_frame = false; // Cờ frame chẵn/lẻ (dùng cho odd-frame cycle skip NTSC)

    // Bộ nhớ tạm cho Background của Tile tiếp theo
    uint8_t bg_next_tile_id = 0x00;
    uint8_t bg_next_tile_attrib = 0x00;
    uint8_t bg_next_tile_lsb = 0x00;
    uint8_t bg_next_tile_msb = 0x00;

    // Thanh ghi trượt (Shifters) chứa dữ liệu 16 pixel để vẽ dần ra màn hình
    uint16_t bg_shifter_pattern_lo = 0x0000;
    uint16_t bg_shifter_pattern_hi = 0x0000;
    uint16_t bg_shifter_attrib_lo = 0x0000;
    uint16_t bg_shifter_attrib_hi = 0x0000;
    // Hàm phụ trợ cho việc quét Background
    void LoadBackgroundShifters();
    void UpdateShifters();

    // Sprite overflow timing: cycle (trong scanline hiện tại, khoảng 65-256)
    // tại đó cờ overflow ($2002 bit 5) THỰC SỰ được set trên phần cứng thật,
    // ước lượng theo sprite (OAM index, 0-63) nào gây ra overflow - xem
    // NESdev "Sprite overflow test" / "PPU sprite evaluation". -1 = không có
    // overflow ở scanline này (chưa tính, hoặc không tìm thấy sprite thứ 9).
    int16_t pending_overflow_cycle = -1;
    //ép xung 
    int extraScanlinesBeforeNMI = 0;

    // DEBUG TẠM - đếm log, reset mỗi lần mở ROM mới (reset() gọi lại)
    int debugM476LogCount = 0;
    int debugVblSetCount = 0;
    long debugLastVblCycle = -1;
    long debugTotalPpuCycles = 0;
};