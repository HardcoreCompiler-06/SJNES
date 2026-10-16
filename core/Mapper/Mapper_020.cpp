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
#include "Mapper_020.h"
#include <QDebug>

Mapper_020::Mapper_020(uint32_t /*prgBanks*/, uint32_t /*chrBanks*/)
    : Mapper(0, 0)
{
    reset();
}

void Mapper_020::reset()
{
    irqReloadValue = 0;
    irqCounter = 0;
    irqTimerEnabled = false;
    irqTimerRepeat = false;
    irqTimerPending = false;

    diskRegsEnabled = false;
    soundRegsEnabled = false;

    motorOn = false;
    resetTransfer = false;
    readMode = true;
    crcControl = false;
    diskReady = false;
    diskIrqEnabled = false;

    // Không reset currentSide ở đây: đĩa vẫn "nằm trong ổ" xuyên suốt reset
    // (giống hardware thật), chỉ InsertDisk()/EjectDisk() mới đổi việc này.
    endOfHead = true;   // giống Mesen::Reset - buộc phiên đọc bắt đầu lại từ
    // đầu side (delay 50000 cycle) ở lần clock kế tiếp
    scanningDisk = false;
    gapEnded = false;
    previousCrcControlFlag = false;
    transferComplete = false;
    delay = 0;
    diskPosition = 0;
    crcAccumulator = 0;

    readDataReg = 0;
    writeDataLatch = 0;
    diskIrqPending = false;

    mirrorMode = VERTICAL;

    // ---- FDS audio: đưa về trạng thái ban đầu (cần cho NSF đổi track) ----
    for (int i = 0; i < 64; i++) { waveTable[i] = 0; modTable[i] = 0; }
    waveWriteEnable = false;
    waveFreq = 0; waveAccum = 0; wavePos = 0;
    waveHalt = false; envelopesDisabled = false;
    volEnvDisable = false; volEnvDirection = false; volEnvSpeed = 0;
    volGain = 0; envVolCounter = 0;
    modTableWritePos = 0; modPos = 0; modFreq = 0; modAccum = 0; modHalt = true;
    modEnvDisable = false; modEnvDirection = false; modEnvSpeed = 0;
    modCounter = 0; modEnvCounter = 0; modAmplitude = 0;
    masterVolume = 0; masterEnvSpeed = 0xE8;
    lpOut = 0.0f;
}

bool Mapper_020::cpuMapRead(uint16_t /*addr*/, uint32_t& /*mapped_addr*/)
{
    return false;
}

bool Mapper_020::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    // ---- FDS Expansion Audio: $4040-$408A ----
    // Tách biệt hoàn toàn khỏi phần disk regs bên dưới - dùng cờ
    // soundRegsEnabled ($4023 bit1) riêng, không phụ thuộc diskRegsEnabled.
    if (addr >= 0x4040 && addr <= 0x408A)
        return WriteSoundReg(addr, data);


    // Port từ Mesen::FDS::WriteRegister - $4024-$4026 chỉ có tác dụng khi
    // diskRegsEnabled (Cartridge đã lọc $6000-$FFFF trước khi vào đây, nên
    // addr vào hàm này chỉ có thể là $4020-$4026).
    if (!diskRegsEnabled && addr >= 0x4024 && addr <= 0x4026)
        return true;

    switch (addr)
    {
    case 0x4020:
        irqReloadValue = (irqReloadValue & 0xFF00) | uint16_t(data);
        return true;

    case 0x4021:
        irqReloadValue = (irqReloadValue & 0x00FF) | (uint16_t(data) << 8);
        return true;

    case 0x4022:
        irqTimerRepeat = (data & 0x01) != 0;
        irqTimerEnabled = (data & 0x02) != 0 && diskRegsEnabled;
        if (irqTimerEnabled)
            irqCounter = irqReloadValue;
        else
            irqTimerPending = false;
        return true;

    case 0x4023:
        diskRegsEnabled = (data & 0x01) != 0;
        soundRegsEnabled = (data & 0x02) != 0;
        if (!diskRegsEnabled)
        {
            irqTimerEnabled = false;
            irqTimerPending = false;
            diskIrqPending = false;
        }
        return true;

    case 0x4024:
        writeDataLatch = data;
        transferComplete = false;
        // Mesen: "Unsure về việc clear irq ở đây: FCEUX/Nintendulator không
        // làm, puNES có làm" - Mesen chọn clear, mình theo Mesen.
        diskIrqPending = false;
        return true;

    case 0x4025:
    {
        motorOn = (data & 0x01) != 0;
        resetTransfer = (data & 0x02) != 0;
        readMode = (data & 0x04) != 0;
        mirrorMode = (data & 0x08) ? HORIZONTAL : VERTICAL;
        crcControl = (data & 0x10) != 0;
        // bit5 không dùng, luôn =1 theo BIOS/game thật
        diskReady = (data & 0x40) != 0;
        diskIrqEnabled = (data & 0x80) != 0;

        // Ghi $4025 luôn clear disk IRQ (theo Mesen, khớp FCEUX/puNES/
        // Nintendulator - sửa lỗi ERR.20 lúc power-on ở một số game
        // unlicensed theo comment gốc của Mesen).
        diskIrqPending = false;
        return true;
    }

    case 0x4026:
        return true;
    }

    return false;
}

