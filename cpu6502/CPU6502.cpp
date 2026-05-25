#include "CPU6502.h"
#include "Bus.h"

// ==============================================================================
// CỤC 1: KHỞI TẠO, BẢNG TỪ ĐIỂN 256 LỆNH VÀ CÁC TÍN HIỆU PHẦN CỨNG
// ==============================================================================

CPU6502::CPU6502()
{
    using a = CPU6502;
    lookup =
    {
        { "BRK", &a::BRK, &a::IMM, 7 },{ "ORA", &a::ORA, &a::IZX, 6 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "XXX", &a::NOP, &a::IMP, 3 },{ "ORA", &a::ORA, &a::ZPG, 3 },{ "ASL", &a::ASL, &a::ZPG, 5 },{ "XXX", &a::XXX, &a::IMP, 5 },{ "PHP", &a::PHP, &a::IMP, 3 },{ "ORA", &a::ORA, &a::IMM, 2 },{ "ASL", &a::ASL, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "ORA", &a::ORA, &a::ABS, 4 },{ "ASL", &a::ASL, &a::ABS, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },
        { "BPL", &a::BPL, &a::REL, 2 },{ "ORA", &a::ORA, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "ORA", &a::ORA, &a::ZPX, 4 },{ "ASL", &a::ASL, &a::ZPX, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },{ "CLC", &a::CLC, &a::IMP, 2 },{ "ORA", &a::ORA, &a::ABY, 4 },{ "XXX", &a::NOP, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 7 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "ORA", &a::ORA, &a::ABX, 4 },{ "ASL", &a::ASL, &a::ABX, 7 },{ "XXX", &a::XXX, &a::IMP, 7 },
        { "JSR", &a::JSR, &a::ABS, 6 },{ "AND", &a::AND, &a::IZX, 6 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "BIT", &a::BIT, &a::ZPG, 3 },{ "AND", &a::AND, &a::ZPG, 3 },{ "ROL", &a::ROL, &a::ZPG, 5 },{ "XXX", &a::XXX, &a::IMP, 5 },{ "PLP", &a::PLP, &a::IMP, 4 },{ "AND", &a::AND, &a::IMM, 2 },{ "ROL", &a::ROL, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "BIT", &a::BIT, &a::ABS, 4 },{ "AND", &a::AND, &a::ABS, 4 },{ "ROL", &a::ROL, &a::ABS, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },
        { "BMI", &a::BMI, &a::REL, 2 },{ "AND", &a::AND, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "AND", &a::AND, &a::ZPX, 4 },{ "ROL", &a::ROL, &a::ZPX, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },{ "SEC", &a::SEC, &a::IMP, 2 },{ "AND", &a::AND, &a::ABY, 4 },{ "XXX", &a::NOP, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 7 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "AND", &a::AND, &a::ABX, 4 },{ "ROL", &a::ROL, &a::ABX, 7 },{ "XXX", &a::XXX, &a::IMP, 7 },
        { "RTI", &a::RTI, &a::IMP, 6 },{ "EOR", &a::EOR, &a::IZX, 6 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "XXX", &a::NOP, &a::IMP, 3 },{ "EOR", &a::EOR, &a::ZPG, 3 },{ "LSR", &a::LSR, &a::ZPG, 5 },{ "XXX", &a::XXX, &a::IMP, 5 },{ "PHA", &a::PHA, &a::IMP, 3 },{ "EOR", &a::EOR, &a::IMM, 2 },{ "LSR", &a::LSR, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "JMP", &a::JMP, &a::ABS, 3 },{ "EOR", &a::EOR, &a::ABS, 4 },{ "LSR", &a::LSR, &a::ABS, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },
        { "BVC", &a::BVC, &a::REL, 2 },{ "EOR", &a::EOR, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "EOR", &a::EOR, &a::ZPX, 4 },{ "LSR", &a::LSR, &a::ZPX, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },{ "CLI", &a::CLI, &a::IMP, 2 },{ "EOR", &a::EOR, &a::ABY, 4 },{ "XXX", &a::NOP, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 7 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "EOR", &a::EOR, &a::ABX, 4 },{ "LSR", &a::LSR, &a::ABX, 7 },{ "XXX", &a::XXX, &a::IMP, 7 },
        { "RTS", &a::RTS, &a::IMP, 6 },{ "ADC", &a::ADC, &a::IZX, 6 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "XXX", &a::NOP, &a::IMP, 3 },{ "ADC", &a::ADC, &a::ZPG, 3 },{ "ROR", &a::ROR, &a::ZPG, 5 },{ "XXX", &a::XXX, &a::IMP, 5 },{ "PLA", &a::PLA, &a::IMP, 4 },{ "ADC", &a::ADC, &a::IMM, 2 },{ "ROR", &a::ROR, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "JMP", &a::JMP, &a::IND, 5 },{ "ADC", &a::ADC, &a::ABS, 4 },{ "ROR", &a::ROR, &a::ABS, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },
        { "BVS", &a::BVS, &a::REL, 2 },{ "ADC", &a::ADC, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "ADC", &a::ADC, &a::ZPX, 4 },{ "ROR", &a::ROR, &a::ZPX, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },{ "SEI", &a::SEI, &a::IMP, 2 },{ "ADC", &a::ADC, &a::ABY, 4 },{ "XXX", &a::NOP, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 7 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "ADC", &a::ADC, &a::ABX, 4 },{ "ROR", &a::ROR, &a::ABX, 7 },{ "XXX", &a::XXX, &a::IMP, 7 },
        { "XXX", &a::NOP, &a::IMP, 2 },{ "STA", &a::STA, &a::IZX, 6 },{ "XXX", &a::NOP, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 6 },{ "STY", &a::STY, &a::ZPG, 3 },{ "STA", &a::STA, &a::ZPG, 3 },{ "STX", &a::STX, &a::ZPG, 3 },{ "XXX", &a::XXX, &a::IMP, 3 },{ "DEY", &a::DEY, &a::IMP, 2 },{ "XXX", &a::NOP, &a::IMP, 2 },{ "TXA", &a::TXA, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "STY", &a::STY, &a::ABS, 4 },{ "STA", &a::STA, &a::ABS, 4 },{ "STX", &a::STX, &a::ABS, 4 },{ "XXX", &a::XXX, &a::IMP, 4 },
        { "BCC", &a::BCC, &a::REL, 2 },{ "STA", &a::STA, &a::IZY, 6 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 6 },{ "STY", &a::STY, &a::ZPX, 4 },{ "STA", &a::STA, &a::ZPX, 4 },{ "STX", &a::STX, &a::ZPY, 4 },{ "XXX", &a::XXX, &a::IMP, 4 },{ "TYA", &a::TYA, &a::IMP, 2 },{ "STA", &a::STA, &a::ABY, 5 },{ "TXS", &a::TXS, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 5 },{ "XXX", &a::NOP, &a::IMP, 5 },{ "STA", &a::STA, &a::ABX, 5 },{ "XXX", &a::XXX, &a::IMP, 5 },{ "XXX", &a::XXX, &a::IMP, 5 },
        { "LDY", &a::LDY, &a::IMM, 2 },{ "LDA", &a::LDA, &a::IZX, 6 },{ "LDX", &a::LDX, &a::IMM, 2 },{ "XXX", &a::XXX, &a::IMP, 6 },{ "LDY", &a::LDY, &a::ZPG, 3 },{ "LDA", &a::LDA, &a::ZPG, 3 },{ "LDX", &a::LDX, &a::ZPG, 3 },{ "XXX", &a::XXX, &a::IMP, 3 },{ "TAY", &a::TAY, &a::IMP, 2 },{ "LDA", &a::LDA, &a::IMM, 2 },{ "TAX", &a::TAX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "LDY", &a::LDY, &a::ABS, 4 },{ "LDA", &a::LDA, &a::ABS, 4 },{ "LDX", &a::LDX, &a::ABS, 4 },{ "XXX", &a::XXX, &a::IMP, 4 },
        { "BCS", &a::BCS, &a::REL, 2 },{ "LDA", &a::LDA, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 5 },{ "LDY", &a::LDY, &a::ZPX, 4 },{ "LDA", &a::LDA, &a::ZPX, 4 },{ "LDX", &a::LDX, &a::ZPY, 4 },{ "XXX", &a::XXX, &a::IMP, 4 },{ "CLV", &a::CLV, &a::IMP, 2 },{ "LDA", &a::LDA, &a::ABY, 4 },{ "TSX", &a::TSX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 4 },{ "LDY", &a::LDY, &a::ABX, 4 },{ "LDA", &a::LDA, &a::ABX, 4 },{ "LDX", &a::LDX, &a::ABY, 4 },{ "XXX", &a::XXX, &a::IMP, 4 },
        { "CPY", &a::CPY, &a::IMM, 2 },{ "CMP", &a::CMP, &a::IZX, 6 },{ "XXX", &a::NOP, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "CPY", &a::CPY, &a::ZPG, 3 },{ "CMP", &a::CMP, &a::ZPG, 3 },{ "DEC", &a::DEC, &a::ZPG, 5 },{ "XXX", &a::XXX, &a::IMP, 5 },{ "INY", &a::INY, &a::IMP, 2 },{ "CMP", &a::CMP, &a::IMM, 2 },{ "DEX", &a::DEX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "CPY", &a::CPY, &a::ABS, 4 },{ "CMP", &a::CMP, &a::ABS, 4 },{ "DEC", &a::DEC, &a::ABS, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },
        { "BNE", &a::BNE, &a::REL, 2 },{ "CMP", &a::CMP, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "CMP", &a::CMP, &a::ZPX, 4 },{ "DEC", &a::DEC, &a::ZPX, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },{ "CLD", &a::CLD, &a::IMP, 2 },{ "CMP", &a::CMP, &a::ABY, 4 },{ "NOP", &a::NOP, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 7 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "CMP", &a::CMP, &a::ABX, 4 },{ "DEC", &a::DEC, &a::ABX, 7 },{ "XXX", &a::XXX, &a::IMP, 7 },
        { "CPX", &a::CPX, &a::IMM, 2 },{ "SBC", &a::SBC, &a::IZX, 6 },{ "XXX", &a::NOP, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "CPX", &a::CPX, &a::ZPG, 3 },{ "SBC", &a::SBC, &a::ZPG, 3 },{ "INC", &a::INC, &a::ZPG, 5 },{ "XXX", &a::XXX, &a::IMP, 5 },{ "INX", &a::INX, &a::IMP, 2 },{ "SBC", &a::SBC, &a::IMM, 2 },{ "NOP", &a::NOP, &a::IMP, 2 },{ "SBC", &a::SBC, &a::IMP, 2 },{ "CPX", &a::CPX, &a::ABS, 4 },{ "SBC", &a::SBC, &a::ABS, 4 },{ "INC", &a::INC, &a::ABS, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },
        { "BEQ", &a::BEQ, &a::REL, 2 },{ "SBC", &a::SBC, &a::IZY, 5 },{ "XXX", &a::XXX, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 8 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "SBC", &a::SBC, &a::ZPX, 4 },{ "INC", &a::INC, &a::ZPX, 6 },{ "XXX", &a::XXX, &a::IMP, 6 },{ "SED", &a::SED, &a::IMP, 2 },{ "SBC", &a::SBC, &a::ABY, 4 },{ "NOP", &a::NOP, &a::IMP, 2 },{ "XXX", &a::XXX, &a::IMP, 7 },{ "XXX", &a::NOP, &a::IMP, 4 },{ "SBC", &a::SBC, &a::ABX, 4 },{ "INC", &a::INC, &a::ABX, 7 },{ "XXX", &a::XXX, &a::IMP, 7 },
    };
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

    addr_rel = 0x0000; addr_abs = 0x0000; fetched = 0x00;
    cycles = 8;
}

