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

#include "EQPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>

static const char* BAND_NAMES[9] = {
    "60Hz", "170Hz", "310Hz",
    "600Hz", "1kHz", "3kHz",
    "6kHz", "12kHz", "16kHz"
};

EQPanel::EQPanel(EmuWorker* worker, const float initialGains[9], bool initialEnabled, QWidget* parent)
    : QWidget(parent), worker(worker)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* groupsLayout = new QHBoxLayout();

    struct GroupInfo { const char* title; int start; int count; };
    GroupInfo groups[3] = {
        { "Bass",   0, 3 },
        { "Mid",    3, 3 },
        { "Treble", 6, 3 }
    };

    for (const auto& g : groups)
    {
        QGroupBox* box = new QGroupBox(g.title, this);
        QHBoxLayout* boxLayout = new QHBoxLayout(box);

        for (int i = g.start; i < g.start + g.count; i++)
        {
            QVBoxLayout* col = new QVBoxLayout();

            QLabel* valLabel = new QLabel("0 dB", box);
            valLabel->setAlignment(Qt::AlignCenter);
            valueLabels[i] = valLabel;

            QSlider* slider = new QSlider(Qt::Vertical, box);
            slider->setRange(-12, 12);
            slider->setValue(0);
            slider->setTickPosition(QSlider::TicksBothSides);
            slider->setTickInterval(3);
            slider->setMinimumHeight(120);
            sliders[i] = slider;

            QLabel* freqLabel = new QLabel(BAND_NAMES[i], box);
            freqLabel->setAlignment(Qt::AlignCenter);

            col->addWidget(valLabel);
            col->addWidget(slider, 0, Qt::AlignHCenter);
            col->addWidget(freqLabel);
            boxLayout->addLayout(col);

            connect(slider, &QSlider::valueChanged, this, [this, i](int value) {
                onSliderChanged(i, value);
                });
        }

        groupsLayout->addWidget(box);
    }

    mainLayout->addLayout(groupsLayout);

    QHBoxLayout* controlLayout = new QHBoxLayout();

    enableButton = new QPushButton("Bật EQ", this);
    enableButton->setCheckable(true);
    connect(enableButton, &QPushButton::toggled, this, &EQPanel::onToggleEnabled);

    QPushButton* resetButton = new QPushButton("Reset", this);
    connect(resetButton, &QPushButton::clicked, this, &EQPanel::onReset);

    presetCombo = new QComboBox(this);
    presetCombo->addItem("Preset: Flat");
    presetCombo->addItem("Preset: Bass Boost");
    presetCombo->addItem("Preset: Treble Boost");
    presetCombo->addItem("Preset: Vocal");
    connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &EQPanel::onPresetSelected);

    controlLayout->addWidget(enableButton);
    controlLayout->addWidget(resetButton);
    controlLayout->addWidget(presetCombo);
    controlLayout->addStretch();

    mainLayout->addLayout(controlLayout);

    for (int i = 0; i < 9; i++)
    {
        sliders[i]->blockSignals(true);
        sliders[i]->setValue(static_cast<int>(initialGains[i]));
        updateLabel(i, static_cast<int>(initialGains[i]));
        sliders[i]->blockSignals(false);
    }

    enableButton->blockSignals(true);
    enableButton->setChecked(initialEnabled);
    enableButton->setText(initialEnabled ? "Tắt EQ" : "Bật EQ");
    enableButton->blockSignals(false);

    setLayout(mainLayout);
}

void EQPanel::updateLabel(int band, int value)
{
    QString text = QString("%1%2 dB").arg(value > 0 ? "+" : "").arg(value);
    valueLabels[band]->setText(text);
}

void EQPanel::onSliderChanged(int band, int value)
{
    updateLabel(band, value);
    if (worker)
        worker->setEQBandGain(band, static_cast<float>(value));
    emit bandGainChanged(band, static_cast<float>(value));
}

void EQPanel::onToggleEnabled(bool checked)
{
    enableButton->setText(checked ? "Tắt EQ" : "Bật EQ");
    if (worker)
        worker->setEQEnabled(checked);
    emit enabledChanged(checked);
}

void EQPanel::onReset()
{
    for (int i = 0; i < 9; i++)
        sliders[i]->setValue(0);
}

void EQPanel::applyPreset(const float gains[9])
{
    for (int i = 0; i < 9; i++)
        sliders[i]->setValue(static_cast<int>(gains[i]));
}

void EQPanel::onPresetSelected(int index)
{
    static const float FLAT[9] = { 0,0,0, 0,0,0, 0,0,0 };
    static const float BASS[9] = { 8,6,4, 0,0,0, 0,0,0 };
    static const float TREBLE[9] = { 0,0,0, 0,0,0, 4,6,8 };
    static const float VOCAL[9] = { -3,-2,0, 4,5,4, 0,-2,-3 };

    switch (index)
    {
    case 0: applyPreset(FLAT); break;
    case 1: applyPreset(BASS); break;
    case 2: applyPreset(TREBLE); break;
    case 3: applyPreset(VOCAL); break;
    }
}
