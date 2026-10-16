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
	CPU6502 - 6502/2A03 CPU emulation for SJNES

	Derived from olc6502 by David Barr (javidx9), OneLoneCoder.com
	https://github.com/OneLoneCoder/olcNES
	Modified for SJNES (illegal opcodes, cycle-accurate execution, several IRQ sources, ...).

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

// Portions (IRQ/NMI timing model) derived from Mesen (https://github.com/SourMesen/Mesen2), GPL-3.0

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include "BinaryIO.h"
class Bus;

class CPU6502
{
public:
    CPU6502();
    ~CPU6502();
    int irqHandledCount = 0;
    int irqBlockedCount = 0;

public:
    // Các thanh ghi cốt lõi
    uint8_t  a = 0x00;
    uint8_t  x = 0x00;
    uint8_t  y = 0x00;
    uint8_t  stkp = 0x00;
    uint16_t pc = 0x0000;
    uint8_t  status = 0x00;

    void reset();
    void irq();
    void nmi(bool latchCurr = false);
    void CancelPendingNmi();
    void clock();

    enum IRQSource : uint8_t
    {
        IRQ_EXTERNAL = 1 << 0,
        IRQ_APU = 1 << 1,
        IRQ_DMC = 1 << 2,
    };
    void SetIrqSource(uint8_t source);
    void ClearIrqSource(uint8_t source);
    bool IsIrqActive() const;

    void ConnectBus(Bus* n) { bus = n; }

    uint8_t fetch();
    std::map<uint16_t, std::string> disassemble(uint16_t nStart, uint16_t nStop);
    bool complete();
    void SaveState(BinaryWriter& out) const;
    void LoadState(BinaryReader& in);
public:
    uint16_t  cycles = 0;
    enum FLAGS6502
    {
        C = (1 << 0), Z = (1 << 1), I = (1 << 2), D = (1 << 3),
        B = (1 << 4), U = (1 << 5), V = (1 << 6), N = (1 << 7),
    };

private:
    uint8_t GetFlag(FLAGS6502 f);
    void    SetFlag(FLAGS6502 f, bool v);
    bool nmi_pending = false;
    bool irq_pending = false;
    // Đánh dấu chuỗi BRK hiện tại đã bị NMI "cướp" vector (xem BRK_OP trong .inc)
    bool brk_hijacked_by_nmi = false;
    uint8_t irq_sources = 0;
    uint8_t  fetched = 0x00;
    uint16_t temp = 0x0000;
    uint16_t addr_abs = 0x0000;
    uint16_t addr_rel = 0x0000;
    uint8_t  opcode = 0x00;
    uint32_t clock_count = 0;

    // ==== State machine cho cycle-accurate execution ====
    int instr_cycle = 0;
    uint8_t  addr_lo = 0;
    uint16_t base_addr = 0;
    uint16_t eff_addr = 0;
    uint8_t  rmw_old_val = 0;
    bool     page_crossed = false;
    bool     branch_taken = false;

    // Trạng thái riêng cho NMI/IRQ - KHÔNG dùng chung với `opcode` vì 0xFE/0xFF
    // là các opcode THẬT (INC abs,X / ISC abs,X) sẽ gây xung đột nghiêm trọng.
    enum class SpecialMode : uint8_t { NONE_MODE, NMI_MODE, IRQ_MODE };
    SpecialMode specialMode = SpecialMode::NONE_MODE;

    // CLI/SEI/PLP trì hoãn hiệu lực IRQ-inhibition đúng 1 lệnh theo chuẩn 6502 thật.
    // irq_disable_shadow là giá trị "I" THỰC SỰ được dùng để xét IRQ (trễ hơn GetFlag(I) thật
    // đúng 1 nhịp bất cứ khi nào lệnh vừa chạy là CLI/SEI/PLP).
    uint8_t irq_disable_shadow = 1;
    bool    owe_shadow_sync = false;

    // Dùng RIÊNG cho rule "nhánh rẽ taken không crosspage bỏ qua IRQ check ở cycle cuối"
    // (không liên quan tới CLI/SEI/PLP, không dùng chung biến).
    bool     suppress_irq_check_this_instruction = false;

