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
#include "ApuEnvelope.h"
#include "PolyBlep.h"

// Duty table chuẩn NES
static const uint8_t sq_duty_table[4][8] = {
    {0,1,0,0,0,0,0,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,0,0,0},
    {1,0,0,1,1,1,1,1}
};

static const uint8_t sq_length_table[32] = {
    10,254,20, 2,40, 4,80, 6,
   160,  8,60,10,14,12,26,14,
    12, 16,24,18,48,20,96,22,
   192, 24,72,26,16,28,32,30
};

struct F1 {
    uint16_t    timer = 0;
    uint16_t    timer_reload = 0;
    uint8_t     duty = 0;
    uint8_t     pulse_phase = 0;
    uint8_t     length_counter = 0;
    bool        enabled = false;
    ApuEnvelope env;

    struct Sweep {
        bool    enabled = false;
        bool    negate = false;
        bool    reload = false;
        uint8_t shift = 0;
        uint8_t period = 0;
        uint8_t timer = 0;
    } sweep;

    // Pha liên tục (0..1) dùng RIÊNG cho tổng hợp âm thanh band-limited.
    // Tách biệt hoàn toàn khỏi timer/pulse_phase (vốn cycle-accurate, dùng cho
    // sweep/envelope/length) để không ảnh hưởng tính đúng đắn của emulation —
    // chỉ ảnh hưởng tới CÁCH sample được vẽ ra khi xuất audio.
    double blep_phase = 0.0;

    // Duty cycle xấp xỉ theo tỷ lệ % thời gian mức cao, khớp với 4 pattern
    // chuẩn NES (12.5%, 25%, 50%, 75% — duty 3 là 25% low đảo pha thành 75% high).
    static constexpr double DutyFraction(uint8_t d) {
        switch (d) {
        case 0: return 0.125;
        case 1: return 0.25;
        case 2: return 0.5;
        default: return 0.75;
        }
    }

    // Output KHÔNG bị alias, dùng cho audio thật. Gọi 1 LẦN MỖI AUDIO SAMPLE
    // (không phải mỗi CPU cycle), sampleRate = tần số lấy mẫu thực tế (thường 44100).
    float BandLimitedOutput(double sampleRate) {
        if (!enabled)             return 0.0f;
        if (IsMuted())            return 0.0f;
        if (length_counter == 0)  return 0.0f;

        double freq = 1789773.0 / (16.0 * (double)(timer_reload + 1));
        if (freq <= 0.0) return 0.0f;

        double dt = freq / sampleRate;
        // Trên Nyquist thực sự (dt >= 0.5): không còn ý nghĩa audio, im lặng
        // thay vì phát ra rác đã bị gập tần số.
        if (dt >= 0.5) return 0.0f;

        double wave = BandLimitedSquare(blep_phase, dt, DutyFraction(duty));

        blep_phase += dt;
        if (blep_phase >= 1.0) blep_phase -= 1.0;

        return (float)(wave * env.Output());
    }

    // Gọi mỗi 2 CPU cycles (half-clock)
    void TickTimer() {
        if (!enabled) return;
        if (timer > 0) timer--;
        else {
            timer = timer_reload;
            pulse_phase = (pulse_phase + 1) % 8;
        }
    }

    // Gọi ở half-frame
    void TickLengthAndSweep()
    {
        if (!env.loop && length_counter > 0)
            length_counter--;

        if (sweep.timer > 0)
            sweep.timer--;

        if (sweep.timer == 0)
        {
            if (sweep.enabled && sweep.shift > 0 && length_counter > 0 && !IsMuted())
            {
                int change = timer_reload >> sweep.shift;
                int target = sweep.negate
                    ? (timer_reload - change - 1)   // Pulse 1
                    : (timer_reload + change);

                if (target >= 0 && target <= 0x7FF)
                    timer_reload = (uint16_t)target;
            }

            sweep.timer = sweep.period;
        }

        if (sweep.reload)
        {
            sweep.timer = sweep.period;
            sweep.reload = false;
        }
    }

    bool IsMuted() const {
        if (timer_reload < 8) return true;
        int change = sweep.shift ? (timer_reload >> sweep.shift) : 0;
        int target = sweep.negate ? (timer_reload - change - 1)
            : (timer_reload + change);
        return (target > 0x7FF || target < 0);
    }

    float Output() const {
        if (!enabled)            return 0.0f;
        if (IsMuted())           return 0.0f;
        if (length_counter == 0) return 0.0f;
        if (!sq_duty_table[duty][pulse_phase]) return 0.0f;
        return (float)env.Output();
    }

    // Viết thanh ghi $4000-$4003
    void WriteReg(uint8_t reg, uint8_t data) {
        switch (reg) {
        case 0:
            duty = (data & 0xC0) >> 6;
            env.loop = (data & 0x20) != 0;
            env.constant_volume = (data & 0x10) != 0;
            env.volume = data & 0x0F;
            break;
        case 1:
            sweep.enabled = (data & 0x80) != 0;
            sweep.period = ((data >> 4) & 0x07) + 1;
            sweep.negate = (data & 0x08) != 0;
            sweep.shift = data & 0x07;
            sweep.reload = true;
            break;
        case 2:
            timer_reload = (timer_reload & 0xFF00) | data;
            break;
        case 3:
            timer_reload = ((uint16_t)(data & 0x07) << 8) | (timer_reload & 0x00FF);
            timer = timer_reload;
            if (enabled) length_counter = sq_length_table[(data & 0xF8) >> 3];
            pulse_phase = 0;
            blep_phase = 0.0;
            env.start = true;
            break;
        }
    }
};