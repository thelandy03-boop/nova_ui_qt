#pragma once

#include "audio/dsp/NovaDspEffect.h"
#include <atomic>
#include <vector>

/**
 * Efecto de retardo estéreo con modulación y saturación analógica (ACE Delay).
 * - Parámetro 0: Tiempo de retardo [10 ms a 1000 ms]
 * - Parámetro 1: Feedback de realimentación [0% a 100%]
 * - Parámetro 2: Mezcla Dry/Wet [0% a 100%]
 */
class NovaStereoDelay : public NovaDspEffect {
public:
    NovaStereoDelay();
    ~NovaStereoDelay() override = default;

    QString id() const override { return QStringLiteral("urn:ardour:a-delay"); }
    QString name() const override { return QStringLiteral("ACE Delay"); }
    QString category() const override { return QStringLiteral("DELAY"); }

    void process(float* interleavedBuffer, int numFrames, int channels, int sampleRate) override;
    void setParameter(int paramIndex, float value) override;
    float getParameter(int paramIndex) const override;
    void reset() override;

private:
    std::atomic<float> m_timeMs{250.0f};      // ms [10, 1000]
    std::atomic<float> m_feedback{30.0f};     // % [0, 100]
    std::atomic<float> m_mix{25.0f};          // % [0, 100]

    std::vector<float> m_delayBufferL;
    std::vector<float> m_delayBufferR;
    size_t m_writePos = 0;
};
