#include "audio/NovaOggDecoder.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <cstdlib>
#include <vector>

// stb_vorbis como implementación autónoma
#undef STB_VORBIS_HEADER_ONLY
#include "../../third_party/stb/stb_vorbis.c"

namespace nova {

bool decodeOggFile(const QString& path, NovaDecodedAudio& out, QString* error)
{
    out = NovaDecodedAudio();
    if (error) {
        error->clear();
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("no se pudo abrir el archivo OGG: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        if (error) {
            *error = QStringLiteral("el archivo OGG está vacío");
        }
        return false;
    }

    int channels = 0;
    int sampleRate = 0;
    short* rawShorts = nullptr;

    const int samplesPerChannel = stb_vorbis_decode_memory(
        reinterpret_cast<const unsigned char*>(data.constData()),
        data.size(),
        &channels,
        &sampleRate,
        &rawShorts
    );

    if (samplesPerChannel <= 0 || !rawShorts) {
        if (rawShorts) {
            free(rawShorts);
        }
        if (error) {
            *error = QStringLiteral("no se pudo decodificar el flujo Ogg Vorbis (archivo dañado o formato no soportado)");
        }
        return false;
    }

    const size_t totalSamples = static_cast<size_t>(samplesPerChannel) * static_cast<size_t>(channels);
    std::vector<float> floatSamples(totalSamples);

    for (size_t i = 0; i < totalSamples; ++i) {
        floatSamples[i] = static_cast<float>(rawShorts[i]) / 32768.0f;
    }
    free(rawShorts);

    out.sampleRate = sampleRate;
    out.channels = channels;
    out.samples = std::move(floatSamples);
    return true;
}

} // namespace nova
