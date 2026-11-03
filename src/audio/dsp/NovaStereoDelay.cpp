#include "audio/dsp/NovaStereoDelay.h"
#include <cmath>
#include <algorithm>

NovaStereoDelay::NovaStereoDelay()
{
    // Buffer para hasta 2 segundos a 96 kHz
    const size_t maxCapacity = 192000;
    m_delayBufferL.resize(maxCapacity, 0.0f);
    m_delayBufferR.resize(maxCapacity, 0.0f);
    reset();
}

void NovaStereoDelay::setParameter(int paramIndex, float value)
{
    switch (paramIndex) {
    case 0:
        m_timeMs.store(std::clamp(value, 10.0f, 1000.0f), std::memory_order_relaxed);
        break;
    case 1:
        m_feedback.store(std::clamp(value, 0.0f, 95.0f), std::memory_order_relaxed);
        break;
    case 2:
        m_mix.store(std::clamp(value, 0.0f, 100.0f), std::memory_order_relaxed);
        break;
    default:
        break;
    }
}

float NovaStereoDelay::getParameter(int paramIndex) const
{
    switch (paramIndex) {
    case 0: return m_timeMs.load(std::memory_order_relaxed);
    case 1: return m_feedback.load(std::memory_order_relaxed);
    case 2: return m_mix.load(std::memory_order_relaxed);
    default: return 0.0f;
    }
}

void NovaStereoDelay::reset()
{
    std::fill(m_delayBufferL.begin(), m_delayBufferL.end(), 0.0f);
    std::fill(m_delayBufferR.begin(), m_delayBufferR.end(), 0.0f);
    m_writePos = 0;
}

static inline float readInterpolated(const std::vector<float>& buf, double readIndex, size_t bufSize)
{
    while (readIndex < 0.0) readIndex += static_cast<double>(bufSize);
    while (readIndex >= static_cast<double>(bufSize)) readIndex -= static_cast<double>(bufSize);

    size_t i0 = static_cast<size_t>(readIndex);
    size_t i1 = (i0 + 1) % bufSize;
    float frac = static_cast<float>(readIndex - static_cast<double>(i0));

    return buf[i0] * (1.0f - frac) + buf[i1] * frac;
}

void NovaStereoDelay::process(float* interleavedBuffer, int numFrames, int channels, int sampleRate)
{
    if (!interleavedBuffer || numFrames <= 0 || channels <= 0) return;
    if (sampleRate <= 0) sampleRate = 48000;

    const float timeMs = m_timeMs.load(std::memory_order_relaxed);
    const float fb = m_feedback.load(std::memory_order_relaxed) / 100.0f;
    const float wet = m_mix.load(std::memory_order_relaxed) / 100.0f;
    const float dry = 1.0f - wet;

    const size_t bufSize = m_delayBufferL.size();
    if (bufSize == 0) return;

    const double delaySamplesL = (static_cast<double>(timeMs) / 1000.0) * static_cast<double>(sampleRate);
    // Canal derecho con un ligero desfase (+15%) para mayor apertura estéreo
    const double delaySamplesR = delaySamplesL * 1.15;

    int effChannels = std::min(channels, 2);

    for (int frame = 0; frame < numFrames; ++frame) {
        float inL = interleavedBuffer[frame * channels];
        float inR = (effChannels > 1) ? interleavedBuffer[frame * channels + 1] : inL;

        double readPosL = static_cast<double>(m_writePos) - delaySamplesL;
        double readPosR = static_cast<double>(m_writePos) - delaySamplesR;

        float delayedL = readInterpolated(m_delayBufferL, readPosL, bufSize);
        float delayedR = readInterpolated(m_delayBufferR, readPosR, bufSize);

        // Saturación tanh para realimentación analógica cálida sin recortes bruscos
        float nextL = inL + std::tanh(delayedL * fb);
        float nextR = inR + std::tanh(delayedR * fb);

        m_delayBufferL[m_writePos] = nextL;
        m_delayBufferR[m_writePos] = nextR;

        m_writePos = (m_writePos + 1) % bufSize;

        interleavedBuffer[frame * channels] = inL * dry + delayedL * wet;
        if (effChannels > 1) {
            interleavedBuffer[frame * channels + 1] = inR * dry + delayedR * wet;
        }
    }
}
