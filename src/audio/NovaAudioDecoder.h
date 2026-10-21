#pragma once

#include "audio/NovaDecodedAudio.h"

#include <QString>

/**
 * Decodifica un fichero de audio a muestras en coma flotante.
 *
 * El formato se deduce del contenido del fichero, no de su extension: un
 * fichero con la extension equivocada se abre igualmente, y uno con la
 * extension correcta pero contenido raro falla con un error claro.
 *
 * @param path  Ruta del fichero a decodificar.
 * @param out   Audio decodificado; se rellena solo si la operacion tiene exito.
 * @param error Mensaje con el motivo del fallo (puede ser nullptr).
 * @return true solo si el audio se ha decodificado entero; si no, false y un
 *         mensaje de error (nunca vacio cuando falla).
 */
bool decodeAudioFile(const QString& path, NovaDecodedAudio& out, QString* error);
