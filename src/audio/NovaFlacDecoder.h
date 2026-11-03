#pragma once

#include "audio/NovaDecodedAudio.h"

#include <QString>

namespace nova {

/**
 * Decodifica un fichero FLAC a muestras en coma flotante (32-bit float).
 *
 * Utiliza dr_flac (dominio público / MIT-0, vendorizado en third_party/dr_libs).
 * No depende de libflac del sistema ni de codecs externos.
 *
 * @param path  Ruta del fichero a decodificar.
 * @param out   Audio decodificado; se rellena solo si la operación tiene éxito.
 * @param error Mensaje con el motivo del fallo (puede ser nullptr).
 * @return true si el fichero se ha decodificado con éxito.
 */
bool decodeFlacFile(const QString& path, NovaDecodedAudio& out, QString* error);

} // namespace nova
