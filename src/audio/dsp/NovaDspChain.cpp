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
    std::vector<NovaMidiEvent> inMidi, outMidi;
    processWithMidi(interleavedBuffer, numFrames, channels, sampleRate, inMidi, outMidi);
}

void NovaDspChain::processWithMidi(float* interleavedBuffer, int numFrames, int channels, int sampleRate,
                                   const std::vector<NovaMidiEvent>& midiIn,
                                   std::vector<NovaMidiEvent>& midiOut)
{
    if (!interleavedBuffer || numFrames <= 0 || channels <= 0 || sampleRate <= 0) {
        return;
    }

    // try_to_lock previene inversión de prioridad en el hilo de audio si la UI está modificando la cadena
    std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return;
    }

    // El transporte se publica sin mutex; se aplica aquí, ya en el hilo de
    // audio, antes de que los efectos ejecuten @block y @sample.
    if (m_transporteActivo.load(std::memory_order_acquire)) {
        const double segundos = m_transporteSegundos.load(std::memory_order_relaxed);
        const double bpm = m_transporteBpm.load(std::memory_order_relaxed);
        const bool reproduciendo =
            m_transporteReproduciendo.load(std::memory_order_relaxed);
        for (const auto& effect : m_effects) {
            if (effect) {
                effect->setTransport(segundos, bpm, reproduciendo);
            }
        }
    }

    std::vector<NovaMidiEvent> corrienteMidiActual = midiIn;
    for (const auto& effect : m_effects) {
        if (effect && !effect->isBypassed()) {
            std::vector<NovaMidiEvent> salidaDelEfecto;
            effect->processWithMidi(interleavedBuffer, numFrames, channels, sampleRate,
                                    corrienteMidiActual, salidaDelEfecto);
            if (!salidaDelEfecto.empty()) {
                corrienteMidiActual = std::move(salidaDelEfecto);
            }
        }
    }
    midiOut = std::move(corrienteMidiActual);
}

void NovaDspChain::setTransport(double playPositionSeconds, double bpm, bool playing)
{
    m_transporteSegundos.store(playPositionSeconds, std::memory_order_relaxed);
    m_transporteBpm.store(bpm > 0.0 ? bpm : 120.0, std::memory_order_relaxed);
    m_transporteReproduciendo.store(playing, std::memory_order_relaxed);
    m_transporteActivo.store(true, std::memory_order_release);
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