bool Mapper_020::cpuReadRegister(uint16_t addr, uint8_t& data)
{
    if (addr >= 0xE000 && addr <= 0xFFFF)
    {
        data = ReadBios(addr);
        return true;
    }
    if ((addr >= 0x4040 && addr <= 0x407F) || addr == 0x4090 || addr == 0x4092)
        return ReadSoundReg(addr, data);

    if (!diskRegsEnabled || addr > 0x4033)
        return false;

    switch (addr)
    {
    case 0x4030:
    {
        // Port từ Mesen: chỉ có bit0 (timer IRQ), bit1 (transfer complete),
        // bit4 (bad CRC) là có ý nghĩa thật - các bit khác là open bus trên
        // hardware thật (Mesen giữ nguyên giá trị bus cũ cho các bit đó).
        // Mô hình đơn giản hoá của mình không track open bus nên để 0.
        uint8_t v = 0;
        if (irqTimerPending) v |= 0x01;
        if (transferComplete) v |= 0x02;
        // bit4 "bad CRC": mô hình đơn giản hoá CRC luôn "passed" (0) - chưa
        // implement kiểm tra CRC thật.

        data = v;

        transferComplete = false;
        irqTimerPending = false;
        diskIrqPending = false;

        return true;
    }

    case 0x4031:
        transferComplete = false;
        diskIrqPending = false;
        data = readDataReg;
        return true;

    case 0x4032:
    {
        // Port từ Mesen: Ready flag phụ thuộc "scanningDisk" (đã thực sự bắt
        // đầu xử lý transfer), KHÔNG phải "motor đang bật hay tắt" như cách
        // mình từng làm sai trước đó.
        uint8_t v = 0;
        if (currentSide < 0) v |= 0x01;                       // không có đĩa
        if (currentSide < 0 || !scanningDisk) v |= 0x02;       // chưa sẵn sàng
        if (currentSide < 0) v |= 0x04;                        // không ghi được (chỉ khi không có đĩa - theo Mesen)

        data = v;
        return true;
    }

    case 0x4033:
        data = 0x80; // luôn báo battery tốt
        return true;
    }

    return false;
}

bool Mapper_020::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x2000)
    {
        mapped_addr = addr;
        return true;
    }
    return false;
}

bool Mapper_020::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x2000)
    {
        mapped_addr = addr;
        return true;
    }
    return false;
}

void Mapper_020::irqStep()
{
    if (irqTimerEnabled)
    {
        if (irqCounter == 0)
        {
            irqTimerPending = true;
            irqCounter = irqReloadValue;
            if (!irqTimerRepeat)
                irqTimerEnabled = false;
        }
        else
        {
            irqCounter--;
        }
    }

    // QUAN TRỌNG (fix sau khi đối chiếu source Mesen thật): ClockDiskTransfer()
    // PHẢI được gọi VÔ ĐIỀU KIỆN mỗi CPU cycle, KHÔNG được gate bởi
    // diskRegsEnabled. Trong Mesen::FDS::ProcessCpuClock(), phần xử lý motor/
    // disk position hoàn toàn không kiểm tra _diskRegEnabled - biến đó CHỈ
    // chặn việc ĐỌC/GHI thanh ghi ($4024-$4033), không liên quan gì tới việc
    // motor/vị trí đĩa có được clock hay không. Bản trước của mình gate nhầm
    // bằng "if (diskRegsEnabled)" ở đây - đây là nguồn gốc thật của toàn bộ
    // hiện tượng "overshoot" (vị trí đọc luôn vượt quá xa so với đúng) đã
    // thấy xuyên suốt mọi lần viết lại mapper trước đó.
    ClockDiskTransfer();
    ClockAudio(); // độc lập hoàn toàn với disk - vẫn chạy dù currentSide < 0 (NSF)
}

