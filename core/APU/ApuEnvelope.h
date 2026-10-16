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

struct ApuEnvelope {
    bool    start           = false;
    bool    loop            = false;
    bool    constant_volume = false;
    uint8_t volume          = 0;
    uint8_t decay_level     = 0;
    uint8_t timer           = 0;

    void Clock() {
        if (start) {
            start       = false;
            decay_level = 15;
            timer       = volume;
        } else {
            if (timer > 0) {
                timer--;
            } else {
                timer = volume;
                if (decay_level > 0) decay_level--;
                else if (loop)       decay_level = 15;
            }
        }
    }

    uint8_t Output() const {
        return constant_volume ? volume : decay_level;
    }
};
