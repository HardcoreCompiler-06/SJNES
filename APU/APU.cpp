#include "APU.h"
#include "Bus.h"

APU::APU() {
    noise.shift_register = 1;
    dmc.reader = [&](uint16_t addr) -> uint8_t {
        return bus ? bus->cpuRead(addr) : 0;
        };
}

void APU::cpuWrite(uint16_t addr, uint8_t data) {
    switch (addr) {
    case 0x4000: f1.WriteReg(0, data); break;
    case 0x4001: f1.WriteReg(1, data); break;
    case 0x4002: f1.WriteReg(2, data); break;
    case 0x4003: f1.WriteReg(3, data); break;
    case 0x4004: f2.WriteReg(0, data); break;
    case 0x4005: f2.WriteReg(1, data); break;
    case 0x4006: f2.WriteReg(2, data); break;
    case 0x4007: f2.WriteReg(3, data); break;
    case 0x4008: tri.WriteReg(0, data); break;
    case 0x400A: tri.WriteReg(1, data); break;
    case 0x400B: tri.WriteReg(2, data); break;
    case 0x400C: noise.WriteReg(0, data); break;
    case 0x400E: noise.WriteReg(1, data); break;
    case 0x400F: noise.WriteReg(2, data); break;
    case 0x4010: dmc.WriteReg(0, data); break;
    case 0x4011: dmc.WriteReg(1, data); break;
    case 0x4012: dmc.WriteReg(2, data); break;
    case 0x4013: dmc.WriteReg(3, data); break;
    case 0x4015:
        f1.enabled = (data & 0x01) != 0;
        f2.enabled = (data & 0x02) != 0;
        tri.enabled = (data & 0x04) != 0;
        noise.enabled = (data & 0x08) != 0;
        if (!f1.enabled)    f1.length_counter = 0;
        if (!f2.enabled)    f2.length_counter = 0;
        if (!tri.enabled)   tri.length_counter = 0;
        if (!noise.enabled) noise.length_counter = 0;
        dmc.SetEnabled((data & 0x10) != 0);
        break;
    case 0x4017:
        frame_seq_count = 0;
        use_5step_mode = (data & 0x80) != 0;
        if (use_5step_mode) {
            f1.env.Clock(); f2.env.Clock(); noise.TickEnvelope();
            tri.TickLinearCounter();
            f1.TickLengthAndSweep();
            f2.TickLengthAndSweep();
            tri.TickLengthCounter();
            noise.TickLengthCounter();
        }
        break;
    }
}

uint8_t APU::cpuRead(uint16_t addr) {
    return bus ? bus->cpuRead(addr) : 0;
}

void APU::Step() {
    apu_half_clock = !apu_half_clock;
    if (apu_half_clock) { f1.TickTimer(); f2.TickTimer(); }

    tri.TickTimer();
    noise.TickTimer();
    dmc.TickTimer();

    frame_seq_count++;
    {
        bool qf = false, hf = false;
        if (!use_5step_mode) {
            if (frame_seq_count == 7457) { qf = true; }
            if (frame_seq_count == 14913) { qf = true; hf = true; }
            if (frame_seq_count == 22371) { qf = true; }
            if (frame_seq_count == 29828) { qf = true; hf = true; frame_seq_count = 0; }
        }
        else {
            if (frame_seq_count == 7457) { qf = true; }
            if (frame_seq_count == 14913) { qf = true; hf = true; }
            if (frame_seq_count == 22371) { qf = true; }
            if (frame_seq_count == 37281) { qf = true; hf = true; frame_seq_count = 0; }
        }
        if (qf) {
            f1.env.Clock();
            f2.env.Clock();
            noise.TickEnvelope();
            tri.TickLinearCounter();
        }
        if (hf) {
            f1.TickLengthAndSweep();
            f2.TickLengthAndSweep();
            tri.TickLengthCounter();
            noise.TickLengthCounter();
        }
    }
}

float APU::GetOutputSample() {
    float left, right;
    GetOutputSampleStereo(left, right);
    return (left + right) * 0.5f;
}

void APU::GetOutputSampleStereo(float& left, float& right) {
    float p1 = f1.Output();
    float p2 = f2.Output();
    float t = tri.Output();
    float n = noise.Output();
    float d = dmc.Output();

    // TND filter qua mono filter chung (hp1/hp2/lp) tránh lệch pha L/R
    float tnd = (t / 8227.0f) + (n / 12241.0f) + (d / 22638.0f);
    float tnd_out = 0.0f;
    if (tnd > 0.0f)
        tnd_out = 159.79f / ((1.0f / tnd) + 100.0f);

    // Filter TND qua mono filter
    hp1 = 0.996f * (hp1 + tnd_out - prev_in1); prev_in1 = tnd_out;
    hp2 = 0.990f * (hp2 + hp1 - prev_in2);     prev_in2 = hp1;
    lp += 0.899f * (hp2 - lp);
    float tnd_filtered = lp;

    // Pulse L/R qua filter riêng
    float outL = (p1 > 0.0f ? 95.88f / ((8128.0f / (p1 * 1.5f)) + 100.0f) : 0.0f);
    float outR = (p2 > 0.0f ? 95.88f / ((8128.0f / (p2 * 1.5f)) + 100.0f) : 0.0f);

    hp1L = 0.996f * (hp1L + outL - prev_in1L); prev_in1L = outL;
    hp2L = 0.990f * (hp2L + hp1L - prev_in2L); prev_in2L = hp1L;
    lpL += 0.899f * (hp2L - lpL);

    hp1R = 0.996f * (hp1R + outR - prev_in1R); prev_in1R = outR;
    hp2R = 0.990f * (hp2R + hp1R - prev_in2R); prev_in2R = hp1R;
    lpR += 0.899f * (hp2R - lpR);

    // Cộng TND mono vào cả 2 kênh
    left = lpL + tnd_filtered;
    right = lpR + tnd_filtered;
}