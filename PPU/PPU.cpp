#include "PPU.h"
#include "Mapper_004.h"
#include "Mapper_005.h"
#include "Mapper_024.h"
// ==============================================================================
// KHỞI TẠO PPU & BẢNG MÀU
// ==============================================================================
PPU::PPU() {
    palScreen[0x00] = QColor(102, 102, 102);
    palScreen[0x01] = QColor(0, 42, 136);
    palScreen[0x02] = QColor(20, 18, 167);
    palScreen[0x03] = QColor(59, 0, 164);
    palScreen[0x04] = QColor(92, 0, 126);
    palScreen[0x05] = QColor(110, 0, 64);
    palScreen[0x06] = QColor(108, 6, 0);
    palScreen[0x07] = QColor(86, 29, 0);
    palScreen[0x08] = QColor(51, 53, 0);
    palScreen[0x09] = QColor(11, 72, 0);
    palScreen[0x0A] = QColor(0, 82, 0);
    palScreen[0x0B] = QColor(0, 79, 8);
    palScreen[0x0C] = QColor(0, 64, 77);
    palScreen[0x0D] = QColor(0, 0, 0);
    palScreen[0x0E] = QColor(0, 0, 0);
    palScreen[0x0F] = QColor(0, 0, 0);

    palScreen[0x10] = QColor(173, 173, 173);
    palScreen[0x11] = QColor(21, 95, 217);
    palScreen[0x12] = QColor(66, 64, 255);
    palScreen[0x13] = QColor(117, 39, 254);
    palScreen[0x14] = QColor(160, 26, 204);
    palScreen[0x15] = QColor(183, 30, 123);
    palScreen[0x16] = QColor(181, 49, 32);
    palScreen[0x17] = QColor(153, 78, 0);
    palScreen[0x18] = QColor(107, 109, 0);
    palScreen[0x19] = QColor(56, 135, 0);
    palScreen[0x1A] = QColor(12, 147, 0);
    palScreen[0x1B] = QColor(0, 143, 50);
    palScreen[0x1C] = QColor(0, 124, 141);
    palScreen[0x1D] = QColor(0, 0, 0);
    palScreen[0x1E] = QColor(0, 0, 0);
    palScreen[0x1F] = QColor(0, 0, 0);

    palScreen[0x20] = QColor(255, 254, 255);
    palScreen[0x21] = QColor(100, 176, 255);
    palScreen[0x22] = QColor(146, 144, 255);
    palScreen[0x23] = QColor(198, 118, 255);
    palScreen[0x24] = QColor(243, 106, 255);
    palScreen[0x25] = QColor(254, 110, 204);
    palScreen[0x26] = QColor(254, 129, 112);
    palScreen[0x27] = QColor(234, 158, 34);
    palScreen[0x28] = QColor(188, 190, 0);
    palScreen[0x29] = QColor(136, 216, 0);
    palScreen[0x2A] = QColor(92, 228, 48);
    palScreen[0x2B] = QColor(69, 224, 130);
    palScreen[0x2C] = QColor(72, 205, 222);
    palScreen[0x2D] = QColor(79, 79, 79);
    palScreen[0x2E] = QColor(0, 0, 0);
    palScreen[0x2F] = QColor(0, 0, 0);

    palScreen[0x30] = QColor(255, 254, 255);
    palScreen[0x31] = QColor(192, 223, 255);
    palScreen[0x32] = QColor(211, 210, 255);
    palScreen[0x33] = QColor(232, 200, 255);
    palScreen[0x34] = QColor(251, 194, 255);
    palScreen[0x35] = QColor(254, 196, 234);
    palScreen[0x36] = QColor(254, 204, 197);
    palScreen[0x37] = QColor(247, 216, 165);
    palScreen[0x38] = QColor(228, 229, 148);
    palScreen[0x39] = QColor(207, 239, 150);
    palScreen[0x3A] = QColor(189, 244, 171);
    palScreen[0x3B] = QColor(179, 243, 204);
    palScreen[0x3C] = QColor(181, 235, 242);
    palScreen[0x3D] = QColor(184, 184, 184);
    palScreen[0x3E] = QColor(0, 0, 0);
    palScreen[0x3F] = QColor(0, 0, 0);
}

