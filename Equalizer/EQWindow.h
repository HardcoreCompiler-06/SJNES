#pragma once
#include <QDialog>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include "EmuWorker.h"

class EQWindow : public QDialog
{
    Q_OBJECT

public:
    explicit EQWindow(EmuWorker* worker, const float initialGains[9], bool initialEnabled, QWidget* parent = nullptr);

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