bool Mapper_020::irqState()
{
    return irqTimerPending || diskIrqPending;
}

void Mapper_020::irqClear()
{
    irqTimerPending = false;
    diskIrqPending = false;
}

// Port trực tiếp từ Mesen::UpdateCrc (CRC-16/CCITT kiểu FDS dùng)
void Mapper_020::UpdateCrc(uint8_t value)
{
    for (uint16_t n = 0x01; n <= 0x80; n <<= 1)
    {
        uint8_t carry = (crcAccumulator & 1);
        crcAccumulator >>= 1;
        if (carry)
            crcAccumulator ^= 0x8408;
        if (value & n)
            crcAccumulator ^= 0x8000;
    }
}

// Port trực tiếp từ Mesen::ProcessCpuClock (phần xử lý disk transfer).
// Đây là thay đổi CỐT LÕI so với mọi phiên bản trước: không còn state
// machine gap/block/CRC tự dựng - chỉ đơn giản đọc byte thô tại diskPosition,
// tăng dần đều, và dùng "gapEnded" latch mềm để quyết định khi nào dữ liệu
// thật sự "xuất hiện" cho CPU.
void Mapper_020::ClockDiskTransfer()
{
    if (currentSide < 0 || !motorOn)
    {
        endOfHead = true;
        scanningDisk = false;
        return;
    }

    if (resetTransfer && !scanningDisk)
        return;

    if (endOfHead)
    {
        delay = 50000;
        endOfHead = false;
        diskPosition = 0;
        gapEnded = false;
        return;
    }

    if (delay > 0)
    {
        delay--;
        return;
    }

    scanningDisk = true;

    auto& side = diskSides[size_t(currentSide)];
    if (diskPosition >= side.size())
    {
        motorOn = false;
        return;
    }

    uint8_t diskData = 0;
    bool needIrq = diskIrqEnabled;

    if (readMode)
    {
        diskData = side[diskPosition];
        if (!previousCrcControlFlag)
            UpdateCrc(diskData);

        if (!diskReady)
        {
            gapEnded = false;
            crcAccumulator = 0;
        }
        else if (diskData != 0 && !gapEnded)
        {
            // Byte khác 0 đầu tiên (start mark 0x80): kết thúc gap. Byte này
            // VẪN được giao cho CPU, chỉ là không bắn IRQ (giống Mesen).
            gapEnded = true;
            needIrq = false;
        }

        if (gapEnded)
        {
            transferComplete = true;
            readDataReg = diskData;
            if (needIrq)
                diskIrqPending = true;
        }
    }
    else
    {
        // Write mode: ghi vào diskSides trong RAM + đánh dấu diskDirty. Việc lưu ra file
        // do phía gọi làm qua BuildDiskPatch() (xem Mapper_020.h). Không có write-protect
        // cứng, theo đúng Mesen (chỉ báo not-writable khi không có đĩa).
        if (!crcControl)
        {
            transferComplete = true;
            diskData = writeDataLatch;
            if (needIrq)
                diskIrqPending = true;
        }
        if (!diskReady)
            diskData = 0x00;

        if (!crcControl)
        {
            UpdateCrc(diskData);
        }
        else
        {
            if (!previousCrcControlFlag)
            {
                UpdateCrc(0x00);
                UpdateCrc(0x00);
            }
            diskData = uint8_t(crcAccumulator & 0xFF);
            crcAccumulator >>= 8;
        }

        // Mesen ghi lệch 2 byte về phía sau (đầu ghi thật sự chậm hơn đầu đọc
        // 2 byte do độ trễ tính CRC) - giữ nguyên hành vi để tương thích, dù
        // dữ liệu ghi không persist ra file thật.
        if (diskPosition >= 2 && diskPosition - 2 < side.size())
        {
            uint8_t& dst = side[diskPosition - 2];
            if (dst != diskData)
            {
                dst = diskData;
                diskDirty = true; // chỉ đánh dấu khi byte thật sự đổi
            }
        }

        gapEnded = false;
    }

    previousCrcControlFlag = crcControl;
    diskPosition++;

    if (diskPosition >= side.size())
    {
        motorOn = false;
    }
    else
    {
        delay = 150;
    }
}

