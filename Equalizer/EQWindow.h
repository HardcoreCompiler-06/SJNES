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
#include "EQPanel.h"
#include "EmuWorker.h"

// EQWindow giờ chỉ là 1 wrapper mỏng chứa EQPanel — UI thật nằm hết trong EQPanel
// (dùng chung được với SettingsDialog). Giữ lại class này để không phải sửa các chỗ
// khác trong SJNES.cpp đang new EQWindow(...) làm cửa sổ riêng.
class EQWindow : public QDialog
{
    Q_OBJECT

public:
    explicit EQWindow(EmuWorker* worker, const float initialGains[9], bool initialEnabled, QWidget* parent = nullptr);

signals:
    void bandGainChanged(int band, float gainDB);
    void enabledChanged(bool on);

private:
    EQPanel* panel = nullptr;
};