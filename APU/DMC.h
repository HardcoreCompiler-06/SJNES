#pragma once
#include <cstdint>
#include <functional>

static const uint16_t dmc_rate_table[16] = {
    428,380,340,320,286,254,226,214,
    190,160,142,128,106, 84, 72, 54
};

struct DMC {
    bool     irq_enable      = false;
    bool     loop            = false;
    uint8_t  rate_index      = 0;
    uint16_t timer_reload    = 0;
    uint16_t timer           = 0;

    uint8_t  output_level    = 0;
    uint16_t sample_address  = 0x0000;
    uint16_t sample_length   = 0;
    uint16_t current_address = 0;
    uint16_t bytes_remaining = 0;
    uint8_t  shift_register  = 0;
    uint8_t  bits_remaining  = 0;

    bool     enabled         = false;

    // Callback để đọc RAM — gán từ APU: dmc.reader = [&](uint16_t a){ return bus->cpuRead(a); };
    std::function<uint8_t(uint16_t)> reader;

    void SetEnabled(bool en) {
        enabled = en;
        if (!enabled) {
            bytes_remaining = 0;
            bits_remaining  = 0;
        } else {
            if (bytes_remaining == 0) Restart();
        }
    }

    void Restart() {
        current_address = sample_address;
        bytes_remaining = sample_length;
        bits_remaining  = 0;
        timer           = timer_reload;
    }

    // Gọi mỗi CPU cycle
    void TickTimer() {
        if (!enabled) return;

        if (timer > 0) { timer--; return; }
        timer = timer_reload;

        // Output unit
        if (bits_remaining > 0) {
            if (shift_register & 1) { if (output_level <= 125) output_level += 2; }
            else                    { if (output_level >= 2)   output_level -= 2; }
            shift_register >>= 1;
            bits_remaining--;
        }

        // Load sample byte
        if (bits_remaining == 0 && bytes_remaining > 0 && reader) {
            shift_register  = reader(current_address);
            bits_remaining  = 8;

            // Wrap address: uint16_t 0xFFFF+1 = 0x0000 → về 0x8000
            if (current_address == 0xFFFF)
                current_address = 0x8000;
            else
                current_address++;

            bytes_remaining--;

            if (bytes_remaining == 0 && loop)
                Restart();
        }
    }

    float Output() const {
        return (float)output_level;
    }

    void WriteReg(uint8_t reg, uint8_t data) {
        switch (reg) {
        case 0:  // $4010
            irq_enable   = (data & 0x80) != 0;
            loop         = (data & 0x40) != 0;
            rate_index   = data & 0x0F;
            timer_reload = dmc_rate_table[rate_index];
            break;
        case 1:  // $4011
            output_level = data & 0x7F;
            break;
        case 2:  // $4012
            sample_address = 0xC000 | ((uint16_t)data << 6);
            break;
        case 3:  // $4013
            sample_length = ((uint16_t)data << 4) + 1;
            break;
        }
    }
};