    // ---- BUG FIX #1: chặn NMI mới "cướp" ngay khi VỪA phục vụ xong 1 chuỗi ngắt ----
    // Phần cứng thật LUÔN chạy trọn lệnh ĐẦU TIÊN của handler (BRK/IRQ/NMI) trước khi
    // một NMI mới có thể ngắt tiếp (Mesen làm việc này bằng `_prevNeedNmi = false` cuối
    // BRK()). Kiến trúc của mình dùng CHUNG 1 cổng `instr_cycle==0` cho cả "vừa xong 1
    // lệnh thường" lẫn "vừa xong phục vụ ngắt", nên nếu nmi_pending bật lên TRONG LÚC
    // BRK_OP/specialMode đang chạy (sau điểm hijack), nó sẽ bị xét lại ngay lập tức ở
    // lượt clock() kế tiếp - PHẢI chặn 1 lần duy nhất bằng cờ này.
    bool nmi_retrigger_guard = false;

    // ---- BUG FIX #2: trễ 1 cycle cho MỨC tín hiệu ngắt (irq_sources/irq_pending) ----
    // irq_disable_shadow ở trên CHỈ trễ hiệu lực của cờ I (đúng cho CLI/SEI/PLP), nhưng
    // KHÔNG trễ chính MỨC tín hiệu IRQ - thứ có thể đổi bất cứ cycle nào (mapper IRQ như
    // MMC3 A12, APU frame IRQ...). Phần cứng thật (và Mesen: _prevRunIrq/_runIrq trong
    // EndCpuCycle()) luôn xét "mức tín hiệu tại cuối cycle ÁP CHÓT", không phải mức hiện
    // tại ngay lúc ranh giới lệnh. 2 biến dưới đây là thanh ghi dịch 2 tầng, cập nhật bởi
    // clock() (wrapper) sau MỖI cycle - không chỉ ở ranh giới lệnh - để mô phỏng đúng.
    bool irq_source_level_curr = false;
    bool irq_source_level_prev = false;

    // ---- BUG FIX #3 (test 05-nmi_timing): NMI cũng cần trễ đúng 1 cycle như IRQ,
    // không phải xét nmi_pending SỐNG ngay tại ranh giới lệnh. Chỉ cần 1 tầng (không
    // phải 2 tầng như IRQ) vì nmi_pending là tín hiệu CẠNH/STICKY (set 1 lần, đứng yên
    // tới khi tiêu thụ), không phải MỨC tính lại mỗi cycle như IRQ - chỉ cần né đúng
    // race cùng-tick với lúc EmuWorker gọi cpu.nmi() (xem PPU set VBL -> dispatch ngay).
    // LƯU Ý RỦI RO: cơ chế NMI-suppression-race (PPU::cancel_pending_nmi, xem
    // CancelPendingNmi()) huỷ nmi_pending SỐNG ngay lập tức, nhưng quyết định service
    // giờ dựa trên giá trị TRỄ 1 cycle - nếu huỷ xảy ra đúng 1 cycle ngay trước ranh
    // giới lệnh, có lý thuyết (dù rất hiếm) có thể "lọt" 1 NMI đáng lẽ đã bị huỷ. Cần
    // chạy lại TOÀN BỘ bộ ppu_vbl_nmi (không chỉ 05) sau khi thêm fix này để xác nhận
    // không hồi quy test 06-suppression.
    // ---- BUG FIX #3 (test 05-nmi_timing): nmi_pending cần "trễ" 2 cycle khi
    // xét ở ranh giới lệnh - THỰC CHẤT là do kiến trúc mô phỏng của mình cho
    // PPU chạy TRƯỚC CPU trong cùng 1 tick (giống nhiều emulator khác), nên
    // để CPU "nhìn thấy" đúng thời điểm mà 1 CPU thật (PPU/CPU chạy song song
    // thật sự) sẽ thấy, phải lùi lại đúng 2 cycle - xem NESdev forum "NMI
    // timing" (nhiều người dùng kiến trúc PPU-trước-CPU y hệt đều xác nhận
    // cùng con số này). Dùng 2 tầng thanh ghi dịch giống IRQ (irq_source_level),
    // nhưng ở đây dùng tầng "prev" (trễ 2 cycle) chứ không phải "curr" (trễ 1)
    // - đã test thực nghiệm, "curr" cho kết quả tệ hơn "prev" khá nhiều.
    bool nmi_pending_level_curr = false;
    bool nmi_pending_level_prev = false;

    enum class AddrClass : uint8_t
    {
        IMP_ACC, IMM_, ZPG_, ZPX_, ZPY_, ABS_, ABX_, ABY_,
        IZX_, IZY_, REL_, IND_, NONE_
    };
    enum class OpClass : uint8_t
    {
        READ, WRITE, RMW, BRANCH, JUMP, JSR_OP, RTS_OP, RTI_OP, BRK_OP,
        PHA_OP, PHP_OP, PLA_OP, PLP_OP, IMPLIED_ONLY
    };

