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
#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include "EmuWorker.h"

// EQPanel: chứa toàn bộ UI 9-band equalizer (slider + preset + bật/tắt), tách ra từ
// EQWindow để dùng chung được ở nhiều nơi — vừa trong EQWindow (cửa sổ riêng cũ),
// vừa nhúng thẳng vào SettingsDialog (tab Audio) mà không cần mở cửa sổ riêng.
class EQPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EQPanel(EmuWorker* worker, const float initialGains[9], bool initialEnabled, QWidget* parent = nullptr);

signals:
    void bandGainChanged(int band, float gainDB);
    void enabledChanged(bool on);

private slots:
    void onSliderChanged(int band, int value);
    void onToggleEnabled(bool checked);
    void onReset();
    void onPresetSelected(int index);

private:
    void applyPreset(const float gains[9]);
    void updateLabel(int band, int value);

    EmuWorker* worker = nullptr;
    QSlider* sliders[9] = {};
    QLabel* valueLabels[9] = {};
    QPushButton* enableButton = nullptr;
    QComboBox* presetCombo = nullptr;
};
