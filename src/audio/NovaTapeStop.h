#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

/**
 * Efecto analógico Tape Stop (Frenado y arranque de cinta de cassette / vinilo DJ).
 *
 * Basado en el algoritmo DSP de Honeydew (crates/audio/src/tape_stop.rs).
 * Utiliza un búfer circular con lectura sub-fotograma interpolada para recrear
 * el frenado de velocidad y tono característico al pausar o como efecto de inserción.
 */
class NovaTapeStop {
public:
    NovaTapeStop(float sampleRate = 48000.0f, float tempoBpm = 120.0f);
    ~NovaTapeStop() = default;

    /** Inicializa o redimensiona el búfer según tasa de muestreo y tempo. */
    void prepare(float sampleRate, float tempoBpm);

    /**
     * Activa el efecto de frenado (stop = true) o recuperación (stop = false).
     * @param stop true para frenar hasta detenerse; false para acelerar a velocidad normal.
     * @param durationSeconds Duración de la frenada en segundos (ej: 0.35s).
     */
    void trigger(bool stop, float durationSeconds = 0.35f);

    /** Procesa un fotograma estéreo (L, R). */
    void processFrame(float inL, float inR, float &outL, float &outR);

    /** Reinicia el estado interno a silencio y velocidad normal. */
    void reset();

    /** ¿Está el efecto activo frenando o acelerando? */
    bool isActive() const { return m_remaining > 0 || m_phase < 0.999f; }

    /** Velocidad actual de reproducción (1.0 = normal, 0.0 = frenado). */
    float playbackSpeed() const;

private:
    static size_t calculateCapacity(float sampleRate, float tempoBpm);
    float readSample(const std::vector<float> &buffer, double head) const;

    float m_sampleRate = 48000.0f;
    float m_tempoBpm = 120.0f;

    std::vector<float> m_leftBuffer;
    std::vector<float> m_rightBuffer;

    uint64_t m_writeHead = 0;
    double m_readHead = 0.0;

    float m_phase = 1.0f;       // 1.0 = normal, 0.0 = detenido
    float m_phaseStep = 0.0f;
    size_t m_remaining = 0;     // Fotogramas restantes en la rampa

    float m_fadeIn = 0.0f;
    float m_heldL = 0.0f;
    float m_heldR = 0.0f;
    bool m_isStopping = false;
};
