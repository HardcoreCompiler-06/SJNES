#include "pch.h"
#include "gtest/gtest.h"
#include "CPU6502.h"
#include "Bus.h"

// KIỂM THỬ CHỨC NĂNG (TC-03)
// 1. Kiểm tra lệnh LDA chế độ Immediate (Ghi trực tiếp vào A)
TEST(CPU6502_Test, TC03_LDA_Immediate) {
    Bus bus; CPU6502 cpu;
    cpu.ConnectBus(&bus); cpu.reset();

    bus.cpuWrite(0x0000, 0xA9); // Opcode LDA Immediate
    bus.cpuWrite(0x0001, 0x05);
    cpu.pc = 0x0000;
    cpu.clock(); cpu.clock();

    EXPECT_EQ(cpu.a, 0x05);
    EXPECT_FALSE(cpu.status & CPU6502::Z);
}

// 2. Kiểm tra lệnh LDA chế độ Zero Page (Đọc từ RAM)
TEST(CPU6502_Test, TC03_LDA_ZeroPage) {
    Bus bus; CPU6502 cpu;
    cpu.ConnectBus(&bus); cpu.reset();

    bus.cpuWrite(0x0020, 0x37); // RAM tại $20 có giá trị 0x37
    bus.cpuWrite(0x0000, 0xA5); // Opcode LDA ZeroPage
    bus.cpuWrite(0x0001, 0x20);
    cpu.pc = 0x0000;
    cpu.clock(); cpu.clock(); cpu.clock();

    EXPECT_EQ(cpu.a, 0x37);
}

// 3. Kiểm tra lệnh STA chế độ Zero Page (Ghi ra RAM)
TEST(CPU6502_Test, TC03_STA_ZeroPage) {
    Bus bus; CPU6502 cpu;
    cpu.ConnectBus(&bus); cpu.reset();

    cpu.a = 0x42; // Giá trị cần lưu
    bus.cpuWrite(0x0000, 0x85); // Opcode STA ZeroPage
    bus.cpuWrite(0x0001, 0x15); // Địa chỉ lưu 0x0015
    cpu.pc = 0x0000;
    cpu.clock(); cpu.clock(); cpu.clock();

    EXPECT_EQ(bus.cpuRead(0x0015), 0x42); // Kiểm tra RAM có lưu đúng 0x42 không
}

// TC-04: Kiểm tra lệnh cộng ADC và cờ quá tải (Carry Flag)
TEST(CPU6502_Test, TC04_ADC_Boundary_Overflow) {
    Bus bus;
    CPU6502 cpu;
    cpu.ConnectBus(&bus);
    cpu.reset();

    // Thiết lập giá trị biên: Nạp 0xFF (255) vào thanh ghi A
    cpu.a = 0xFF;

    // Đưa lệnh ADC #$01 (Cộng thêm 1) vào vùng RAM
    bus.cpuWrite(0x0000, 0x69);
    bus.cpuWrite(0x0001, 0x01);
    cpu.pc = 0x0000;

    // Thực thi lệnh (tốn 2 chu kỳ)
    cpu.clock();
    cpu.clock();

    // KIỂM ĐỊNH KẾT QUẢ (Assertions)
    // 0xFF + 0x01 = 0x00 (vượt giới hạn 8-bit)
    EXPECT_EQ(cpu.a, 0x00);
    EXPECT_TRUE(cpu.status & CPU6502::C); // Cờ quá tải (Carry) phải BẬT
    EXPECT_TRUE(cpu.status & CPU6502::Z); // Cờ Zero (Z) phải BẬT
}