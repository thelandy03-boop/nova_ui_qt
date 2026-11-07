#pragma once

#include "audio/dsp/NovaMidiEvent.h"
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

    /**
     * Procesa audio y eventos MIDI de forma sincronizada dentro del bloque.
     * La implementación base redirige a process() ignorando el MIDI.
     */
    virtual void processWithMidi(float* interleavedBuffer, int numFrames, int channels, int sampleRate,
                                 const std::vector<NovaMidiEvent>& midiIn,
                                 std::vector<NovaMidiEvent>& midiOut)
    {
        (void)midiIn;
        (void)midiOut;
        process(interleavedBuffer, numFrames, channels, sampleRate);
    }

    /** Ajusta un parámetro del efecto según su índice. */
    virtual void setParameter(int paramIndex, float value) = 0;

    /** Obtiene el valor actual de un parámetro. */
    virtual float getParameter(int paramIndex) const = 0;

    /**
     * Informa del transporte del proyecto. Se llama desde el hilo de la UI
     * (no desde el callback de audio) y debe ser barato: los efectos que no
     * usan transporte lo ignoran.
     *
     * @param playPositionSeconds Posición del playhead en segundos.
     * @param bpm                 Tempo del proyecto.
     * @param playing             true si el transporte está reproduciendo.
     */
    virtual void setTransport(double playPositionSeconds, double bpm, bool playing)
    {
        (void)playPositionSeconds;
        (void)bpm;
        (void)playing;
    }

    /** Reinicia buffers internos y estados de filtros. */
    virtual void reset() = 0;

    void setBypassed(bool bypassed) { m_bypassed.store(bypassed, std::memory_order_relaxed); }
    bool isBypassed() const { return m_bypassed.load(std::memory_order_relaxed); }

protected:
    std::atomic<bool> m_bypassed{false};
};
