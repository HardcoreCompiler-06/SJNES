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

#pragma once
#include <cstdint>
#include <functional>
#include <algorithm>

static const uint16_t dmc_rate_table[16] = {
    428,380,340,320,286,254,226,214,
    190,160,142,128,106, 84, 72, 54
};

// Bảng lookup đảo thứ tự bit trong 1 byte (LSB<->MSB), dùng cho "Reverse DPCM Bit Order".
// Một số game bên thứ 3 (Double Dribble, Gimmick!) và famiclone
// đời đầu encode/phát sample DPCM theo thứ tự bit ngược so với chuẩn hardware NES thật,
// khiến âm thanh (thường là giọng nói/trống) bị méo/rè. Bật tùy chọn này sẽ đảo bit
// khi đọc từng byte sample để bù lại, giống tính năng "Reverse Bits" trong puNES/
static inline uint8_t ReverseBits8(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

// Mô hình DMC theo đúng phần cứng (nesdev "APU DMC"):
//   Reader ---> Sample buffer ---> Shift register (output unit) ---> 7-bit counter ---> mixer
//  - Memory reader và output unit là 2 khối ĐỘC LẬP chạy song song.
//  - Output unit chạy LIÊN TỤC, bất kể $4015 bit 4 đang bật hay tắt.
//  - Reader nạp trước 1 byte vào sample buffer; khi shift register hết 8 bit mà buffer đang đầy
//    thì byte trong buffer được chuyển sang shift register, buffer trống -> reader nạp byte kế.
//  - Ghi $4015 bit 4 = 0 chỉ đặt bytes_remaining = 0 (không cắt byte đang phát/byte trong buffer).
//  - Mỗi lần reader nạp 1 byte, CPU bị dừng vài cycle (DMA) -> xem dmaStallCycles.
struct DMC {
    bool     irq_enable = false;
    bool     loop = false;
    uint8_t  rate_index = 0;
    // Số cycle giữa 2 lần clock output unit = timer_reload + 1 = giá trị trong dmc_rate_table
    // (428, 380, ...). Vì vậy nạp (rate - 1), không phải rate.
    uint16_t timer_reload = 427;
    uint16_t timer = 0;

    uint8_t  output_level = 0;
    uint16_t sample_address = 0xC000;
    uint16_t sample_length = 1;
    uint16_t current_address = 0xC000;
    uint16_t bytes_remaining = 0;

    // ---- Output unit ----
    uint8_t  shift_register = 0;
    uint8_t  bits_remaining = 8;   // luôn trong khoảng 1..8 (đếm ngược từ 8)
    bool     silence = true;       // true: output unit không đổi output_level trong 8 clock tới

    // ---- Memory reader ----
    uint8_t  sample_buffer = 0;
    bool     sample_buffer_full = false;

    // Cờ DMC IRQ (bit 7 của $4015 khi đọc). APU đồng bộ cờ này sang CPU (IRQ_DMC).
    bool     irq_flag = false;

    // Bit D của lần ghi $4015 gần nhất (chỉ để APU/oscilloscope biết kênh có "bật" không).
    bool     enabled = false;

    // Số cycle CPU phải dừng do DMC DMA vừa phát sinh. APU lấy giá trị này ra rồi đặt về 0.
    int      dmaStallCycles = 0;

    // ---- Đo period lặp thật (dùng cho oscilloscope, KHÔNG ảnh hưởng audio) ----
    // Ý tưởng: mỗi lần bytes_remaining chạm 0 và loop=true, DMC tự Restart() —
    // đây là mốc lặp THẬT của hardware, chính xác tuyệt đối, không cần đoán.
    // Ta chỉ cần đếm số CPU cycle giữa 2 lần Restart() liên tiếp do loop gây ra.
    uint64_t dbgCycleCounter = 0;          // đếm cycle nội bộ, chỉ phục vụ debug/UI
    uint64_t dbgLastLoopRestartCycle = 0;  // cycle tại lần Restart() do loop gần nhất
    uint32_t dbgLoopPeriodCycles = 0;      // số cycle của 1 vòng lặp đầy đủ (0 = chưa đo được)
    bool     dbgHasLoopPeriod = false;     // true khi đã đo được ít nhất 1 chu kỳ trọn vẹn
    bool     dbgLoopMarkPending = false;   // đã Restart() do loop, chờ byte ĐẦU của vòng mới được reader nạp
    bool     buffer_is_loop_start = false; // byte đang nằm trong sample_buffer là byte ĐẦU của vòng loop mới

    // Bật để đảo bit mỗi byte sample DPCM trước khi phát — dùng cho các ROM
    // encode sai thứ tự bit (xem ReverseBits8 phía trên). Mặc định TẮT vì đa số
    // game encode đúng chuẩn; chỉ bật khi người dùng phát hiện DMC bị méo/rè.
    bool     reverseBits = false;

    // Bật để giảm tiếng "pop" khi CPU ghi trực tiếp vào $4011 (output level).
    // Thay vì set output_level ngay lập tức (gây delta lớn đột ngột -> click/pop),
    // ta cho giá trị "trườn" dần tới target với bước +-2 mỗi timer clock, y hệt
    // cơ chế tăng/giảm bình thường của DMC khi phát sample. Ý tưởng của tùy chọn
    // "Reduce popping sounds" tham khảo từ Mesen, nhưng cách cài đặt KHÁC: Mesen chia đôi
    // chênh lệch ngay khi ghi $4011 (nếu lệch > 50), còn ở đây trườn dần từng bước.
    // Mặc định TẮT vì hardware thật KHÔNG làm vậy (pop là đặc trưng phần cứng).
    bool     reducePopping = false;
    bool     rampActive = false;
    uint8_t  rampTarget = 0;

    // Callback để đọc RAM — gán từ APU: dmc.reader = [&](uint16_t a){ return bus->cpuRead(a); };
    std::function<uint8_t(uint16_t)> reader;

    // Đưa trạng thái chạy về lúc mới bật nguồn (KHÔNG đụng output_level và các tùy chọn của người dùng).
    void Reset() {
        enabled = false;
        irq_flag = false;
        bytes_remaining = 0;
        current_address = sample_address;
        sample_buffer_full = false;
        sample_buffer = 0;
        silence = true;
        shift_register = 0;
        bits_remaining = 8;
        timer = 0;
        rampActive = false;
        dmaStallCycles = 0;
        buffer_is_loop_start = false;
        dbgHasLoopPeriod = false;
        dbgLastLoopRestartCycle = 0;
        dbgLoopMarkPending = false;
    }

    // Ghi $4015 (bit D = en).
    void SetEnabled(bool en) {
        enabled = en;
        irq_flag = false; // mọi lần ghi $4015 đều xóa cờ DMC IRQ
        if (!en) {
            // Chỉ chặn việc nạp byte MỚI. Byte đang phát trong shift register và byte đã
            // nằm trong sample_buffer vẫn được phát nốt (đúng phần cứng).
            bytes_remaining = 0;
        }
        else if (bytes_remaining == 0) {
            Restart();
            FetchSampleByte(); // buffer trống -> reader nạp ngay byte đầu
        }
    }

    void Restart() {
        // Phần cứng thật chỉ nạp lại địa chỉ + độ dài. KHÔNG đụng vào shift register,
        // bits_remaining hay timer: byte cuối của vòng lặp vẫn phải phát nốt 8 bit.
        current_address = sample_address;
        bytes_remaining = sample_length;
    }

    // Memory reader: nếu sample buffer đang trống và còn byte cần đọc thì nạp 1 byte.
    void FetchSampleByte() {
        if (sample_buffer_full || bytes_remaining == 0 || !reader) return;

        uint8_t sampleByte = reader(current_address);
        if (reverseBits)
            sampleByte = ReverseBits8(sampleByte);

        sample_buffer = sampleByte;
        sample_buffer_full = true;

        // DMC DMA làm CPU dừng ~4 cycle mỗi lần đọc 1 byte (mô hình đơn giản: không xét
        // xung đột đọc thanh ghi, không xét chồng với OAM DMA).
        dmaStallCycles += 4;

        // Mốc lặp cho oscilloscope: byte ĐẦU của vòng loop mới vừa được nạp vào buffer;
        // thời điểm nghe thấy thật là lúc nó được chuyển sang shift register (xem TickTimer).
        if (dbgLoopMarkPending) {
            dbgLoopMarkPending = false;
            buffer_is_loop_start = true;
        }

        // Wrap address: 0xFFFF + 1 -> 0x8000
        if (current_address == 0xFFFF)
            current_address = 0x8000;
        else
            current_address++;

        bytes_remaining--;
        if (bytes_remaining == 0) {
            if (loop) {
                Restart();
                dbgLoopMarkPending = true;
            }
            else if (irq_enable) {
                irq_flag = true;
            }
        }
    }

    void TickTimer() {
        dbgCycleCounter++;

        if (timer > 0) { timer--; return; }
        timer = timer_reload;

        // Ramp output_level dần tới rampTarget (chỉ chạy khi reducePopping bật và
        // đang có 1 lần ghi $4011 trực tiếp cần làm mượt). Bước +-2 mỗi timer clock,
        // giống hệt tốc độ thay đổi bình thường của DMC nên nghe tự nhiên, không lộ.
        if (rampActive) {
            if (output_level < rampTarget) {
                output_level = (uint8_t)std::min<int>(output_level + 2, rampTarget);
            }
            else if (output_level > rampTarget) {
                output_level = (uint8_t)std::max<int>((int)output_level - 2, rampTarget);
            }
            if (output_level == rampTarget) rampActive = false;
        }

        // ---- Output unit ----
        if (!silence) {
            if (shift_register & 1) { if (output_level <= 125) output_level += 2; }
            else { if (output_level >= 2)   output_level -= 2; }
            shift_register >>= 1;
        }

        if (--bits_remaining == 0) {
            bits_remaining = 8;
            if (!sample_buffer_full) {
                silence = true;
            }
            else {
                silence = false;
                shift_register = sample_buffer;
                sample_buffer_full = false;

                // Byte đầu của vòng loop mới bắt đầu được PHÁT từ đây (mốc cho oscilloscope).
                if (buffer_is_loop_start) {
                    buffer_is_loop_start = false;
                    if (dbgLastLoopRestartCycle != 0) {
                        dbgLoopPeriodCycles = (uint32_t)(dbgCycleCounter - dbgLastLoopRestartCycle);
                        dbgHasLoopPeriod = true;
                    }
                    dbgLastLoopRestartCycle = dbgCycleCounter;
                }

                // Buffer vừa trống -> reader nạp ngay byte kế (nếu còn).
                FetchSampleByte();
            }
        }
    }

    float Output() const {
        return (float)output_level;
    }

    void WriteReg(uint8_t reg, uint8_t data) {
        switch (reg) {
        case 0:  // $4010
        {
            bool oldLoop = loop;
            irq_enable = (data & 0x80) != 0;
            loop = (data & 0x40) != 0;
            rate_index = data & 0x0F;
            timer_reload = dmc_rate_table[rate_index] - 1; // chu kỳ thật = timer_reload + 1 = rate
            if (!irq_enable)
                irq_flag = false; // xóa bit I cũng xóa cờ DMC IRQ
            if (oldLoop && !loop) {
                // Tắt loop giữa chừng -> period cũ không còn nghĩa lý gì nữa
                dbgHasLoopPeriod = false;
                dbgLastLoopRestartCycle = 0;
                dbgLoopMarkPending = false;
                buffer_is_loop_start = false;
            }
            break;
        }
        case 1:  // $4011
        {
            uint8_t newLevel = data & 0x7F;
            if (reducePopping) {
                // Không set ngay, chỉ đặt target rồi để TickTimer() trườn dần tới đó.
                rampTarget = newLevel;
                rampActive = (rampTarget != output_level);
            }
            else {
                output_level = newLevel;
                rampActive = false; // đảm bảo tắt hẳn nếu toggle bị bật/tắt giữa chừng
            }
            break;
        }
        case 2:  // $4012
            sample_address = 0xC000 | ((uint16_t)data << 6);
            // Đổi sample -> period lặp đo được trước đó (nếu có) không còn đúng nữa
            dbgHasLoopPeriod = false;
            dbgLastLoopRestartCycle = 0;
            dbgLoopMarkPending = false;
            buffer_is_loop_start = false;
            break;
        case 3:  // $4013
            sample_length = ((uint16_t)data << 4) + 1;
            dbgHasLoopPeriod = false;
            dbgLastLoopRestartCycle = 0;
            dbgLoopMarkPending = false;
            buffer_is_loop_start = false;
            break;
        }
    }
};
