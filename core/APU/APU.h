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
#include <vector>
#include "F1.h"
#include "F2.h"
#include "Triangle.h"
#include "DMC.h"
#include "Noise.h"
#include "Mapper_024.h"
#include <QMetaType>
#include <QString>
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

    // Phase thật 0..1 trong vòng wavetable N163 (wrap 1->0 = đầu wavetable), cùng view index với n163WaveN.
    float n163Phase1 = 0.0f;
    float n163Phase2 = 0.0f;
    float n163Phase3 = 0.0f;
    float n163Phase4 = 0.0f;
    float n163Phase5 = 0.0f;
    float n163Phase6 = 0.0f;
    float n163Phase7 = 0.0f;
    float n163Phase8 = 0.0f;

    float fdsWave = 0.0f;

    float pulse1Period = 0.0f;
    float pulse2Period = 0.0f;
    float trianglePeriod = 0.0f;
    float noisePeriod = 0.0f;
    float dmcPeriod = 0.0f;

    // Mốc Restart() DMC tuyệt đối (đơn vị: sample 44.1kHz-equivalent, CÙNG trục
    // thời gian với dmcPeriod/totalPushed bên AudioWaveWindow), quy đổi từ
    // DMC::dbgLastLoopRestartCycle. Dùng double vì cycle-count có thể lên tới
    // hàng tỷ sau phiên chơi dài — float sẽ mất độ chính xác dưới-sample.
    // -1.0 = chưa đo được loop nào (sample one-shot hoặc vừa đổi $4010-$4013).
    double dmcRestartAbs = -1.0;

    // Tuổi của lần loop-restart gần nhất, tính bằng SAMPLE, đo tại đúng thời điểm lấy mẫu này
    // (= (dbgCycleCounter - dbgLastLoopRestartCycle) / cycles_per_sample). Đây là đại lượng
    // TƯƠNG ĐỐI nên không phụ thuộc trục thời gian tuyệt đối của scope (totalPushed bị
    // reset khi clearSamples, còn cycle-count thì không). -1.0 = chưa có loop/đo được.
    double dmcAgeSamples = -1.0;

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

    float fdsWavePeriod = 0.0f;

    float pulse1Duty = -1.0f;
    float pulse2Duty = -1.0f;
    float trianglePhase = -1.0f;

    float vrc6Pulse1Duty = -1.0f;
    float vrc6Pulse2Duty = -1.0f;

    float s5bToneADuty = 0.5f;
    float s5bToneBDuty = 0.5f;
    float s5bToneCDuty = 0.5f;

    float mmc5Pulse1Duty = -1.0f;
    float mmc5Pulse2Duty = -1.0f;
};
Q_DECLARE_METATYPE(AudioDebugChannels)
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
    // Áp filter chain thật (2 highpass + 1 lowpass, giả lập mạch NES) lên TỔNG
    // đã cộng cả NES 5 kênh + expansion audio (VRC6/S5B/VRC7/MMC5/N163).
    // Gọi hàm này SAU KHI cộng expansion vào, KHÔNG gọi trước, vì trên phần cứng
    // thật expansion audio hòa vào cùng đường tín hiệu analog trước khi qua filter.
    void    ApplyOutputFilterStereo(float& left, float& right);
    float   ApplyOutputFilterMono(float sample);
    bool mutePulse1 = false;
    bool mutePulse2 = false;
    bool muteTriangle = false;
    bool muteNoise = false;
    bool muteDMC = false;

    // Gain riêng từng kênh (0.0 - 2.0, mặc định 1.0 = không đổi so với gốc).
    // Tách biệt hoàn toàn với mute: mute = tắt hẳn, gain = chỉnh to/nhỏ khi
    // KHÔNG mute. Áp dụng ngay tại GetOutputSampleStereo(), trước khi mix TND/Pulse.
    float gainPulse1 = 1.0f;
    float gainPulse2 = 1.0f;
    float gainTriangle = 1.0f;
    float gainNoise = 1.0f;
    float gainDMC = 1.0f;

    // channelId khớp với chuỗi SettingsDialog gửi qua channelGainChanged:
    // "nes.pulse1", "nes.pulse2", "nes.triangle", "nes.noise", "nes.dmc".
    // Kênh không khớp id nào thì bị bỏ qua (không crash), để mở rộng dần
    // sang expansion chip (VRC6/S5B/VRC7/MMC5/N163) ở các mapper riêng sau này.
    void SetChannelGain(const QString& channelId, float gain);
    bool smoothSawEnabled = false;
    void SetSmoothSaw(bool enable);
    // Bật/tắt "Reverse DPCM Bit Order" — bù cho ROM encode sample DMC sai thứ tự bit
    // (Double Dribble, Gimmick!, một số famiclone...). Mặc định tắt.
    void SetReverseDpcmBits(bool enable) { dmc.reverseBits = enable; }
    bool GetReverseDpcmBits() const { return dmc.reverseBits; }
    // Bật/tắt "Reduce popping sounds on DMC channel" — làm mượt (ramp +-2 mỗi
    // timer clock) khi CPU ghi trực tiếp $4011 thay vì set output_level ngay
    // lập tức, tránh tiếng click/pop. Mặc định tắt (giữ hardware-accurate).
    void SetDMCReducePopping(bool enable) { dmc.reducePopping = enable; }
    bool GetDMCReducePopping() const { return dmc.reducePopping; }
    Mapper* mapper = nullptr;

    // Mô hình filter đầu ra:
    //  - true  (mặc định) = Famicom: chỉ 1 high-pass bậc 1 ở 37 Hz (đúng cho cartridge
    //    có expansion audio như VRC6, vốn chỉ tồn tại trên Famicom).
    //  - false = NES front-loader: high-pass 90 Hz nối tiếp high-pass 440 Hz.
    //  Cả hai đều có thêm low-pass 14 kHz sau cùng (nesdev, "APU Mixer").
    void SetFamicomFilter(bool famicom) { famicomFilter = famicom; lastFilterSampleRate = -1.0; }
    bool GetFamicomFilter() const { return famicomFilter; }
    void SetSmoothTriangle(bool smooth) { tri.smooth = smooth; }
    bool GetSmoothTriangle() const { return tri.smooth; }
    uint8_t readStatus();

