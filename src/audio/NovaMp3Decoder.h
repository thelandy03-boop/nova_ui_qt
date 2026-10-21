#pragma once

#include "audio/NovaDecodedAudio.h"

#include <QString>

namespace nova {

/**
 * Decodifica un fichero MP3 entero a muestras en coma flotante.
 *
 * Se usa minimp3 (dominio publico, vendorizada en third_party/minimp3), que
 * no depende de mpg123 ni de libmad: las dos son la razon por la que el
 * motor de Nova lleva su propio decodificador (en Android no existen).
 *
 * Las etiquetas ID3 no cuentan como audio: se saltan. Si el fichero trae la
 * cabecera Xing/Info con la etiqueta LAME, se quita el retardo del
 * codificador y el relleno final, de modo que la duracion sea la real.
 *
 * @param path  Ruta del fichero a decodificar.
 * @param out   Audio decodificado; se rellena solo si la operacion tiene exito.
 * @param error Mensaje con el motivo del fallo (puede ser nullptr).
 * @return true si el fichero se ha decodificado entero.
 */
bool decodeMp3File(const QString& path, NovaDecodedAudio& out, QString* error);

} // namespace nova
