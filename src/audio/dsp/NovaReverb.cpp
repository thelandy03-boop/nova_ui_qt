#include "audio/dsp/NovaReverb.h"
#include <cmath>
#include <algorithm>

static const int kCombLengths[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int kAllpassLengths[4] = { 556, 441, 341, 225 };
static const int kStereoSpread = 23;

NovaReverb::NovaReverb()
{
    reconfigurarTasa(48000);
    reset();
}

void NovaReverb::setParameter(int paramIndex, float value)
{
    switch (paramIndex) {
    case 0:
        m_roomSize.store(std::clamp(value, 0.0f, 100.0f), std::memory_order_relaxed);
        break;
    case 1:
        m_wetMix.store(std::clamp(value, 0.0f, 100.0f), std::memory_order_relaxed);
        break;
    default:
        break;
    }
}

float NovaReverb::getParameter(int paramIndex) const
{
    switch (paramIndex) {
    case 0: return m_roomSize.load(std::memory_order_relaxed);
    case 1: return m_wetMix.load(std::memory_order_relaxed);
    default: return 0.0f;
    }
}

void NovaReverb::reset()
{
    for (int i = 0; i < 8; ++i) {
        std::fill(m_combL[i].buffer.begin(), m_combL[i].buffer.end(), 0.0f);
        m_combL[i].bufIdx = 0;
        m_combL[i].filterStore = 0.0f;

        std::fill(m_combR[i].buffer.begin(), m_combR[i].buffer.end(), 0.0f);
        m_combR[i].bufIdx = 0;
        m_combR[i].filterStore = 0.0f;
    }
    for (int i = 0; i < 4; ++i) {
        std::fill(m_allpassL[i].buffer.begin(), m_allpassL[i].buffer.end(), 0.0f);
        m_allpassL[i].bufIdx = 0;

        std::fill(m_allpassR[i].buffer.begin(), m_allpassR[i].buffer.end(), 0.0f);
        m_allpassR[i].bufIdx = 0;
    }
}

void NovaReverb::reconfigurarTasa(int sampleRate)
{
    if (sampleRate <= 0) sampleRate = 48000;
    m_lastSampleRate = sampleRate;

    float scale = static_cast<float>(sampleRate) / 44100.0f;

    for (int i = 0; i < 8; ++i) {
        size_t lenL = static_cast<size_t>(std::round(kCombLengths[i] * scale));
        size_t lenR = static_cast<size_t>(std::round((kCombLengths[i] + kStereoSpread) * scale));

        m_combL[i].buffer.assign(lenL, 0.0f);
        m_combL[i].bufIdx = 0;
        m_combL[i].filterStore = 0.0f;

        m_combR[i].buffer.assign(lenR, 0.0f);
        m_combR[i].bufIdx = 0;
        m_combR[i].filterStore = 0.0f;
    }

    for (int i = 0; i < 4; ++i) {
        size_t lenL = static_cast<size_t>(std::round(kAllpassLengths[i] * scale));
        size_t lenR = static_cast<size_t>(std::round((kAllpassLengths[i] + kStereoSpread) * scale));

        m_allpassL[i].buffer.assign(lenL, 0.0f);
        m_allpassL[i].bufIdx = 0;

        m_allpassR[i].buffer.assign(lenR, 0.0f);
        m_allpassR[i].bufIdx = 0;
    }
}

static inline float processComb(NovaReverb::CombFilter &c, float in, float feedback, float damp)
{
    float output = c.buffer[c.bufIdx];
    c.filterStore = (output * (1.0f - damp)) + (c.filterStore * damp);
    c.buffer[c.bufIdx] = in + (c.filterStore * feedback);
    if (++c.bufIdx >= c.buffer.size()) c.bufIdx = 0;
    return output;
}

static inline float processAllpass(NovaReverb::AllpassFilter &a, float in)
{
    float bufOut = a.buffer[a.bufIdx];
    float output = -in + bufOut;
    a.buffer[a.bufIdx] = in + (bufOut * 0.5f);
    if (++a.bufIdx >= a.buffer.size()) a.bufIdx = 0;
    return output;
}

void NovaReverb::process(float* interleavedBuffer, int numFrames, int channels, int sampleRate)
{
    if (!interleavedBuffer || numFrames <= 0 || channels <= 0) return;

    if (sampleRate != m_lastSampleRate && sampleRate > 0) {
        reconfigurarTasa(sampleRate);
    }

    const float room = m_roomSize.load(std::memory_order_relaxed) / 100.0f;
    const float feedback = room * 0.28f + 0.70f;
    const float damp = 0.25f;
    const float wet = m_wetMix.load(std::memory_order_relaxed) / 100.0f;
    const float dry = 1.0f - wet;

    int effChannels = std::min(channels, 2);

    for (int frame = 0; frame < numFrames; ++frame) {
        float inL = interleavedBuffer[frame * channels];
        float inR = (effChannels > 1) ? interleavedBuffer[frame * channels + 1] : inL;
        float inMono = (inL + inR) * 0.015f; // Nivel de inyección atenuado para no saturar

        float outL = 0.0f;
        float outR = 0.0f;

        // 8 filtros peine en paralelo
        for (int i = 0; i < 8; ++i) {
            outL += processComb(m_combL[i], inMono, feedback, damp);
            outR += processComb(m_combR[i], inMono, feedback, damp);
        }

        // 4 filtros todos-paso en serie
        for (int i = 0; i < 4; ++i) {
            outL = processAllpass(m_allpassL[i], outL);
            outR = processAllpass(m_allpassR[i], outR);
        }

        interleavedBuffer[frame * channels] = inL * dry + outL * wet;
        if (effChannels > 1) {
            interleavedBuffer[frame * channels + 1] = inR * dry + outR * wet;
        }
    }
}
