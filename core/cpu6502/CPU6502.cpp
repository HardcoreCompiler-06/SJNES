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

#include "CPU6502.h"
#include "Bus.h"
#include "BinaryIO.h"
// CỤC 1: KHỞI TẠO, BẢNG TỪ ĐIỂN 256 LỆNH VÀ CÁC TÍN HIỆU PHẦN CỨNG

CPU6502::CPU6502()
{
    using a = CPU6502;
    lookup =
    {
        // 0x00 - 0x0F
        { "BRK", &a::BRK, &a::IMP, 7 },{ "ORA", &a::ORA, &a::IZX, 6 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "SLO", &a::SLO, &a::IZX, 8 },{ "NOP", &a::NOP, &a::ZPG, 3 },{ "ORA", &a::ORA, &a::ZPG, 3 },{ "ASL", &a::ASL, &a::ZPG, 5 },{ "SLO", &a::SLO, &a::ZPG, 5 },{ "PHP", &a::PHP, &a::IMP, 3 },{ "ORA", &a::ORA, &a::IMM, 2 },{ "ASL", &a::ASL, &a::IMP, 2 },{ "AAC", &a::AAC, &a::IMM, 2 },{ "NOP", &a::NOP, &a::ABS, 4 },{ "ORA", &a::ORA, &a::ABS, 4 },{ "ASL", &a::ASL, &a::ABS, 6 },{ "SLO", &a::SLO, &a::ABS, 6 },
        // 0x10 - 0x1F
        { "BPL", &a::BPL, &a::REL, 2 },{ "ORA", &a::ORA, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "SLO", &a::SLO, &a::IZY, 8 },{ "NOP", &a::NOP, &a::ZPX, 4 },{ "ORA", &a::ORA, &a::ZPX, 4 },{ "ASL", &a::ASL, &a::ZPX, 6 },{ "SLO", &a::SLO, &a::ZPX, 6 },{ "CLC", &a::CLC, &a::IMP, 2 },{ "ORA", &a::ORA, &a::ABY, 4 },{ "NOP", &a::NOP, &a::IMP, 2 },{ "SLO", &a::SLO, &a::ABY, 7 },{ "NOP", &a::NOP, &a::ABX, 4 },{ "ORA", &a::ORA, &a::ABX, 4 },{ "ASL", &a::ASL, &a::ABX, 7 },{ "SLO", &a::SLO, &a::ABX, 7 },
        // 0x20 - 0x2F
        { "JSR", &a::JSR, &a::ABS, 6 },{ "AND", &a::AND, &a::IZX, 6 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "RLA", &a::RLA, &a::IZX, 8 },{ "BIT", &a::BIT, &a::ZPG, 3 },{ "AND", &a::AND, &a::ZPG, 3 },{ "ROL", &a::ROL, &a::ZPG, 5 },{ "RLA", &a::RLA, &a::ZPG, 5 },{ "PLP", &a::PLP, &a::IMP, 4 },{ "AND", &a::AND, &a::IMM, 2 },{ "ROL", &a::ROL, &a::IMP, 2 },{ "AAC", &a::AAC, &a::IMM, 2 },{ "BIT", &a::BIT, &a::ABS, 4 },{ "AND", &a::AND, &a::ABS, 4 },{ "ROL", &a::ROL, &a::ABS, 6 },{ "RLA", &a::RLA, &a::ABS, 6 },
        // 0x30 - 0x3F
        { "BMI", &a::BMI, &a::REL, 2 },{ "AND", &a::AND, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "RLA", &a::RLA, &a::IZY, 8 },{ "NOP", &a::NOP, &a::ZPX, 4 },{ "AND", &a::AND, &a::ZPX, 4 },{ "ROL", &a::ROL, &a::ZPX, 6 },{ "RLA", &a::RLA, &a::ZPX, 6 },{ "SEC", &a::SEC, &a::IMP, 2 },{ "AND", &a::AND, &a::ABY, 4 },{ "NOP", &a::NOP, &a::IMP, 2 },{ "RLA", &a::RLA, &a::ABY, 7 },{ "NOP", &a::NOP, &a::ABX, 4 },{ "AND", &a::AND, &a::ABX, 4 },{ "ROL", &a::ROL, &a::ABX, 7 },{ "RLA", &a::RLA, &a::ABX, 7 },
        // 0x40 - 0x4F
        { "RTI", &a::RTI, &a::IMP, 6 },{ "EOR", &a::EOR, &a::IZX, 6 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "SRE", &a::SRE, &a::IZX, 8 },{ "NOP", &a::NOP, &a::ZPG, 3 },{ "EOR", &a::EOR, &a::ZPG, 3 },{ "LSR", &a::LSR, &a::ZPG, 5 },{ "SRE", &a::SRE, &a::ZPG, 5 },{ "PHA", &a::PHA, &a::IMP, 3 },{ "EOR", &a::EOR, &a::IMM, 2 },{ "LSR", &a::LSR, &a::IMP, 2 },{ "ASR", &a::ASR, &a::IMM, 2 },{ "JMP", &a::JMP, &a::ABS, 3 },{ "EOR", &a::EOR, &a::ABS, 4 },{ "LSR", &a::LSR, &a::ABS, 6 },{ "SRE", &a::SRE, &a::ABS, 6 },
        // 0x50 - 0x5F
        { "BVC", &a::BVC, &a::REL, 2 },{ "EOR", &a::EOR, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "SRE", &a::SRE, &a::IZY, 8 },{ "NOP", &a::NOP, &a::ZPX, 4 },{ "EOR", &a::EOR, &a::ZPX, 4 },{ "LSR", &a::LSR, &a::ZPX, 6 },{ "SRE", &a::SRE, &a::ZPX, 6 },{ "CLI", &a::CLI, &a::IMP, 2 },{ "EOR", &a::EOR, &a::ABY, 4 },{ "NOP", &a::NOP, &a::IMP, 2 },{ "SRE", &a::SRE, &a::ABY, 7 },{ "NOP", &a::NOP, &a::ABX, 4 },{ "EOR", &a::EOR, &a::ABX, 4 },{ "LSR", &a::LSR, &a::ABX, 7 },{ "SRE", &a::SRE, &a::ABX, 7 },
        // 0x60 - 0x6F
        { "RTS", &a::RTS, &a::IMP, 6 },{ "ADC", &a::ADC, &a::IZX, 6 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "RRA", &a::RRA, &a::IZX, 8 },{ "NOP", &a::NOP, &a::ZPG, 3 },{ "ADC", &a::ADC, &a::ZPG, 3 },{ "ROR", &a::ROR, &a::ZPG, 5 },{ "RRA", &a::RRA, &a::ZPG, 5 },{ "PLA", &a::PLA, &a::IMP, 4 },{ "ADC", &a::ADC, &a::IMM, 2 },{ "ROR", &a::ROR, &a::IMP, 2 },{ "ARR", &a::ARR, &a::IMM, 2 },{ "JMP", &a::JMP, &a::IND, 5 },{ "ADC", &a::ADC, &a::ABS, 4 },{ "ROR", &a::ROR, &a::ABS, 6 },{ "RRA", &a::RRA, &a::ABS, 6 },
        // 0x70 - 0x7F
        { "BVS", &a::BVS, &a::REL, 2 },{ "ADC", &a::ADC, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "RRA", &a::RRA, &a::IZY, 8 },{ "NOP", &a::NOP, &a::ZPX, 4 },{ "ADC", &a::ADC, &a::ZPX, 4 },{ "ROR", &a::ROR, &a::ZPX, 6 },{ "RRA", &a::RRA, &a::ZPX, 6 },{ "SEI", &a::SEI, &a::IMP, 2 },{ "ADC", &a::ADC, &a::ABY, 4 },{ "NOP", &a::NOP, &a::IMP, 2 },{ "RRA", &a::RRA, &a::ABY, 7 },{ "NOP", &a::NOP, &a::ABX, 4 },{ "ADC", &a::ADC, &a::ABX, 4 },{ "ROR", &a::ROR, &a::ABX, 7 },{ "RRA", &a::RRA, &a::ABX, 7 },
        // 0x80 - 0x8F
        { "NOP", &a::NOP, &a::IMM, 2 },{ "STA", &a::STA, &a::IZX, 6 },{ "NOP", &a::NOP, &a::IMM, 2 },{ "SAX", &a::SAX, &a::IZX, 6 },{ "STY", &a::STY, &a::ZPG, 3 },{ "STA", &a::STA, &a::ZPG, 3 },{ "STX", &a::STX, &a::ZPG, 3 },{ "SAX", &a::SAX, &a::ZPG, 3 },{ "DEY", &a::DEY, &a::IMP, 2 },{ "NOP", &a::NOP, &a::IMM, 2 },{ "TXA", &a::TXA, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMM, 2 },{ "STY", &a::STY, &a::ABS, 4 },{ "STA", &a::STA, &a::ABS, 4 },{ "STX", &a::STX, &a::ABS, 4 },{ "SAX", &a::SAX, &a::ABS, 4 },
        // 0x90 - 0x9F
        { "BCC", &a::BCC, &a::REL, 2 },{ "STA", &a::STA, &a::IZY, 6 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "SHA", &a::SHA, &a::IZY, 6 },{ "STY", &a::STY, &a::ZPX, 4 },{ "STA", &a::STA, &a::ZPX, 4 },{ "STX", &a::STX, &a::ZPY, 4 },{ "SAX", &a::SAX, &a::ZPY, 4 },{ "TYA", &a::TYA, &a::IMP, 2 },{ "STA", &a::STA, &a::ABY, 5 },{ "TXS", &a::TXS, &a::IMP, 2 },{ "TAS", &a::TAS, &a::ABY, 5 },{ "SYA", &a::SYA, &a::ABX, 5 },{ "STA", &a::STA, &a::ABX, 5 },{ "SXA", &a::SXA, &a::ABY, 5 },{ "SHA", &a::SHA, &a::ABY, 5 },
        // 0xA0 - 0xAF
        { "LDY", &a::LDY, &a::IMM, 2 },{ "LDA", &a::LDA, &a::IZX, 6 },{ "LDX", &a::LDX, &a::IMM, 2 },{ "LAX", &a::LAX, &a::IZX, 6 },{ "LDY", &a::LDY, &a::ZPG, 3 },{ "LDA", &a::LDA, &a::ZPG, 3 },{ "LDX", &a::LDX, &a::ZPG, 3 },{ "LAX", &a::LAX, &a::ZPG, 3 },{ "TAY", &a::TAY, &a::IMP, 2 },{ "LDA", &a::LDA, &a::IMM, 2 },{ "TAX", &a::TAX, &a::IMP, 2 },{ "ATX", &a::ATX, &a::IMM, 2 },{ "LDY", &a::LDY, &a::ABS, 4 },{ "LDA", &a::LDA, &a::ABS, 4 },{ "LDX", &a::LDX, &a::ABS, 4 },{ "LAX", &a::LAX, &a::ABS, 4 },
        // 0xB0 - 0xBF
        { "BCS", &a::BCS, &a::REL, 2 },{ "LDA", &a::LDA, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "LAX", &a::LAX, &a::IZY, 5 },{ "LDY", &a::LDY, &a::ZPX, 4 },{ "LDA", &a::LDA, &a::ZPX, 4 },{ "LDX", &a::LDX, &a::ZPY, 4 },{ "LAX", &a::LAX, &a::ZPY, 4 },{ "CLV", &a::CLV, &a::IMP, 2 },{ "LDA", &a::LDA, &a::ABY, 4 },{ "TSX", &a::TSX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::ABY, 4 },{ "LDY", &a::LDY, &a::ABX, 4 },{ "LDA", &a::LDA, &a::ABX, 4 },{ "LDX", &a::LDX, &a::ABY, 4 },{ "LAX", &a::LAX, &a::ABY, 4 },
        // 0xC0 - 0xCF
        { "CPY", &a::CPY, &a::IMM, 2 },{ "CMP", &a::CMP, &a::IZX, 6 },{ "NOP", &a::NOP, &a::IMM, 2 },{ "DCP", &a::DCP, &a::IZX, 8 },{ "CPY", &a::CPY, &a::ZPG, 3 },{ "CMP", &a::CMP, &a::ZPG, 3 },{ "DEC", &a::DEC, &a::ZPG, 5 },{ "DCP", &a::DCP, &a::ZPG, 5 },{ "INY", &a::INY, &a::IMP, 2 },{ "CMP", &a::CMP, &a::IMM, 2 },{ "DEX", &a::DEX, &a::IMP, 2 },{ "AXS", &a::AXS, &a::IMM, 2 },{ "CPY", &a::CPY, &a::ABS, 4 },{ "CMP", &a::CMP, &a::ABS, 4 },{ "DEC", &a::DEC, &a::ABS, 6 },{ "DCP", &a::DCP, &a::ABS, 6 },
        // 0xD0 - 0xDF
        { "BNE", &a::BNE, &a::REL, 2 },{ "CMP", &a::CMP, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "DCP", &a::DCP, &a::IZY, 8 },{ "NOP", &a::NOP, &a::ZPX, 4 },{ "CMP", &a::CMP, &a::ZPX, 4 },{ "DEC", &a::DEC, &a::ZPX, 6 },{ "DCP", &a::DCP, &a::ZPX, 6 },{ "CLD", &a::CLD, &a::IMP, 2 },{ "CMP", &a::CMP, &a::ABY, 4 },{ "NOP", &a::NOP, &a::IMP, 2 },{ "DCP", &a::DCP, &a::ABY, 7 },{ "NOP", &a::NOP, &a::ABX, 4 },{ "CMP", &a::CMP, &a::ABX, 4 },{ "DEC", &a::DEC, &a::ABX, 7 },{ "DCP", &a::DCP, &a::ABX, 7 },
        // 0xE0 - 0xEF
        { "CPX", &a::CPX, &a::IMM, 2 },{ "SBC", &a::SBC, &a::IZX, 6 },{ "NOP", &a::NOP, &a::IMM, 2 },{ "ISC", &a::ISC, &a::IZX, 8 },{ "CPX", &a::CPX, &a::ZPG, 3 },{ "SBC", &a::SBC, &a::ZPG, 3 },{ "INC", &a::INC, &a::ZPG, 5 },{ "ISC", &a::ISC, &a::ZPG, 5 },{ "INX", &a::INX, &a::IMP, 2 },{ "SBC", &a::SBC, &a::IMM, 2 },{ "NOP", &a::NOP, &a::IMP, 2 },{ "SBC", &a::SBC, &a::IMM, 2 },{ "CPX", &a::CPX, &a::ABS, 4 },{ "SBC", &a::SBC, &a::ABS, 4 },{ "INC", &a::INC, &a::ABS, 6 },{ "ISC", &a::ISC, &a::ABS, 6 },
        // 0xF0 - 0xFF
        { "BEQ", &a::BEQ, &a::REL, 2 },{ "SBC", &a::SBC, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "ISC", &a::ISC, &a::IZY, 8 },{ "NOP", &a::NOP, &a::ZPX, 4 },{ "SBC", &a::SBC, &a::ZPX, 4 },{ "INC", &a::INC, &a::ZPX, 6 },{ "ISC", &a::ISC, &a::ZPX, 6 },{ "SED", &a::SED, &a::IMP, 2 },{ "SBC", &a::SBC, &a::ABY, 4 },{ "NOP", &a::NOP, &a::IMP, 2 },{ "ISC", &a::ISC, &a::ABY, 7 },{ "NOP", &a::NOP, &a::ABX, 4 },{ "SBC", &a::SBC, &a::ABX, 4 },{ "INC", &a::INC, &a::ABX, 7 },{ "ISC", &a::ISC, &a::ABX, 7 },
    };

    BuildLookup2(); // BẮT BUỘC: build bảng phân loại cycle-accurate ngay sau lookup[]
}

