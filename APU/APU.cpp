#include "APU.h"
#include "Bus.h"

// --- CÁC BẢNG TRA CỨU CHUẨN NES ---
const uint16_t dmc_rate_table[16] = {
    428, 380, 340, 320, 286, 254, 226, 214,
    190, 160, 142, 128, 106, 84, 72, 54
};

const uint8_t length_table[32] = {
    10, 254, 20,  2, 40,  4, 80,  6,
    160,  8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

const uint16_t noise_period_table[16] = {
    4, 8, 16, 32, 64, 96, 128, 160,
    202, 254, 380, 508, 762, 1016, 2034, 4068
};

const uint8_t duty_table[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 0, 0, 0},
    {1, 0, 0, 1, 1, 1, 1, 1}
};

APU::APU() {
    noise.shift_register = 1;
}

APU::~APU() {}

// --- NHẬN LỆNH TỪ CPU (0x4000 - 0x4017) ---
void APU::cpuWrite(uint16_t addr, uint8_t data) {
    switch (addr) {

    case 0x4000:
        pulse1.duty = (data & 0xC0) >> 6;
        pulse1.env.loop = (data & 0x20) != 0;
        pulse1.env.constant_volume = (data & 0x10) != 0;
        pulse1.env.volume = data & 0x0F;
        break;

    case 0x4001:
        pulse1.sweep.enabled = (data & 0x80) != 0;
        pulse1.sweep.period = (data >> 4) & 0x07;
        pulse1.sweep.negate = (data & 0x08) != 0;
        pulse1.sweep.shift = data & 0x07;
        pulse1.sweep.reload = true;
        break;

    case 0x4002:
        pulse1.timer_reload = (pulse1.timer_reload & 0xFF00) | data;
        break;

    case 0x4003:
        pulse1.timer_reload = ((uint16_t)(data & 0x07) << 8) | (pulse1.timer_reload & 0x00FF);
        pulse1.timer = pulse1.timer_reload;
        if (pulse1_enabled) pulse1.length_counter = length_table[(data & 0xF8) >> 3];
        pulse1.pulse_phase = 0;
        pulse1.env.start = true;
        break;

    case 0x4004:
        pulse2.duty = (data & 0xC0) >> 6;
        pulse2.env.loop = (data & 0x20) != 0;
        pulse2.env.constant_volume = (data & 0x10) != 0;
        pulse2.env.volume = data & 0x0F;
        break;

    case 0x4005:
        pulse2.sweep.enabled = (data & 0x80) != 0;
        pulse2.sweep.period = (data >> 4) & 0x07;
        pulse2.sweep.negate = (data & 0x08) != 0;
        pulse2.sweep.shift = data & 0x07;
        pulse2.sweep.reload = true;
        break;

    case 0x4006:
        pulse2.timer_reload = (pulse2.timer_reload & 0xFF00) | data;
        break;

    case 0x4007:
        pulse2.timer_reload = ((uint16_t)(data & 0x07) << 8) | (pulse2.timer_reload & 0x00FF);
        pulse2.timer = pulse2.timer_reload;
        if (pulse2_enabled) pulse2.length_counter = length_table[(data & 0xF8) >> 3];
        pulse2.pulse_phase = 0;
        pulse2.env.start = true;
        break;

    case 0x4008:
        triangle.control_flag = (data & 0x80) != 0;
        triangle.linear_reload_value = data & 0x7F;
        break;

    case 0x400A:
        triangle.timer_reload = (triangle.timer_reload & 0xFF00) | data;
        break;

    case 0x400B:
        triangle.timer_reload = ((uint16_t)(data & 0x07) << 8) | (triangle.timer_reload & 0x00FF);
        triangle.timer = triangle.timer_reload;
        if (triangle_enabled) triangle.length_counter = length_table[(data & 0xF8) >> 3];
        triangle.linear_reload_flag = true;
        break;

    case 0x400C:
        noise.env.loop = (data & 0x20) != 0;
        noise.env.constant_volume = (data & 0x10) != 0;
        noise.env.volume = data & 0x0F;
        break;

    case 0x400E:
        noise.mode = (data & 0x80) != 0;
        noise.timer_reload = noise_period_table[data & 0x0F];
        break;

    case 0x400F:
    if (noise_enabled) noise.length_counter = length_table[(data & 0xF8) >> 3];
    noise.env.start = true;
    break;

    case 0x4010:
        dmc.irq_enable = (data & 0x80) != 0;
        dmc.loop = (data & 0x40) != 0;
        dmc.rate_index = data & 0x0F;
        dmc.timer_reload = dmc_rate_table[dmc.rate_index];
        break;

    case 0x4011:
        dmc.output_level = data & 0x7F;
        break;

    case 0x4012:
        dmc.sample_address = 0xC000 | (data << 6);
        break;

    case 0x4013:
        dmc.sample_length = (data << 4) + 1;
        break;

    case 0x4015:
        pulse1_enabled = (data & 0x01) != 0;
        pulse2_enabled = (data & 0x02) != 0;
        triangle_enabled = (data & 0x04) != 0;
        noise_enabled = (data & 0x08) != 0;
        dmc_enabled = (data & 0x10) != 0;

        if (!pulse1_enabled)   pulse1.length_counter = 0;
        if (!pulse2_enabled)   pulse2.length_counter = 0;
        if (!triangle_enabled) triangle.length_counter = 0;
        if (!noise_enabled)    noise.length_counter = 0;

        if (dmc_enabled) {
            if (dmc.bytes_remaining == 0) {
                dmc.current_address = dmc.sample_address;
                dmc.bytes_remaining = dmc.sample_length;
                dmc.bits_remaining = 0;
                dmc.timer = dmc.timer_reload;
            }
        }
        else {
            dmc.bytes_remaining = 0;
            dmc.bits_remaining = 0;
        }
        break;

    case 0x4017:
        frame_seq_count = 0;
        step_mode = 0;
        use_5step_mode = (data & 0x80) != 0;

        // Ghi $4017 ngay lập tức fire quarter+half nếu 5-step
        if (use_5step_mode) {
            pulse1.env.Clock();
            pulse2.env.Clock();
            noise.env.Clock();

            if (triangle.linear_reload_flag) triangle.linear_counter = triangle.linear_reload_value;
            else if (triangle.linear_counter > 0) triangle.linear_counter--;
            if (!triangle.control_flag) triangle.linear_reload_flag = false;

            if (!pulse1.env.loop && pulse1.length_counter > 0) pulse1.length_counter--;
            if (!pulse2.env.loop && pulse2.length_counter > 0) pulse2.length_counter--;
            if (!noise.env.loop && noise.length_counter > 0) noise.length_counter--;
            if (!triangle.control_flag && triangle.length_counter > 0) triangle.length_counter--;
        }
        break;
    }
}

