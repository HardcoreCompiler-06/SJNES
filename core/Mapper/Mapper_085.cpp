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

#include "Mapper_085.h"
#include <QString>
#include <algorithm>
#include <cmath>

// Quy đổi gain (0.0-1.0) sang số step attenuation CỘNG THÊM vào nibble volume
// gốc của game. VRC7 volume register: 0 = to nhất, 15 = im lặng gần hoàn
// toàn, mỗi step ~3dB. gain=1.0 -> 0 step thêm (giữ nguyên), gain=0.0 ->
// +15 step (ép về im lặng, tương đương cơ chế mute).
static uint8_t Vrc7GainToExtraAtten(float gain)
{
	gain = std::clamp(gain, 0.0f, 1.0f);
	return uint8_t(std::lround((1.0f - gain) * 15.0f));
}

Mapper_085::Mapper_085(uint8_t prgBanks, uint8_t chrBanks)
	: Mapper(prgBanks, chrBanks)
{
	opll = OPLL_new(3579545, 44100);

	if (opll)
	{
		OPLL_setChipType(opll, 1); // 1 = VRC7 (chip type, KHONG phai enum tone)
		OPLL_resetPatch(opll, OPLL_VRC7_TONE);
	}
	reset();
}

void Mapper_085::SetSampleRate(double hz)
{
	if (!opll || hz < 8000.0 || hz == opllSampleRate)
		return;

	opllSampleRate = hz;
	OPLL_setRate(opll, uint32_t(hz + 0.5));
}
Mapper_085::~Mapper_085()
{
	if (opll)
	{
		OPLL_delete(opll);
		opll = nullptr;
	}
}
void Mapper_085::reset()
{
	uint8_t total8K = nPRGBanks * 2;

	prgBank[0] = 0;
	prgBank[1] = 1;
	prgBank[2] = total8K >= 2 ? total8K - 2 : 0;

	for (int i = 0; i < 8; i++)
		chrBank[i] = i;

	mirrorMode = VERTICAL;

	irqEnabled = false;
	irqEnableAfterAck = false;
	irqCycleMode = false;
	irqPending = false;
	irqLatch = 0;
	irqCounter = 0;
	irqPrescaler = 341;
	totalIrqStepCalls = 0;
	lastFireCycle = 0;

	audioAddr = 0;
	audioDisabled = false;
	for (int i = 0; i < 0x40; i++)
		audioRegs[i] = 0;
	lastVrc7Sample = 0.0f;

	for (int i = 0; i < 6; i++)
	{
		lastVrc7Debug[i] = 0.0f;
		lastVrc7Phase[i] = 0.0f;
		lastVrc7Period[i] = 0.0f;
		lastVrc7PhaseValid[i] = false;
	}

	if (opll)
	{
		OPLL_reset(opll);
		OPLL_setChipType(opll, 1); // 1 = VRC7
		OPLL_resetPatch(opll, OPLL_VRC7_TONE);
	}
}

bool Mapper_085::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
	if (addr < 0x8000)
		return false;

	uint32_t total8KBanks = uint32_t(nPRGBanks) * 2;
	if (total8KBanks == 0)
		total8KBanks = 1;

	uint32_t bank = 0;

	if (addr >= 0x8000 && addr <= 0x9FFF)
	{
		bank = prgBank[0] % total8KBanks;
		mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
		return true;
	}

	if (addr >= 0xA000 && addr <= 0xBFFF)
	{
		bank = prgBank[1] % total8KBanks;
		mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
		return true;
	}

	if (addr >= 0xC000 && addr <= 0xDFFF)
	{
		bank = prgBank[2] % total8KBanks;
		mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
		return true;
	}

	// $E000-$FFFF fixed last 8KB
	bank = total8KBanks - 1;
	mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
	return true;
}

