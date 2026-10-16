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

/*
	Derived from olc::NES - Cartridge
	by David Barr (javidx9), OneLoneCoder.com
	Source: https://github.com/OneLoneCoder/olcNES

	License (OLC-3)
	~~~~~~~~~~~~~~~

	Copyright 2018-2019 OneLoneCoder.com

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	1. Redistributions or derivations of source code must retain the above
	copyright notice, this list of conditions and the following disclaimer.

	2. Redistributions or derivative works in binary form must reproduce
	the above copyright notice. This list of conditions and the following
	disclaimer must be reproduced in the documentation and/or other
	materials provided with the distribution.

	3. Neither the name of the copyright holder nor the names of its
	contributors may be used to endorse or promote products derived
	from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "Mapper.h"
#include "BinaryIO.h"
class BinaryWriter;
class BinaryReader;

class Cartridge {
public:
    ~Cartridge();

    Cartridge(const std::string& sFileName);
    Cartridge(const std::vector<uint8_t>& romData);

    bool LoadFromFile(const std::string& sFileName);
    bool LoadFromData(const std::vector<uint8_t>& romData);
    bool ImageValid();
    bool ppuRead(uint16_t addr, uint8_t& data);
    bool ppuWrite(uint16_t addr, uint8_t data);
    bool cpuRead(uint16_t addr, uint8_t& data);
    bool cpuWrite(uint16_t addr, uint8_t data);
    MIRROR mirror = MIRROR::HORIZONTAL;
    std::vector<uint8_t> PRGRAM;

    // ---- Battery save (.sav) ----
    // bBattery: cờ đọc từ header iNES (mapper1 bit 1), báo hiệu game có
    // PRG-RAM có pin (battery-backed) cần lưu ra đĩa, ví dụ Kirby's Adventure,
    // Zelda, Final Fantasy...
    bool bBattery = false;
    // Đường dẫn file .sav tương ứng, được tính từ đường dẫn ROM (đổi đuôi
    // thành .sav). Rỗng nếu chưa gọi InitBatterySave().
    std::string sSavePath;

    // Gọi 1 lần ngay sau khi load ROM xong (ImageValid() == true), truyền vào
    // đường dẫn file ROM gốc (kể cả .zip). Hàm sẽ tự tính sSavePath và, nếu
    // bBattery == true, tự nạp save cũ (nếu file .sav đã tồn tại) vào PRGRAM.
    // Trả về true nếu có nạp được save cũ, false nếu không (bao gồm cả
    // trường hợp game không có battery, hoặc chưa từng có file save).
    bool InitBatterySave(const std::string& romPath);

    // Ghi PRGRAM hiện tại ra file sSavePath. Không làm gì nếu bBattery ==
    // false hoặc sSavePath rỗng. Gọi khi: đóng ROM, chuyển ROM khác, thoát
    // chương trình, hoặc autosave định kỳ.
    bool SavePRGRAMToFile();

    // Đọc file sSavePath (nếu tồn tại) và ghi đè vào đầu PRGRAM. Trả về false
    // (không phải lỗi) nếu file chưa tồn tại - trường hợp chơi game lần đầu.
    bool LoadPRGRAMFromFile();

    std::vector<uint8_t> vPRGMemory;
    std::vector<uint8_t> vCHRMemory;

    uint16_t nMapperID = 0;
    uint32_t nPRGBanks = 0;
    uint32_t nCHRBanks = 0;

    std::shared_ptr<Mapper> pMapper;
    Mapper* GetMapper()
    {
        return pMapper.get();
    }
    void SaveState(BinaryWriter& out) const;
    void LoadState(BinaryReader& in);

    // ---- FDS (Mapper 20) ----
    // BIOS disksys.rom là phần cứng của máy FDS, KHÔNG nằm trong file .fds của
    // từng game, nên nạp 1 lần dùng chung cho mọi Cartridge (static), thay vì
    // per-instance. Gọi hàm này 1 lần lúc khởi động app, trước khi load game
    // FDS đầu tiên (ví dụ trong SJNES constructor).
    static bool LoadFDSBios(const std::string& path);
    static bool IsFDSBiosLoaded();
private:
    bool bImageValid = false;
    static std::vector<uint8_t> s_fdsBios;
};