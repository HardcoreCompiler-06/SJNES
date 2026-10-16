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
#include <cmath>
#include <array>
#include <algorithm>

class BiquadFilter
{
public:
    void SetPeaking(float sampleRate, float freq, float gainDB, float Q)
    {
        float A = std::pow(10.0f, gainDB / 40.0f);
        float w0 = 2.0f * 3.14159265f * freq / sampleRate;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cosw0 = std::cos(w0);

        float b0 = 1 + alpha * A;
        float b1 = -2 * cosw0;
        float b2 = 1 - alpha * A;
        float a0 = 1 + alpha / A;
        float a1 = -2 * cosw0;
        float a2 = 1 - alpha / A;

        m_b0 = b0 / a0; m_b1 = b1 / a0; m_b2 = b2 / a0;
        m_a1 = a1 / a0; m_a2 = a2 / a0;
    }

    float Process(float in)
    {
        float out = m_b0 * in + m_b1 * m_x1 + m_b2 * m_x2
            - m_a1 * m_y1 - m_a2 * m_y2;
        m_x2 = m_x1; m_x1 = in;
        m_y2 = m_y1; m_y1 = out;
        return out;
    }

    void Reset() { m_x1 = m_x2 = m_y1 = m_y2 = 0.0f; }

private:
    float m_b0 = 1, m_b1 = 0, m_b2 = 0, m_a1 = 0, m_a2 = 0;
    float m_x1 = 0, m_x2 = 0, m_y1 = 0, m_y2 = 0;
};

class AudioEQ9Band
{
public:
    static constexpr int BAND_COUNT = 9;
    static constexpr float FREQS[BAND_COUNT] = {
        60.f, 170.f, 310.f,
        600.f, 1000.f, 3000.f,
        6000.f, 12000.f, 16000.f
    };

    AudioEQ9Band() { SetSampleRate(44100.0f); }

    void SetSampleRate(float sr)
    {
        m_sampleRate = sr;
        for (int i = 0; i < BAND_COUNT; i++)
        {
            m_bandsL[i].SetPeaking(sr, FREQS[i], m_gainsDB[i], 1.4f);
            m_bandsR[i].SetPeaking(sr, FREQS[i], m_gainsDB[i], 1.4f);
        }
    }

    void SetBandGain(int band, float gainDB)
    {
        if (band < 0 || band >= BAND_COUNT) return;
        m_gainsDB[band] = gainDB;
        m_bandsL[band].SetPeaking(m_sampleRate, FREQS[band], gainDB, 1.4f);
        m_bandsR[band].SetPeaking(m_sampleRate, FREQS[band], gainDB, 1.4f);
    }

    float GetBandGain(int band) const
    {
        if (band < 0 || band >= BAND_COUNT) return 0.0f;
        return m_gainsDB[band];
    }

    bool enabled = false;

    float ProcessLeft(float in)
    {
        float out = in;
        for (int i = 0; i < BAND_COUNT; i++)
            out = m_bandsL[i].Process(out);
        return std::clamp(out, -1.0f, 1.0f);
    }

    float ProcessRight(float in)
    {
        float out = in;
        for (int i = 0; i < BAND_COUNT; i++)
            out = m_bandsR[i].Process(out);
        return std::clamp(out, -1.0f, 1.0f);
    }

    void Reset()
    {
        for (int i = 0; i < BAND_COUNT; i++) { m_bandsL[i].Reset(); m_bandsR[i].Reset(); }
    }

private:
    float m_sampleRate = 44100.0f;
    std::array<BiquadFilter, BAND_COUNT> m_bandsL;
    std::array<BiquadFilter, BAND_COUNT> m_bandsR;
    float m_gainsDB[BAND_COUNT] = {};
};