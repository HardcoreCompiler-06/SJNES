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
#include <QObject>
#include <QImage>
#include <QTimer>
#include "AudioEQ.h"
#include <QElapsedTimer>
#include <atomic>
#include <vector>
#include "Bus.h"
#include "CPU6502.h"
#include "PPU.h"
#include "APU.h"  
#include "NSFFile.h"
#include "Mapper_024.h"
#include "Mapper_069.h"
#include "Mapper_085.h"
#include "Mapper_005.h"
#include "Mapper_019.h"
#include "Mapper_020.h"
class EmuWorker : public QObject
{
    Q_OBJECT

public:
    explicit EmuWorker(QObject* parent = nullptr);
    ~EmuWorker() override; // lưu nốt đĩa FDS nếu còn thay đổi chưa ghi

    Bus nes_bus;
    CPU6502 nes_cpu;
    PPU nes_ppu;

public slots:
    void start();
    void stop();
    void loadRom(QString fileName, bool smoothSaw); // chạy hẳn trên emuThread, không block UI thread nữa
    void loadNSF(NSFFile nsf);
    void nsfNextTrack();
    void nsfPrevTrack();
    void restartCurrentNSFTrack();
    void setTurboA(bool on) { holdturboA = on; }
    void setTurboB(bool on) { holdturboB = on; }
    void setReverseDPCM(bool on) { nes_bus.n_apu.SetReverseDpcmBits(on); }
    void setDMCReducePopping(bool on) { nes_bus.n_apu.SetDMCReducePopping(on); }
    void setZapperPosition(int x, int y) { nes_bus.zapperX = x; nes_bus.zapperY = y; }
    void setZapperOffScreen() { nes_bus.zapperX = -1; nes_bus.zapperY = -1; }
    void setZapperTrigger(bool pressed) { nes_bus.zapperTrigger = pressed; }
    void setZapperEnabled(bool enabled) { nes_bus.zapperConnected = enabled; }
    void setFdsDiskSide(int side);   // side = -1 -> eject
    void saveFdsDisk();              // ghi patch đĩa FDS ra .fdssave nếu có thay đổi (gọi được từ UI qua QueuedConnection khi đóng app)
    void setChannelGain(QString channelId, float gain) {
        // Chơi ROM thường: đi qua APU -> Bus -> cart mapper (đã có sẵn).
        nes_bus.n_apu.SetChannelGain(channelId, gain);

        // Chơi NSF: VRC6/S5B/MMC5 KHÔNG nằm trong Bus/Cartridge mà là object
        // riêng (nsfVRC6/nsfS5B/nsfMMC5) sống ngay trong EmuWorker, nên phải
        // forward thêm 1 lần nữa vào đây mới có tác dụng khi đang phát NSF.
        if (nsfVRC6 && channelId.startsWith("vrc6."))
            nsfVRC6->SetChannelGain(channelId, gain);
        else if (nsfS5B && channelId.startsWith("s5b."))
            nsfS5B->SetChannelGain(channelId, gain);
        else if (nsfMMC5 && channelId.startsWith("mmc5."))
            nsfMMC5->SetChannelGain(channelId, gain);
        else if (nsfN163 && channelId.startsWith("n163."))
            nsfN163->SetChannelGain(channelId, gain);
        else if (nsfVRC7 && channelId.startsWith("vrc7."))
            nsfVRC7->SetChannelGain(channelId, gain);
        else if (nsfFDS && channelId.startsWith("fds."))
            nsfFDS->SetChannelGain(channelId, gain);
    }
    void setPowered(bool on) { machinePowered = on; }
    void setFastForward(bool on, int multiplier) { fastForward = on; fastForwardMultiplier = multiplier; }
    void setVideo60fps(bool on) { video60fps = on; }
    void setStereo(bool on) { is_stereo = on; }
    // Sample rate THẬT mà audio device đang mở (ví dụ 44100 hoặc 48000).
    // Trước đây con số này bị hardcode "44100.0" thẳng trong vòng lặp sinh
    // sample ở runFrame()/runNSFFrame(), hoàn toàn không liên quan tới việc
    // người dùng đổi sample rate trong Settings -> khi đổi 44.1<->48kHz,
    // emu vẫn sinh PCM theo nhịp 44100Hz cố định trong khi audio device đã
    // đổi sang định dạng khác -> lệch tốc độ nạp buffer -> audio bị "treo"
    // (underrun/desync), phải tắt mở lại app mới hết.
    // Gọi hàm này TỪ UI THREAD (qua QueuedConnection, giống các slot khác)
    // ngay khi người dùng đổi sample rate, TRƯỚC hoặc CÙNG lúc mở lại audio
    // device ở tầng UI/audio backend.
    void setOutputSampleRate(double hz) {
        if (hz <= 0.0) return;
        outputSampleRate = hz;
        // Đánh dấu để runFrame()/runNSFFrame() reset accumulator ngay lần
        // chạy kế tiếp -> tránh cộng dồn lệch nhịp từ rate cũ sang rate mới
        // (nếu không reset, accumulator có thể đang mang giá trị tính theo
        // 44100Hz, gây dồn cục hoặc thiếu sample ngay sau khi đổi).
        sampleRateChangePending = true;
    }
    void setControllerState(uint8_t p1, uint8_t p2) { controllerState1 = p1; controllerState2 = p2; }
    void setNeedAudioMix(bool needAudio, bool needDebug) { wantOutputAudio = needAudio; wantWaveDebug = needDebug; }
    void setNsfSmoothSaw(bool enable) { if (nsfVRC6) nsfVRC6->setSmoothSaw(enable); }
    void setNsfMuteVRC6(bool mute) { if (nsfVRC6) nsfVRC6->muteVRC6 = mute; }
    void setNsfMuteVRC6Pulse1(bool mute) { if (nsfVRC6) nsfVRC6->muteVRC6Pulse1 = mute; }
    void setNsfMuteVRC6Pulse2(bool mute) { if (nsfVRC6) nsfVRC6->muteVRC6Pulse2 = mute; }
    void setNsfMuteVRC6Saw(bool mute) { if (nsfVRC6) nsfVRC6->muteVRC6Saw = mute; }
    void setNsfMuteS5BToneA(bool mute) { if (nsfS5B) nsfS5B->muteToneA = mute; }
    void setNsfMuteS5BToneB(bool mute) { if (nsfS5B) nsfS5B->muteToneB = mute; }
    void setNsfMuteS5BToneC(bool mute) { if (nsfS5B) nsfS5B->muteToneC = mute; }
    // Chế độ âm thanh N163 (false = chia trung bình, true = 8 kênh qua 1 DAC).
    // Chạy trên emuThread (queued connection) -> an toàn cho cả ROM thường lẫn NSF.
    // Khi tạo/reset mapper 19, Mapper_019::reset() tự đọc accurateAudioDefault.
    void setN163Accurate(bool on) {
        if (nsfN163)
            nsfN163->SetAccurateAudio(on);

        if (nes_bus.cart && nes_bus.cart->pMapper)
        {
            if (auto* m = dynamic_cast<Mapper_019*>(nes_bus.cart->pMapper.get()))
                m->SetAccurateAudio(on);
        }
    }
    void setNsfMuteN163Channel(int viewIndex, bool mute) { if (nsfN163) nsfN163->SetN163ChannelMuted(viewIndex, mute); }
    void setNsfMuteVRC7Channel(int ch, bool mute) { if (nsfVRC7) nsfVRC7->SetVrc7ChannelMuted(ch, mute); }
    void setNsfMuteMMC5Pulse1(bool mute) { if (nsfMMC5) nsfMMC5->muteMMC5Pulse1 = mute; }
    void setNsfMuteMMC5Pulse2(bool mute) { if (nsfMMC5) nsfMMC5->muteMMC5Pulse2 = mute; }
    void setNsfMuteMMC5PCM(bool mute) { if (nsfMMC5) nsfMMC5->muteMMC5PCM = mute; }
    bool isNsfMode() const { return nes_bus.nsfMode; }
    void setEQEnabled(bool on) { audioEQ.enabled = on; }
    void setEQBandGain(int band, float gainDB) { audioEQ.SetBandGain(band, gainDB); }
private slots:
    void runFrame();

signals:
    void frameReady(QImage frame);
    void audioReady(std::vector<float> samples, bool stereo);
    void debugChannelsReady(std::vector<AudioDebugChannels> dbgBatch);
    void gameFrameTicked();
    void realGameFrameTicked(); // frame mà game thật sự chạy logic (có poll tay cầm)
    void nsfConsoleMessage(QString msg);
    void nsfTrackChanged(int current, int total);
    void nsfExpansionDetected(bool vrc6, bool s5b, bool vrc7, bool mmc5, bool n163, bool fds);
    void romLoadResult(bool ok, QString fileName, QString errorMsg, uint32_t mapperID, uint16_t startPC);
    void fdsDiskChanged(int currentSide, int sideCount);
private:
    int nmi_write_delay = 0;