PPU::~PPU() {}

void PPU::ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge) {
    this->cart = cartridge;
}

void PPU::reset() {
    fine_x = 0x00;
    address_latch = 0x00;
    ppu_data_buffer = 0x00;

    scanline = 0;
    cycle = 0;

    bg_next_tile_id = 0x00;
    bg_next_tile_attrib = 0x00;
    bg_next_tile_lsb = 0x00;
    bg_next_tile_msb = 0x00;

    bg_shifter_pattern_lo = 0x0000;
    bg_shifter_pattern_hi = 0x0000;
    bg_shifter_attrib_lo = 0x0000;
    bg_shifter_attrib_hi = 0x0000;

    status = 0x00;
    ppu_mask = 0x00;
    ppu_ctrl = 0x00;

    vram_addr.reg = 0x0000;
    tram_addr.reg = 0x0000;

    nmi_requested = false;
    oam_addr = 0x00;

    mapper_a12 = false;
    mapper_a12_low_cycles = 255;

    sprite_count = 0;

    for (int i = 0; i < 256; i++)
        OAM[i] = 0x00;

    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 1024; i++)
            tblName[t][i] = 0x00;

    for (int i = 0; i < 32; i++)
        tblPalette[i] = 0x00;

    for (int i = 0; i < 64; i++)
    {
        sprite_pattern_lo[i] = 0x00;
        sprite_pattern_hi[i] = 0x00;
        sprite_x[i] = 0x00;
        sprite_attribute[i] = 0x00;
        sprite_zero_being_rendered[i] = false;
    }

    for (int i = 0; i < 256 * 240; i++)
        frame_pixels[i] = palScreen[0x0F].rgb();
}

// ==============================================================================
// GIAO TIẾP VỚI CPU
// ==============================================================================
uint8_t PPU::cpuRead(uint16_t addr, bool rdonly) {
    uint8_t data = 0x00;
    if (rdonly) {
        switch (addr) {
        case 0x0000: data = ppu_ctrl; break;
        case 0x0001: data = ppu_mask; break;
        case 0x0002: data = status; break;
        case 0x0003: data = 0; break;
        case 0x0004: data = 0; break;
        case 0x0005: data = 0; break;
        case 0x0006: data = 0; break;
        case 0x0007: data = 0; break;
        }
    }
    else {
        switch (addr) {
        case 0x0000: break;
        case 0x0001: break;
        case 0x0002:
            data = (status & 0xE0) | (ppu_data_buffer & 0x1F);
            status &= ~0x80; address_latch = 0;
            break;
        case 0x0003: break;
        case 0x0004: data = OAM[oam_addr]; break;
        case 0x0005: break;
        case 0x0006: break;
        case 0x0007:
        {
            uint16_t addr = vram_addr.reg & 0x3FFF;

            if (addr >= 0x3F00)
            {
                // Palette read không bị delay
                data = ppuRead(addr);

                // Nhưng buffer vẫn được cập nhật từ nametable mirror bên dưới
                ppu_data_buffer = ppuRead(addr - 0x1000);
            }
            else
            {
                data = ppu_data_buffer;
                ppu_data_buffer = ppuRead(addr);
            }

            vram_addr.reg += (ppu_ctrl & 0x04) ? 32 : 1;
        }
        break;
        }
    }
    return data;
}

