#pragma once

#include "audio/dsp/NovaDspEffect.h"
#include <atomic>

/**
 * Compresor de dinámica analógico (ACE Compressor).
 * - Parámetro 0: Umbral [-40 dB a 0 dB]
 * - Parámetro 1: Ratio [1.0 a 20.0 :1]
 * - Parámetro 2: Ganancia de compensación Makeup Gain [0 dB a 20 dB]
 */
class NovaCompressor : public NovaDspEffect {
public:
    NovaCompressor();
    ~NovaCompressor() override = default;

    QString id() const override { return QStringLiteral("urn:ardour:a-comp"); }
    QString name() const override { return QStringLiteral("ACE Compressor"); }
    QString category() const override { return QStringLiteral("DINÁMICA"); }

    void process(float* interleavedBuffer, int numFrames, int channels, int sampleRate) override;
    void setParameter(int paramIndex, float value) override;
    float getParameter(int paramIndex) const override;
    void reset() override;

private:
    std::atomic<float> m_threshold{-12.0f}; // dB [-40, 0]
    std::atomic<float> m_ratio{3.0f};       // [1, 20]
    std::atomic<float> m_makeupGain{2.0f};  // dB [0, 20]

    float m_envelope = 0.0f;
    float m_gainReduction = 1.0f;
};
