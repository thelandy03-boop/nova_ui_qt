#pragma once

#include "audio/dsp/NovaDspEffect.h"
#include <atomic>
#include <vector>

/**
 * Procesador de reverberación espacial de sala y estudio (ACE Reverb).
 * Arquitectura basada en red de filtros peine y todos-paso de Freeverb.
 * - Parámetro 0: Tamaño de Sala (Room Size) [0% a 100%]
 * - Parámetro 1: Mezcla Wet [0% a 100%]
 */
class NovaReverb : public NovaDspEffect {
public:
    NovaReverb();
    ~NovaReverb() override = default;

    QString id() const override { return QStringLiteral("urn:ardour:a-reverb"); }
    QString name() const override { return QStringLiteral("ACE Reverb"); }
    QString category() const override { return QStringLiteral("ESPACIO"); }

    void process(float* interleavedBuffer, int numFrames, int channels, int sampleRate) override;
    void setParameter(int paramIndex, float value) override;
    float getParameter(int paramIndex) const override;
    void reset() override;

    struct CombFilter {
        std::vector<float> buffer;
        size_t bufIdx = 0;
        float filterStore = 0.0f;
    };

    struct AllpassFilter {
        std::vector<float> buffer;
        size_t bufIdx = 0;
    };

private:

    void reconfigurarTasa(int sampleRate);

    std::atomic<float> m_roomSize{45.0f}; // % [0, 100]
    std::atomic<float> m_wetMix{20.0f};   // % [0, 100]

    int m_lastSampleRate = 48000;

    CombFilter m_combL[8];
    CombFilter m_combR[8];
    AllpassFilter m_allpassL[4];
    AllpassFilter m_allpassR[4];
};
