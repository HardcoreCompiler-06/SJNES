#include "APU.h"
#include "Bus.h"
#include "Mapper_024.h"
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
    float p1 = mutePulse1 ? 0.0f : f1.Output();
    float p2 = mutePulse2 ? 0.0f : f2.Output();
    float t = muteTriangle ? 0.0f : tri.Output();
    float n = muteNoise ? 0.0f : noise.Output();
    float d = muteDMC ? 0.0f : dmc.Output();

    // 1. Xử lý nhóm TND (Mono ở giữa)
    float tnd_sum = (t / 8227.0f) + (n / 12241.0f) + (d / 22638.0f);
    float tnd_out = 0.0f;
    if (tnd_sum > 0.0f) {
        tnd_out = 159.79f / ((1.0f / tnd_sum) + 100.0f);
    }
    const float NES_TND_BOOST = 2.0f;
    tnd_out *= NES_TND_BOOST;
    
    float pulseL_sum = (p1 * 1.50f) + (p2 * 0.50f);
    float pulseR_sum = (p1 * 0.50f) + (p2 * 1.50f);

    float pulse_out_L = 0.0f;
    if (pulseL_sum > 0.0f) {
        pulse_out_L = 95.88f / ((8128.0f / pulseL_sum) + 100.0f);
    }

    float pulse_out_R = 0.0f;
    if (pulseR_sum > 0.0f) {
        pulse_out_R = 95.88f / ((8128.0f / pulseR_sum) + 100.0f);
    }
    const float NES_PULSE_BOOST = 2.0f;
    pulse_out_L *= NES_PULSE_BOOST;
    pulse_out_R *= NES_PULSE_BOOST;
    float mix_L = pulse_out_L + tnd_out;
    float mix_R = pulse_out_R + tnd_out;

    // 4. Đưa qua hệ thống lọc (Filter) kép
    // Kênh Trái
    hp1L = 0.996f * (hp1L + mix_L - prev_in1L); prev_in1L = mix_L;
    hp2L = 0.990f * (hp2L + hp1L - prev_in2L);  prev_in2L = hp1L;
    lpL += 0.899f * (hp2L - lpL);

    // Kênh Phải
    hp1R = 0.996f * (hp1R + mix_R - prev_in1R); prev_in1R = mix_R;
    hp2R = 0.990f * (hp2R + hp1R - prev_in2R);  prev_in2R = hp1R;
    lpR += 0.899f * (hp2R - lpR);

    // Xuất ra loa
    left = lpL;
    right = lpR;
}
AudioDebugChannels APU::GetDebugChannels()
{
    AudioDebugChannels ch;

    float p1 = f1.Output();
    float p2 = f2.Output();
    float n = noise.Output();
    float d = dmc.Output();

    ch.pulse1 = mutePulse1 ? 0.0f : ((p1 / 15.0f) * 2.0f - 1.0f);
    ch.pulse2 = mutePulse2 ? 0.0f : ((p2 / 15.0f) * 2.0f - 1.0f);
    ch.triangle = muteTriangle ? 0.0f : tri.DebugOutput();
    ch.noise = muteNoise ? 0.0f : ((n / 15.0f) * 2.0f - 1.0f);
    ch.dmc = muteDMC ? 0.0f : ((d / 127.0f) * 2.0f - 1.0f);

    // Period hint theo đơn vị sample audio. AudioWaveWindow dùng cái này để
    // khóa pha theo timer thật, không đo chu kỳ từ hình sóng nữa.
    constexpr float CPU_TO_SAMPLE = 44100.0f / 1789773.0f;
    auto clampPeriod = [](float v) -> float {
        if (v < 2.0f) return 0.0f;
        if (v > 8192.0f) return 8192.0f;
        return v;
        };

    // Pulse: timer clock mỗi 2 CPU cycle, 8 bước duty / 1 chu kỳ.
    ch.pulse1Period = (!mutePulse1 && f1.enabled && !f1.IsMuted())
        ? clampPeriod(float(f1.timer_reload + 1) * 16.0f * CPU_TO_SAMPLE)
        : 0.0f;

    ch.pulse2Period = (!mutePulse2 && f2.enabled && !f2.IsMuted())
        ? clampPeriod(float(f2.timer_reload + 1) * 16.0f * CPU_TO_SAMPLE)
        : 0.0f;

    // Triangle: timer clock mỗi CPU cycle, 32 bước / 1 chu kỳ.
    ch.trianglePeriod = (!muteTriangle && tri.enabled && tri.length_counter > 0 && tri.linear_counter > 0)
        ? clampPeriod(float(tri.timer_reload + 1) * 32.0f * CPU_TO_SAMPLE)
        : 0.0f;

    // Noise/DMC không có waveform chu kỳ ổn định như pulse/triangle, vẫn để hint mềm.
    ch.noisePeriod = (!muteNoise && noise.enabled)
        ? clampPeriod(float(noise.timer_reload + 1) * CPU_TO_SAMPLE)
        : 0.0f;

    ch.dmcPeriod = (!muteDMC && dmc.enabled)
        ? clampPeriod(float(dmc.timer_reload + 1) * 8.0f * CPU_TO_SAMPLE)
        : 0.0f;

    // Chip-shape hint cho waveform debug:
    // duty/phase lấy trực tiếp từ chip state, tránh phải đo duty bằng zero-crossing.
    ch.pulse1Duty = (!mutePulse1 && f1.enabled && !f1.IsMuted() && f1.length_counter > 0)
        ? float(f1.duty)
        : -1.0f;

    ch.pulse2Duty = (!mutePulse2 && f2.enabled && !f2.IsMuted() && f2.length_counter > 0)
        ? float(f2.duty)
        : -1.0f;

    ch.trianglePhase = (!muteTriangle && tri.enabled && tri.length_counter > 0 && tri.linear_counter > 0)
        ? float(tri.tri_phase & 31)
        : -1.0f;

    return ch;
}

void APU::reset()
{
    // Tắt toàn bộ kênh qua $4015
    cpuWrite(0x4015, 0x00);

    // Reset frame sequencer
    frame_seq_count = 0;
    use_5step_mode = false;
    apu_half_clock = false;

    // Reset noise cơ bản
    noise.shift_register = 1;

    // Reset filter để hết tiếng ù/ì còn sót lại
    hp1 = 0.0f;
    hp2 = 0.0f;
    lp = 0.0f;
    prev_in1 = 0.0f;
    prev_in2 = 0.0f;

    hp1L = 0.0f;
    hp2L = 0.0f;
    lpL = 0.0f;
    prev_in1L = 0.0f;
    prev_in2L = 0.0f;

    hp1R = 0.0f;
    hp2R = 0.0f;
    lpR = 0.0f;
    prev_in1R = 0.0f;
    prev_in2R = 0.0f;
}
void APU::SetSmoothSaw(bool enable)
{
    smoothSawEnabled = enable;

    Mapper* activeMapper = mapper;

    if (!activeMapper && bus && bus->cart && bus->cart->pMapper)
        activeMapper = bus->cart->pMapper.get();

    auto vrc6 = dynamic_cast<Mapper_024*>(activeMapper);
    if (vrc6)
        vrc6->setSmoothSaw(enable);
}