CPU6502::~CPU6502() {}

uint8_t CPU6502::read(uint16_t a) { return bus->cpuRead(a); }
void CPU6502::write(uint16_t a, uint8_t d) { bus->cpuWrite(a, d); }

uint8_t CPU6502::GetFlag(FLAGS6502 f) { return ((status & f) > 0) ? 1 : 0; }
void CPU6502::SetFlag(FLAGS6502 f, bool v) {
    if (v) status |= f; else status &= ~f;
}

void CPU6502::reset() {
    addr_abs = 0xFFFC;
    uint16_t lo = read(0xFFFC);
    uint16_t hi = read(0xFFFD);
    pc = (hi << 8) | lo;

    a = 0; x = 0; y = 0;
    stkp = 0xFD;
    status = U | I;

    nmi_pending = false;
    irq_pending = false;
    irq_sources = 0;
    brk_hijacked_by_nmi = false;

    addr_rel = 0x0000; addr_abs = 0x0000; fetched = 0x00;
    cycles = 0;
    instr_cycle = 0;
    opcode = 0x00;
    specialMode = SpecialMode::NONE_MODE;
    suppress_irq_check_this_instruction = false;
    irq_disable_shadow = 1; // sau reset, I=1 (interrupt disabled), shadow khớp luôn
    owe_shadow_sync = false;

    // Reset 2 cơ chế fix mới (xem CPU6502.h để biết chi tiết từng cái)
    nmi_retrigger_guard = false;
    irq_source_level_curr = false;
    irq_source_level_prev = false;
}

