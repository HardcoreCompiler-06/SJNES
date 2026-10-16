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

#include "Mapper_026.h"

Mapper_026::Mapper_026(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper_024(prgBanks, chrBanks)
{
}

bool Mapper_026::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    // VRC6b: A0 và A1 bị đảo vai trò so với VRC6a khi đấu vào chip.
    // Cách xử lý: hoán đổi 2 bit thấp của địa chỉ trước khi đưa vào
    // logic gốc của VRC6a (Mapper_024), tái sử dụng toàn bộ audio/banking.
    uint16_t bit0 = addr & 0x0001;
    uint16_t bit1 = addr & 0x0002;

    uint16_t swappedLow2 = (bit0 << 1) | (bit1 >> 1);
    uint16_t swappedAddr = (addr & 0xFFFC) | swappedLow2;

    return Mapper_024::cpuMapWrite(swappedAddr, mapped_addr, data);
}