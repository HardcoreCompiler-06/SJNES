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
#include "F1.h"   // dùng chung sq_duty_table, sq_length_table
#include "PolyBlep.h"

struct F2 {
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

    // Xem giải thích chi tiết trong F1.h — cùng cơ chế, tách biệt khỏi
    // timer/pulse_phase cycle-accurate, chỉ phục vụ audio output.
    double blep_phase = 0.0;

    static constexpr double DutyFraction(uint8_t d) {
        switch (d) {
        case 0: return 0.125;
        case 1: return 0.25;
        case 2: return 0.5;
        default: return 0.75;
        }
    }

    float BandLimitedOutput(double sampleRate) {
        if (!enabled)             return 0.0f;
        if (IsMuted())            return 0.0f;
        if (length_counter == 0)  return 0.0f;

        double freq = 1789773.0 / (16.0 * (double)(timer_reload + 1));
        if (freq <= 0.0) return 0.0f;

        double dt = freq / sampleRate;
        if (dt >= 0.5) return 0.0f;

        double wave = BandLimitedSquare(blep_phase, dt, DutyFraction(duty));

        blep_phase += dt;
        if (blep_phase >= 1.0) blep_phase -= 1.0;

        return (float)(wave * env.Output());
    }

    void TickTimer() {
        if (!enabled) return;
        if (timer > 0) timer--;
        else {
            timer = timer_reload;
            pulse_phase = (pulse_phase + 1) % 8;
        }
    }

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
                    ? (timer_reload - change)       // Pulse 2
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
        int target = sweep.negate ? (timer_reload - change)
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