void CPU6502::nmi(bool latchCurr) {
    nmi_pending = true;
    // Nếu NMI nổ ở dot thứ 2 của chu kỳ CPU (system_clock_counter % 3 == 1),
    // cho phép nó lọt luôn vào tầng curr của chu kỳ CPU vừa chạy (1 dot trước).
    if (latchCurr)
        nmi_pending_level_curr = true;
}

void CPU6502::CancelPendingNmi() {
    nmi_pending = false;
    nmi_pending_level_curr = false;
    nmi_pending_level_prev = false;
}

void CPU6502::irq() {
    irq_pending = true;
}

void CPU6502::SetIrqSource(uint8_t source) {
    irq_sources |= source;
}

void CPU6502::ClearIrqSource(uint8_t source) {
    irq_sources &= ~source;
}

bool CPU6502::IsIrqActive() const {
    return irq_pending || irq_sources != 0;
}

uint8_t CPU6502::fetch() {
    // QUAN TRỌNG: trong kiến trúc cycle-accurate mới, state machine (StepInstructionCycle)
    // đã đọc bus và set "fetched" đúng giá trị TRƯỚC KHI gọi operate(). Không được đọc lại
    // bus ở đây nữa — nếu không các thanh ghi có side-effect khi đọc (như $2002 VBlank,
    // $2007 PPUDATA, $4015 APU status) sẽ bị đọc 2 lần, gây sai lệch trạng thái nghiêm trọng
    // (ví dụ: cờ VBlank bị chính CPU tự xóa mất trước khi game kịp thấy).
    return fetched;
}
// CỤC 2: 12 CHẾ ĐỘ ĐỊA CHỈ (ADDRESSING MODES)

