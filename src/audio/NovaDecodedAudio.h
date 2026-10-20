#pragma once

#include <QtGlobal>

#include <vector>

/**
 * Audio decodificado y listo para usarse: muestras en coma flotante
 * intercaladas (L R L R ...), con la tasa y el numero de canales del
 * fichero original.
 *
 * Es el formato que produce `decodeAudioFile()` y que consumen el motor de
 * audio y el dibujado de la forma de onda.
 */
struct NovaDecodedAudio {
    /** Tasa de muestreo en Hz (0 si no se ha decodificado nada). */
    int sampleRate = 0;

    /** Numero de canales (0 si no se ha decodificado nada). */
    int channels = 0;

    /** Muestras intercaladas: L R L R ... */
    std::vector<float> samples;

    /** Numero de fotogramas (muestras por canal). */
    qint64 frameCount() const
    {
        return channels > 0 ? static_cast<qint64>(samples.size()) / channels : 0;
    }
};
