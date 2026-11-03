#include "audio/dsp/NovaBiquadEq.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

NovaBiquadEq::NovaBiquadEq()
{
    recalcularCoeficientes(48000);
    reset();
}

void NovaBiquadEq::setParameter(int paramIndex, float value)
{
    switch (paramIndex) {
    case 0:
        m_gainLow.store(std::clamp(value, -12.0f, 12.0f), std::memory_order_relaxed);
        break;
    case 1:
        m_gainMid.store(std::clamp(value, -12.0f, 12.0f), std::memory_order_relaxed);
        break;
    case 2:
        m_gainHigh.store(std::clamp(value, -12.0f, 12.0f), std::memory_order_relaxed);
        break;
    default:
        break;
    }
}

float NovaBiquadEq::getParameter(int paramIndex) const
{
    switch (paramIndex) {
    case 0: return m_gainLow.load(std::memory_order_relaxed);
    case 1: return m_gainMid.load(std::memory_order_relaxed);
    case 2: return m_gainHigh.load(std::memory_order_relaxed);
    default: return 0.0f;
    }
}

void NovaBiquadEq::reset()
{
    m_stateLow = FilterState{};
    m_stateMid = FilterState{};
    m_stateHigh = FilterState{};
}

void NovaBiquadEq::recalcularCoeficientes(int sampleRate)
{
    if (sampleRate <= 0) sampleRate = 48000;
    m_lastSampleRate = sampleRate;
    const double Fs = static_cast<double>(sampleRate);

    // 1. Low Shelf: 80 Hz, Q = 0.707
    {
        double f0 = 80.0;
        double gainDb = static_cast<double>(m_gainLow.load(std::memory_order_relaxed));
        if (std::abs(gainDb) < 0.01) {
            m_coeffsLow = BiquadCoeffs{1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        } else {
            double A = std::pow(10.0, gainDb / 40.0);
            double w0 = 2.0 * M_PI * f0 / Fs;
            double cos_w0 = std::cos(w0);
            double sin_w0 = std::sin(w0);
            double Q = 0.7071;
            double alpha = sin_w0 / (2.0 * Q);

            double b0 =    A * ((A + 1.0) - (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha);
            double b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_w0);
            double b2 =    A * ((A + 1.0) - (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha);
            double a0 =        (A + 1.0) + (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha;
            double a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cos_w0);
            double a2 =        (A + 1.0) + (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha;

            m_coeffsLow.b0 = static_cast<float>(b0 / a0);
            m_coeffsLow.b1 = static_cast<float>(b1 / a0);
            m_coeffsLow.b2 = static_cast<float>(b2 / a0);
            m_coeffsLow.a1 = static_cast<float>(a1 / a0);
            m_coeffsLow.a2 = static_cast<float>(a2 / a0);
        }
    }

    // 2. Peaking: 1000 Hz, Q = 1.0
    {
        double f0 = 1000.0;
        double gainDb = static_cast<double>(m_gainMid.load(std::memory_order_relaxed));
        if (std::abs(gainDb) < 0.01) {
            m_coeffsMid = BiquadCoeffs{1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        } else {
            double A = std::pow(10.0, gainDb / 40.0);
            double w0 = 2.0 * M_PI * f0 / Fs;
            double cos_w0 = std::cos(w0);
            double sin_w0 = std::sin(w0);
            double Q = 1.0;
            double alpha = sin_w0 / (2.0 * Q);

            double b0 =  1.0 + alpha * A;
            double b1 = -2.0 * cos_w0;
            double b2 =  1.0 - alpha * A;
            double a0 =  1.0 + alpha / A;
            double a1 = -2.0 * cos_w0;
            double a2 =  1.0 - alpha / A;

            m_coeffsMid.b0 = static_cast<float>(b0 / a0);
            m_coeffsMid.b1 = static_cast<float>(b1 / a0);
            m_coeffsMid.b2 = static_cast<float>(b2 / a0);
            m_coeffsMid.a1 = static_cast<float>(a1 / a0);
            m_coeffsMid.a2 = static_cast<float>(a2 / a0);
        }
    }

    // 3. High Shelf: 10000 Hz, Q = 0.707
    {
        double f0 = 10000.0;
        double gainDb = static_cast<double>(m_gainHigh.load(std::memory_order_relaxed));
        if (std::abs(gainDb) < 0.01) {
            m_coeffsHigh = BiquadCoeffs{1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        } else {
            double A = std::pow(10.0, gainDb / 40.0);
            double w0 = 2.0 * M_PI * f0 / Fs;
            double cos_w0 = std::cos(w0);
            double sin_w0 = std::sin(w0);
            double Q = 0.7071;
            double alpha = sin_w0 / (2.0 * Q);

            double b0 =    A * ((A + 1.0) + (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha);
            double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w0);
            double b2 =    A * ((A + 1.0) + (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha);
            double a0 =        (A + 1.0) - (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha;
            double a1 =  2.0 * ((A - 1.0) - (A + 1.0) * cos_w0);
            double a2 =        (A + 1.0) - (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha;

            m_coeffsHigh.b0 = static_cast<float>(b0 / a0);
            m_coeffsHigh.b1 = static_cast<float>(b1 / a0);
            m_coeffsHigh.b2 = static_cast<float>(b2 / a0);
            m_coeffsHigh.a1 = static_cast<float>(a1 / a0);
            m_coeffsHigh.a2 = static_cast<float>(a2 / a0);
        }
    }
}

static inline float processBiquadSample(float in, const NovaBiquadEq::BiquadCoeffs &c, float &s1, float &s2)
{
    float out = c.b0 * in + s1;
    s1 = c.b1 * in - c.a1 * out + s2;
    s2 = c.b2 * in - c.a2 * out;
    return out;
}

void NovaBiquadEq::process(float* interleavedBuffer, int numFrames, int channels, int sampleRate)
{
    if (!interleavedBuffer || numFrames <= 0 || channels <= 0) return;

    recalcularCoeficientes(sampleRate);

    int effChannels = std::min(channels, 2);

    for (int frame = 0; frame < numFrames; ++frame) {
        for (int ch = 0; ch < effChannels; ++ch) {
            int idx = frame * channels + ch;
            float sample = interleavedBuffer[idx];

            // Pasamos en cascada por Low Shelf -> Peaking -> High Shelf
            sample = processBiquadSample(sample, m_coeffsLow, m_stateLow.s1[ch], m_stateLow.s2[ch]);
            sample = processBiquadSample(sample, m_coeffsMid, m_stateMid.s1[ch], m_stateMid.s2[ch]);
            sample = processBiquadSample(sample, m_coeffsHigh, m_stateHigh.s1[ch], m_stateHigh.s2[ch]);

            interleavedBuffer[idx] = sample;
        }
    }
}