void CPU6502::nmi() {
    nmi_pending = true;
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
void CPU6502::clock() {
    if (cycles == 0) {
        if (nmi_pending) {
            nmi_pending = false;

            write(0x0100 + stkp, (pc >> 8) & 0x00FF); stkp--;
            write(0x0100 + stkp, pc & 0x00FF); stkp--;

            // Hardware pushes P with B clear, U set, then sets I.
            uint8_t pushedStatus = (status & ~B) | U;
            write(0x0100 + stkp, pushedStatus); stkp--;
            SetFlag(I, true);
            SetFlag(U, true);

            uint16_t lo = read(0xFFFA);
            uint16_t hi = read(0xFFFB);
            pc = (hi << 8) | lo;
            cycles = 8;
        }
        else if (IsIrqActive() && GetFlag(I) == 0) {
            // If this was the old one-shot irq() request, consume it.
            // Held IRQ sources remain active until ClearIrqSource() is called.
            if (irq_sources == 0) {
                irq_pending = false;
            }
            irqHandledCount++;

            write(0x0100 + stkp, (pc >> 8) & 0x00FF); stkp--;
            write(0x0100 + stkp, pc & 0x00FF); stkp--;

            // Hardware pushes P with B clear, U set, then sets I.
            uint8_t pushedStatus = (status & ~B) | U;
            write(0x0100 + stkp, pushedStatus); stkp--;
            SetFlag(I, true);
            SetFlag(U, true);

            uint16_t lo = read(0xFFFE);
            uint16_t hi = read(0xFFFF);
            pc = (hi << 8) | lo;
            cycles = 7;
        }
        else {
            if (IsIrqActive() && GetFlag(I) != 0) {
                irqBlockedCount++;
            }

            opcode = read(pc);
            SetFlag(U, true);
            pc++;
            cycles = lookup[opcode].cycles;
            uint8_t c1 = (this->*lookup[opcode].addrmode)();
            uint8_t c2 = (this->*lookup[opcode].operate)();
            cycles += (c1 & c2);
            SetFlag(U, true);
        }
    }
    clock_count++;
    cycles--;
}

uint8_t CPU6502::fetch() {
    if (!(lookup[opcode].addrmode == &CPU6502::IMP))
        fetched = read(addr_abs);
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

    // Giả lập lỗi phần cứng "huyền thoại" của chip 6502
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

uint8_t CPU6502::BCC() { if (GetFlag(C) == 0) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
uint8_t CPU6502::BCS() { if (GetFlag(C) == 1) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
uint8_t CPU6502::BEQ() { if (GetFlag(Z) == 1) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
uint8_t CPU6502::BNE() { if (GetFlag(Z) == 0) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
uint8_t CPU6502::BMI() { if (GetFlag(N) == 1) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
uint8_t CPU6502::BPL() { if (GetFlag(N) == 0) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
uint8_t CPU6502::BVC() { if (GetFlag(V) == 0) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
uint8_t CPU6502::BVS() { if (GetFlag(V) == 1) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
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
uint8_t CPU6502::PHP() { write(0x0100 + stkp, status | B | U); SetFlag(B, 0); SetFlag(U, 0); stkp--; return 0; }
uint8_t CPU6502::PLA() { stkp++; a = read(0x0100 + stkp); SetFlag(Z, a == 0x00); SetFlag(N, a & 0x80); return 0; }
uint8_t CPU6502::PLP() { stkp++; status = read(0x0100 + stkp); SetFlag(U, 1); return 0; }
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
uint8_t CPU6502::BRK() {
    pc++;
    write(0x0100 + stkp, (pc >> 8) & 0x00FF); stkp--;
    write(0x0100 + stkp, pc & 0x00FF); stkp--;
    uint8_t pushedStatus = status | B | U;
    write(0x0100 + stkp, pushedStatus); stkp--;
    SetFlag(I, true);
    SetFlag(B, false);
    SetFlag(U, true);
    pc = (uint16_t)read(0xFFFE) | ((uint16_t)read(0xFFFF) << 8);
    return 0;
}
uint8_t CPU6502::TAX() { x = a; SetFlag(Z, x == 0x00); SetFlag(N, x & 0x80); return 0; }
uint8_t CPU6502::TAY() { y = a; SetFlag(Z, y == 0x00); SetFlag(N, y & 0x80); return 0; }
uint8_t CPU6502::TSX() { x = stkp; SetFlag(Z, x == 0x00); SetFlag(N, x & 0x80); return 0; }
uint8_t CPU6502::TXA() { a = x; SetFlag(Z, a == 0x00); SetFlag(N, a & 0x80); return 0; }
uint8_t CPU6502::TXS() { stkp = x; return 0; }
uint8_t CPU6502::TYA() { a = y; SetFlag(Z, a == 0x00); SetFlag(N, a & 0x80); return 0; }
uint8_t CPU6502::XXX() { return 0; }

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

                                                            // Vì hiện tại mình đang để tạm, nên để tránh lỗi Bus, anh em mình comment tạm 3 dòng này lại:
                                                            // uint8_t opcode = bus->cpuRead(addr); addr++;
                                                            // sInst += lookup[opcode].name + " ";

    mapLines[line_addr] = sInst;
    addr++; 
    }
  return mapLines;
 }
                                                   
                                                   