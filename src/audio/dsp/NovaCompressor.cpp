#include "audio/dsp/NovaCompressor.h"
#include <cmath>
#include <algorithm>

NovaCompressor::NovaCompressor()
{
    reset();
}

void NovaCompressor::setParameter(int paramIndex, float value)
{
    switch (paramIndex) {
    case 0:
        m_threshold.store(std::clamp(value, -40.0f, 0.0f), std::memory_order_relaxed);
        break;
    case 1:
        m_ratio.store(std::clamp(value, 1.0f, 20.0f), std::memory_order_relaxed);
        break;
    case 2:
        m_makeupGain.store(std::clamp(value, 0.0f, 20.0f), std::memory_order_relaxed);
        break;
    default:
        break;
    }
}

float NovaCompressor::getParameter(int paramIndex) const
{
    switch (paramIndex) {
    case 0: return m_threshold.load(std::memory_order_relaxed);
    case 1: return m_ratio.load(std::memory_order_relaxed);
    case 2: return m_makeupGain.load(std::memory_order_relaxed);
    default: return 0.0f;
    }
}

void NovaCompressor::reset()
{
    m_envelope = 0.0f;
    m_gainReduction = 1.0f;
}

void NovaCompressor::process(float* interleavedBuffer, int numFrames, int channels, int sampleRate)
{
    if (!interleavedBuffer || numFrames <= 0 || channels <= 0) return;
    if (sampleRate <= 0) sampleRate = 48000;

    const float threshDb = m_threshold.load(std::memory_order_relaxed);
    const float ratio = m_ratio.load(std::memory_order_relaxed);
    const float makeupDb = m_makeupGain.load(std::memory_order_relaxed);

    const float makeupLinear = std::pow(10.0f, makeupDb / 20.0f);

    // Ataque 10ms, Release 100ms
    const float attCoeff = std::exp(-1.0f / (0.010f * static_cast<float>(sampleRate)));
    const float relCoeff = std::exp(-1.0f / (0.100f * static_cast<float>(sampleRate)));

    int effChannels = std::min(channels, 2);

    for (int frame = 0; frame < numFrames; ++frame) {
        float maxSample = 0.0f;
        for (int ch = 0; ch < effChannels; ++ch) {
            float s = std::abs(interleavedBuffer[frame * channels + ch]);
            if (s > maxSample) maxSample = s;
        }

        // Seguidor de envolvente pico
        if (maxSample > m_envelope) {
            m_envelope = attCoeff * m_envelope + (1.0f - attCoeff) * maxSample;
        } else {
            m_envelope = relCoeff * m_envelope + (1.0f - relCoeff) * maxSample;
        }

        float envDb = 20.0f * std::log10(std::max(m_envelope, 1e-5f));
        float targetGain = 1.0f;

        if (envDb > threshDb && ratio > 1.0f) {
            float excess = envDb - threshDb;
            float grDb = excess * (1.0f - 1.0f / ratio);
            targetGain = std::pow(10.0f, -grDb / 20.0f);
        }

        // Suavizado de la ganancia aplicada
        m_gainReduction = 0.9f * m_gainReduction + 0.1f * targetGain;
        float finalGain = m_gainReduction * makeupLinear;

        for (int ch = 0; ch < effChannels; ++ch) {
            interleavedBuffer[frame * channels + ch] *= finalGain;
        }
    }
}