private:
    F1       f1;
    F2       f2;
    Triangle tri;
    DMC      dmc;
    Noise    noise;

    int  frame_seq_count = 0;
    bool use_5step_mode = false;
    bool apu_half_clock = false;
    bool frame_irq_flag = false;
    bool irq_inhibit = false;

    // Delay áp dụng write $4017: hardware thật KHÔNG apply ngay lập tức trong
    // cùng cycle ghi lệnh, mà trễ 3 CPU cycle (nếu write rơi vào cycle chẵn)
    // hoặc 4 CPU cycle (nếu rơi vào cycle lẻ). pending_4017_delay = -1 nghĩa
    // là không có write nào đang chờ áp dụng.
    long long total_cpu_cycle_count = 0;
    int     pending_4017_delay = -1;
    uint8_t pending_4017_value = 0;

    bool  famicomFilter = true;
    bool  filterTwoHighpass = false; // true khi đang dùng mô hình NES (2 high-pass)

    float hp1 = 0.0f, prev_in1 = 0.0f;
    float hp2 = 0.0f, prev_in2 = 0.0f;
    float lp = 0.0f;

    float hp1L = 0.0f, prev_in1L = 0.0f;
    float hp2L = 0.0f, prev_in2L = 0.0f;
    float lpL = 0.0f;
    float hp1R = 0.0f, prev_in1R = 0.0f;
    float hp2R = 0.0f, prev_in2R = 0.0f;
    float lpR = 0.0f;
    double lastFilterSampleRate = -1.0;
    float outFilterA1 = 0.99716f;
    float outFilterA2 = 0.97910f;
    float outFilterB = 0.66620f;
    void UpdateOutputFilterCoeffs(double sampleRateHz);

    // Đồng bộ trạng thái DMC sang phần còn lại của máy: đường IRQ_DMC của CPU và
    // số cycle CPU bị dừng do DMC DMA (Bus::dmc_stall_cycles).
    bool dmc_irq_line = false;
    void SyncDmcToBus();
};