void PPU::cpuWrite(uint16_t addr, uint8_t data) {
    static int scrollLogCount = 0;

    auto logScrollWrite = [&](const char* regName, uint8_t value)
        {
            if (scrollLogCount < 300)
            {
                scrollLogCount++;
            }
        };
    switch (addr) {
    case 0x0000:
    {
        bool nmi_was_enabled = (ppu_ctrl & 0x80) > 0;

        ppu_ctrl = data;
        tram_addr.nametable_x = ppu_ctrl & 0x01;
        tram_addr.nametable_y = (ppu_ctrl & 0x02) >> 1;
        if (!nmi_was_enabled && (ppu_ctrl & 0x80) && (status & 0x80)) {
            nmi_requested = true;
        }
    }
    break;
    case 0x0001: ppu_mask = data; break;
    case 0x0002: break;
    case 0x0003: oam_addr = data; break;
    case 0x0004: OAM[oam_addr] = data; oam_addr++; break;
    case 0x0005: // PPUSCROLL
    {
        if (address_latch == 0)
        {
            fine_x = data & 0x07;
            tram_addr.coarse_x = data >> 3;
            address_latch = 1;
        }
        else
        {
            tram_addr.fine_y = data & 0x07;
            tram_addr.coarse_y = data >> 3;
            address_latch = 0;
        }
    }
    break;
    case 0x0006:
    {
        if (address_latch == 0) {
            tram_addr.reg = (uint16_t)((data & 0x3F) << 8) | (tram_addr.reg & 0x00FF);
            address_latch = 1;
        }
        else {
            tram_addr.reg = (tram_addr.reg & 0xFF00) | data;
            vram_addr = tram_addr;
            address_latch = 0;

            NotifyMapperA12(vram_addr.reg);
        }
    }
    break;
    case 0x0007:
        ppuWrite(vram_addr.reg, data);
        vram_addr.reg += (ppu_ctrl & 0x04) ? 32 : 1;
        break;
    }
}

//BỘ NHỚ BUS GIAO TIẾP VỚI CARD ĐỒ HỌA
uint8_t PPU::ppuRead(uint16_t addr, bool rdonly) {
    uint8_t data = 0x00;
    addr &= 0x3FFF;
    if (addr <= 0x1FFF)
        NotifyMapperA12(addr);
    if (cart && cart->ppuRead(addr, data)) return data;
    else if (addr <= 0x1FFF)
        data = tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF];
    else if (addr >= 0x2000 && addr <= 0x3EFF)
    {
        uint16_t ntAddr = addr & 0x0FFF;
        int ntIndex = ntAddr / 0x0400;
        uint16_t offset = ntAddr & 0x03FF;

        // =========================
        // MMC5 nametable mapping
        // =========================
        if (cart && cart->pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
            {
                uint8_t source = mmc5->GetNtSource(ntIndex);

                switch (source)
                {
                case 0:
                    data = tblName[0][offset];
                    return data;

                case 1:
                    data = tblName[1][offset];
                    return data;

                case 2:
                    data = mmc5->ReadExRam(offset);
                    return data;

                case 3:
                    if (offset < 0x03C0)
                        data = mmc5->GetFillTile();
                    else
                        data = mmc5->GetFillAttr();

                    return data;
                }
            }
        }
        // =========================
        // VRC6 nametable banking
        // =========================
        if (cart && cart->pMapper)
        {
            if (auto* vrc6 = dynamic_cast<Mapper_024*>(cart->pMapper.get()))
            {
                uint8_t source = vrc6->GetNtSource(ntIndex);
                data = tblName[source & 0x01][offset];
                return data;
            }
        }
        // =========================
        // Mirroring cũ cho mapper thường
        // =========================
        addr = ntAddr;
        MIRROR m = cart->mirror;
        if (cart->pMapper != nullptr) {
            MIRROR mapper_mirror = cart->pMapper->mirror();
            if (mapper_mirror != MIRROR::HARDWARE) {
                m = mapper_mirror;
            }
        }
        if (m == MIRROR::VERTICAL) {
            if (addr <= 0x03FF) data = tblName[0][addr & 0x03FF];
            else if (addr <= 0x07FF) data = tblName[1][addr & 0x03FF];
            else if (addr <= 0x0BFF) data = tblName[0][addr & 0x03FF];
            else data = tblName[1][addr & 0x03FF];
        }
        else if (m == MIRROR::HORIZONTAL) {
            if (addr <= 0x03FF) data = tblName[0][addr & 0x03FF];
            else if (addr <= 0x07FF) data = tblName[0][addr & 0x03FF];
            else if (addr <= 0x0BFF) data = tblName[1][addr & 0x03FF];
            else data = tblName[1][addr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_LO) {
            data = tblName[0][addr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_HI) {
            data = tblName[1][addr & 0x03FF];
        }
    }
    else if (addr >= 0x3F00) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000; if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008; if (addr == 0x001C) addr = 0x000C;
        data = tblPalette[addr] & 0x3F;
    }

    return data;
}

