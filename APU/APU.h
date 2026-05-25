#pragma once
#include <cstdint>
#include <vector>
#include "F1.h"
#include "F2.h"
#include "Triangle.h"
#include "DMC.h"
#include "Noise.h"

class Bus;

class APU {
public:
    APU();
    ~APU() = default;

    Bus* bus = nullptr;

    double             sample_counter = 0.0;
    double             cycles_per_sample = 1789773.0 / 44100.0;
    std::vector<float> audio_buffer;

    void    cpuWrite(uint16_t addr, uint8_t data);
    uint8_t cpuRead(uint16_t addr);
    void    Step();
    float   GetOutputSample();
    void    GetOutputSampleStereo(float& left, float& right);
    bool mutePulse1 = false;
    bool mutePulse2 = false;
    bool muteTriangle = false;
    bool muteNoise = false;
    bool muteDMC = false;
    uint8_t readStatus() {
        uint8_t s = 0;
        if (f1.length_counter > 0) s |= 0x01;
        if (f2.length_counter > 0) s |= 0x02;
        if (tri.length_counter > 0) s |= 0x04;
        if (noise.length_counter > 0) s |= 0x08;
        if (dmc.bytes_remaining > 0) s |= 0x10;
        return s;
    }

private:
    F1       f1;
    F2       f2;
    Triangle tri;
    DMC      dmc;
    Noise    noise;

    // Frame Sequencer
    int  frame_seq_count = 0;
    bool use_5step_mode = false;
    bool apu_half_clock = false;

    // Filters - mono
    float hp1 = 0.0f, prev_in1 = 0.0f;
    float hp2 = 0.0f, prev_in2 = 0.0f;
    float lp = 0.0f;

    // Filters - stereo (L = F1+tri+noise+dmc, R = F2+tri+noise+dmc)
    float hp1L = 0.0f, prev_in1L = 0.0f;
    float hp2L = 0.0f, prev_in2L = 0.0f;
    float lpL = 0.0f;
    float hp1R = 0.0f, prev_in1R = 0.0f;
    float hp2R = 0.0f, prev_in2R = 0.0f;
    float lpR = 0.0f;
};