    QTimer* timer = nullptr;
    QElapsedTimer framePacer;
    qint64 nextFrameNs = 0;

    uint32_t system_clock_counter = 0;
    uint16_t dma_dummy_counter = 0;
    uint64_t videoFrameCounter = 0;
    int dma_cycles_needed = 513;
    uint32_t cpu_cycle_parity = 0;

    std::atomic<bool> machinePowered{ true };
    std::atomic<bool> fastForward{ false };
    std::atomic<int>  fastForwardMultiplier{ 3 };
    std::atomic<bool> is_stereo{ true };
    std::atomic<bool> wantOutputAudio{ false };
    std::atomic<bool> wantWaveDebug{ false };
    std::atomic<uint8_t> controllerState1{ 0 };
    std::atomic<uint8_t> controllerState2{ 0 };
    std::atomic<bool> video60fps{ true };
    std::vector<AudioDebugChannels> dbgBatch;

    // Sample rate output thật (mặc định 44100 để tương thích ngược nếu UI
    // chưa từng gọi setOutputSampleRate()). atomic vì được ghi từ UI thread,
    // đọc từ emuThread trong runFrame()/runNSFFrame().
    std::atomic<double> outputSampleRate{ 44100.0 };
    std::atomic<bool>   sampleRateChangePending{ false };