void PPU::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        if (cart && cart->ppuWrite(addr, data))
            return;

        tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF] = data;
    }
    else if (addr >= 0x2000 && addr <= 0x3EFF)
    {
        uint16_t ntAddr = addr & 0x0FFF;
        int ntIndex = ntAddr / 0x0400;
        uint16_t offset = ntAddr & 0x03FF;

        // =========================
        // MMC5 nametable mapping
        // =========================
        if (cart && cart->pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
            {
                uint8_t source = mmc5->GetNtSource(ntIndex);

                switch (source)
                {
                case 0:
                    tblName[0][offset] = data;
                    return;

                case 1:
                    tblName[1][offset] = data;
                    return;

                case 2:
                    // ExRAM làm nametable
                    mmc5->WriteExRam(offset, data);
                    return;

                case 3:
                    // Fill mode không ghi trực tiếp vào nametable
                    return;
                }
            }
        }
        // =========================
        // VRC6 nametable banking
        // =========================
        if (cart && cart->pMapper)
        {
            if (auto* vrc6 = dynamic_cast<Mapper_024*>(cart->pMapper.get()))
            {
                uint8_t source = vrc6->GetNtSource(ntIndex);
                tblName[source & 0x01][offset] = data;
                return;
            }
        }
        // =========================
        // Mirroring cũ cho mapper thường
        // =========================
        addr = ntAddr;
        MIRROR m = cart->mirror;
        if (cart->pMapper != nullptr) {
            MIRROR mapper_mirror = cart->pMapper->mirror();
            if (mapper_mirror != MIRROR::HARDWARE) {
                m = mapper_mirror;
            }
        }
        if (m == MIRROR::VERTICAL) {
            if (addr <= 0x03FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x07FF) tblName[1][addr & 0x03FF] = data;
            else if (addr <= 0x0BFF) tblName[0][addr & 0x03FF] = data;
            else tblName[1][addr & 0x03FF] = data;
        }
        else if (m == MIRROR::HORIZONTAL) {
            if (addr <= 0x03FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x07FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x0BFF) tblName[1][addr & 0x03FF] = data;
            else tblName[1][addr & 0x03FF] = data;
        }
        else if (m == MIRROR::ONESCREEN_LO) {
            tblName[0][addr & 0x03FF] = data;
        }
        else if (m == MIRROR::ONESCREEN_HI) {
            tblName[1][addr & 0x03FF] = data;
        }

    }
    else if (addr >= 0x3F00) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000; if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008; if (addr == 0x001C) addr = 0x000C;
        tblPalette[addr] = data & 0x3F;
    }
}


void PPU::LoadBackgroundShifters() {
    bg_shifter_pattern_lo = (bg_shifter_pattern_lo & 0xFF00) | bg_next_tile_lsb;
    bg_shifter_pattern_hi = (bg_shifter_pattern_hi & 0xFF00) | bg_next_tile_msb;
    bg_shifter_attrib_lo = (bg_shifter_attrib_lo & 0xFF00) | ((bg_next_tile_attrib & 0b01) ? 0xFF : 0x00);
    bg_shifter_attrib_hi = (bg_shifter_attrib_hi & 0xFF00) | ((bg_next_tile_attrib & 0b10) ? 0xFF : 0x00);
}

void PPU::UpdateShifters() {
    if (ppu_mask & 0x18) {
        bg_shifter_pattern_lo <<= 1; bg_shifter_pattern_hi <<= 1;
        bg_shifter_attrib_lo <<= 1; bg_shifter_attrib_hi <<= 1;
    }
}

