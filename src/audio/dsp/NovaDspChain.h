#pragma once

#include "audio/dsp/NovaDspEffect.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

/**
 * Cadena de procesamiento de efectos DSP secuencial.
 * Ejecuta en serie cada efecto sobre el buffer de audio intercalado.
 * Segura para llamadas desde el hilo de audio en tiempo real.
 */
class NovaDspChain {
public:
    NovaDspChain();
    ~NovaDspChain();

    void addEffect(std::shared_ptr<NovaDspEffect> effect);
    void replaceEffect(int index, std::shared_ptr<NovaDspEffect> effect);
    void removeEffect(int index);
    void clear();
    int effectCount() const;
    std::shared_ptr<NovaDspEffect> effectAt(int index) const;

    /**
     * Procesa secuencialmente el buffer a través de todos los efectos no muteados.
     */
    void process(float* interleavedBuffer, int numFrames, int channels, int sampleRate);

    /**
     * Procesa secuencialmente el buffer y encadena los eventos MIDI a través de los efectos.
     */
    void processWithMidi(float* interleavedBuffer, int numFrames, int channels, int sampleRate,
                         const std::vector<NovaMidiEvent>& midiIn,
                         std::vector<NovaMidiEvent>& midiOut);

    /**
     * Publica el estado del transporte. No toma el mutex de la cadena: los
     * valores se aplican al inicio del siguiente bloque de audio, de modo que
     * la UI nunca bloquea ni hace perder un bloque al callback de audio.
     */
    void setTransport(double playPositionSeconds, double bpm, bool playing);

    /** Reinicia todos los efectos de la cadena. */
    void reset();

private:
    mutable std::mutex m_mutex;
    std::vector<std::shared_ptr<NovaDspEffect>> m_effects;

    std::atomic<bool> m_transporteActivo{false};
    std::atomic<double> m_transporteSegundos{0.0};
    std::atomic<double> m_transporteBpm{120.0};
    std::atomic<bool> m_transporteReproduciendo{false};
};