// --- BỘ MÁY CHẠY NHỊP APU ---
void APU::Step() {

    // =========================
    // 1) Pulse timer: đếm mỗi 2 CPU cycles
    //    Step() gọi mỗi CPU cycle → dùng half-clock toggle
    // =========================
    apu_half_clock = !apu_half_clock;

    if (apu_half_clock) {
        if (pulse1_enabled) {
            if (pulse1.timer > 0) pulse1.timer--;
            else {
                pulse1.timer = pulse1.timer_reload;
                pulse1.pulse_phase = (pulse1.pulse_phase + 1) % 8;
            }
        }
        if (pulse2_enabled) {
            if (pulse2.timer > 0) pulse2.timer--;
            else {
                pulse2.timer = pulse2.timer_reload;
                pulse2.pulse_phase = (pulse2.pulse_phase + 1) % 8;
            }
        }
    }

    // =========================
    // 2) Triangle: đếm mỗi CPU cycle
    //    timer_reload < 2: không advance phase (ultrasonic) nhưng không mute cứng
    // =========================
    if (triangle_enabled &&
        triangle.length_counter > 0 &&
        triangle.linear_counter > 0)
    {
        if (triangle.timer > 0) {
            triangle.timer--;
        }
        else {
            triangle.timer = triangle.timer_reload;
            // Chỉ advance phase khi timer hợp lệ — tránh "buzzing" siêu âm
            if (triangle.timer_reload >= 2) {
                triangle.tri_phase = (triangle.tri_phase + 1) & 31;
            }
        }
    }

    // =========================
    // 3) Noise: đếm mỗi CPU cycle
    // =========================
    if (noise_enabled) {
        if (noise.timer > 0) noise.timer--;
        else {
            noise.timer = noise.timer_reload;
            int      bit_to_mux = noise.mode ? 6 : 1;
            uint16_t feedback = (noise.shift_register & 0x01) ^
                ((noise.shift_register >> bit_to_mux) & 0x01);
            noise.shift_register = (noise.shift_register >> 1) | (feedback << 14);
        }
    }

    // =========================
    // 4) DMC: đếm mỗi CPU cycle
    // =========================
    if (dmc_enabled) {
        if (dmc.timer > 0) dmc.timer--;
        else {
            dmc.timer = dmc.timer_reload;

            if (dmc.bits_remaining > 0) {
                if (dmc.shift_register & 1) {
                    if (dmc.output_level <= 125) dmc.output_level += 2;
                }
                else {
                    if (dmc.output_level >= 2)  dmc.output_level -= 2;
                }
                dmc.shift_register >>= 1;
                dmc.bits_remaining--;
            }

            if (dmc.bits_remaining == 0 && dmc.bytes_remaining > 0) {
                dmc.shift_register = cpuRead(dmc.current_address);
                dmc.bits_remaining = 8;

                // FIX: wrap address đúng — uint16_t 0xFFFF+1 → 0x0000, phải về 0x8000
                if (dmc.current_address == 0xFFFF)
                    dmc.current_address = 0x8000;
                else
                    dmc.current_address++;

                dmc.bytes_remaining--;

                if (dmc.bytes_remaining == 0 && dmc.loop) {
                    dmc.current_address = dmc.sample_address;
                    dmc.bytes_remaining = dmc.sample_length;
                }
            }
        }
    }

    // =========================
    // 5) Frame Sequencer ~240Hz
    //    4-step : quarter tại 7457,14913,22371,29828  | half tại 14913,29828
    //    5-step : quarter tại 7457,14913,22371,37281  | half tại 14913,37281
    // =========================
    frame_seq_count++;

    {
        bool do_quarter = false;
        bool do_half = false;

        if (!use_5step_mode) {
            if (frame_seq_count == 7457) { do_quarter = true; }
            if (frame_seq_count == 14913) { do_quarter = true; do_half = true; }
            if (frame_seq_count == 22371) { do_quarter = true; }
            if (frame_seq_count == 29828) { do_quarter = true; do_half = true; frame_seq_count = 0; }
        }
        else {
            if (frame_seq_count == 7457) { do_quarter = true; }
            if (frame_seq_count == 14913) { do_quarter = true; do_half = true; }
            if (frame_seq_count == 22371) { do_quarter = true; }
            // step 4 (29829): im lặng
            if (frame_seq_count == 37281) { do_quarter = true; do_half = true; frame_seq_count = 0; }
        }

        if (do_quarter) {
            pulse1.env.Clock();
            pulse2.env.Clock();
            noise.env.Clock();

            if (triangle.linear_reload_flag)
                triangle.linear_counter = triangle.linear_reload_value;
            else if (triangle.linear_counter > 0)
                triangle.linear_counter--;
            if (!triangle.control_flag)
                triangle.linear_reload_flag = false;
        }

        if (do_half) {
            // Length counters
            if (!pulse1.env.loop && pulse1.length_counter > 0) pulse1.length_counter--;
            if (!pulse2.env.loop && pulse2.length_counter > 0) pulse2.length_counter--;
            if (!noise.env.loop && noise.length_counter > 0) noise.length_counter--;
            if (!triangle.control_flag && triangle.length_counter > 0) triangle.length_counter--;

            // Sweep Pulse 1 (one's complement negate)
            if (pulse1.sweep.reload) {
                pulse1.sweep.timer = pulse1.sweep.period;
                pulse1.sweep.reload = false;
            }
            else if (pulse1.sweep.timer > 0) {
                pulse1.sweep.timer--;
            }
            else {
                pulse1.sweep.timer = pulse1.sweep.period;
                if (pulse1.sweep.enabled && pulse1.sweep.shift > 0 && pulse1.length_counter > 0) {
                    uint16_t change = pulse1.timer_reload >> pulse1.sweep.shift;
                    uint16_t target = pulse1.sweep.negate
                        ? (pulse1.timer_reload - change - 1)   // pulse1: one's complement
                        : (pulse1.timer_reload + change);
                    if (target >= 8 && target <= 0x7FF)
                        pulse1.timer_reload = target;
                }
            }

            // Sweep Pulse 2 (two's complement negate)
            if (pulse2.sweep.reload) {
                pulse2.sweep.timer = pulse2.sweep.period;
                pulse2.sweep.reload = false;
            }
            else if (pulse2.sweep.timer > 0) {
                pulse2.sweep.timer--;
            }
            else {
                pulse2.sweep.timer = pulse2.sweep.period;
                if (pulse2.sweep.enabled && pulse2.sweep.shift > 0 && pulse2.length_counter > 0) {
                    uint16_t change = pulse2.timer_reload >> pulse2.sweep.shift;
                    uint16_t target = pulse2.sweep.negate
                        ? (pulse2.timer_reload - change)        // pulse2: two's complement
                        : (pulse2.timer_reload + change);
                    if (target >= 8 && target <= 0x7FF)
                        pulse2.timer_reload = target;
                }
            }
        }
    }
}

