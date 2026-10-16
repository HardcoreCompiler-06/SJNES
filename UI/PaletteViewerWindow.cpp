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

#include "PaletteViewerWindow.h"
#include <QPainter>
#include <QFont>

PaletteViewerWindow::PaletteViewerWindow(PPU* ppu, QWidget* parent)
    : QDialog(parent), ppu(ppu)
{
    setWindowTitle("Palette Viewer");
    resize(420, 260);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PaletteViewerWindow::refresh);
    timer->start(200);
}

void PaletteViewerWindow::refresh()
{
    update();
}

void PaletteViewerWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (!ppu)
        return;

    const int cellSize = 40;
    const int marginX = 20;
    const int marginY = 20;
    const int gap = 4;

    p.setFont(QFont("Consolas", 9));

    for (int row = 0; row < 8; row++)
    {
        const char* label = (row < 4)
            ? (row == 0 ? "BG0" : row == 1 ? "BG1" : row == 2 ? "BG2" : "BG3")
            : (row == 4 ? "SP0" : row == 5 ? "SP1" : row == 6 ? "SP2" : "SP3");

        p.setPen(Qt::white);
        p.drawText(marginX, marginY + row * (cellSize + gap) + cellSize / 2 + 4, label);

        for (int col = 0; col < 4; col++)
        {
            int paletteIndex = row * 4 + col;
            uint8_t colorIndex = ppu->GetPaletteRAM(static_cast<uint8_t>(paletteIndex));

            PPU::RGBColor c = ppu->GetNESColor(colorIndex);
            QColor qc(c.r, c.g, c.b);

            int x = marginX + 50 + col * (cellSize + gap);
            int y = marginY + row * (cellSize + gap);

            p.fillRect(x, y, cellSize, cellSize, qc);
            p.setPen(Qt::gray);
            p.drawRect(x, y, cellSize, cellSize);

            p.setPen(Qt::white);
            p.drawText(x + 2, y + cellSize + 12,
                QString("0x%1").arg(colorIndex, 2, 16, QChar('0')).toUpper());
        }
    }
}