//PHẦN CHÍNH CỦA CARD ĐỒ HỌA
void PPU::Step() {
    if (!mapper_a12 && mapper_a12_low_cycles < 255)
    {
        mapper_a12_low_cycles++;
    }
    if (cycle == 0)
    {
        if (cart && cart->pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
            {
                mmc5->NotifyScanline(scanline);
            }
        }
    }
    auto lam_IncScrollX = [&]() {
        if (ppu_mask & 0x18) {
            if (vram_addr.coarse_x == 31) { vram_addr.coarse_x = 0; vram_addr.nametable_x = ~vram_addr.nametable_x; }
            else { vram_addr.coarse_x++; }
        }
        };
    auto lam_IncScrollY = [&]() {
        if (ppu_mask & 0x18) {
            if (vram_addr.fine_y < 7) { vram_addr.fine_y++; }
            else {
                vram_addr.fine_y = 0;
                if (vram_addr.coarse_y == 29) { vram_addr.coarse_y = 0; vram_addr.nametable_y = ~vram_addr.nametable_y; }
                else if (vram_addr.coarse_y == 31) { vram_addr.coarse_y = 0; }
                else { vram_addr.coarse_y++; }
            }
        }
        };
    auto lam_TransAddrX = [&]() { if (ppu_mask & 0x18) { vram_addr.nametable_x = tram_addr.nametable_x; vram_addr.coarse_x = tram_addr.coarse_x; } };
    auto lam_TransAddrY = [&]() { if (ppu_mask & 0x18) { vram_addr.fine_y = tram_addr.fine_y; vram_addr.nametable_y = tram_addr.nametable_y; vram_addr.coarse_y = tram_addr.coarse_y; } };

    if (scanline >= -1 && scanline < 240) {
        if (scanline == -1 && cycle == 1) status &= ~0xE0;
        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338)) {
            UpdateShifters();

            if (ppu_mask & 0x18) {
                switch ((cycle - 1) % 8) {
                case 0:
                    LoadBackgroundShifters();
                    NotifyMapperA12(0x0000);
                    bg_next_tile_id = ppuRead(0x2000 | (vram_addr.reg & 0x0FFF));
                    break;
                case 2:
                {
                    NotifyMapperA12(0x0000);

                    bool usedMmc5ExtendedAttr = false;

                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                        {
                            if (mmc5->GetExRamMode() == 1)
                            {
                                uint16_t exOffset =
                                    (uint16_t(vram_addr.coarse_y) * 32)
                                    + vram_addr.coarse_x;

                                uint8_t ex = mmc5->ReadExRam(exOffset);

                                // bit 6-7: palette cho tile hiện tại
                                bg_next_tile_attrib = (ex >> 6) & 0x03;

                                // bit 0-5: CHR 4KB page cho tile hiện tại
                                mmc5->SetExtendedChrBank(ex & 0x3F);

                                usedMmc5ExtendedAttr = true;
                            }
                        }
                    }

                    if (!usedMmc5ExtendedAttr)
                    {
                        bg_next_tile_attrib = ppuRead(
                            0x23C0
                            | (vram_addr.nametable_y << 11)
                            | (vram_addr.nametable_x << 10)
                            | ((vram_addr.coarse_y >> 2) << 3)
                            | (vram_addr.coarse_x >> 2)
                        );

                        if (vram_addr.coarse_y & 0x02)
                            bg_next_tile_attrib >>= 4;

                        if (vram_addr.coarse_x & 0x02)
                            bg_next_tile_attrib >>= 2;

                        bg_next_tile_attrib &= 0x03;
                    }
                }
                break;
                case 4:
                {
                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                            mmc5->SetChrFetchModeBackground();
                    }

                    bg_next_tile_lsb = ppuRead(
                        ((ppu_ctrl & 0x10) ? 0x1000 : 0x0000)
                        + ((uint16_t)bg_next_tile_id << 4)
                        + vram_addr.fine_y
                        + 0
                    );
                }
                break;

                case 6:
                {
                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                            mmc5->SetChrFetchModeBackground();
                    }

                    bg_next_tile_msb = ppuRead(
                        ((ppu_ctrl & 0x10) ? 0x1000 : 0x0000)
                        + ((uint16_t)bg_next_tile_id << 4)
                        + vram_addr.fine_y
                        + 8
                    );
                }
                break;
                case 7: lam_IncScrollX(); break;
                }
            }
        }
        if (cycle == 256) lam_IncScrollY();
        if (cycle == 257) {
            LoadBackgroundShifters();
            lam_TransAddrX();

            if (ppu_mask & 0x18) {
                sprite_count = 0;

                int target_scanline = scanline + 1;

                // Không evaluate sprite ngoài vùng visible
                if (target_scanline >= 0 && target_scanline < 240) {
                    int spriteLimit = bRemoveSpriteLimit ? 64 : 8;
                    int foundSprites = 0;
                    for (int i = 0; i < 64; i++)
                    {
                        uint8_t sprite_y = OAM[i * 4 + 0];
                        uint8_t sprite_id = OAM[i * 4 + 1];
                        uint8_t sprite_attr = OAM[i * 4 + 2];
                        uint8_t sprite_x_pos = OAM[i * 4 + 3];

                        int spriteHeight = (ppu_ctrl & 0x20) ? 16 : 8;
                        int diffY = target_scanline - (sprite_y + 1);

                        if (diffY >= 0 && diffY < spriteHeight)
                        {
                            foundSprites++;

                            if (foundSprites > 8)
                                status |= 0x20; // sprite overflow, gần đúng

                            if (sprite_count >= spriteLimit)
                                continue;

                            bool flipY = (sprite_attr & 0x80) != 0;
                            int row = flipY ? (spriteHeight - 1 - diffY) : diffY;

                            uint16_t pattern_addr = 0;

                            if (spriteHeight == 8)
                            {
                                pattern_addr =
                                    ((ppu_ctrl & 0x08) ? 0x1000 : 0x0000) |
                                    ((uint16_t)sprite_id << 4) |
                                    row;
                            }
                            else
                            {
                                uint16_t table = (sprite_id & 0x01) ? 0x1000 : 0x0000;
                                uint16_t tile = (sprite_id & 0xFE);

                                if (row < 8)
                                    pattern_addr = table | (tile << 4) | row;
                                else
                                    pattern_addr = table | ((tile + 1) << 4) | (row - 8);
                            }

                            if (cart && cart->pMapper)
                            {
                                if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                                    mmc5->SetChrFetchModeSprite();
                            }

                            sprite_pattern_lo[sprite_count] = ppuRead(pattern_addr);
                            sprite_pattern_hi[sprite_count] = ppuRead(pattern_addr + 8);
                            sprite_x[sprite_count] = sprite_x_pos;
                            sprite_attribute[sprite_count] = sprite_attr;
                            sprite_zero_being_rendered[sprite_count] = (i == 0);

                            sprite_count++;
                        }
                    }
                }

                // dummy sprite fetch cho các slot còn lại
                for (int i = sprite_count; i < 8; i++) {
                    uint16_t dummy_addr = ((ppu_ctrl & 0x08) ? 0x1000 : 0x0000) | (0xFF << 4);

                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                            mmc5->SetChrFetchModeSprite();
                    }

                    ppuRead(dummy_addr);
                    ppuRead(dummy_addr + 8);
                }
            }
        }
        if (cycle == 338 || cycle == 340) {
            if (ppu_mask & 0x18) {
                NotifyMapperA12(0x0000);
                bg_next_tile_id = ppuRead(0x2000 | (vram_addr.reg & 0x0FFF));
            }
        }
        if (scanline == -1 && cycle >= 280 && cycle < 305) lam_TransAddrY();
    }

    if (scanline >= 0 && scanline < 240 && cycle >= 1 && cycle <= 256) {
        uint8_t bg_pixel = 0x00; uint8_t bg_palette = 0x00;
        if (ppu_mask & 0x08) {
            uint16_t bit_mux = 0x8000 >> fine_x;
            uint8_t p0_pixel = (bg_shifter_pattern_lo & bit_mux) > 0; uint8_t p1_pixel = (bg_shifter_pattern_hi & bit_mux) > 0;
            bg_pixel = (p1_pixel << 1) | p0_pixel;
            uint8_t bg_pal0 = (bg_shifter_attrib_lo & bit_mux) > 0; uint8_t bg_pal1 = (bg_shifter_attrib_hi & bit_mux) > 0;
            bg_palette = (bg_pal1 << 1) | bg_pal0;
        }

        uint8_t fg_pixel = 0x00; uint8_t fg_palette = 0x00; uint8_t fg_priority = 0; bool bSpriteZeroBeingRendered = false;
        if (ppu_mask & 0x10) {
            for (int i = 0; i < sprite_count; i++) {
                int diffX = (cycle - 1) - sprite_x[i];
                if (diffX >= 0 && diffX < 8) {
                    uint8_t flipX = (sprite_attribute[i] & 0x40) > 0;
                    int col = flipX ? (7 - diffX) : diffX;
                    uint8_t bit_mux = 0x80 >> col;

                    if ((sprite_pattern_lo[i] & bit_mux) || (sprite_pattern_hi[i] & bit_mux)) {
                        uint8_t p0_pixel = (sprite_pattern_lo[i] & bit_mux) > 0;
                        uint8_t p1_pixel = (sprite_pattern_hi[i] & bit_mux) > 0;
                        fg_pixel = (p1_pixel << 1) | p0_pixel;
                        fg_palette = (sprite_attribute[i] & 0x03) + 0x04;
                        fg_priority = (sprite_attribute[i] & 0x20) == 0;
                        bSpriteZeroBeingRendered = sprite_zero_being_rendered[i];
                        break;
                    }
                }
            }
            if (!(ppu_mask & 0x02) && (cycle >= 1 && cycle <= 8)) bg_pixel = 0;
            if (!(ppu_mask & 0x04) && (cycle >= 1 && cycle <= 8)) fg_pixel = 0;
        }

        if (bSpriteZeroBeingRendered && (ppu_mask & 0x18) == 0x18) {
            if (cycle >= 1 && cycle < 256) {
                if (bg_pixel > 0 && fg_pixel > 0) {
                    status |= 0x40;
                }
            }
        }
        uint8_t final_pixel = 0;
        uint8_t final_palette = 0;
        if (bg_pixel == 0 && fg_pixel == 0) {
            final_pixel = 0; final_palette = 0;
        }
        else
            if (bg_pixel == 0 && fg_pixel > 0) {
                final_pixel = fg_pixel; final_palette = fg_palette;
            }
            else
                if (bg_pixel > 0 && fg_pixel == 0) {
                    final_pixel = bg_pixel; final_palette = bg_palette;
                }
                else {
                    if (fg_priority) {
                        final_pixel = fg_pixel;
                        final_palette = fg_palette;
                    }
                    else {
                        final_pixel = bg_pixel;
                        final_palette = bg_palette;
                    }
                }

        uint8_t pal_idx = (final_pixel != 0)
            ? (ppuRead(0x3F00 + (final_palette << 2) + final_pixel) & 0x3F)
            : (ppuRead(0x3F00) & 0x3F);

        if (ppu_mask & 0x01)
            pal_idx &= 0x30;

        frame_pixels[scanline * 256 + (cycle - 1)] = palScreen[pal_idx].rgb();
    }

    if (scanline == 241 && cycle == 1) {
        status |= 0x80;
        if (ppu_ctrl & 0x80) nmi_requested = true;
    }

    cycle++;
    if (cycle >= 341) {
        cycle = 0;
        scanline++;
        if (scanline >= 261) {
            scanline = -1;
        }
    }

}

