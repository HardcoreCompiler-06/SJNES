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

#include "APU.h"
#include "Bus.h"
#include "Mapper_024.h"
#include "Mapper_069.h"
#include "Mapper_005.h"
#include "Mapper_019.h"
#include "Mapper_085.h"
#include "Mapper_020.h"
#include <algorithm>
#include <cmath>
APU::APU() {
    noise.shift_register = 1;
    dmc.reader = [&](uint16_t addr) -> uint8_t {
        return bus ? bus->cpuRead(addr) : 0;
        };
}

void APU::SyncDmcToBus()
{
    // DMC IRQ là đường IRQ MỨC riêng (IRQ_DMC), tách khỏi frame IRQ (IRQ_APU).
    if (dmc.irq_flag != dmc_irq_line) {
        dmc_irq_line = dmc.irq_flag;
        if (bus && bus->cpu) {
            if (dmc_irq_line) bus->cpu->SetIrqSource(CPU6502::IRQ_DMC);
            else              bus->cpu->ClearIrqSource(CPU6502::IRQ_DMC);
        }
    }

    // Mỗi lần reader nạp 1 byte sample, CPU bị dừng vài cycle (EmuWorker đọc Bus::dmc_stall_cycles).
    // NSF không chạy qua vòng lặp đó nên bỏ qua để không tích lũy vô hạn.
    if (dmc.dmaStallCycles > 0) {
        if (bus && !bus->nsfMode)
            bus->dmc_stall_cycles = std::min(bus->dmc_stall_cycles + dmc.dmaStallCycles, 64);
        dmc.dmaStallCycles = 0;
    }
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
    case 0x4010: dmc.WriteReg(0, data); SyncDmcToBus(); break;
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
        SyncDmcToBus();
        break;
    case 0x4017:
        // KHÔNG apply ngay ở đây. Hardware thật trễ việc reset frame sequencer
        // (và clock ngay các unit nếu bit7=1) 3 CPU cycle (write ở cycle chẵn)
        // hoặc 4 CPU cycle (write ở cycle lẻ) sau khi lệnh ghi này thực thi.
        // Việc apply thật sự nằm trong Step(), xem pending_4017_delay.
        pending_4017_value = data;
        pending_4017_delay = (total_cpu_cycle_count % 2 == 0) ? 3 : 4;

        // Bit 6 (IRQ inhibit) chỉ ảnh hưởng tới việc CÓ set IRQ trong tương lai
        // hay không, còn việc clear flag đang chờ CŨNG bị delay theo cùng cơ chế
        // trên hardware thật -> xử lý luôn trong khối pending ở Step().
        break;
    }
}

uint8_t APU::cpuRead(uint16_t addr) {
    return bus ? bus->cpuRead(addr) : 0;
}

