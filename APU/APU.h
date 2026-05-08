#ifndef APU_H
#define APU_H

#include <cstdint>
#include <vector>
class Bus;

class APU {
public:
    double sample_counter = 0.0;
    double cycles_per_sample = 1789773.0 / 44100.0;
    std::vector<float> audio_buffer;
    APU();
    ~APU();
    Bus* bus = nullptr;
    float filter_x = 0.0f;
    float filter_y = 0.0f;

    void cpuWrite(uint16_t addr, uint8_t data);
    uint8_t cpuRead(uint16_t addr);
    void Step();
    float GetOutputSample();
    uint8_t readStatus() {
        uint8_t status = 0x00;
        if (pulse1.length_counter > 0)   status |= 0x01;
        if (pulse2.length_counter > 0)   status |= 0x02;
        if (triangle.length_counter > 0) status |= 0x04;
        if (noise.length_counter > 0)    status |= 0x08;
        if (dmc.bytes_remaining > 0)     status |= 0x10;
        return status;
    }
private:
    float triangle_smooth = 0.0f;
    int  frame_seq_count = 0;
    int  step_mode = 0;
    bool use_5step_mode = false;
    bool apu_half_clock = false;

    // Filter state — member variables (không dùng static, reset sạch khi restart)
    float hp1 = 0.0f, prev_in1 = 0.0f;
    float hp2 = 0.0f, prev_in2 = 0.0f;
    float lp = 0.0f;
    bool pulse1_enabled = false;
    bool pulse2_enabled = false;
    bool triangle_enabled = false;
    bool noise_enabled = false;
    bool dmc_enabled = false;

    struct Envelope {
        bool start = false;
        bool loop = false;
        uint8_t volume = 0;
        uint8_t decay_level = 0;
        uint8_t timer = 0;
        bool constant_volume = false;

        void Clock() {
            if (start) {
                start = false;
                decay_level = 15;
                timer = volume;
            }
            else {
                if (timer > 0) {
                    timer--;
                }
                else {
                    timer = volume;
                    if (decay_level > 0) decay_level--;
                    else if (loop) decay_level = 15;
                }
            }
        }

        uint8_t Output() const {
            return constant_volume ? volume : decay_level;
        }
    };

    struct Sweep {
        bool enabled = false;
        uint8_t shift = 0;
        uint8_t timer = 0;
        uint8_t period = 0;
        bool negate = false;
        bool reload = false;
    };

    struct SquareWave {
        uint16_t timer = 0;
        uint16_t timer_reload = 0;
        uint8_t duty = 0;
        uint8_t pulse_phase = 0;
        uint8_t length_counter = 0;
        Envelope env;
        Sweep sweep;
    } pulse1, pulse2;

    struct TriangleWave {
        uint16_t timer = 0;
        uint16_t timer_reload = 0;
        uint8_t tri_phase = 0;
        uint8_t length_counter = 0;
        uint8_t linear_counter = 0;
        bool linear_reload_flag = false;
        bool control_flag = false;
        uint8_t linear_reload_value = 0;
    } triangle;

    struct NoiseWave {
        uint16_t timer = 0;
        uint16_t timer_reload = 0;
        uint16_t shift_register = 1;
        uint8_t length_counter = 0;
        bool mode = false;
        Envelope env;
    } noise;

    struct dmc_t {
        bool irq_enable = false;
        bool loop = false;
        uint8_t rate_index = 0;
        uint16_t timer_reload = 0;
        uint16_t timer = 0;

        uint8_t output_level = 0;
        uint16_t sample_address = 0x0000;
        uint16_t sample_length = 0;
        uint16_t current_address = 0;
        uint16_t bytes_remaining = 0;
        uint8_t shift_register = 0;
        uint8_t bits_remaining = 0;
    } dmc;
};

#endif