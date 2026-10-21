#include "audio/NovaAudioPeaks.h"
#include "core/NovaSimdPeaks.h"

#include <cmath>
#include <vector>

namespace {

// Un bin sin muestras no vale nada: su mínimo y su máximo siguen siendo los
// valores de inicialización (+FLT_MAX y -FLT_MAX), que no son audio. Un bin
// vacío se dibuja como silencio.
PeakPoint puntoDeBin(float minimo, float maximo, float sumaCuadrados, int muestras)
{
    if (muestras <= 0) {
        return PeakPoint{0.0f, 0.0f, 0.0f};
    }
    const float rms = std::sqrt(sumaCuadrados / static_cast<float>(muestras));
    return PeakPoint{minimo, maximo, rms};
}

} // namespace

PeakData buildPeakData(const NovaDecodedAudio& audio, int detailPoints, int overviewPoints)
{
    PeakData data;
    data.version = 2;
    data.sampleRate = audio.sampleRate;
    data.channels = audio.channels;

    const qint64 frames = audio.frameCount();
    if (frames <= 0 || detailPoints <= 0 || overviewPoints <= 0) {
        return data;
    }

    // Las muestras van intercaladas (L R L R ...), así que el paso de cada
    // bin se cuenta en MUESTRAS, no en fotogramas: con el paso en fotogramas,
    // en un fichero estéreo solo se llenaría la mitad de los bins y la otra
    // mitad se dibujaría como si fuera silencio.
    const int canales = (audio.channels > 0) ? audio.channels : 1;
    const double detailStep = (static_cast<double>(frames) / detailPoints) * canales;
    const double overviewStep = (static_cast<double>(frames) / overviewPoints) * canales;

    // Los acumuladores se inicializan con initPeakAccumulators(): el mínimo
    // a +FLT_MAX y el máximo a -FLT_MAX, porque el audio es bipolar (ver
    // src/core/NovaSimdPeaks.h).
    std::vector<float> detailMin, detailMax, detailRms, overviewMin, overviewMax, overviewRms;
    std::vector<int> detailCount, overviewCount;
    initPeakAccumulators(detailMin, detailMax, detailRms, detailCount, detailPoints);
    initPeakAccumulators(overviewMin, overviewMax, overviewRms, overviewCount, overviewPoints);

    processPeakBlockSimd(audio.samples.data(), audio.samples.size(),
                         0.0, detailStep, overviewStep,
                         detailPoints, overviewPoints,
                         detailMin, detailMax, detailRms, detailCount,
                         overviewMin, overviewMax, overviewRms, overviewCount);

    data.peaks.reserve(static_cast<size_t>(detailPoints));
    for (int i = 0; i < detailPoints; ++i) {
        data.peaks.push_back(puntoDeBin(detailMin[i], detailMax[i], detailRms[i], detailCount[i]));
    }

    data.overview.reserve(static_cast<size_t>(overviewPoints));
    for (int i = 0; i < overviewPoints; ++i) {
        data.overview.push_back(puntoDeBin(overviewMin[i], overviewMax[i], overviewRms[i], overviewCount[i]));
    }

    return data;
}