QImage PPU::GetScreen() {
    return QImage((const uchar*)frame_pixels, 256, 240, QImage::Format_RGB32);
}
void PPU::NotifyMapperA12(uint16_t addr)
{
    bool new_a12 = (addr & 0x1000) != 0;

    if (!mapper_a12 && new_a12)
    {
        if (mapper_a12_low_cycles >= 3)
        {
            if (cart && cart->pMapper)
            {
                cart->pMapper->ClockA12();
            }
        }

        mapper_a12_low_cycles = 0;
    }

    if (mapper_a12 && !new_a12)
    {
        mapper_a12_low_cycles = 0;
    }

    mapper_a12 = new_a12;
}
uint8_t PPU::GetOAMByte(uint8_t index) const
{
    return OAM[index];
}

uint8_t PPU::GetPPUCtrl() const
{
    return ppu_ctrl;
}

QColor PPU::GetNESColor(uint8_t index) const
{
    return palScreen[index & 0x3F];
}
uint8_t PPU::DebugPPURead(uint16_t addr)
{
    uint8_t data = 0x00;
    addr &= 0x3FFF;

    if (cart && cart->ppuRead(addr, data))
    {
        return data;
    }
    else if (addr <= 0x1FFF)
    {
        data = tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF];
    }
    else if (addr >= 0x2000 && addr <= 0x3EFF)
    {
        uint16_t ntAddr = addr & 0x0FFF;

        MIRROR m = cart->mirror;

        if (cart->pMapper != nullptr)
        {
            MIRROR mapper_mirror = cart->pMapper->mirror();
            if (mapper_mirror != MIRROR::HARDWARE)
            {
                m = mapper_mirror;
            }
        }

        if (m == MIRROR::VERTICAL)
        {
            if (ntAddr <= 0x03FF) data = tblName[0][ntAddr & 0x03FF];
            else if (ntAddr <= 0x07FF) data = tblName[1][ntAddr & 0x03FF];
            else if (ntAddr <= 0x0BFF) data = tblName[0][ntAddr & 0x03FF];
            else data = tblName[1][ntAddr & 0x03FF];
        }
        else if (m == MIRROR::HORIZONTAL)
        {
            if (ntAddr <= 0x03FF) data = tblName[0][ntAddr & 0x03FF];
            else if (ntAddr <= 0x07FF) data = tblName[0][ntAddr & 0x03FF];
            else if (ntAddr <= 0x0BFF) data = tblName[1][ntAddr & 0x03FF];
            else data = tblName[1][ntAddr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_LO)
        {
            data = tblName[0][ntAddr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_HI)
        {
            data = tblName[1][ntAddr & 0x03FF];
        }
    }
    else if (addr >= 0x3F00)
    {
        addr &= 0x001F;

        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;

        data = tblPalette[addr] & 0x3F;
    }

    return data;
}
void PPU::SetExtraScanlinesBeforeNMI(int value)
{
    if (value < 0) value = 0;
    if (value > 300) value = 300; // giới hạn an toàn
    extraScanlinesBeforeNMI = value;
}

int PPU::GetExtraScanlinesBeforeNMI() const
{
    return extraScanlinesBeforeNMI;
}
void PPU::SaveState(QDataStream& out) const
{
    out << nmi_requested;
    out << oam_addr;

    for (int i = 0; i < 256; i++)
        out << OAM[i];

    out << mapper_a12;
    out << mapper_a12_low_cycles;

    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 1024; i++)
            out << tblName[t][i];

    for (int i = 0; i < 32; i++)
        out << tblPalette[i];

    out << ppu_ctrl;
    out << ppu_mask;
    out << status;
    out << ppu_data_buffer;
    out << address_latch;

    out << vram_addr.reg;
    out << tram_addr.reg;
    out << fine_x;

    out << scanline;
    out << cycle;

    out << bg_next_tile_id;
    out << bg_next_tile_attrib;
    out << bg_next_tile_lsb;
    out << bg_next_tile_msb;

    out << bg_shifter_pattern_lo;
    out << bg_shifter_pattern_hi;
    out << bg_shifter_attrib_lo;
    out << bg_shifter_attrib_hi;

    out << extraScanlinesBeforeNMI;
    out << bRemoveSpriteLimit;
    out << sprite_count;

    for (int i = 0; i < 64; i++)
    {
        out << sprite_pattern_lo[i];
        out << sprite_pattern_hi[i];
        out << sprite_x[i];
        out << sprite_attribute[i];
        out << sprite_zero_being_rendered[i];
    }
}

