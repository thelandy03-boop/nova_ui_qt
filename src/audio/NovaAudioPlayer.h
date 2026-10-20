#pragma once

#include "audio/NovaDecodedAudio.h"
#include "audio/NovaResampler.h"

#include <QString>

#include <atomic>

/**
 * Reproductor de un audio ya decodificado, con remuestreo a la tasa de
 * salida.
 *
 * Es la pieza que, en el móvil, alimentará el callback de Oboe: el motor le
 * pide fotogramas y el reproductor le entrega muestras intercaladas
 * (L R L R ...) listas para escribir en el buffer del stream, ya convertidas
 * a la tasa que quiere el stream (48 kHz en el móvil).
 *
 * El reproductor no sabe nada de Oboe ni de Qt Audio: solo mueve muestras.
 * Por eso se puede probar entero en el host.
 *
 * Garantía importante: `renderFrames()` nunca escribe fuera del buffer que
 * le da el llamante y nunca devuelve más fotogramas de los que quedan, así
 * que la posición nunca pasa de la duración. Es lo que evita el overrun del
 * buffer en el móvil.
 *
 * La clase no es segura entre hilos: el llamante (el motor de Oboe) la usa
 * desde el hilo de audio. La única excepción es la bandera de "sonando", que
 * es atómica porque el hilo de la interfaz la pone y el hilo de audio la lee.
 * El resto del estado (el audio cargado, la posición, el remuestreador) solo
 * lo toca un hilo cada vez: el motor de Oboe garantiza que el callback no
 * corre mientras carga o rearranca el audio.
 */
class NovaAudioPlayer {
public:
    /**
     * Carga un audio decodificado y calcula su duración con la tasa de
     * salida vigente.
     *
     * El reproductor guarda su propia copia del audio, así que el llamante
     * puede destruir el suyo.
     *
     * @param audio Audio a reproducir.
     * @param error Mensaje con el motivo del fallo (puede ser nullptr).
     * @return true si el audio se ha cargado; si no, false y un mensaje de
     *         error (nunca vacío cuando falla).
     */
    bool load(const NovaDecodedAudio& audio, QString* error);

    /**
     * Cambia la tasa de salida, en Hz (48.000 en el móvil).
     *
     * Si ya hay audio cargado, la duración se recalcula con la tasa nueva y
     * la reproducción vuelve al principio: el audio ya emitido a otra tasa
     * no tiene sentido.
     *
     * Las tasas no válidas (cero o negativas) se ignoran.
     */
    void setOutputSampleRate(int sampleRate);

    /** Empieza (o sigue) la reproducción, si hay audio cargado. */
    void play();

    /** Detiene la reproducción; la posición se conserva. */
    void stop();

    /** true si el reproductor está sonando. */
    bool isPlaying() const;

    /** Fotogramas de salida ya emitidos. */
    qint64 positionFrames() const;

    /** Fotogramas de salida que ocupa el fichero completo. */
    qint64 durationFrames() const;

    /** Duración del fichero en segundos, a la tasa de salida vigente. */
    double durationSeconds() const;

    /**
     * Escribe en `interleavedOut` los fotogramas que queden de audio.
     *
     * Las muestras se escriben intercaladas (L R L R ...), con los canales
     * del fichero cargado. Si el fichero se agota, devuelve menos fotogramas
     * de los pedidos (incluso 0) y deja de estar sonando.
     *
     * Al final del fichero, los últimos fotogramas repiten el último
     * fotograma del audio (como mucho uno por cada fotograma de entrada que
     * "cabe" en uno de salida), porque no hay muestra siguiente con la que
     * interpolar.
     *
     * @param interleavedOut Buffer donde escribir las muestras (debe caber
     *                       `frames` fotogramas con los canales del
     *                       fichero).
     * @param frames          Número de fotogramas que caben en el buffer.
     * @return Número de fotogramas escritos (0 si no está sonando).
     */
    int renderFrames(float* interleavedOut, int frames);

    /** Vuelve al principio del audio y limpia el remuestreador. */
    void reset();

private:
    /** Recalcula la duración con la tasa de salida vigente. */
    void recalcularDuracion();

    /** Fotogramas de entrada por cada fotograma de salida. */
    double ratioEntradaPorSalida() const;

    /** Audio cargado (copia propia). */
    NovaDecodedAudio m_audio;

    /** Remuestreador entre la tasa del fichero y la tasa de salida. */
    NovaResampler m_remuestreador;

    /** Tasa de salida, en Hz (48.000 en el móvil, que es la tasa por defecto). */
    int m_tasaSalida = 48000;

    /** Tasa del fichero cargado, en Hz. */
    int m_tasaEntrada = 0;

    /** Número de canales del fichero cargado. */
    int m_canales = 0;

    /** Fotogramas que tiene el fichero cargado. */
    qint64 m_fotogramasEntrada = 0;

    /** Fotogramas de entrada que ya se le han pasado al remuestreador. */
    qint64 m_fotogramasAlimentados = 0;

    /** Fotogramas de salida ya emitidos. */
    qint64 m_fotogramasPosicion = 0;

    /** Fotogramas de salida que ocupa el fichero completo. */
    qint64 m_fotogramasDuracion = 0;

    /** true si hay audio cargado. */
    bool m_cargado = false;

    /**
     * true si el reproductor está sonando.
     *
     * Es atómico porque lo pone el hilo de la interfaz (`play()`/`stop()`) y
     * lo lee el hilo de audio (el callback de Oboe), que son hilos distintos.
     */
    std::atomic<bool> m_sonando{false};
};
