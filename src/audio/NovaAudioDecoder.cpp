#include "audio/NovaAudioDecoder.h"

#include "audio/NovaFlacDecoder.h"
#include "audio/NovaMp3Decoder.h"
#include "audio/NovaOggDecoder.h"
#include "audio/NovaWavReader.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>

#include <cstring>

namespace {

enum class FormatoFichero {
    Desconocido,
    Wav,
    Mp3,
    Flac,
    Ogg,
};

// El formato se deduce del contenido y no de la extension: un fichero con la
// extension equivocada debe abrirse igualmente, y uno con la extension
// correcta pero contenido raro debe fallar con un error claro.
FormatoFichero detectarFormato(const QString& path)
{
    QFile archivo(path);
    if (archivo.open(QIODevice::ReadOnly)) {
        const QByteArray inicio = archivo.read(16);
        archivo.close();

        if (inicio.size() >= 12
            && std::memcmp(inicio.constData(), "RIFF", 4) == 0
            && std::memcmp(inicio.constData() + 8, "WAVE", 4) == 0) {
            return FormatoFichero::Wav;
        }
        if (inicio.size() >= 4 && std::memcmp(inicio.constData(), "fLaC", 4) == 0) {
            return FormatoFichero::Flac;
        }
        if (inicio.size() >= 4 && std::memcmp(inicio.constData(), "OggS", 4) == 0) {
            return FormatoFichero::Ogg;
        }
        if (inicio.size() >= 3 && std::memcmp(inicio.constData(), "ID3", 3) == 0) {
            return FormatoFichero::Mp3; // etiqueta ID3v2 al principio
        }
        if (inicio.size() >= 2
            && static_cast<unsigned char>(inicio.at(0)) == 0xFF
            && (static_cast<unsigned char>(inicio.at(1)) & 0xE0) == 0xE0) {
            return FormatoFichero::Mp3; // sincronizacion de un frame MPEG
        }
    }

    const QString extension = QFileInfo(path).suffix().toLower();
    if (extension == "wav" || extension == "wave") {
        return FormatoFichero::Wav;
    }
    if (extension == "mp3" || extension == "mp2") {
        return FormatoFichero::Mp3;
    }
    if (extension == "flac") {
        return FormatoFichero::Flac;
    }
    if (extension == "ogg" || extension == "oga") {
        return FormatoFichero::Ogg;
    }
    return FormatoFichero::Desconocido;
}

} // namespace

bool decodeAudioFile(const QString& path, NovaDecodedAudio& out, QString* error)
{
    out = NovaDecodedAudio();
    if (error) {
        error->clear();
    }

    if (!QFileInfo::exists(path)) {
        if (error) {
            *error = QStringLiteral("el fichero '%1' no existe").arg(path);
        }
        return false;
    }
    if (QFileInfo(path).isDir()) {
        if (error) {
            *error = QStringLiteral("'%1' es un directorio, no un fichero de audio").arg(path);
        }
        return false;
    }

    switch (detectarFormato(path)) {
    case FormatoFichero::Wav:
        return nova::decodeWavFile(path, out, error);
    case FormatoFichero::Mp3:
        return nova::decodeMp3File(path, out, error);
    case FormatoFichero::Flac:
        return nova::decodeFlacFile(path, out, error);
    case FormatoFichero::Ogg:
        return nova::decodeOggFile(path, out, error);
    case FormatoFichero::Desconocido:
        break;
    }

    if (error) {
        *error = QStringLiteral("'%1' no tiene un formato de audio soportado: solo se admiten WAV, MP3, FLAC y OGG").arg(path);
    }
    return false;
}