uint8_t CPU6502::IMP() {
    fetched = a;
    return 0;
}

uint8_t CPU6502::IMM() {
    addr_abs = pc++;
    return 0;
}

uint8_t CPU6502::ZPG() {
    addr_abs = read(pc);
    pc++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU6502::ZPX() {
    addr_abs = (read(pc) + x);
    pc++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU6502::ZPY() {
    addr_abs = (read(pc) + y);
    pc++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU6502::REL() {
    addr_rel = read(pc);
    pc++;
    if (addr_rel & 0x80)
        addr_rel |= 0xFF00;
    return 0;
}

uint8_t CPU6502::ABS() {
    uint16_t lo = read(pc);
    pc++;
    uint16_t hi = read(pc);
    pc++;
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU6502::ABX() {
    uint16_t lo = read(pc);
    pc++;
    uint16_t hi = read(pc);
    pc++;
    addr_abs = (hi << 8) | lo;
    addr_abs += x;

    if ((addr_abs & 0xFF00) != (hi << 8))
        return 1;
    else
        return 0;
}

uint8_t CPU6502::ABY() {
    uint16_t lo = read(pc);
    pc++;
    uint16_t hi = read(pc);
    pc++;
    addr_abs = (hi << 8) | lo;
    addr_abs += y;

    if ((addr_abs & 0xFF00) != (hi << 8))
        return 1;
    else
        return 0;
}

uint8_t CPU6502::IND() {
    uint16_t ptr_lo = read(pc);
    pc++;
    uint16_t ptr_hi = read(pc);
    pc++;
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;

    if (ptr_lo == 0x00FF)
        addr_abs = (read(ptr & 0xFF00) << 8) | read(ptr);
    else
        addr_abs = (read(ptr + 1) << 8) | read(ptr);
    return 0;
}

uint8_t CPU6502::IZX() {
    uint16_t t = read(pc);
    pc++;
    uint16_t lo = read((uint16_t)(t + (uint16_t)x) & 0x00FF);
    uint16_t hi = read((uint16_t)(t + (uint16_t)x + 1) & 0x00FF);
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU6502::IZY() {
    uint16_t t = read(pc);
    pc++;
    uint16_t lo = read(t & 0x00FF);
    uint16_t hi = read((t + 1) & 0x00FF);
    addr_abs = (hi << 8) | lo;
    addr_abs += y;

    if ((addr_abs & 0xFF00) != (hi << 8))
        return 1;
    else
        return 0;
}
// ==============================================================================
// CỤC 3: CÁC LỆNH HÀNH ĐỘNG (PHẦN 1: LOGIC, CỘNG TRỪ, RẼ NHÁNH)
// ==============================================================================

uint8_t CPU6502::ADC() {
    fetch();
    temp = (uint16_t)a + (uint16_t)fetched + (uint16_t)GetFlag(C);
    SetFlag(C, temp > 255);
    SetFlag(Z, (temp & 0x00FF) == 0);
    SetFlag(V, (~((uint16_t)a ^ (uint16_t)fetched) & ((uint16_t)a ^ (uint16_t)temp)) & 0x0080);
    SetFlag(N, temp & 0x0080);
    a = temp & 0x00FF;
    return 1;
}

uint8_t CPU6502::SBC() {
    fetch();
    uint16_t value = ((uint16_t)fetched) ^ 0x00FF;
    temp = (uint16_t)a + value + (uint16_t)GetFlag(C);
    SetFlag(C, temp & 0xFF00);
    SetFlag(Z, (temp & 0x00FF) == 0);
    SetFlag(V, ((temp ^ (uint16_t)a) & (temp ^ value) & 0x0080));
    SetFlag(N, temp & 0x0080);
    a = temp & 0x00FF;
    return 1;
}

uint8_t CPU6502::AND() { fetch(); a = a & fetched; SetFlag(Z, a == 0x00); SetFlag(N, a & 0x80); return 1; }
uint8_t CPU6502::ASL() {
    fetch();
    temp = (uint16_t)fetched << 1;
    SetFlag(C, (temp & 0xFF00) > 0);
    SetFlag(Z, (temp & 0x00FF) == 0x00);
    SetFlag(N, temp & 0x80);
    if (lookup[opcode].addrmode == &CPU6502::IMP) a = temp & 0x00FF;
    else write(addr_abs, temp & 0x00FF);
    return 0;
}

uint8_t CPU6502::BCC() { return GetFlag(C) == 0 ? 1 : 0; }
uint8_t CPU6502::BCS() { return GetFlag(C) == 1 ? 1 : 0; }
uint8_t CPU6502::BEQ() { return GetFlag(Z) == 1 ? 1 : 0; }
uint8_t CPU6502::BNE() { return GetFlag(Z) == 0 ? 1 : 0; }
uint8_t CPU6502::BMI() { return GetFlag(N) == 1 ? 1 : 0; }
uint8_t CPU6502::BPL() { return GetFlag(N) == 0 ? 1 : 0; }
uint8_t CPU6502::BVC() { return GetFlag(V) == 0 ? 1 : 0; }
uint8_t CPU6502::BVS() { return GetFlag(V) == 1 ? 1 : 0; }
uint8_t CPU6502::BIT() { fetch(); temp = a & fetched; SetFlag(Z, (temp & 0x00FF) == 0); SetFlag(N, fetched & (1 << 7)); SetFlag(V, fetched & (1 << 6)); return 0; }
uint8_t CPU6502::CLC() { SetFlag(C, false); return 0; }
uint8_t CPU6502::CLD() { SetFlag(D, false); return 0; }
uint8_t CPU6502::CLI() { SetFlag(I, false); return 0; }
uint8_t CPU6502::CLV() { SetFlag(V, false); return 0; }
uint8_t CPU6502::CMP() { fetch(); temp = (uint16_t)a - (uint16_t)fetched; SetFlag(C, a >= fetched); SetFlag(Z, (temp & 0x00FF) == 0x0000); SetFlag(N, temp & 0x0080); return 1; }
uint8_t CPU6502::CPX() { fetch(); temp = (uint16_t)x - (uint16_t)fetched; SetFlag(C, x >= fetched); SetFlag(Z, (temp & 0x00FF) == 0x0000); SetFlag(N, temp & 0x0080); return 0; }
uint8_t CPU6502::CPY() { fetch(); temp = (uint16_t)y - (uint16_t)fetched; SetFlag(C, y >= fetched); SetFlag(Z, (temp & 0x00FF) == 0x0000); SetFlag(N, temp & 0x0080); return 0; }
uint8_t CPU6502::DEC() { fetch(); temp = fetched - 1; write(addr_abs, temp & 0x00FF); SetFlag(Z, (temp & 0x00FF) == 0x0000); SetFlag(N, temp & 0x0080); return 0; }
uint8_t CPU6502::DEX() { x--; SetFlag(Z, x == 0x00); SetFlag(N, x & 0x80); return 0; }
uint8_t CPU6502::DEY() { y--; SetFlag(Z, y == 0x00); SetFlag(N, y & 0x80); return 0; }
uint8_t CPU6502::INC() { fetch(); temp = fetched + 1; write(addr_abs, temp & 0x00FF); SetFlag(Z, (temp & 0x00FF) == 0x0000); SetFlag(N, temp & 0x0080); return 0; }
uint8_t CPU6502::INX() { x++; SetFlag(Z, x == 0x00); SetFlag(N, x & 0x80); return 0; }
uint8_t CPU6502::INY() { y++; SetFlag(Z, y == 0x00); SetFlag(N, y & 0x80); return 0; }
uint8_t CPU6502::EOR() { fetch(); a = a ^ fetched; SetFlag(Z, a == 0x00); SetFlag(N, a & 0x80); return 1; }
uint8_t CPU6502::JMP() { pc = addr_abs; return 0; }
uint8_t CPU6502::JSR() { pc--; write(0x0100 + stkp, (pc >> 8) & 0x00FF); stkp--; write(0x0100 + stkp, pc & 0x00FF); stkp--; pc = addr_abs; return 0; }
uint8_t CPU6502::LDA() { fetch(); a = fetched; SetFlag(Z, a == 0x00); SetFlag(N, a & 0x80); return 1; }
uint8_t CPU6502::LDX() { fetch(); x = fetched; SetFlag(Z, x == 0x00); SetFlag(N, x & 0x80); return 1; }
uint8_t CPU6502::LDY() { fetch(); y = fetched; SetFlag(Z, y == 0x00); SetFlag(N, y & 0x80); return 1; }
uint8_t CPU6502::LSR() { fetch(); SetFlag(C, fetched & 0x0001); temp = fetched >> 1; SetFlag(Z, (temp & 0x00FF) == 0x0000); SetFlag(N, temp & 0x0080); if (lookup[opcode].addrmode == &CPU6502::IMP) a = temp & 0x00FF; else write(addr_abs, temp & 0x00FF); return 0; }
uint8_t CPU6502::NOP() { switch (opcode) { case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: return 1; break; } return 0; }
                                                    uint8_t CPU6502::ORA() { fetch(); a = a | fetched; SetFlag(Z, a == 0x00); SetFlag(N, a & 0x80); return 1; }
                                                    uint8_t CPU6502::PHA() { write(0x0100 + stkp, a); stkp--; return 0; }
                                                    uint8_t CPU6502::PHP()
                                                    {
                                                        write(0x0100 + stkp, status | B | U);
                                                        stkp--;
                                                        return 0;
                                                    }
                                                    uint8_t CPU6502::PLA() { stkp++; a = read(0x0100 + stkp); SetFlag(Z, a == 0x00); SetFlag(N, a & 0x80); return 0; }
                                                    uint8_t CPU6502::PLP()
                                                    {
                                                        stkp++;
                                                        status = read(0x0100 + stkp);

                                                        status &= ~B;
                                                        status |= U;

                                                        return 0;
                                                    }
                                                    uint8_t CPU6502::ROL() { fetch(); temp = (uint16_t)(fetched << 1) | GetFlag(C); SetFlag(C, temp & 0xFF00); SetFlag(Z, (temp & 0x00FF) == 0x0000); SetFlag(N, temp & 0x0080); if (lookup[opcode].addrmode == &CPU6502::IMP) a = temp & 0x00FF; else write(addr_abs, temp & 0x00FF); return 0; }
                                                    uint8_t CPU6502::ROR() { fetch(); temp = (uint16_t)(GetFlag(C) << 7) | (fetched >> 1); SetFlag(C, fetched & 0x01); SetFlag(Z, (temp & 0x00FF) == 0x0000); SetFlag(N, temp & 0x0080); if (lookup[opcode].addrmode == &CPU6502::IMP) a = temp & 0x00FF; else write(addr_abs, temp & 0x00FF); return 0; }
                                                    uint8_t CPU6502::RTI() { stkp++; status = read(0x0100 + stkp); status &= ~B; status |= U; stkp++; pc = (uint16_t)read(0x0100 + stkp); stkp++; pc |= (uint16_t)read(0x0100 + stkp) << 8; return 0; }
                                                    uint8_t CPU6502::RTS() { stkp++; pc = (uint16_t)read(0x0100 + stkp); stkp++; pc |= (uint16_t)read(0x0100 + stkp) << 8; pc++; return 0; }
                                                    uint8_t CPU6502::SEC() { SetFlag(C, true); return 0; }
                                                    uint8_t CPU6502::SED() { SetFlag(D, true); return 0; }
                                                    uint8_t CPU6502::SEI() { SetFlag(I, true); return 0; }
                                                    uint8_t CPU6502::STA() { write(addr_abs, a); return 0; }
                                                    uint8_t CPU6502::STX() { write(addr_abs, x); return 0; }
                                                    uint8_t CPU6502::STY() { write(addr_abs, y); return 0; }
                                                    uint8_t CPU6502::BRK()
                                                    {
                                                        pc++;
                                                        write(0x0100 + stkp, (pc >> 8) & 0x00FF);
                                                        stkp--;
                                                        write(0x0100 + stkp, pc & 0x00FF);
                                                        stkp--;
                                                        write(0x0100 + stkp, status | B | U);
                                                        stkp--;
                                                        SetFlag(I, true);
                                                        SetFlag(U, true);
                                                        status &= ~B;
                                                        pc = (uint16_t)read(0xFFFE) | ((uint16_t)read(0xFFFF) << 8);
                                                        return 0;
                                                    }
                                                    uint8_t CPU6502::TAX() { x = a; SetFlag(Z, x == 0x00); SetFlag(N, x & 0x80); return 0; }
                                                    uint8_t CPU6502::TAY() { y = a; SetFlag(Z, y == 0x00); SetFlag(N, y & 0x80); return 0; }
                                                    uint8_t CPU6502::TSX() { x = stkp; SetFlag(Z, x == 0x00); SetFlag(N, x & 0x80); return 0; }
                                                    uint8_t CPU6502::TXA() { a = x; SetFlag(Z, a == 0x00); SetFlag(N, a & 0x80); return 0; }
                                                    uint8_t CPU6502::TXS() { stkp = x; return 0; }
                                                    uint8_t CPU6502::TYA() { a = y; SetFlag(Z, a == 0x00); SetFlag(N, a & 0x80); return 0; }
                                                    uint8_t CPU6502::LAX()
                                                    {
                                                        fetch();
                                                        a = fetched;
                                                        x = fetched;
                                                        SetFlag(Z, a == 0x00);
                                                        SetFlag(N, a & 0x80);
                                                        return 1;
                                                    }

                                                    uint8_t CPU6502::SAX()
                                                    {
                                                        write(addr_abs, a & x);
                                                        return 0;
                                                    }

                                                    uint8_t CPU6502::DCP()
                                                    {
                                                        fetch();

                                                        uint8_t value = (fetched - 1) & 0xFF;
                                                        write(addr_abs, value);

                                                        temp = (uint16_t)a - (uint16_t)value;
                                                        SetFlag(C, a >= value);
                                                        SetFlag(Z, (temp & 0x00FF) == 0x0000);
                                                        SetFlag(N, temp & 0x0080);

                                                        return 0;
                                                    }

                                                    uint8_t CPU6502::ISC()
                                                    {
                                                        fetch();

                                                        uint8_t value = (fetched + 1) & 0xFF;
                                                        write(addr_abs, value);

                                                        value ^= 0xFF;
                                                        temp = (uint16_t)a + (uint16_t)value + (uint16_t)GetFlag(C);

                                                        SetFlag(C, temp & 0xFF00);
                                                        SetFlag(Z, (temp & 0x00FF) == 0);
                                                        SetFlag(V, ((temp ^ (uint16_t)a) & (temp ^ (uint16_t)value) & 0x0080));
                                                        SetFlag(N, temp & 0x0080);

                                                        a = temp & 0x00FF;
                                                        return 0;
                                                    }

                                                    uint8_t CPU6502::SLO()
                                                    {
                                                        fetch();

                                                        temp = (uint16_t)fetched << 1;
                                                        SetFlag(C, temp & 0xFF00);

                                                        uint8_t value = temp & 0x00FF;
                                                        write(addr_abs, value);

                                                        a = a | value;
                                                        SetFlag(Z, a == 0x00);
                                                        SetFlag(N, a & 0x80);

                                                        return 0;
                                                    }

                                                    uint8_t CPU6502::RLA()
                                                    {
                                                        fetch();

                                                        temp = ((uint16_t)fetched << 1) | GetFlag(C);
                                                        SetFlag(C, temp & 0xFF00);

                                                        uint8_t value = temp & 0x00FF;
                                                        write(addr_abs, value);

                                                        a = a & value;
                                                        SetFlag(Z, a == 0x00);
                                                        SetFlag(N, a & 0x80);

                                                        return 0;
                                                    }

                                                    uint8_t CPU6502::SRE()
                                                    {
                                                        fetch();

                                                        SetFlag(C, fetched & 0x01);

                                                        uint8_t value = fetched >> 1;
                                                        write(addr_abs, value);

                                                        a = a ^ value;
                                                        SetFlag(Z, a == 0x00);
                                                        SetFlag(N, a & 0x80);

                                                        return 0;
                                                    }

                                                    uint8_t CPU6502::RRA()
                                                    {
                                                        fetch();

                                                        uint8_t oldCarry = GetFlag(C);
                                                        SetFlag(C, fetched & 0x01);

                                                        uint8_t value = (fetched >> 1) | (oldCarry << 7);
                                                        write(addr_abs, value);

                                                        temp = (uint16_t)a + (uint16_t)value + (uint16_t)GetFlag(C);

                                                        SetFlag(C, temp > 255);
                                                        SetFlag(Z, (temp & 0x00FF) == 0);
                                                        SetFlag(V, (~((uint16_t)a ^ (uint16_t)value) & ((uint16_t)a ^ (uint16_t)temp)) & 0x0080);
                                                        SetFlag(N, temp & 0x0080);

                                                        a = temp & 0x00FF;
                                                        return 0;
                                                    }
                                                    // ---- AAC / ANC (0x0B, 0x2B) - immediate ----
                                                    uint8_t CPU6502::AAC()
                                                    {
                                                        fetch();
                                                        a = a & fetched;
                                                        SetFlag(Z, a == 0x00);
                                                        SetFlag(N, a & 0x80);
                                                        SetFlag(C, a & 0x80); // C lấy từ bit7 của kết quả, giống ASL/ROL
                                                        return 0;
                                                    }

                                                    // ---- ASR / ALR (0x4B) - immediate ----
                                                    uint8_t CPU6502::ASR()
                                                    {
                                                        fetch();
                                                        uint8_t v = a & fetched;
                                                        SetFlag(C, v & 0x01); // carry lấy bit0 TRƯỚC khi shift phải
                                                        v >>= 1;
                                                        a = v;
                                                        SetFlag(Z, a == 0x00);
                                                        SetFlag(N, a & 0x80); // luôn 0 vì shift phải 1 bit của giá trị 8-bit
                                                        return 0;
                                                    }

                                                    // ---- ARR (0x6B) - immediate ----
                                                    // NES không có decimal mode nên chỉ cần nhánh binary:
                                                    // A = (A & imm) ROR qua carry cũ; C = bit6 kết quả; V = bit6 XOR bit5 kết quả.
                                                    uint8_t CPU6502::ARR()
                                                    {
                                                        fetch();
                                                        uint8_t v = a & fetched;
                                                        uint8_t carryIn = GetFlag(C);
                                                        a = (v >> 1) | (carryIn << 7);
                                                        SetFlag(Z, a == 0x00);
                                                        SetFlag(N, a & 0x80);
                                                        SetFlag(C, (a & 0x40) != 0);
                                                        SetFlag(V, ((a & 0x40) >> 6) ^ ((a & 0x20) >> 5));
                                                        return 0;
                                                    }

                                                    // ---- ATX / LXA (0xAB) - immediate ----
                                                    // LƯU Ý: tài liệu 6502 illegal opcode ghi ATX là "AND A với imm rồi chuyển vào X",
                                                    // nhưng theo xác nhận từ tác giả Nestopia (và blargg's test rom đồng ý): hành vi
                                                    // THỰC TẾ trên phần cứng mà test kỳ vọng là NẠP THẲNG, A cũ không ảnh hưởng gì cả -
                                                    // KHÔNG AND với A cũ, KHÔNG OR với hằng số "magic" analog nào. Bản cũ dùng công thức
                                                    // (A|0xEE)&imm chỉ đúng tình cờ khi A đã = 0xFF từ trước, nên fail khi A khác giá trị đó.
                                                    uint8_t CPU6502::ATX()
                                                    {
                                                        fetch();
                                                        a = fetched;
                                                        x = fetched;
                                                        SetFlag(Z, a == 0x00);
                                                        SetFlag(N, a & 0x80);
                                                        return 0;
                                                    }

                                                    // ---- AXS / SBX (0xCB) - immediate ----
                                                    uint8_t CPU6502::AXS()
                                                    {
                                                        fetch();
                                                        uint16_t andVal = (uint16_t)(a & x);
                                                        uint16_t result = andVal - (uint16_t)fetched;
                                                        SetFlag(C, andVal >= (uint16_t)fetched); // set nếu KHÔNG borrow
                                                        x = result & 0x00FF;
                                                        SetFlag(Z, x == 0x00);
                                                        SetFlag(N, x & 0x80);
                                                        // Chú ý: AXS/SBX KHÔNG đụng tới cờ V.
                                                        return 0;
                                                    }

                                                    // ---- SYA / SHY (0x9C) - abs,X ----
                                                    // Giá trị ghi = Y & (high_byte của TOÁN HẠNG GỐC, chưa cộng X, + 1).
                                                    // base_addr = hi<<8|lo TRƯỚC khi cộng index (state machine ABX_/ABY_ trong .inc set sẵn).
                                                    // QUIRK PHẦN CỨNG THẬT: khi abs,X crosspage, byte cao của ĐỊA CHỈ GHI THỰC TẾ cũng
                                                    // bị "ăn" luôn giá trị AND vừa tính ra (không chỉ giá trị ghi bị ảnh hưởng) - vì mạch
                                                    // tính effective-address và mạch AND dùng chung 1 bus nội bộ. page_crossed đã được
                                                    // state machine set đúng cho lệnh hiện tại trước khi FinishInstruction() gọi tới đây.
                                                    uint8_t CPU6502::SYA()
                                                    {
                                                        uint8_t hiPlus1 = (uint8_t)((base_addr >> 8) + 1);
                                                        uint8_t val = y & hiPlus1;
                                                        uint16_t target = addr_abs; // base_addr + X, đã fix đúng bởi state machine
                                                        if (page_crossed)
                                                        {
                                                            // byte cao của địa chỉ ghi bị thay bằng chính giá trị AND vừa tính
                                                            target = ((uint16_t)val << 8) | (target & 0x00FF);
                                                        }
                                                        write(target, val);
                                                        return 0;
                                                    }

                                                    // ---- SXA / SHX (0x9E) - abs,Y ----
                                                    uint8_t CPU6502::SXA()
                                                    {
                                                        uint8_t hiPlus1 = (uint8_t)((base_addr >> 8) + 1);
                                                        uint8_t val = x & hiPlus1;
                                                        uint16_t target = addr_abs; // base_addr + Y, đã fix đúng bởi state machine
                                                        if (page_crossed)
                                                        {
                                                            target = ((uint16_t)val << 8) | (target & 0x00FF);
                                                        }
                                                        write(target, val);
                                                        return 0;
                                                    }

                                                    uint8_t CPU6502::XXX() { return 0; }

                                                    // ---- SHA / AHX (0x93 izy, 0x9F aby) ----
                                                    // Giá trị ghi = A & X & (high_byte của TOÁN HẠNG GỐC, chưa cộng index, + 1).
                                                    // base_addr = địa chỉ gốc TRƯỚC khi cộng index (state machine IZY_/ABY_ trong
                                                    // .inc set sẵn, giống hệt quy ước SYA/SXA đã dùng). Cùng quirk phần cứng thật
                                                    // với SYA/SXA: khi crosspage, byte cao của địa chỉ ghi THỰC TẾ cũng bị "ăn"
                                                    // luôn giá trị AND vừa tính ra.
                                                    uint8_t CPU6502::SHA()
                                                    {
                                                        uint8_t hiPlus1 = (uint8_t)((base_addr >> 8) + 1);
                                                        uint8_t val = a & x & hiPlus1;
                                                        uint16_t target = addr_abs; // đã fix đúng bởi state machine
                                                        if (page_crossed)
                                                        {
                                                            target = ((uint16_t)val << 8) | (target & 0x00FF);
                                                        }
                                                        write(target, val);
                                                        return 0;
                                                    }

                                                    // ---- TAS / XAS (0x9B) - abs,Y ----
                                                    // SP = A & X trước, rồi ghi SP & (high_byte gốc + 1) - cùng quirk crosspage
                                                    // như SHA/SYA/SXA (byte cao địa chỉ ghi bị thay bằng giá trị AND).
                                                    uint8_t CPU6502::TAS()
                                                    {
                                                        stkp = a & x;
                                                        uint8_t hiPlus1 = (uint8_t)((base_addr >> 8) + 1);
                                                        uint8_t val = stkp & hiPlus1;
                                                        uint16_t target = addr_abs;
                                                        if (page_crossed)
                                                        {
                                                            target = ((uint16_t)val << 8) | (target & 0x00FF);
                                                        }
                                                        write(target, val);
                                                        return 0;
                                                    }

                                                    bool CPU6502::complete() {
                                                        return cycles == 0;
                                                    }

                                                    std::string hex(uint32_t n, uint8_t d) {
                                                        std::string s(d, '0');
                                                        for (int i = d - 1; i >= 0; i--, n >>= 4)
                                                            s[i] = "0123456789ABCDEF"[n & 0xF];
                                                        return s;
                                                    }

                                                    std::map<uint16_t, std::string> CPU6502::disassemble(uint16_t nStart, uint16_t nStop) {
                                                        uint32_t addr = nStart;
                                                        std::map<uint16_t, std::string> mapLines;
                                                        while (addr <= (uint32_t)nStop) {
                                                            uint16_t line_addr = addr;
                                                            std::string sInst = "$ " + hex(addr, 4) + ": ";

                                                            mapLines[line_addr] = sInst;
                                                            addr++;
                                                        }
                                                        return mapLines;
                                                    }

                                                    void CPU6502::SaveState(BinaryWriter& out) const
                                                    {
                                                        out << a;
                                                        out << x;
                                                        out << y;
                                                        out << stkp;
                                                        out << pc;
                                                        out << status;

                                                        out << fetched;
                                                        out << addr_abs;
                                                        out << addr_rel;
                                                        out << opcode;
                                                        out << cycles;

                                                        out << irq_sources;
                                                    }

                                                    void CPU6502::LoadState(BinaryReader& in)
                                                    {
                                                        in >> a;
                                                        in >> x;
                                                        in >> y;
                                                        in >> stkp;
                                                        in >> pc;
                                                        in >> status;

                                                        in >> fetched;
                                                        in >> addr_abs;
                                                        in >> addr_rel;
                                                        in >> opcode;
                                                        in >> cycles;

                                                        in >> irq_sources;
                                                    }

#include "CPU6502_CycleAccurate.inc"