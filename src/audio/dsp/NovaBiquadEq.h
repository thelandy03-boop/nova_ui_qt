#pragma once

#include "audio/dsp/NovaDspEffect.h"
#include <atomic>
#include <vector>

/**
 * Ecualizador paramétrico nativo de 3 bandas (ACE EQ).
 * - Banda 0: Low Shelf (80 Hz) [-12 dB a +12 dB]
 * - Banda 1: Campana Peaking (1 kHz) [-12 dB a +12 dB]
 * - Banda 2: High Shelf (10 kHz) [-12 dB a +12 dB]
 */
class NovaBiquadEq : public NovaDspEffect {
public:
    NovaBiquadEq();
    ~NovaBiquadEq() override = default;

    QString id() const override { return QStringLiteral("urn:ardour:a-eq"); }
    QString name() const override { return QStringLiteral("ACE EQ"); }
    QString category() const override { return QStringLiteral("EQ"); }

    void process(float* interleavedBuffer, int numFrames, int channels, int sampleRate) override;
    void setParameter(int paramIndex, float value) override;
    float getParameter(int paramIndex) const override;
    void reset() override;

    struct BiquadCoeffs {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };

    struct FilterState {
        float s1[2] = {0.0f, 0.0f}; // canales L, R
        float s2[2] = {0.0f, 0.0f};
    };

private:

    void recalcularCoeficientes(int sampleRate);

    std::atomic<float> m_gainLow{0.0f};   // dB [-12, 12]
    std::atomic<float> m_gainMid{0.0f};   // dB [-12, 12]
    std::atomic<float> m_gainHigh{0.0f};  // dB [-12, 12]

    int m_lastSampleRate = 48000;
    BiquadCoeffs m_coeffsLow;
    BiquadCoeffs m_coeffsMid;
    BiquadCoeffs m_coeffsHigh;

    FilterState m_stateLow;
    FilterState m_stateMid;
    FilterState m_stateHigh;
};
