#ifndef NOVA_PEAK_DATA_H
#define NOVA_PEAK_DATA_H

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
 * Contains detail (high zoom) and overview (low zoom) peak arrays
 */
struct PeakData {
    std::vector<PeakPoint> peaks;      // detail (e.g. 2000 points)
    std::vector<PeakPoint> overview;   // overview (e.g. 200 points)
    uint32_t version = 1;
    uint32_t crc32 = 0;
    int sampleRate = 0;
    int channels = 0;
};

#endif // NOVA_PEAK_DATA_H