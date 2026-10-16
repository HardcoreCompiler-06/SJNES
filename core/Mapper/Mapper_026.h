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
#include "Mapper_024.h"

// VRC6b (Konami chip 351949) — giống hệt VRC6a (Mapper_024) về mặt logic,
// chỉ khác cách đấu 2 chân địa chỉ A0/A1 trên board mạch.
class Mapper_026 : public Mapper_024
{
public:
    Mapper_026(uint8_t prgBanks, uint8_t chrBanks);

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
};