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

#include <QDialog>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "SpriteScreenWidget.h"

class Bus;

class SpriteViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SpriteViewerWindow(Bus* bus, QWidget* parent = nullptr);

private slots:
    void updateSprites();

private:
    Bus* bus = nullptr;
    QTableWidget* table = nullptr;
    SpriteScreenWidget* screen = nullptr;
    QTimer* timer = nullptr;
};