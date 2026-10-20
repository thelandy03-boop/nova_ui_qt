#pragma once

#include "audio/NovaDecodedAudio.h"

#include <QString>

namespace nova {

/**
 * Decodifica un fichero WAV (RIFF/WAVE) entero a muestras en coma flotante.
 *
 * Se admiten los formatos de muestra habituales: PCM de 16, 24 y 32 bits
 * enteros y coma flotante de 32 bits (WAVE_FORMAT_IEEE_FLOAT), tambien
 * dentro de un chunk WAVE_FORMAT_EXTENSIBLE, cuyo subformato real se usa
 * para decidir como interpretar los bytes.
 *
 * @param path  Ruta del fichero a decodificar.
 * @param out   Audio decodificado; se rellena solo si la operacion tiene exito.
 * @param error Mensaje con el motivo del fallo (puede ser nullptr).
 * @return true si el fichero se ha decodificado entero.
 */
bool decodeWavFile(const QString& path, NovaDecodedAudio& out, QString* error);

} // namespace nova
