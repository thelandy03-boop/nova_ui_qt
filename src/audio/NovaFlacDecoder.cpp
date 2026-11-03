#include "audio/NovaFlacDecoder.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <vector>

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#include "../../third_party/dr_libs/dr_flac.h"

namespace nova {

bool decodeFlacFile(const QString& path, NovaDecodedAudio& out, QString* error)
{
    out = NovaDecodedAudio();
    if (error) {
        error->clear();
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("no se pudo abrir el archivo FLAC: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        if (error) {
            *error = QStringLiteral("el archivo FLAC está vacío");
        }
        return false;
    }

    drflac* pFlac = drflac_open_memory(reinterpret_cast<const void*>(data.constData()),
                                       static_cast<size_t>(data.size()),
                                       nullptr);
    if (!pFlac) {
        if (error) {
            *error = QStringLiteral("cabecera o flujo FLAC corrupto");
        }
        return false;
    }

    const uint32_t channels = pFlac->channels;
    const uint32_t sampleRate = pFlac->sampleRate;
    const drflac_uint64 totalPCMFrameCount = pFlac->totalPCMFrameCount;

    if (channels == 0 || sampleRate == 0 || totalPCMFrameCount == 0) {
        drflac_close(pFlac);
        if (error) {
            *error = QStringLiteral("fichero FLAC sin pistas de audio válidas");
        }
        return false;
    }

    std::vector<float> samples(static_cast<size_t>(totalPCMFrameCount * channels));
    const drflac_uint64 framesRead = drflac_read_pcm_frames_f32(pFlac, totalPCMFrameCount, samples.data());
    drflac_close(pFlac);

    if (framesRead == 0) {
        if (error) {
            *error = QStringLiteral("no se pudieron leer fotogramas del archivo FLAC");
        }
        return false;
    }

    samples.resize(static_cast<size_t>(framesRead * channels));
    out.sampleRate = static_cast<int>(sampleRate);
    out.channels = static_cast<int>(channels);
    out.samples = std::move(samples);
    return true;
}

} // namespace nova
