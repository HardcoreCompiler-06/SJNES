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

#include "EQWindow.h"
#include <QVBoxLayout>

EQWindow::EQWindow(EmuWorker* worker, const float initialGains[9], bool initialEnabled, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Equalizer");
    resize(520, 380);

    panel = new EQPanel(worker, initialGains, initialEnabled, this);
    connect(panel, &EQPanel::bandGainChanged, this, &EQWindow::bandGainChanged);
    connect(panel, &EQPanel::enabledChanged, this, &EQWindow::enabledChanged);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(panel);
    setLayout(layout);
}