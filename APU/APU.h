#pragma once
#include <cstdint>
#include <vector>
#include "F1.h"
#include "F2.h"
#include "Triangle.h"
#include "DMC.h"
#include "Noise.h"
#include "Mapper_024.h"
struct AudioDebugChannels {
    float pulse1 = 0.0f;
    float pulse2 = 0.0f;
    float triangle = 0.0f;
    float noise = 0.0f;
    float dmc = 0.0f;

    float vrc6Pulse1 = 0.0f;
    float vrc6Pulse2 = 0.0f;
    float vrc6Saw = 0.0f;

    float s5bToneA = 0.0f;
    float s5bToneB = 0.0f;
    float s5bToneC = 0.0f;

    float vrc7Wave1 = 0.0f;
    float vrc7Wave2 = 0.0f;
    float vrc7Wave3 = 0.0f;
    float vrc7Wave4 = 0.0f;
    float vrc7Wave5 = 0.0f;
    float vrc7Wave6 = 0.0f;

    float mmc5Pulse1 = 0.0f;
    float mmc5Pulse2 = 0.0f;
    float mmc5PCM = 0.0f;

    float n163Wave1 = 0.0f;
    float n163Wave2 = 0.0f;
    float n163Wave3 = 0.0f;
    float n163Wave4 = 0.0f;
    float n163Wave5 = 0.0f;
    float n163Wave6 = 0.0f;
    float n163Wave7 = 0.0f;
    float n163Wave8 = 0.0f;


    // Period hint tính theo đơn vị: số audio sample / 1 chu kỳ sóng.
    // Dùng cho AudioWaveWindow để khóa pha theo timer thật, tránh trôi ngang.
    float pulse1Period = 0.0f;
    float pulse2Period = 0.0f;
    float trianglePeriod = 0.0f;
    float noisePeriod = 0.0f;
    float dmcPeriod = 0.0f;

    float vrc6Pulse1Period = 0.0f;
    float vrc6Pulse2Period = 0.0f;
    float vrc6SawPeriod = 0.0f;

    float s5bToneAPeriod = 0.0f;
    float s5bToneBPeriod = 0.0f;
    float s5bToneCPeriod = 0.0f;

    float vrc7Wave1Period = 0.0f;
    float vrc7Wave2Period = 0.0f;
    float vrc7Wave3Period = 0.0f;
    float vrc7Wave4Period = 0.0f;
    float vrc7Wave5Period = 0.0f;
    float vrc7Wave6Period = 0.0f;

    // VRC7 carrier phase hint, lấy từ emu2413 pg_phase của carrier slot.
    // 0.0..1.0 là một vòng phase; AudioWaveWindow dùng để neo VRC7 theo chip thật,
    // không bắt zero-crossing từ waveform nữa.
    float vrc7Wave1Phase = 0.0f;
    float vrc7Wave2Phase = 0.0f;
    float vrc7Wave3Phase = 0.0f;
    float vrc7Wave4Phase = 0.0f;
    float vrc7Wave5Phase = 0.0f;
    float vrc7Wave6Phase = 0.0f;

    float mmc5Pulse1Period = 0.0f;
    float mmc5Pulse2Period = 0.0f;

    float n163Period1 = 0.0f;
    float n163Period2 = 0.0f;
    float n163Period3 = 0.0f;
    float n163Period4 = 0.0f;
    float n163Period5 = 0.0f;
    float n163Period6 = 0.0f;
    float n163Period7 = 0.0f;
    float n163Period8 = 0.0f;

    // Chip-shape hint: dùng để AudioWaveWindow vẽ sóng vuông/tam giác bám sát timer+duty thật,
    // không bắt nhầm cạnh waveform lúc note vừa nổi lên.
    // Pulse duty dùng raw code 0..3 theo duty table NES; -1 = không hợp lệ/tắt.
    float pulse1Duty = -1.0f;
    float pulse2Duty = -1.0f;
    // Triangle phase raw 0..31; hiện dùng để tham khảo/debug, renderer vẽ theo shape chuẩn 32 bước.
    float trianglePhase = -1.0f;

    float vrc6Pulse1Duty = -1.0f;
    float vrc6Pulse2Duty = -1.0f;

    float s5bToneADuty = 0.5f;
    float s5bToneBDuty = 0.5f;
    float s5bToneCDuty = 0.5f;

    float mmc5Pulse1Duty = -1.0f;
    float mmc5Pulse2Duty = -1.0f;
};
class Bus;

class APU {
public:
    APU();
    ~APU() = default;
    void reset();
    Bus* bus = nullptr;

    double             sample_counter = 0.0;
    double             cycles_per_sample = 1789773.0 / 44100.0;
    std::vector<float> audio_buffer;
    AudioDebugChannels GetDebugChannels();
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
    bool smoothSawEnabled = false;
    void SetSmoothSaw(bool enable);
    Mapper* mapper = nullptr;
    void SetSmoothTriangle(bool smooth) { tri.smooth = smooth; }
    bool GetSmoothTriangle() const { return tri.smooth; }
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