void Mapper_020::SetBIOS(const std::vector<uint8_t>& biosData)
{
    bios = biosData;
    bios.resize(8192, 0xFF);
}

uint8_t Mapper_020::ReadBios(uint16_t addr) const
{
    if (bios.empty())
        return 0xFF;

    uint32_t offset = uint32_t(addr) - 0xE000;
    if (offset < bios.size())
        return bios[offset];

    return 0xFF;
}

void Mapper_020::SetDiskSides(std::vector<std::vector<uint8_t>> sides)
{
    // Lưu THẲNG dữ liệu thô, không tổng hợp gap/marker/CRC gì thêm (khác
    // hoàn toàn thiết kế cũ - xem giải thích ở đầu Mapper_020.h).
    diskSides = std::move(sides);
    originalSides = diskSides;
    diskDirty = false;
    currentSide = diskSides.empty() ? -1 : 0;

    endOfHead = true;
    scanningDisk = false;
    gapEnded = false;
    previousCrcControlFlag = false;
    transferComplete = false;
    delay = 0;
    diskPosition = 0;
    crcAccumulator = 0;
}

// Patch IPS trên dữ liệu các side nối liền nhau (offset = tổng kích thước các side trước + vị trí).
std::vector<uint8_t> Mapper_020::BuildDiskPatch() const
{
    std::vector<uint8_t> out;
    if (originalSides.size() != diskSides.size())
        return out;

    auto put24 = [&](size_t v) { out.push_back(uint8_t(v >> 16)); out.push_back(uint8_t(v >> 8)); out.push_back(uint8_t(v)); };
    auto put16 = [&](size_t v) { out.push_back(uint8_t(v >> 8)); out.push_back(uint8_t(v)); };

    const char* magic = "PATCH";
    bool any = false;
    size_t base = 0;

    for (size_t s = 0; s < diskSides.size(); ++s)
    {
        const auto& cur = diskSides[s];
        const auto& org = originalSides[s];
        if (cur.size() != org.size())
            return std::vector<uint8_t>(); // kích thước side đổi -> không thể vá bằng IPS an toàn

        size_t i = 0;
        while (i < cur.size())
        {
            if (cur[i] == org[i]) { ++i; continue; }

            size_t start = i;
            size_t last = i;      // byte khác cuối cùng đã thấy
            while (i < cur.size() && (i - last) <= 4 && (i - start) < 0xFFFF)
            {
                if (cur[i] != org[i]) last = i;
                ++i;
            }
            size_t len = last - start + 1;

            if (!any)
            {
                for (int k = 0; k < 5; ++k) out.push_back(uint8_t(magic[k]));
                any = true;
            }
            put24(base + start);
            put16(len);
            out.insert(out.end(), cur.begin() + start, cur.begin() + start + len);
            i = last + 1;
        }
        base += cur.size();
    }

    if (any)
    {
        out.push_back('E'); out.push_back('O'); out.push_back('F');
    }
    return out;
}

// Áp IPS lên bản sao; chỉ khi cả patch hợp lệ mới thay vào diskSides (tránh đĩa bị vá dở dang).
static bool ApplyIpsToSides(std::vector<std::vector<uint8_t>>& sides, const std::vector<uint8_t>& ips)
{
    if (ips.size() < 8 || ips[0] != 'P' || ips[1] != 'A' || ips[2] != 'T' || ips[3] != 'C' || ips[4] != 'H')
        return false;

    auto writeAbs = [&](size_t abs, uint8_t v) -> bool
        {
            size_t base = 0;
            for (auto& side : sides)
            {
                if (abs < base + side.size()) { side[abs - base] = v; return true; }
                base += side.size();
            }
            return false;
        };

    size_t p = 5;
    while (p + 3 <= ips.size())
    {
        if (ips[p] == 'E' && ips[p + 1] == 'O' && ips[p + 2] == 'F')
            return true;

        if (p + 5 > ips.size()) return false;
        size_t off = (size_t(ips[p]) << 16) | (size_t(ips[p + 1]) << 8) | ips[p + 2];
        size_t len = (size_t(ips[p + 3]) << 8) | ips[p + 4];
        p += 5;

        if (len == 0) // RLE
        {
            if (p + 3 > ips.size()) return false;
            size_t rle = (size_t(ips[p]) << 8) | ips[p + 1];
            uint8_t v = ips[p + 2];
            p += 3;
            for (size_t k = 0; k < rle; ++k)
                if (!writeAbs(off + k, v)) return false;
        }
        else
        {
            if (p + len > ips.size()) return false;
            for (size_t k = 0; k < len; ++k)
                if (!writeAbs(off + k, ips[p + k])) return false;
            p += len;
        }
    }
    return false; 
}

