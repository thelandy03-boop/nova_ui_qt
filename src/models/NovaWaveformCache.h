#ifndef NOVA_WAVEFORM_CACHE_H
#define NOVA_WAVEFORM_CACHE_H

#include <QString>
#include <vector>
#include <mutex>
#include <cstdint>
#include <memory>

struct PeakPoint {
    float min = 0.0f;
    float max = 0.0f;
    float rms = 0.0f;
};

struct PeakData {
    std::vector<PeakPoint> peaks;      // detalle (ej. 2000 pts)
    std::vector<PeakPoint> overview;   // overview (ej. 200 pts)
    uint32_t version = 1;
    uint32_t crc32 = 0;
    int sampleRate = 0;
    int channels = 0;
};

class NovaWaveformCache {
public:
    static void setPeaks(const QString &regionId, const PeakData &data);
    static bool getPeaks(const QString &regionId, PeakData &outData);
    static bool getOverview(const QString &regionId, std::vector<PeakPoint> &outPeaks);
    static void removePeaks(const QString &regionId);
    static void clear();

private:
    static inline std::map<QString, PeakData> s_cache;
    static inline std::mutex s_mutex;
};

#endif // NOVA_WAVEFORM_CACHE_H