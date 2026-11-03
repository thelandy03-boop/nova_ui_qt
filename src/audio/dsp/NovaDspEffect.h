#pragma once

#include <QString>
#include <atomic>
#include <vector>

/**
 * Interfaz base para procesadores de efectos DSP en tiempo real en NOVA Studio.
 * Diseñada para ejecución sin bloqueos en el callback de audio (Oboe en Android,
 * ALSA/JACK/PipeWire o pruebas en Linux).
 */
class NovaDspEffect {
public:
    virtual ~NovaDspEffect() = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString category() const = 0;

    /**
     * Procesa un buffer de muestras de audio intercaladas (L R L R ...).
     *
     * @param interleavedBuffer Puntero al buffer de entrada/salida.
     * @param numFrames         Número de fotogramas a procesar.
     * @param channels          Número de canales (típicamente 1 o 2).
     * @param sampleRate        Frecuencia de muestreo en Hz (ej. 48000).
     */
    virtual void process(float* interleavedBuffer, int numFrames, int channels, int sampleRate) = 0;

    /** Ajusta un parámetro del efecto según su índice. */
    virtual void setParameter(int paramIndex, float value) = 0;

    /** Obtiene el valor actual de un parámetro. */
    virtual float getParameter(int paramIndex) const = 0;

    /** Reinicia buffers internos y estados de filtros. */
    virtual void reset() = 0;

    void setBypassed(bool bypassed) { m_bypassed.store(bypassed, std::memory_order_relaxed); }
    bool isBypassed() const { return m_bypassed.load(std::memory_order_relaxed); }

protected:
    std::atomic<bool> m_bypassed{false};
};
