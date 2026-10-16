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

#include "MapperViewerWindow.h"
#include "Bus.h"
#include "Cartridge.h"
#include "Mapper.h"

MapperViewerWindow::MapperViewerWindow(Bus* bus, QWidget* parent)
    : QDialog(parent), bus(bus)
{
    setWindowTitle("Mapper Viewer");
    resize(680, 760);

    text = new QPlainTextEdit(this);
    text->setReadOnly(true);
    text->setFocusPolicy(Qt::NoFocus);
    text->setFont(QFont("Consolas", 10));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(text);
    setLayout(layout);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MapperViewerWindow::updateInfo);
    timer->start(200);

    updateInfo();
}

void MapperViewerWindow::updateInfo()
{
    if (!bus || !bus->cart || !bus->cart->pMapper)
    {
        text->setPlainText("Chưa nạp ROM hoặc chưa có mapper.");
        return;
    }

    std::string s;

    s += "===== TRINH XEM MAPPER =====\n\n";
    s += "Mapper ID : " + std::to_string(bus->cart->nMapperID) + "\n";
    s += "Submapper : " + std::to_string(bus->cart->pMapper->nSubmapper) + "\n";
    s += "\n";

    s += bus->cart->pMapper->GetDebugInfo();

    text->setPlainText(QString::fromStdString(s));
}