// --- MIXER ---
float APU::GetOutputSample() {
    // --- Sweep mute check ---
    // Pulse 1: one's complement negate
    bool p1_muted = (pulse1.timer_reload < 8);
    if (!p1_muted) {
        int change = (pulse1.sweep.shift == 0) ? 0 : (int)(pulse1.timer_reload >> pulse1.sweep.shift);
        int target = pulse1.sweep.negate ? ((int)pulse1.timer_reload - change - 1)
            : ((int)pulse1.timer_reload + change);
        if (target > 0x7FF || target < 0) p1_muted = true;
    }

    // Pulse 2: two's complement negate
    bool p2_muted = (pulse2.timer_reload < 8);
    if (!p2_muted) {
        int change = (pulse2.sweep.shift == 0) ? 0 : (int)(pulse2.timer_reload >> pulse2.sweep.shift);
        int target = pulse2.sweep.negate ? ((int)pulse2.timer_reload - change)
            : ((int)pulse2.timer_reload + change);
        if (target > 0x7FF || target < 0) p2_muted = true;
    }

    // --- Pulse ---
    float p1_raw = 0.0f;
    float p2_raw = 0.0f;

    if (pulse1_enabled && !p1_muted && pulse1.length_counter > 0 &&
        duty_table[pulse1.duty][pulse1.pulse_phase])
        p1_raw = (float)pulse1.env.Output();

    if (pulse2_enabled && !p2_muted && pulse2.length_counter > 0 &&
        duty_table[pulse2.duty][pulse2.pulse_phase])
        p2_raw = (float)pulse2.env.Output();

   
    float tri_raw = 0.0f;
    if (triangle_enabled) {
        static const float tri_table[32] = {
            15,14,13,12,11,10,9,8,
             7, 6, 5, 4, 3, 2,1,0,
             0, 1, 2, 3, 4, 5,6,7,
             8, 9,10,11,12,13,14,15
        };
        tri_raw = tri_table[triangle.tri_phase];
    }

    // --- Noise ---
    float n_raw = 0.0f;
    if (noise_enabled && noise.length_counter > 0 && !(noise.shift_register & 0x01))
        n_raw = (float)noise.env.Output();

    // --- DMC ---
    float dmc_raw = (float)dmc.output_level;
    float pulse_out = 0.0f;
    if ((p1_raw + p2_raw) > 0.0f)
        pulse_out = 95.88f / ((8128.0f / (p1_raw + p2_raw)) + 100.0f);

    float tnd_sum = (tri_raw / 8227.0f) + (n_raw / 12241.0f) + (dmc_raw / 22638.0f);
    float tnd_out = 0.0f;
    if (tnd_sum > 0.0f)
        tnd_out = 159.79f / ((1.0f / tnd_sum) + 100.0f);

    float output = pulse_out + tnd_out;

    // High-pass 90Hz
    hp1 = 0.996f * (hp1 + output - prev_in1);
    prev_in1 = output;

    // High-pass 440Hz
    hp2 = 0.990f * (hp2 + hp1 - prev_in2);
    prev_in2 = hp1;

    // Low-pass 14kHz
    lp += 0.899f * (hp2 - lp);

    // Không nhân thêm hệ số – tránh clipping
    return lp;
}

// --- DMC đọc sample từ BUS ---
uint8_t APU::cpuRead(uint16_t addr) {
    if (bus != nullptr)
        return bus->cpuRead(addr);
    return 0;
}