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

#include "Mapper_155.h"

Mapper_155::Mapper_155(uint8_t prgBanks, uint8_t chrBanks) : Mapper_001(prgBanks, chrBanks) {}

std::string Mapper_155::GetDebugInfo() {
    return "===== MAPPER 155 - MMC1A =====\n"
        "Giong MMC1 (001); PRG-RAM luon bat, bo qua bit WRAM disable.\n\n"
        + Mapper_001::GetDebugInfo();
}