bool Mapper_020::ApplyDiskPatch(const std::vector<uint8_t>& ips)
{
    auto work = diskSides;
    if (!ApplyIpsToSides(work, ips))
        return false;
    diskSides = std::move(work);
    return true;
}

void Mapper_020::InsertDisk(int sideIndex)
{
    if (sideIndex < 0 || sideIndex >= int(diskSides.size()))
        currentSide = -1;
    else
        currentSide = sideIndex;

    endOfHead = true;
    scanningDisk = false;
    gapEnded = false;
    previousCrcControlFlag = false;
    transferComplete = false;
    delay = 0;
    diskPosition = 0;
    crcAccumulator = 0;
}

// ============================================================================
// FDS Expansion Audio
// ============================================================================
// Port theo NesDev wiki "FDS audio" + đối chiếu Mesen::FdsAudio. Chỉ gate
// bởi soundRegsEnabled ($4023 bit1) - KHÔNG động vào diskRegsEnabled/
// currentSide/motorOn, nên NSF (không có đĩa, không có BIOS) vẫn phát được
// âm thanh FDS bình thường như đã bàn ở phần trước.

bool Mapper_020::WriteSoundReg(uint16_t addr, uint8_t data)
{
    if (!soundRegsEnabled)
        return true; // HW thật: ghi bị bỏ qua khi bit chưa bật, nhưng vẫn "handled"

    if (addr >= 0x4040 && addr <= 0x407F)
    {
        if (waveWriteEnable)
            waveTable[addr - 0x4040] = data & 0x3F;
        return true;
    }

    switch (addr)
    {
    case 0x4080: // Volume envelope
        volEnvDisable = (data & 0x80) != 0;
        volEnvDirection = (data & 0x40) != 0;
        volEnvSpeed = data & 0x3F;
        envVolCounter = 8 * (volEnvSpeed + 1) * int32_t(masterEnvSpeed); // ghi $4080 reset timer
        if (volEnvDisable)
            volGain = data & 0x3F; // direct: gain = giá trị ghi; envelope sau đó chạy tiếp TỪ gain này
        return true;

    case 0x4082: // Freq low
        waveFreq = (waveFreq & 0x0F00) | data;
        return true;

    case 0x4083: // Freq high + halt + envelope disable
        waveFreq = (waveFreq & 0x00FF) | (uint16_t(data & 0x0F) << 8);
        envelopesDisabled = (data & 0x40) != 0;
        waveHalt = (data & 0x80) != 0;
        if (waveHalt)
        {
            wavePos = 0;
            waveAccum = 0; // halt reset cả accumulator, không chỉ vị trí
        }
        if (envelopesDisabled) // bit6 reset timer của cả 2 envelope
        {
            envVolCounter = 8 * (volEnvSpeed + 1) * int32_t(masterEnvSpeed);
            modEnvCounter = 8 * (modEnvSpeed + 1) * int32_t(masterEnvSpeed);
        }
        return true;

    case 0x4084: // Mod envelope
        modEnvDisable = (data & 0x80) != 0;
        modEnvDirection = (data & 0x40) != 0;
        modEnvSpeed = data & 0x3F;
        modEnvCounter = 8 * (modEnvSpeed + 1) * int32_t(masterEnvSpeed); // ghi $4084 reset timer
        if (modEnvDisable)
            modAmplitude = data & 0x3F;
        return true;

    case 0x4085: // Mod counter direct write
        modCounter = signExtend7(data);
        modAccum = 0;
        return true;

    case 0x4086: // Mod freq low
        modFreq = (modFreq & 0x0F00) | data;
        return true;

    case 0x4087: // Mod freq high + halt
        modFreq = (modFreq & 0x00FF) | (uint16_t(data & 0x0F) << 8);
        modHalt = (data & 0x80) != 0;
        if (modHalt) { modAccum = 0; modPos = 0; }
        return true;

    case 0x4088: // Mod table write (chỉ khi mod halt)
        if (modHalt)
        {
            modTable[modTableWritePos] = data & 0x07;
            modTable[(modTableWritePos + 1) & 0x3F] = data & 0x07;
            modTableWritePos = (modTableWritePos + 2) & 0x3F;
        }
        return true;

    case 0x4089: // Wave write enable + master volume
        waveWriteEnable = (data & 0x80) != 0;
        masterVolume = data & 0x03;
        return true;

    case 0x408A: // Envelope speed unit
        masterEnvSpeed = data;
        return true;
    }

    return false;
}