bool Mapper_085::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
	mapped_addr = 0;

	if ((addr & 0xF030) == 0x9030)
	{
		audioRegs[audioAddr] = data;

		uint8_t sendData = data;

		// Thanh ghi Instrument/Volume: $30-$36 (1 thanh ghi / kênh, 6 kênh).
		// Nếu kênh đang bị mute, ép volume nibble (bit 0-3) về 0x0F (attenuation
		// tối đa ~45dB) trước khi gửi cho OPLL, giữ nguyên instrument nibble.
		if (audioAddr >= 0x30 && audioAddr <= 0x36)
		{
			int ch = audioAddr - 0x30;
			if (ch >= 0 && ch < 6)
			{
				if (muteChannel[ch])
					sendData = data | 0x0F;
				else
				{
					uint8_t extraAtten = Vrc7GainToExtraAtten(gainVrc7[ch]);
					uint8_t nibble = uint8_t(std::min(15, (data & 0x0F) + extraAtten));
					sendData = (data & 0xF0) | nibble;
				}
			}
		}

		// Thanh ghi Key-on/Sustain/Octave/F-num-hi: $20-$25 (1 thanh ghi / kênh).
		// Chỉ ép volume KHÔNG đủ vì đó là attenuation (~45dB tối đa), vẫn còn
		// nghe "lí nhí". Phải ép luôn bit4 (key-on) về 0 để envelope thật sự
		// release về im lặng hoàn toàn.
		if (audioAddr >= 0x20 && audioAddr <= 0x25)
		{
			int ch = audioAddr - 0x20;
			if (ch >= 0 && ch < 6 && muteChannel[ch])
				sendData = data & ~0x10;
		}

		if (opll)
		{
			OPLL_writeIO(opll, 0, audioAddr);
			OPLL_writeIO(opll, 1, sendData);
		}

		return true;
	}

	uint16_t r = addr & 0xF010;

	switch (r)
	{
	case 0x8000:
		prgBank[0] = data & 0x3F;
		return true;

	case 0x8010:
		prgBank[1] = data & 0x3F;
		return true;

	case 0x9000:
		prgBank[2] = data & 0x3F;
		return true;

		// Audio VRC7: cần check bằng addr gốc vì $9010 trùng PRG/audio nếu chỉ mask 0xF010
	case 0x9010:
		audioAddr = data & 0x3F;
		return true;

		// CHR banks
	case 0xA000:
		chrBank[0] = data;
		return true;

	case 0xA010:
		chrBank[1] = data;
		return true;

	case 0xB000:
		chrBank[2] = data;
		return true;

	case 0xB010:
		chrBank[3] = data;
		return true;

	case 0xC000:
		chrBank[4] = data;
		return true;

	case 0xC010:
		chrBank[5] = data;
		return true;

	case 0xD000:
		chrBank[6] = data;
		return true;

	case 0xD010:
		chrBank[7] = data;
		return true;

	case 0xE000:
	{
		// Bit 6: tắt âm thanh mở rộng. Theo nesdev, khi bit này được set thì chip
		// im lặng và xóa thanh ghi (kể cả LFO tremolo, không kể LFO vibrato).
		// Ở đây reset cả OPLL (xóa luôn vibrato, lệch rất nhỏ so với máy thật).
		const bool disable = (data & 0x40) != 0;
		if (disable && !audioDisabled)
		{
			for (int i = 0; i < 0x40; i++)
				audioRegs[i] = 0;
			audioAddr = 0;
			lastVrc7Sample = 0.0f;

			if (opll)
			{
				OPLL_reset(opll);
				OPLL_setChipType(opll, 1);
				OPLL_resetPatch(opll, OPLL_VRC7_TONE);
			}
		}
		audioDisabled = disable;

		uint8_t m = data & 0x03;

		if (m == 0)
			mirrorMode = VERTICAL;
		else if (m == 1)
			mirrorMode = HORIZONTAL;
		else if (m == 2)
			mirrorMode = ONESCREEN_LO;
		else
			mirrorMode = ONESCREEN_HI;

		return true;
	}

	case 0xE010:
		irqLatch = data;
		return true;

	case 0xF000:
		irqEnabled = (data & 0x02) != 0;
		irqEnableAfterAck = (data & 0x01) != 0;
		irqCycleMode = (data & 0x04) != 0; // Bit2: 0=scanline mode, 1=cycle mode (chuẩn VRC7 thật)
		irqPending = false;

		if (irqEnabled)
		{
			irqCounter = irqLatch;
			irqPrescaler = 341;
		}

		return true;

	case 0xF010:
		irqPending = false;
		irqEnabled = irqEnableAfterAck;

		return true;
	}

	return false;
}

bool Mapper_085::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
	if (addr < 0x2000)
	{
		// VRC7 vẫn bank CHR-RAM theo 1KB.
		// Nếu nCHRBanks == 0 thì coi như có 8 bank 1KB CHR-RAM.
		uint32_t total1KBanks = (nCHRBanks == 0)
			? 8
			: uint32_t(nCHRBanks) * 8;

		uint8_t slot = addr / 0x0400;
		uint32_t bank = chrBank[slot] % total1KBanks;

		mapped_addr = bank * 0x0400 + (addr & 0x03FF);
		return true;
	}

	return false;
}

