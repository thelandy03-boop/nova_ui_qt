#pragma once

#include "audio/dsp/NovaDspEffect.h"
#include <vector>
#include <memory>
#include <mutex>

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

    /** Reinicia todos los efectos de la cadena. */
    void reset();

private:
    mutable std::mutex m_mutex;
    std::vector<std::shared_ptr<NovaDspEffect>> m_effects;
};