bool Mapper_020::ReadSoundReg(uint16_t addr, uint8_t& data)
{
    if (addr >= 0x4040 && addr <= 0x407F)
    {
        data = waveTable[addr - 0x4040] & 0x3F;
        return true;
    }
    if (addr == 0x4090)
    {
        data = (volGain & 0x3F) | 0x40; // volume gain hiện tại, bit6 luôn set theo HW thật
        return true;
    }
    if (addr == 0x4092)
    {
        data = (uint8_t(modAmplitude) & 0x3F) | 0x40; // $4092 = sweep/mod GAIN (không phải mod counter)
        return true;
    }
    return false;
}

void Mapper_020::ClockAudio()
{
    if (!soundRegsEnabled)
        return;

    // ---- Envelopes: chu ky = 8 * (speed+1) * $408A ----
    // Wave halt ($4083 bit7) cũng dừng luôn envelope.
    if (!waveHalt && !envelopesDisabled && masterEnvSpeed != 0)
    {
        if (!volEnvDisable && --envVolCounter <= 0)
        {
            envVolCounter = 8 * (volEnvSpeed + 1) * int32_t(masterEnvSpeed);
            if (volEnvDirection) { if (volGain < 32) volGain++; }
            else { if (volGain > 0) volGain--; }
        }
        if (!modEnvDisable && --modEnvCounter <= 0)
        {
            modEnvCounter = 8 * (modEnvSpeed + 1) * int32_t(masterEnvSpeed);
            if (modEnvDirection) { if (modAmplitude < 32) modAmplitude++; }
            else { if (modAmplitude > 0) modAmplitude--; }
        }
    }

    // ---- Modulator: moi lan tran accum doc 1 o bang mod (64 o) ----
    if (!modHalt && modFreq != 0)
    {
        modAccum += modFreq;
        if (modAccum >= 0x10000)
        {
            modAccum &= 0xFFFF;
            static const int8_t adj[8] = { 0, 1, 2, 4, 0, -4, -2, -1 };
            uint8_t v = modTable[modPos] & 0x07;
            modPos = (modPos + 1) & 0x3F;
            if (v == 4)
                modCounter = 0;
            else
                modCounter = signExtend7(uint32_t(modCounter + adj[v]));
        }
    }

    // ---- Wave oscillator, bend theo cong thuc phan cung ----
    if (!waveHalt && waveFreq != 0)
    {
        int32_t temp = modCounter * modAmplitude;
        int32_t rem = temp & 0x0F;
        temp >>= 4;
        if (rem > 0 && (temp & 0x80) == 0)
            temp += (modCounter < 0) ? -1 : 2;
        if (temp >= 192) temp -= 256;
        else if (temp < -64) temp += 256;

        temp = int32_t(waveFreq) * temp;
        rem = temp & 0x3F;
        temp >>= 6;
        if (rem >= 32) temp += 1;

        int32_t effFreq = int32_t(waveFreq) + temp;
        if (effFreq < 0) effFreq = 0;

        waveAccum += uint32_t(effFreq);
        if (waveAccum >= 0x10000)
        {
            waveAccum &= 0xFFFF;
            wavePos = (wavePos + 1) & 0x3F;
        }
    }

    // ---- Output + lowpass ~2kHz (tinh moi CPU cycle, khong phu thuoc sample rate) ----
    static const float masterDiv[4] = { 1.0f, 2.0f / 3.0f, 0.5f, 0.4f };
    int vol = volGain;
    if (vol > 32) vol = 32;
    float x = float((waveTable[wavePos] & 0x3F) - 32) * float(vol) * masterDiv[masterVolume & 3] / 1024.0f;
    lpOut += (x - lpOut) * 0.0070f; // 1 - exp(-2*pi*2000/1789773)
}