bool Mapper_085::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
	if (addr < 0x2000)
	{
		// Chỉ cho ghi khi là CHR-RAM
		if (nCHRBanks == 0)
		{
			uint32_t total1KBanks = 8;

			uint8_t slot = addr / 0x0400;
			uint32_t bank = chrBank[slot] % total1KBanks;

			mapped_addr = bank * 0x0400 + (addr & 0x03FF);
			return true;
		}

		return false;
	}

	return false;
}
void Mapper_085::irqStep()
{
	totalIrqStepCalls++; // tương đương clock_count của CPU, vì hàm này gọi đúng 1 lần/CPU cycle

	if (!irqEnabled)
		return;

	if (irqCycleMode)
	{
		if (irqCounter == 0xFF)
		{
			irqCounter = irqLatch;
			irqPending = true;
		}
		else
		{
			irqCounter++;
		}
		return;
	}

	// Scanline mode (xấp xỉ nhịp 1 scanline ~113.67 CPU cycle bằng prescaler 341 PPU-dot-tương-đương)
	irqPrescaler -= 3;

	while (irqPrescaler <= 0)
	{
		irqPrescaler += 341;

		if (irqCounter == 0xFF)
		{
			irqCounter = irqLatch;
			irqPending = true;
		}
		else
		{
			irqCounter++;
		}
	}
}

bool Mapper_085::irqState()
{
	return irqPending;
}

void Mapper_085::irqClear()
{
	irqPending = false;
}

void Mapper_085::SetVrc7ChannelMuted(int ch, bool muted)
{
	if (ch < 0 || ch >= 6)
		return;

	muteChannel[ch] = muted;

	if (!opll)
		return;

	// Gửi lại NGAY 2 thanh ghi của kênh này với trạng thái mute mới, không
	// đợi game tự ghi lại (ví dụ note đang giữ dài, không ghi thêm gì).

	// 1) Volume: ép attenuation tối đa khi mute, khôi phục gốc khi unmute.
	uint8_t volReg = 0x30 + ch;
	uint8_t rawVol = audioRegs[volReg];
	uint8_t sendVol = muted ? (rawVol | 0x0F) : rawVol;
	OPLL_writeIO(opll, 0, volReg);
	OPLL_writeIO(opll, 1, sendVol);

	// 2) Key-on: chỉ ép volume không đủ tắt hẳn (attenuation ~45dB max vẫn
	// nghe "lí nhí"), phải ép luôn key-off để envelope release về 0 thật.
	// Khi unmute, khôi phục đúng bit key-on gốc (nếu game vẫn đang giữ note).
	uint8_t keyReg = 0x20 + ch;
	uint8_t rawKey = audioRegs[keyReg];
	uint8_t sendKey = muted ? (rawKey & ~0x10) : rawKey;
	OPLL_writeIO(opll, 0, keyReg);
	OPLL_writeIO(opll, 1, sendKey);
}

void Mapper_085::SetChannelGain(const QString& channelId, float gain)
{
	// channelId dạng "vrc7.ch1".."vrc7.ch6" -> index 0..5
	if (!channelId.startsWith("vrc7.ch"))
		return;

	bool ok = false;
	int chNumber = channelId.mid(7).toInt(&ok); // "vrc7.ch" dài 7 ký tự
	if (!ok || chNumber < 1 || chNumber > 6)
		return;

	int ch = chNumber - 1;
	gainVrc7[ch] = std::clamp(gain, 0.0f, 1.0f);

	if (!opll || muteChannel[ch])
		return; // đang mute thì để nguyên nibble ép cứng của mute, gain không cần đụng vào

	// Đẩy lại NGAY thanh ghi Volume với attenuation mới, không đợi game ghi lại
	// (giống hệt cơ chế SetVrc7ChannelMuted ở trên).
	uint8_t volReg = 0x30 + ch;
	uint8_t rawVol = audioRegs[volReg];
	uint8_t extraAtten = Vrc7GainToExtraAtten(gainVrc7[ch]);
	uint8_t nibble = uint8_t(std::min(15, (rawVol & 0x0F) + extraAtten));
	uint8_t sendVol = (rawVol & 0xF0) | nibble;

	OPLL_writeIO(opll, 0, volReg);
	OPLL_writeIO(opll, 1, sendVol);
}