void PPU::LoadState(QDataStream& in)
{
    in >> nmi_requested;
    in >> oam_addr;

    for (int i = 0; i < 256; i++)
        in >> OAM[i];

    in >> mapper_a12;
    in >> mapper_a12_low_cycles;

    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 1024; i++)
            in >> tblName[t][i];

    for (int i = 0; i < 32; i++)
        in >> tblPalette[i];

    in >> ppu_ctrl;
    in >> ppu_mask;
    in >> status;
    in >> ppu_data_buffer;
    in >> address_latch;

    in >> vram_addr.reg;
    in >> tram_addr.reg;
    in >> fine_x;

    in >> scanline;
    in >> cycle;

    in >> bg_next_tile_id;
    in >> bg_next_tile_attrib;
    in >> bg_next_tile_lsb;
    in >> bg_next_tile_msb;

    in >> bg_shifter_pattern_lo;
    in >> bg_shifter_pattern_hi;
    in >> bg_shifter_attrib_lo;
    in >> bg_shifter_attrib_hi;

    in >> extraScanlinesBeforeNMI;
    in >> bRemoveSpriteLimit;
    in >> sprite_count;

    for (int i = 0; i < 64; i++)
    {
        in >> sprite_pattern_lo[i];
        in >> sprite_pattern_hi[i];
        in >> sprite_x[i];
        in >> sprite_attribute[i];
        in >> sprite_zero_being_rendered[i];
    }
}