    struct INSTRUCTION2
    {
        AddrClass amode = AddrClass::NONE_;
        OpClass   otype = OpClass::IMPLIED_ONLY;
        uint8_t(CPU6502::* operate)(void) = nullptr;
    };
    std::vector<INSTRUCTION2> lookup2;
    void BuildLookup2();

    void StepInstructionCycle();
    void FinishInstruction();
    void clockInner(); // thân clock() cũ - luôn được bọc bởi clock() để cập nhật 2 shadow bên dưới sau MỖI cycle

    Bus* bus = nullptr;
    uint8_t read(uint16_t a);
    void    write(uint16_t a, uint8_t d);

    struct INSTRUCTION
    {
        std::string name;
        uint8_t(CPU6502::* operate)(void) = nullptr;
        uint8_t(CPU6502::* addrmode)(void) = nullptr;
        uint8_t cycles = 0;
    };

    std::vector<INSTRUCTION> lookup;

private:
    uint8_t IMP(); uint8_t IMM(); uint8_t ZPG(); uint8_t ZPX();
    uint8_t ZPY(); uint8_t REL(); uint8_t ABS(); uint8_t ABX();
    uint8_t ABY(); uint8_t IND(); uint8_t IZX(); uint8_t IZY();
    uint8_t ADC(); uint8_t AND(); uint8_t ASL(); uint8_t BCC();
    uint8_t BCS(); uint8_t BEQ(); uint8_t BIT(); uint8_t BMI();
    uint8_t BNE(); uint8_t BPL(); uint8_t BRK(); uint8_t BVC();
    uint8_t BVS(); uint8_t CLC(); uint8_t CLD(); uint8_t CLI();
    uint8_t CLV(); uint8_t CMP(); uint8_t CPX(); uint8_t CPY();
    uint8_t DEC(); uint8_t DEX(); uint8_t DEY(); uint8_t EOR();
    uint8_t INC(); uint8_t INX(); uint8_t INY(); uint8_t JMP();
    uint8_t JSR(); uint8_t LDA(); uint8_t LDX(); uint8_t LDY();
    uint8_t LSR(); uint8_t NOP(); uint8_t ORA(); uint8_t PHA();
    uint8_t PHP(); uint8_t PLA(); uint8_t PLP(); uint8_t ROL();
    uint8_t ROR(); uint8_t RTI(); uint8_t RTS(); uint8_t SBC();
    uint8_t SEC(); uint8_t SED(); uint8_t SEI(); uint8_t STA();
    uint8_t STX(); uint8_t STY(); uint8_t TAX(); uint8_t TAY();
    uint8_t TSX(); uint8_t TXA(); uint8_t TXS(); uint8_t TYA();
    uint8_t XXX(); uint8_t LAX();
    uint8_t SAX();
    uint8_t DCP();
    uint8_t ISC();
    uint8_t SLO();
    uint8_t RLA();
    uint8_t SRE();
    uint8_t RRA();
    // ---- Illegal opcode nhóm immediate (0x0B/0x2B/0x4B/0x6B/0xAB/0xCB) ----
    uint8_t AAC(); // ANC: A &= imm, rồi C = bit7 của A (như ASL)
    uint8_t ASR(); // ALR: A = (A & imm) >> 1, C = bit0 trước khi shift
    uint8_t ARR(); // A = (A & imm) ROR qua C, rồi C/V set theo bit6/bit5 kết quả
    uint8_t ATX(); // LXA: A = X = (A | 0xEE) & imm (opcode "unstable" trên phần cứng thật)
    uint8_t AXS(); // SBX: X = (A & X) - imm, C = không borrow, không đụng V
    // ---- Illegal opcode "unstable" ghi theo high-byte địa chỉ (0x9C/0x9E) ----
    uint8_t SYA(); // SHY, abs,X: ghi Y & (high_byte(addr)+1)
    uint8_t SXA(); // SHX, abs,Y: ghi X & (high_byte(addr)+1)
    // ---- Illegal opcode "unstable" cùng họ, dùng cả A lẫn X (0x93/0x9B/0x9F) ----
    uint8_t SHA(); // AHX, (izy)=0x93 và abs,Y=0x9F: ghi A & X & (high_byte(addr)+1)
    uint8_t TAS(); // 0x9B, abs,Y: SP = A & X, rồi ghi SP & (high_byte(addr)+1)
};

std::string hex(uint32_t n, uint8_t d);