float Mapper_085::GetExpansionAudio()
{
	if (!opll)
		return 0.0f;

	if (audioDisabled)
	{
		lastVrc7Sample = 0.0f;
		for (int i = 0; i < 6; i++)
			lastVrc7Debug[i] = 0.0f;
		return 0.0f;
	}

	int sample = OPLL_calc(opll);

	float out = float(sample) / 32768.0f;
	out *= 3.0f;

	lastVrc7Sample = out;

	// 6 kênh VRC7 riêng
	for (int i = 0; i < 6; i++)
	{
		if (muteChannel[i])
		{
			lastVrc7Debug[i] = 0.0f;
			continue;
		}

		lastVrc7Debug[i] = float(opll->ch_out[i]) / 32768.0f;

		lastVrc7Debug[i] *= 2.5f;
		const int carrierSlot = i * 2 + 1;
		const uint32_t DP_WIDTH_LOCAL = 1u << 19;
		float phase = float(opll->slot[carrierSlot].pg_phase & (DP_WIDTH_LOCAL - 1)) / float(DP_WIDTH_LOCAL);

		if (lastVrc7PhaseValid[i])
		{
			float delta = phase - lastVrc7Phase[i];
			if (delta <= 0.0f)
				delta += 1.0f;

			// delta là số vòng phase / 1 audio sample. Period = sample / vòng.
			// Bỏ qua delta bất thường khi key-on/reset phase.
			if (delta > 0.00001f && delta < 0.50f)
			{
				float p = 1.0f / delta;
				if (p >= 4.0f && p <= 8192.0f)
				{
					if (lastVrc7Period[i] <= 0.0f)
						lastVrc7Period[i] = p;
					else
						lastVrc7Period[i] = lastVrc7Period[i] * 0.82f + p * 0.18f;
				}
			}
		}

		lastVrc7Phase[i] = phase;
		lastVrc7PhaseValid[i] = true;
	}

	return out;
}

void Mapper_085::GetVrc7DebugChannels(
	float& ch1, float& ch2, float& ch3,
	float& ch4, float& ch5, float& ch6
)
{
	ch1 = lastVrc7Debug[0];
	ch2 = lastVrc7Debug[1];
	ch3 = lastVrc7Debug[2];
	ch4 = lastVrc7Debug[3];
	ch5 = lastVrc7Debug[4];
	ch6 = lastVrc7Debug[5];
}

void Mapper_085::GetVrc7DebugPeriods(
	float& ch1, float& ch2, float& ch3,
	float& ch4, float& ch5, float& ch6
)
{
	ch1 = lastVrc7Period[0];
	ch2 = lastVrc7Period[1];
	ch3 = lastVrc7Period[2];
	ch4 = lastVrc7Period[3];
	ch5 = lastVrc7Period[4];
	ch6 = lastVrc7Period[5];
}

void Mapper_085::GetVrc7DebugPhases(
	float& ch1, float& ch2, float& ch3,
	float& ch4, float& ch5, float& ch6
)
{
	ch1 = lastVrc7Phase[0];
	ch2 = lastVrc7Phase[1];
	ch3 = lastVrc7Phase[2];
	ch4 = lastVrc7Phase[3];
	ch5 = lastVrc7Phase[4];
	ch6 = lastVrc7Phase[5];
}

void Mapper_085::GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3)
{
	ch1 = lastVrc7Debug[0];
	ch2 = lastVrc7Debug[1];
	ch3 = lastVrc7Debug[2];
}

std::string Mapper_085::GetDebugInfo()
{
	std::string s;

	s += "===== Konami VRC7 / Mapper 85 =====\n";
	s += "PRG $8000: " + std::to_string(prgBank[0]) + "\n";
	s += "PRG $A000: " + std::to_string(prgBank[1]) + "\n";
	s += "PRG $C000: " + std::to_string(prgBank[2]) + "\n";
	s += "$E000: fixed last\n\n";

	s += "CHR Banks 1KB:\n";
	for (int i = 0; i < 8; i++)
		s += "CHR[" + std::to_string(i) + "] = " + std::to_string(chrBank[i]) + "\n";

	s += "\n";
	s += "Mirror: " + std::to_string(int(mirrorMode)) + "\n";
	s += "IRQ Enabled: " + std::to_string(irqEnabled) + "\n";
	s += "IRQ After Ack: " + std::to_string(irqEnableAfterAck) + "\n";
	s += "IRQ Pending: " + std::to_string(irqPending) + "\n";
	s += "IRQ Latch: " + std::to_string(irqLatch) + "\n";
	s += "IRQ Counter: " + std::to_string(irqCounter) + "\n";
	s += "Audio Addr: " + std::to_string(audioAddr) + "\n";

	return s;
}