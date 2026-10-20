#ifndef NOVA_SIMD_PEAKS_H
#define NOVA_SIMD_PEAKS_H

#include <cstddef>
#include <vector>
#include "../core/NovaPeakData.h"

/**
 * @brief Process a block of audio samples to update peak bins using SIMD if available.
 *
 * @param samples Pointer to the audio samples (float*).
 * @param numSamples Number of samples to process.
 * @param startPos Starting sample position in the region (relative to region start).
 * @param detailStep Step size for detail bin (samples per bin).
 * @param overviewStep Step size for overview bin.
 * @param detailPoints Number of detail bins (for clamping).
 * @param overviewPoints Number of overview bins (for clamping).
 * @param detailMin Array of detail min values (to be updated).
 * @param detailMax Array of detail max values.
 * @param detailRms Array of detail sum of squares.
 * @param detailCount Array of detail sample counts.
 * @param overviewMin, overviewMax, overviewRms, overviewCount: same for overview.
 */
void processPeakBlockSimd(const float* samples, size_t numSamples,
                          double startPos, double detailStep, double overviewStep,
                          int detailPoints, int overviewPoints,
                          std::vector<float>& detailMin, std::vector<float>& detailMax,
                          std::vector<float>& detailRms, std::vector<int>& detailCount,
                          std::vector<float>& overviewMin, std::vector<float>& overviewMax,
                          std::vector<float>& overviewRms, std::vector<int>& overviewCount);

/**
 * @brief Inicializa los acumuladores de picos de un conjunto de bins.
 *
 * El minimo se inicializa a +FLT_MAX y el maximo a -FLT_MAX: el audio es
 * bipolar, asi que inicializarlos a 0 haria que los minimos negativos
 * NUNCA se registrasen (nunca son menores que 0) y la forma de onda
 * apareciese solo por encima del eje.
 */
void initPeakAccumulators(std::vector<float>& min, std::vector<float>& max,
                          std::vector<float>& rms, std::vector<int>& count, int points);

#endif // NOVA_SIMD_PEAKS_H