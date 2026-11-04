#include "audio/dsp/NovaDspChain.h"

NovaDspChain::NovaDspChain() = default;
NovaDspChain::~NovaDspChain() = default;

void NovaDspChain::addEffect(std::shared_ptr<NovaDspEffect> effect)
{
    if (!effect) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_effects.push_back(std::move(effect));
}

void NovaDspChain::replaceEffect(int index, std::shared_ptr<NovaDspEffect> effect)
{
    if (!effect) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_effects.size())) {
        m_effects[static_cast<size_t>(index)] = std::move(effect);
    }
}

void NovaDspChain::removeEffect(int index)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_effects.size())) {
        m_effects.erase(m_effects.begin() + index);
    }
}

void NovaDspChain::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_effects.clear();
}

int NovaDspChain::effectCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_effects.size());
}

std::shared_ptr<NovaDspEffect> NovaDspChain::effectAt(int index) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_effects.size())) {
        return m_effects[static_cast<size_t>(index)];
    }
    return nullptr;
}

void NovaDspChain::process(float* interleavedBuffer, int numFrames, int channels, int sampleRate)
{
    if (!interleavedBuffer || numFrames <= 0 || channels <= 0 || sampleRate <= 0) {
        return;
    }

    // try_to_lock previene inversión de prioridad en el hilo de audio si la UI está modificando la cadena
    std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return;
    }

    for (const auto& effect : m_effects) {
        if (effect && !effect->isBypassed()) {
            effect->process(interleavedBuffer, numFrames, channels, sampleRate);
        }
    }
}

void NovaDspChain::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& effect : m_effects) {
        if (effect) {
            effect->reset();
        }
    }
}
