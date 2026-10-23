#ifndef NOVA_PEAK_DATA_H
#define NOVA_PEAK_DATA_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

/**
 * @brief Single peak point containing min, max, and RMS values
 */
struct PeakPoint {
    float min = 0.0f;
    float max = 0.0f;
    float rms = 0.0f;
};

/**
 * @brief Multi-resolution peak data for waveform rendering
 * Contains detail (high zoom) and overview (low zoom) peak arrays.
 *
 * La version incluye la resolucion y el algoritmo de calculo: al cambiarla,
 * los ficheros .novapeak antiguos se descartan y se recalculan (ver
 * NovaRegionModel::loadPeakFile).
 */
struct PeakData {
    std::vector<PeakPoint> peaks;      // detalle (alta resolucion)
    std::vector<PeakPoint> overview;   // vista general (zoom out)
    uint32_t version = 3;
    uint32_t crc32 = 0;
    int sampleRate = 0;
    int channels = 0;
};

/**
 * Numero de puntos de detalle para una duracion dada.
 *
 * 180 puntos por segundo son ~5,5 ms por punto: suficiente para que al
 * hacer zoom no se vean escalones, sin guardar un pico por muestra (que
 * para un tema de 5 minutos serian 14 millones de puntos). Se pone un
 * minimo para los clips cortos y un maximo para no disparar la memoria.
 */
inline int novaDetailPeakPoints(double durationSeconds)
{
    const double objetivo = durationSeconds * 180.0;
    const int puntos = static_cast<int>(std::lround(objetivo));
    return std::clamp(puntos, 8192, 32768);
}

/**
 * Numero de puntos de la vista general. Mantiene una proporcion de 1/16
 * respecto al detalle, con un minimo y un maximo razonables.
 */
inline int novaOverviewPeakPoints(int detailPoints)
{
    return std::clamp(detailPoints / 16, 512, 2048);
}

#endif // NOVA_PEAK_DATA_H