float Mapper_020::GetExpansionAudio()
{
    if (!soundRegsEnabled)
        return 0.0f;
    // FDS_OUTPUT_SCALE: hạ mức ra của FDS xuống ngang tầm APU. `lpOut` đang có
    // biên độ tối đa ±1.0 (wave đầy thang * volume 32 * master 100%), trong khi
    // APU thường chỉ ở mức ~0..1 cho CẢ 5 kênh cộng lại, nên FDS lấn át NES.
    // Chỉnh hằng số này theo tai nếu vẫn thấy to/nhỏ (0.25 = điểm khởi đầu).
    static constexpr float FDS_OUTPUT_SCALE = 0.15f;
    return lpOut * gainFds * FDS_OUTPUT_SCALE;
}

void Mapper_020::SetChannelGain(const QString& /*channelId*/, float gain)
{
    // Hiện tại FDS chỉ có 1 kênh audio duy nhất (wave + modulator hoà làm
    // một), nên mọi channelId bắt đầu bằng "fds." đều chỉnh chung 1 gain.
    // Nếu sau này tách riêng gain cho wave/mod, so sánh channelId ở đây.
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 2.0f) gain = 2.0f;
    gainFds = gain;
}

void Mapper_020::GetFdsDebugChannels(float& wave)
{
    // Kênh im lặng thật sự -> sóng về 0 (đường thẳng giữa), không vẽ tiếp
    // wavetable "thô" đang quay: soundRegs tắt, wave bị halt ($4083 bit7),
    // tần số = 0, hoặc đang ở chế độ ghi wavetable ($4089 bit7 giữ kênh lại).
    if (!soundRegsEnabled || waveHalt || waveFreq == 0 || waveWriteEnable)
    {
        wave = 0.0f;
        return;
    }

    // Biên độ theo volume hiện tại (envelope hoặc volume trực tiếp, 0..32) để
    // khi note tắt / volume = 0 thì sóng tắt dần về 0 đúng như tai nghe được.
    int vol = volGain;
    if (vol > 32) vol = 32;
    if (vol < 0) vol = 0;

    // Hình dạng wavetable vẫn giữ nguyên (chuẩn hoá -1..1), chỉ nhân volume.
    wave = ((float(waveTable[wavePos] & 0x3F) - 32.0f) / 32.0f) * (float(vol) / 32.0f);
}

void Mapper_020::GetFdsDebugPeriod(float& period, float outputRate)
{
    if (outputRate < 8000.0f) outputRate = 44100.0f;
    const float CPU_TO_SAMPLE = outputRate / 1789773.0f;

    if (waveHalt || waveFreq == 0 || !soundRegsEnabled)
    {
        period = 0.0f;
        return;
    }

    // 1 chu kỳ sóng đầy đủ = 64 bước wavePos, mỗi bước cần waveAccum tích
    // luỹ đủ 0x10000 mới nhảy - tổng số CPU cycle cho 1 chu kỳ hoàn chỉnh:
    float cyclesPerStep = 65536.0f / float(waveFreq);
    float totalCycles = cyclesPerStep * 64.0f;
    float samples = totalCycles * CPU_TO_SAMPLE;

    if (samples < 2.0f) samples = 0.0f;
    if (samples > 8192.0f) samples = 8192.0f;
    period = samples;
}

std::string Mapper_020::GetDebugInfo()
{
    std::string s;
    s += "===== Famicom Disk System / Mapper 020 =====\n";
    s += "Disk sides: " + std::to_string(diskSides.size()) + "\n";
    s += "Current side: " + std::to_string(currentSide) + "\n";
    s += "Disk position: " + std::to_string(diskPosition) + "\n";
    s += "Motor: " + std::string(motorOn ? "ON" : "OFF") + "\n";
    s += "Scanning: " + std::string(scanningDisk ? "YES" : "NO") + "\n";
    s += "Transfer mode: " + std::string(readMode ? "READ" : "WRITE") + "\n";
    s += "Disk regs enabled: " + std::string(diskRegsEnabled ? "YES" : "NO") + "\n";
    s += "BIOS loaded: " + std::string(bios.empty() ? "NO" : "YES") + "\n";
    return s;
}