    // Chuyển từ "static double audio_accumulator" cục bộ trong runFrame()
    // thành member, để setOutputSampleRate() có thể reset nó về 0 ngay khi
    // đổi rate (biến static local trước đây không cách nào reset từ ngoài).
    double audio_accumulator = 0.0;

    QString fdsSavePath; // đường dẫn .fdssave của game FDS đang chạy (rỗng nếu không phải FDS)

    bool holdturboA = false;
    bool holdturboB = false;

    void runNSFFrame();
    int callNSFRoutine(uint16_t address, uint8_t a, uint8_t x, int maxCpuCycles, std::vector<float>& audioSamples);
    void pushNSFAudioSample(std::vector<float>& audioSamples);

    std::unique_ptr<Mapper_024> nsfVRC6;
    std::unique_ptr<Mapper_069> nsfS5B;
    std::unique_ptr<Mapper_085> nsfVRC7;
    std::unique_ptr<Mapper_005> nsfMMC5;
    std::unique_ptr<Mapper_019> nsfN163;
    std::unique_ptr<Mapper_020> nsfFDS;
    NSFFile currentNSF;
    int currentNSFSong = 1;
    double nsfAudioAccumulator = 0.0;
    QElapsedTimer nsfFramePacer;
    qint64 nsfNextFrameNs = 0;

    AudioEQ9Band audioEQ;

};