/*
 * SJNES - Nintendo Entertainment System Emulator
 * Copyright (C) 2026 Chiến
 *
 * Portions derived from Mesen (https://github.com/SourMesen/Mesen2)
 * Copyright (c) SourMesen and contributors
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
// Portions derived from Mesen (https://github.com/SourMesen/Mesen2), GPL-3.0
#pragma once
#include "Mapper.h"
#include <vector>
#include <cstdint>
#include <QString>

// ============================================================================
// Mapper 020 - Famicom Disk System (FDS)
// ============================================================================
// PHIÊN BẢN NÀY PORT TRỰC TIẾP TỪ SOURCE THẬT CỦA MESEN (Core/FDS.cpp,
// SourMesen/Mesen trên GitHub, đã verify qua rất nhiều game trong nhiều năm),
// sau khi mọi hướng tự suy luận trước đó đều thất bại qua nhiều vòng debug.
//
// PHÁT HIỆN MẤU CHỐT làm thay đổi hoàn toàn kiến trúc:
// Mesen KHÔNG hề tổng hợp "physical stream" với gap/marker/CRC chèn sẵn như
// cách cũ (BuildPhysicalSide cũ của mình). Nó đọc THẲNG dữ liệu thô, nguyên
// văn từ file .fds (block nối liền nhau, không chèn gì thêm). "Gap" hoàn
// toàn là một LATCH PHẦN MỀM (_gapEnded), được set khi:
//   - Bit6 $4025 (_diskReady, KHÔNG PHẢI "CRC Enabled" như wiki mình đọc
//     nhầm trước đó) đang bật, VÀ
//     byte hiện tại đọc được từ đĩa KHÁC 0.
// Vị trí đọc (_diskPosition) luôn tăng đều 1 byte mỗi ~150 CPU cycle SUỐT
// khi motor bật, HOÀN TOÀN không phụ thuộc gap/block/CRC gì cả - đơn giản
// hơn nhiều so với state machine phức tạp mình tự dựng trước đó.
//
// BIT-MAPPING $4025 THEO MESEN (KHÁC với cách đọc NESdev wiki trước đó,
// nay xác nhận Mesen đúng vì đã được cộng đồng kiểm chứng qua hàng trăm game):
//   bit0 = Motor ON (1=on)                    [cũ: đọc nhầm là "Scan Disk"]
//   bit1 = Reset Transfer (1=reset/tạm dừng)  [cũ: đọc nhầm là "Motor Stop"]
//   bit2 = Read Mode (1=read, 0=write)
//   bit3 = Mirroring
//   bit4 = CRC Control
//   bit6 = Disk Ready (1=ready)                [cũ: đọc nhầm là "CRC Enabled"]
//   bit7 = Disk IRQ Enabled
//
// LỊCH SỬ DEBUG (để tránh lặp lại các hướng đã thử-và-sai trước khi có source
// Mesen thật để đối chiếu):
//   1. Rewind diskPos về 0 mỗi lần toggle bit -> ERR.23.
//   2. Gap cố định theo "spec" tự đọc (sai) -> ERR.22 liên tục, mỗi lần
//      theo một hướng khác (nhồi gap, freeze theo cycle đo được, state
//      machine chờ "cạnh lên CRC enable") đều chỉ vá được MỘT phần, vì gốc
//      rễ là kiến trúc "tổng hợp physical stream" sai hoàn toàn ngay từ đầu.
// ============================================================================

class Mapper_020 : public Mapper
{
public:
    Mapper_020(uint32_t prgBanks, uint32_t chrBanks);
    ~Mapper_020() override = default;

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool cpuReadRegister(uint16_t addr, uint8_t& data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    MIRROR mirror() override { return mirrorMode; }

    void irqStep() override;
    bool irqState() override;
    void irqClear() override;

    std::string GetDebugInfo() override;

    void SetBIOS(const std::vector<uint8_t>& biosData);
    bool HasBIOS() const { return !bios.empty(); }
    void SetDiskSides(std::vector<std::vector<uint8_t>> sides);
    void InsertDisk(int sideIndex);
    void EjectDisk() { InsertDisk(-1); }
    int  GetCurrentSide() const { return currentSide; }
    int  GetSideCount() const { return (int)diskSides.size(); }

    uint8_t ReadBios(uint16_t addr) const;

    // ---- Lưu đĩa (save game FDS) ----
    // Game ghi đĩa qua ClockDiskTransfer() (write mode) -> chỉnh trực tiếp diskSides trong RAM.
    // Mapper chỉ theo dõi thay đổi và tạo/áp dụng patch IPS so với đĩa gốc; việc đọc/ghi
    // FILE do phía gọi (EmuWorker/SJNES) làm: lúc nạp gọi ApplyDiskPatch, lúc thoát
    // (hoặc định kỳ) nếu IsDiskDirty() thì ghi BuildDiskPatch() ra file .ips cạnh game.
    bool IsDiskDirty() const { return diskDirty; }
    void ClearDiskDirty() { diskDirty = false; }
    std::vector<uint8_t> BuildDiskPatch() const;                 // rỗng nếu không có thay đổi
    bool ApplyDiskPatch(const std::vector<uint8_t>& ips);        // gọi SAU SetDiskSides()

    // ---- Expansion Audio ----
    // Override hàm ảo của Mapper base class - đây là chỗ EmuWorker.cpp thực
    // sự gọi để mix audio (mapper->GetExpansionAudio()), KHÔNG phải qua
    // dynamic_cast. Nhờ vậy chỉ cần override đúng tên là game mode tự động
    // có tiếng FDS, không cần sửa gì ở EmuWorker.cpp.
    float GetExpansionAudio() override;
    void SetChannelGain(const QString& channelId, float gain); // channelId dạng "fds.xxx"

    // Gọi 1 lần khi dùng Mapper_020 độc lập cho NSF (không qua $4023 thật)
    // để bật thẳng sound regs, vì NSF FDS tune không nhất thiết ghi $4023.
    void EnableForNSF() { soundRegsEnabled = true; }

    // ---- Debug/Oscilloscope: dùng cho AudioWaveWindow, đối chiếu pattern
    // GetVRC6DebugPeriods/GetS5BDebugDuty... của các mapper khác. FDS chỉ có
    // 1 kênh wavetable duy nhất nên chỉ cần 1 wave + 1 period.
    void GetFdsDebugChannels(float& wave);
    // outputRate = sample rate THẬT của luồng audio đưa vào oscilloscope (đừng để 44100 nếu app chạy 48000...)
    void GetFdsDebugPeriod(float& period, float outputRate = 44100.0f);

private:
    std::vector<uint8_t> bios;

    MIRROR mirrorMode = VERTICAL;

    // ---- Timer IRQ ($4020-$4022) ----
    uint16_t irqReloadValue = 0;
    uint16_t irqCounter = 0;
    bool     irqTimerEnabled = false;
    bool     irqTimerRepeat = false;
    bool     irqTimerPending = false;

    // ---- Master I/O enable ($4023) ----
    bool diskRegsEnabled = false;
    bool soundRegsEnabled = false;

    // ---- FDS Control ($4025) - bit-mapping theo Mesen (xem giải thích ở
    // đầu file) ----
    bool motorOn = false;          // bit0
    bool resetTransfer = false;    // bit1
    bool readMode = true;          // bit2
    bool crcControl = false;       // bit4
    bool diskReady = false;        // bit6 - latch phần mềm điều khiển gap
    bool diskIrqEnabled = false;   // bit7

    // ---- Dữ liệu đĩa: LƯU THẲNG dữ liệu thô từ file .fds, không tổng hợp
    // gì thêm (khác hoàn toàn thiết kế cũ) ----
    std::vector<std::vector<uint8_t>> diskSides; // mỗi side là mảng byte thô
    int currentSide = -1;
    std::vector<std::vector<uint8_t>> originalSides; // bản sao đĩa gốc, dùng để so sánh khi tạo patch
    bool diskDirty = false;                          // đã có byte nào trên đĩa bị game ghi đổi

    // ---- Trạng thái transfer (port trực tiếp từ Mesen::ProcessCpuClock) ----
    bool   endOfHead = true;
    bool   scanningDisk = false;
    bool   gapEnded = false;
    bool   previousCrcControlFlag = false;
    bool   transferComplete = false;
    int    delay = 0;
    size_t diskPosition = 0;
    uint16_t crcAccumulator = 0;

    uint8_t readDataReg = 0;
    uint8_t writeDataLatch = 0;
    bool    diskIrqPending = false;

    void UpdateCrc(uint8_t value);
    void ClockDiskTransfer();

    // ================== FDS Expansion Audio ==================
    // Port theo NesDev wiki "FDS audio" + đối chiếu Mesen::FdsAudio.
    // Chỉ active khi soundRegsEnabled (bit1 của $4023) = true. Hoàn toàn
    // độc lập với phần disk transfer ở trên - vì vậy NSF (không có disk
    // system) vẫn phát được âm thanh FDS bình thường.

    uint8_t waveTable[64] = {};
    bool    waveWriteEnable = false;   // $4089 bit7

    uint16_t waveFreq = 0;             // $4082/$4083 (12-bit)
    uint32_t waveAccum = 0;
    uint8_t  wavePos = 0;
    bool     waveHalt = false;         // $4083 bit7
    bool     envelopesDisabled = false;// $4083 bit6

    bool    volEnvDisable = false;     // $4080 bit7 (direct volume mode)
    bool    volEnvDirection = false;   // $4080 bit6
    uint8_t volEnvSpeed = 0;           // $4080 bits0-5
    uint8_t volGain = 0;               // gain hiện tại 0..63 (1 thanh ghi DUNG CHUNG cho direct & envelope, như HW)
    int32_t envVolCounter = 0;

    uint8_t  modTable[64] = {};        // gia tri tho 3-bit (moi lan ghi $4088 chiem 2 o)
    uint8_t  modTableWritePos = 0;     // con tro ghi ($4088)
    uint8_t  modPos = 0;               // con tro doc khi modulator chay
    uint16_t modFreq = 0;              // $4086/$4087 (12-bit)
    uint32_t modAccum = 0;
    bool     modHalt = true;           // $4087 bit7

    bool    modEnvDisable = false;     // $4084 bit7
    bool    modEnvDirection = false;   // $4084 bit6
    uint8_t modEnvSpeed = 0;           // $4084 bits0-5
    int32_t modCounter = 0;            // -64..63, "pitch bend" hiện tại
    int32_t modEnvCounter = 0;
    int32_t modAmplitude = 0;          // biên độ điều biến, điều khiển bởi mod envelope

    uint8_t masterVolume = 0;          // $4089 bits0-1 (00=full,01=2/3,10=2/5,11=1/4)
    uint8_t masterEnvSpeed = 0xE8;     // $408A, base timer cho envelope
    float   lpOut = 0.0f;              // output da qua lowpass ~2kHz (tinh moi CPU cycle)

    float gainFds = 1.0f;              // gain người dùng chỉnh qua SetChannelGain("fds.channel")

    static int32_t signExtend7(uint32_t v) { v &= 0x7F; return (v & 0x40) ? int32_t(v) - 0x80 : int32_t(v); }

    bool WriteSoundReg(uint16_t addr, uint8_t data); // trả false nếu addr không thuộc FDS audio
    bool ReadSoundReg(uint16_t addr, uint8_t& data);
    void ClockAudio();
};