void APU::Step() {
    total_cpu_cycle_count++;

    // Áp dụng write $4017 trễ (3-4 CPU cycle sau lệnh ghi, xem cpuWrite()).
    if (pending_4017_delay > 0) {
        pending_4017_delay--;
        if (pending_4017_delay == 0) {
            frame_seq_count = 0;
            use_5step_mode = (pending_4017_value & 0x80) != 0;
            irq_inhibit = (pending_4017_value & 0x40) != 0;

            // Chỉ clear frame IRQ flag khi bit 6 (inhibit) = 1.
            if (irq_inhibit) {
                frame_irq_flag = false;
                if (bus && bus->cpu) bus->cpu->ClearIrqSource(CPU6502::IRQ_APU);
            }

            if (use_5step_mode) {
                f1.env.Clock(); f2.env.Clock(); noise.TickEnvelope();
                tri.TickLinearCounter();
                f1.TickLengthAndSweep();
                f2.TickLengthAndSweep();
                tri.TickLengthCounter();
                noise.TickLengthCounter();
            }
            pending_4017_delay = -1;
        }
    }

    apu_half_clock = !apu_half_clock;
    if (apu_half_clock) { f1.TickTimer(); f2.TickTimer(); }

    tri.TickTimer();
    noise.TickTimer();
    dmc.TickTimer();
    SyncDmcToBus();

    frame_seq_count++;
    {
        bool qf = false, hf = false;
        if (!use_5step_mode) {
            if (frame_seq_count == 7457) { qf = true; }
            if (frame_seq_count == 14913) { qf = true; hf = true; }
            if (frame_seq_count == 22371) { qf = true; }
            // Mốc 29828: CHỈ set cờ IRQ, KHÔNG clock length/envelope ở đây -
            // đây là điểm khác biệt tinh vi so với 3 mốc trước (nơi set cờ
            // và clock luôn đi cùng nhau). Xem NESdev wiki Talk:APU Frame
            // Counter - IRQ set tại cycle 29828, còn length/envelope clock
            // (mốc "step 4" thực sự) lại xảy ra ở cycle 29829, TRỄ hơn 1
            // cycle so với lúc IRQ được set.
            if (frame_seq_count == 29828) {
                if (!irq_inhibit)
                {
                    frame_irq_flag = true;
                    if (bus && bus->cpu) bus->cpu->SetIrqSource(CPU6502::IRQ_APU);
                }
            }
            // Mốc 29829: clock length/envelope/sweep thật sự (tương đương
            // "step 4"), tách biệt khỏi việc set IRQ ở trên.
            if (frame_seq_count == 29829) {
                qf = true; hf = true;
            }
            // Mốc 29830: điểm wrap về 0, không clock gì thêm - tổng độ dài
            // 1 chu kỳ 4-step là 29830 cycle, không phải 29829.
            if (frame_seq_count == 29830) {
                frame_seq_count = 0;
            }
        }
        else {
            if (frame_seq_count == 7457) { qf = true; }
            if (frame_seq_count == 14913) { qf = true; hf = true; }
            if (frame_seq_count == 22371) { qf = true; }
            // Mốc 29829 của mode 1 (5-step) CỐ Ý im lặng (không clock gì cả,
            // không set IRQ) - khớp đúng thiết kế "bước rỗng" của 5-step mode,
            // không cần code riêng vì không có nhánh nào khớp là đủ.
            if (frame_seq_count == 37281) { qf = true; hf = true; }
            // Giống mode 0: điểm wrap tách riêng, tổng độ dài chu kỳ 5-step
            // là 37282 cycle chứ không phải 37281.
            if (frame_seq_count == 37282) { frame_seq_count = 0; }
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
    // Sample rate thực tế đang chạy (suy ra từ cycles_per_sample thay vì
    // hardcode 44100, để chỉnh sửa vẫn đúng nếu sample rate đổi sau này).
    const double sampleRateHz = 1789773.0 / cycles_per_sample;

    // Pulse dùng band-limited synthesis (polyBLEP) để triệt alias ở note cao —
    // đây là nguồn nhiễu số lớn nhất trong toàn bộ APU khi sinh sóng vuông
    // "naive" trực tiếp từ bảng duty. f1.Output()/f2.Output() (giá trị digital
    // thô, cycle-accurate) vẫn được giữ nguyên và dùng cho GetDebugChannels()
    // để oscilloscope hiển thị đúng dạng sóng thật của hardware.
    float p1 = mutePulse1 ? 0.0f : f1.BandLimitedOutput(sampleRateHz) * gainPulse1;
    float p2 = mutePulse2 ? 0.0f : f2.BandLimitedOutput(sampleRateHz) * gainPulse2;
    float t = muteTriangle ? 0.0f : tri.Output() * gainTriangle;
    float n = muteNoise ? 0.0f : noise.Output() * gainNoise;
    float d = muteDMC ? 0.0f : dmc.Output() * gainDMC;

    // 1. Xử lý nhóm TND (Mono ở giữa)
    float tnd_sum = (t / 8227.0f) + (n / 12241.0f) + (d / 22638.0f);
    float tnd_out = 0.0f;
    if (tnd_sum > 0.0f) {
        tnd_out = 159.79f / ((1.0f / tnd_sum) + 100.0f);
    }
    const float NES_TND_BOOST = 1.0f;
    tnd_out *= NES_TND_BOOST;

    // Phần cứng NES: 2 kênh pulse cộng vào CÙNG một DAC 4-bit trước khi qua bảng
    // phi tuyến (nesdev, "APU Mixer") -> đầu ra pulse là MONO thật, không có pan.
    const float pulse_sum = p1 + p2;
    float pulse_out = 0.0f;
    if (pulse_sum > 0.0f) {
        pulse_out = 95.88f / ((8128.0f / pulse_sum) + 100.0f);
    }
    const float NES_PULSE_BOOST = 1.0f;
    pulse_out *= NES_PULSE_BOOST;
    float mix_L = pulse_out + tnd_out;
    float mix_R = pulse_out + tnd_out;


    left = mix_L;
    right = mix_R;
}

void APU::ApplyOutputFilterStereo(float& left, float& right)
{
    // cycles_per_sample là NGUỒN SỰ THẬT duy nhất về sample rate hiện tại
    // (EmuWorker set lại mỗi khi đổi 44.1/48/96kHz) — dùng đúng giá trị này,
    // KHÔNG hard-code 44100, để bộ lọc thật luôn đúng đáp ứng tần số dù
    // output rate là bao nhiêu.
    UpdateOutputFilterCoeffs(1789773.0 / cycles_per_sample);

    float in_L = left;
    float in_R = right;

    hp1L = outFilterA1 * (hp1L + in_L - prev_in1L); prev_in1L = in_L;
    float stageL = hp1L;
    if (filterTwoHighpass) {
        hp2L = outFilterA2 * (hp2L + hp1L - prev_in2L);  prev_in2L = hp1L;
        stageL = hp2L;
    }
    lpL += outFilterB * (stageL - lpL);

    hp1R = outFilterA1 * (hp1R + in_R - prev_in1R); prev_in1R = in_R;
    float stageR = hp1R;
    if (filterTwoHighpass) {
        hp2R = outFilterA2 * (hp2R + hp1R - prev_in2R);  prev_in2R = hp1R;
        stageR = hp2R;
    }
    lpR += outFilterB * (stageR - lpR);

    left = lpL;
    right = lpR;
}

float APU::ApplyOutputFilterMono(float sample)
{
    UpdateOutputFilterCoeffs(1789773.0 / cycles_per_sample);

    hp1 = outFilterA1 * (hp1 + sample - prev_in1); prev_in1 = sample;
    float stage = hp1;
    if (filterTwoHighpass) {
        hp2 = outFilterA2 * (hp2 + hp1 - prev_in2);  prev_in2 = hp1;
        stage = hp2;
    }
    lp += outFilterB * (stage - lp);
    return lp;
}

// Hệ số highpass 1 tầng theo đúng công thức bilinear cho mạch RC thật:
// a = RC / (RC + 1/fs), với RC = 1 / (2*pi*fc).
static float RcHighpassCoeff(double cutoffHz, double sampleRateHz)
{
    const double dt = 1.0 / sampleRateHz;
    const double rc = 1.0 / (2.0 * 3.14159265358979323846 * cutoffHz);
    return static_cast<float>(rc / (rc + dt));
}

// Hệ số lowpass 1 tầng: b = (1/fs) / (RC + 1/fs).
static float RcLowpassCoeff(double cutoffHz, double sampleRateHz)
{
    const double dt = 1.0 / sampleRateHz;
    const double rc = 1.0 / (2.0 * 3.14159265358979323846 * cutoffHz);
    return static_cast<float>(dt / (rc + dt));
}

void APU::UpdateOutputFilterCoeffs(double sampleRateHz)
{
    // Dong bo sample rate cho emu2413 (VRC7). Ham nay chay moi sample nen chi
    // goi SetSampleRate() (idempotent) - dung ca khi doi 44.1/48/96kHz lan khi
    // load ROM VRC7 moi.
    {
        Mapper* activeMapper = mapper;
        if (!activeMapper && bus && bus->cart && bus->cart->pMapper)
            activeMapper = bus->cart->pMapper.get();
        if (auto vrc7 = dynamic_cast<Mapper_085*>(activeMapper))
            vrc7->SetSampleRate(sampleRateHz);
        else if (auto n163 = dynamic_cast<Mapper_019*>(activeMapper))
            n163->SetSampleRate(sampleRateHz);
        else if (auto s5b = dynamic_cast<Mapper_069*>(activeMapper))
            s5b->SetSampleRate(sampleRateHz);
    }

    if (sampleRateHz == lastFilterSampleRate)
        return; // sample rate không đổi -> hệ số cũ vẫn đúng, khỏi tính lại

    lastFilterSampleRate = sampleRateHz;

    // Tần số cắt THẬT theo nesdev ("APU Mixer"):
    //  - NES front-loader: high-pass 90 Hz nối tiếp high-pass 440 Hz, rồi low-pass 14 kHz.
    //  - Famicom: CHỈ MỘT high-pass 37 Hz (không có tầng 2), rồi low-pass 14 kHz để làm
    //    mượt bậc DAC — mạch RF thật của Famicom không xác định nên không mô phỏng được.
    // Tính trực tiếp bằng công thức RC ở mọi sample rate, không bảng tra cố định,
    // nên 44.1/48/96kHz hay bất kỳ rate nào khác đều ra cùng một đáp ứng tần số.
    if (famicomFilter)
    {
        filterTwoHighpass = false;
        outFilterA1 = RcHighpassCoeff(37.0, sampleRateHz);
        outFilterA2 = 1.0f; // không dùng khi filterTwoHighpass == false
    }
    else
    {
        filterTwoHighpass = true;
        outFilterA1 = RcHighpassCoeff(90.0, sampleRateHz);
        outFilterA2 = RcHighpassCoeff(440.0, sampleRateHz);
    }
    outFilterB = RcLowpassCoeff(14000.0, sampleRateHz);
}
AudioDebugChannels APU::GetDebugChannels()
{
    AudioDebugChannels ch;

    float p1 = f1.Output();
    float p2 = f2.Output();
    float n = noise.Output();
    float d = dmc.Output();

    ch.pulse1 = mutePulse1 ? 0.0f : std::clamp(((p1 / 15.0f) * 2.0f - 1.0f) * gainPulse1, -1.0f, 1.0f);
    ch.pulse2 = mutePulse2 ? 0.0f : std::clamp(((p2 / 15.0f) * 2.0f - 1.0f) * gainPulse2, -1.0f, 1.0f);
    ch.triangle = muteTriangle ? 0.0f : std::clamp(tri.DebugOutput() * gainTriangle, -1.0f, 1.0f);
    ch.noise = muteNoise ? 0.0f : std::clamp(((n / 15.0f) * 2.0f - 1.0f) * gainNoise, -1.0f, 1.0f);
    ch.dmc = muteDMC ? 0.0f : std::clamp(((d / 127.0f) * 2.0f - 1.0f) * gainDMC, -1.0f, 1.0f);

    // Hệ số đổi CPU cycle -> sample theo sample rate THẬT (cycles_per_sample là nguồn sự thật).
    // Trước đây hard-code 44100: khi thiết bị chạy 48 kHz thì period pulse/triangle/noise trong
    // oscilloscope thấp hơn thật ~8% -> trigger/khung bị lệch và nhảy.
    const float CPU_TO_SAMPLE = (cycles_per_sample > 1.0)
        ? float(1.0 / cycles_per_sample)
        : 44100.0f / 1789773.0f;
    auto clampPeriod = [](float v) -> float {
        if (v < 2.0f) return 0.0f;
        if (v > 8192.0f) return 8192.0f;
        return v;
        };

    ch.pulse1Period = (!mutePulse1 && f1.enabled && !f1.IsMuted())
        ? clampPeriod(float(f1.timer_reload + 1) * 16.0f * CPU_TO_SAMPLE)
        : 0.0f;

    ch.pulse2Period = (!mutePulse2 && f2.enabled && !f2.IsMuted())
        ? clampPeriod(float(f2.timer_reload + 1) * 16.0f * CPU_TO_SAMPLE)
        : 0.0f;

    ch.trianglePeriod = (!muteTriangle && tri.enabled && tri.length_counter > 0 && tri.linear_counter > 0)
        ? clampPeriod(float(tri.timer_reload + 1) * 32.0f * CPU_TO_SAMPLE)
        : 0.0f;

    ch.noisePeriod = (!muteNoise && noise.enabled)
        ? clampPeriod(float(noise.timer_reload + 1) * CPU_TO_SAMPLE)
        : 0.0f;

    // QUAN TRỌNG: đây phải là chu kỳ LẶP THẬT của sample (khi $4010 bit6=loop bật),
    // KHÔNG PHẢI thời gian phát 1 byte. dmc.dbgLoopPeriodCycles được DMC.h tự đo bằng
    // cách đếm cycle giữa 2 lần Restart() do loop gây ra — chính xác tuyệt đối, không đoán.
    // Sample one-shot (không loop) thì không có gì để "khoá" -> trả về 0, oscilloscope
    // sẽ tự fallback sang cross-correlation guess như trước.
    // Đổi cycle -> sample bằng cycles_per_sample THỰC TẾ (nguồn sự thật về sample rate),
    // không dùng hằng 44100 cứng: nếu thiết bị âm thanh chạy 48 kHz thì hằng cứng sẽ
    // làm period/tuổi sai ~8% và khóa pha trôi.
    const bool dmcLoopOk = !muteDMC && dmc.enabled && dmc.loop && dmc.dbgHasLoopPeriod;
    const double dmcCycToSample = (cycles_per_sample > 1.0) ? (1.0 / cycles_per_sample)
        : double(CPU_TO_SAMPLE);
    ch.dmcPeriod = dmcLoopOk
        ? clampPeriod(float(double(dmc.dbgLoopPeriodCycles) * dmcCycToSample))
        : 0.0f;
    ch.dmcAgeSamples = (dmcLoopOk && dmc.dbgCycleCounter >= dmc.dbgLastLoopRestartCycle)
        ? double(dmc.dbgCycleCounter - dmc.dbgLastLoopRestartCycle) * dmcCycToSample
        : -1.0;

    // Mốc Restart() tuyệt đối, quy đổi bằng CÙNG hệ số CPU_TO_SAMPLE ở trên, để
    // AudioWaveWindow có thể khoá pha bằng modulo trực tiếp (fmod(nowAbs - dmcRestartAbs,
    // dmcPeriod)) thay vì phải dò cạnh trên waveform DPCM. Tính bằng double vì
    // dbgLastLoopRestartCycle là cycle-count tuyệt đối kể từ lúc APU khởi tạo,
    // có thể rất lớn sau phiên chơi dài.
    ch.dmcRestartAbs = (!muteDMC && dmc.enabled && dmc.loop && dmc.dbgHasLoopPeriod)
        ? double(dmc.dbgLastLoopRestartCycle) * double(CPU_TO_SAMPLE)
        : -1.0;

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
    dmc.Reset();
    dmc_irq_line = false;
    if (bus) {
        bus->dmc_stall_cycles = 0;
        if (bus->cpu) bus->cpu->ClearIrqSource(CPU6502::IRQ_DMC);
    }

    // Reset frame sequencer
    frame_seq_count = 0;
    use_5step_mode = false;
    apu_half_clock = false;
    frame_irq_flag = false;
    irq_inhibit = false;
    pending_4017_delay = -1;
    pending_4017_value = 0;
    total_cpu_cycle_count = 0;

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
uint8_t APU::readStatus() {
    uint8_t s = 0;
    if (f1.length_counter > 0) s |= 0x01;
    if (f2.length_counter > 0) s |= 0x02;
    if (tri.length_counter > 0) s |= 0x04;
    if (noise.length_counter > 0) s |= 0x08;
    if (dmc.bytes_remaining > 0) s |= 0x10;
    // Bit 7: DMC IRQ. Đọc $4015 KHÔNG xóa cờ này (chỉ xóa frame IRQ) - ghi $4015 mới xóa.
    if (dmc.irq_flag) s |= 0x80;

    // Bit 6: frame IRQ đang chờ xử lý
    if (frame_irq_flag) s |= 0x40;

    // Đọc $4015 sẽ TỰ ĐỘNG xóa cờ frame IRQ (đúng hành vi hardware thật)
    frame_irq_flag = false;
    if (bus && bus->cpu) bus->cpu->ClearIrqSource(CPU6502::IRQ_APU);

    return s;
}

void APU::SetChannelGain(const QString& channelId, float gain)
{
    gain = std::clamp(gain, 0.0f, 2.0f);

    if (channelId == "nes.pulse1")        gainPulse1 = gain;
    else if (channelId == "nes.pulse2")   gainPulse2 = gain;
    else if (channelId == "nes.triangle") gainTriangle = gain;
    else if (channelId == "nes.noise")    gainNoise = gain;
    else if (channelId == "nes.dmc")      gainDMC = gain;
    else if (channelId.startsWith("vrc6."))
    {
        // Kênh expansion chip nằm trong Mapper riêng, không phải trong APU —
        // tìm đúng mapper đang active (giống hệt cách SetSmoothSaw() làm bên dưới)
        // rồi forward gain sang đó.
        Mapper* activeMapper = mapper;
        if (!activeMapper && bus && bus->cart && bus->cart->pMapper)
            activeMapper = bus->cart->pMapper.get();

        auto vrc6 = dynamic_cast<Mapper_024*>(activeMapper);
        if (vrc6)
            vrc6->SetChannelGain(channelId, gain);
    }
    else if (channelId.startsWith("s5b."))
    {
        Mapper* activeMapper = mapper;
        if (!activeMapper && bus && bus->cart && bus->cart->pMapper)
            activeMapper = bus->cart->pMapper.get();

        auto s5b = dynamic_cast<Mapper_069*>(activeMapper);
        if (s5b)
            s5b->SetChannelGain(channelId, gain);
    }
    else if (channelId.startsWith("mmc5."))
    {
        Mapper* activeMapper = mapper;
        if (!activeMapper && bus && bus->cart && bus->cart->pMapper)
            activeMapper = bus->cart->pMapper.get();

        auto mmc5 = dynamic_cast<Mapper_005*>(activeMapper);
        if (mmc5)
            mmc5->SetChannelGain(channelId, gain);
    }
    else if (channelId.startsWith("n163."))
    {
        Mapper* activeMapper = mapper;
        if (!activeMapper && bus && bus->cart && bus->cart->pMapper)
            activeMapper = bus->cart->pMapper.get();

        auto n163 = dynamic_cast<Mapper_019*>(activeMapper);
        if (n163)
            n163->SetChannelGain(channelId, gain);
    }
    else if (channelId.startsWith("vrc7."))
    {
        Mapper* activeMapper = mapper;
        if (!activeMapper && bus && bus->cart && bus->cart->pMapper)
            activeMapper = bus->cart->pMapper.get();

        auto vrc7 = dynamic_cast<Mapper_085*>(activeMapper);
        if (vrc7)
            vrc7->SetChannelGain(channelId, gain);
    }
    else if (channelId.startsWith("fds."))
    {
        // Kênh expansion FDS nằm trong Mapper_020, không phải trong APU —
        // forward gain giống hệt cách các expansion chip khác làm ở trên.
        Mapper* activeMapper = mapper;
        if (!activeMapper && bus && bus->cart && bus->cart->pMapper)
            activeMapper = bus->cart->pMapper.get();

        auto fds = dynamic_cast<Mapper_020*>(activeMapper);
        if (fds)
            fds->SetChannelGain(channelId, gain);
    }
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