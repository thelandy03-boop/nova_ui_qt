#pragma once

#include "audio/NovaDecodedAudio.h"

#include <QString>

namespace nova {

/**
 * Decodifica un fichero OGG Vorbis a muestras en coma flotante (32-bit float).
 *
 * Utiliza stb_vorbis (dominio público, vendorizado en third_party/stb).
 * No depende de libvorbis del sistema ni de codecs externos.
 *
 * @param path  Ruta del fichero a decodificar.
 * @param out   Audio decodificado; se rellena solo si la operación tiene éxito.
 * @param error Mensaje con el motivo del fallo (puede ser nullptr).
 * @return true si el fichero se ha decodificado con éxito.
 */
bool decodeOggFile(const QString& path, NovaDecodedAudio& out, QString* error);

} // namespace nova
