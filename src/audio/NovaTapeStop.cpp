#include "audio/NovaTapeStop.h"

NovaTapeStop::NovaTapeStop(float sampleRate, float tempoBpm)
{
    prepare(sampleRate, tempoBpm);
}

void NovaTapeStop::prepare(float sampleRate, float tempoBpm)
{
    m_sampleRate = (sampleRate > 0.0f) ? sampleRate : 48000.0f;
    m_tempoBpm = (tempoBpm > 0.0f) ? tempoBpm : 120.0f;

    size_t cap = calculateCapacity(m_sampleRate, m_tempoBpm);
    if (m_leftBuffer.size() != cap) {
        m_leftBuffer.assign(cap, 0.0f);
        m_rightBuffer.assign(cap, 0.0f);
    }
    reset();
}

size_t NovaTapeStop::calculateCapacity(float sampleRate, float tempoBpm)
{
    float maxSecs = std::max(4.0f, 960.0f / std::max(20.0f, tempoBpm));
    size_t count = static_cast<size_t>(std::ceil(sampleRate * maxSecs));
    size_t cap = 2;
    while (cap < count && cap < 524288) { // Límite seguro de 512k muestras (~10 seg)
        cap <<= 1;
    }
    return cap;
}

void NovaTapeStop::reset()
{
    std::fill(m_leftBuffer.begin(), m_leftBuffer.end(), 0.0f);
    std::fill(m_rightBuffer.begin(), m_rightBuffer.end(), 0.0f);
    m_writeHead = 0;
    m_readHead = 0.0;
    m_phase = 1.0f;
    m_phaseStep = 0.0f;
    m_remaining = 0;
    m_fadeIn = 0.0f;
    m_heldL = 0.0f;
    m_heldR = 0.0f;
    m_isStopping = false;
}

void NovaTapeStop::trigger(bool stop, float durationSeconds)
{
    m_isStopping = stop;
    float target = stop ? 0.0f : 1.0f;
    float delta = std::abs(target - m_phase);
    m_remaining = static_cast<size_t>(std::max(0.01f, durationSeconds) * m_sampleRate * delta);
    if (m_remaining < 1) m_remaining = 1;
    m_phaseStep = (target - m_phase) / static_cast<float>(m_remaining);
    m_fadeIn = 0.0f;
}

float NovaTapeStop::playbackSpeed() const
{
    // Curva cuadrática emulando inercia analógica de polea de cinta
    return std::clamp(m_phase * m_phase, 0.0f, 1.0f);
}

float NovaTapeStop::readSample(const std::vector<float> &buffer, double head) const
{
    if (buffer.empty()) return 0.0f;
    uint64_t len = static_cast<uint64_t>(buffer.size());
    uint64_t frame = static_cast<uint64_t>(std::floor(head));
    float frac = static_cast<float>(head - std::floor(head));
    float a = buffer[static_cast<size_t>(frame % len)];
    float b = buffer[static_cast<size_t>((frame + 1) % len)];
    return a + (b - a) * frac;
}

void NovaTapeStop::processFrame(float inL, float inR, float &outL, float &outR)
{
    if (m_leftBuffer.empty()) {
        outL = inL;
        outR = inR;
        return;
    }

    size_t cap = m_leftBuffer.size();
    size_t writeIdx = static_cast<size_t>(m_writeHead % cap);
    m_leftBuffer[writeIdx] = inL;
    m_rightBuffer[writeIdx] = inR;
    m_writeHead++;

    if (m_remaining == 0) {
        m_readHead = static_cast<double>(m_writeHead);
        if (!m_isStopping && m_phase >= 0.999f) {
            m_heldL = inL;
            m_heldR = inR;
            outL = inL;
            outR = inR;
            return;
        }
        // Cinta totalmente detenida: desvanecimiento suave contra clics
        float smoothing = std::max(0.0f, 1.0f - 44.1f / m_sampleRate);
        m_heldL *= smoothing;
        m_heldR *= smoothing;
        outL = m_heldL;
        outR = m_heldR;
        return;
    }

    outL = readSample(m_leftBuffer, m_readHead);
    outR = readSample(m_rightBuffer, m_readHead);

    float speed = playbackSpeed();
    m_readHead += static_cast<double>(speed);
    m_phase = std::clamp(m_phase + m_phaseStep, 0.0f, 1.0f);
    m_remaining--;

    if (!m_isStopping) {
        size_t crossfade = static_cast<size_t>(std::max(1.0f, 0.05f * m_sampleRate));
        if (m_remaining < crossfade) {
            m_fadeIn = std::min(1.0f, m_fadeIn + 1.0f / static_cast<float>(crossfade));
            outL += (inL - outL) * m_fadeIn;
            outR += (inR - outR) * m_fadeIn;
        }
    }

    m_heldL = outL;
    m_heldR = outR;
}
