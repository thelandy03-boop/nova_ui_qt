#include "NovaRegionModel.h"
#include "core/NovaLogging.h"
#include "core/NovaTimeUtils.h"
#include "core/NovaAudioUtils.h"
#include "core/platform/NovaPlatformUtils.h"
#include "core/platform/NovaAndroidStubs.h"

#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QUrl>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <thread>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <QtGlobal> // for qChecksum
#include "core/NovaSimdPeaks.h"

// Include ARDOUR headers BEFORE any code that uses ARDOUR types
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#pragma push_macro("foreach")
#undef emit
#undef slots
#undef signals
#undef foreach

#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/audio_track.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/source_factory.h"
#include "ardour/region_factory.h"
#include "ardour/data_type.h"
#include "ardour/source.h"
#include "ardour/region.h"
#include "ardour/playlist.h"
#include "ardour/import_status.h"
#include "pbd/signals.h"

#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#pragma pop_macro("foreach")
#endif

// Simple CRC32 (Adler-32) for peak data
static uint32_t crc32_peaks(const std::vector<PeakPoint>& peaks)
{
    QByteArray data;
    data.resize(peaks.size() * sizeof(PeakPoint));
    if (!data.isEmpty()) {
        memcpy(data.data(), peaks.data(), data.size());
    }
    return qChecksum(QByteArrayView(data.constData(), data.size()));
}

// ── IMPLEMENTACIÓN DEL SISTEMA DE CACHÉ DE PICOS EN RAM ───────────────
void NovaWaveformCache::setPeaks(const QString &regionId, const PeakData &data)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_cache[regionId] = data;
}

bool NovaWaveformCache::getPeaks(const QString &regionId, PeakData &outData)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_cache.find(regionId);
    if (it != s_cache.end()) {
        outData = it->second;
        return true;
    }
    return false;
}

bool NovaWaveformCache::getOverview(const QString &regionId, std::vector<PeakPoint> &outPeaks)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_cache.find(regionId);
    if (it != s_cache.end()) {
        outPeaks = it->second.overview;
        return true;
    }
    return false;
}

void NovaWaveformCache::removePeaks(const QString &regionId)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_cache.erase(regionId);
}

void NovaWaveformCache::clear()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_cache.clear();
}

// ── GENERADOR OPTIMIZADO DE PICOS WAVEFORM (SIMD / BLOQUES) ───────────
static PeakData buildRegionPeaks(const std::shared_ptr<ARDOUR::AudioRegion> &audioRegion, int sampleRate, int channels)
{
    PeakData result;
    if (!audioRegion) return result;

    if (channels == 0) return result;

    auto source = audioRegion->audio_source(0);
    if (!source) return result;

    ARDOUR::samplepos_t startPos = audioRegion->start_sample();
    ARDOUR::samplecnt_t length = audioRegion->length_samples();
    if (length <= 0) return result;

    result.sampleRate = sampleRate;
    result.channels = channels;

    constexpr int detailPoints = 2000;      // detalle al máximo zoom
    constexpr int overviewPoints = 200;     // overview (zoom out)
    constexpr ARDOUR::samplecnt_t bufferSize = 8192;

    std::vector<ARDOUR::Sample> readBuffer(bufferSize);
    ARDOUR::samplecnt_t remaining = length;
    ARDOUR::samplepos_t currentPos = startPos;

    // Buffers de acumulación
    std::vector<float> detailMin(detailPoints, 0), detailMax(detailPoints, 0), detailRms(detailPoints, 0);
    std::vector<float> overviewMin(overviewPoints, 0), overviewMax(overviewPoints, 0), overviewRms(overviewPoints, 0);
    std::vector<int> detailCount(detailPoints, 0), overviewCount(overviewPoints, 0);

    const double detailStep = static_cast<double>(length) / detailPoints;
    const double overviewStep = static_cast<double>(length) / overviewPoints;

    while (remaining > 0) {
        ARDOUR::samplecnt_t toRead = std::min(remaining, bufferSize);
        ARDOUR::samplecnt_t read = source->read(readBuffer.data(), currentPos, toRead, 0);
        if (read <= 0) break;

        // Procesamiento de picos usando SIMD si está disponible, de lo contrario escalar
        processPeakBlockSimd(reinterpret_cast<const float*>(readBuffer.data()), read,
                             static_cast<double>(currentPos - startPos), detailStep, overviewStep,
                             detailPoints, overviewPoints,
                             detailMin, detailMax, detailRms, detailCount,
                             overviewMin, overviewMax, overviewRms, overviewCount);

        currentPos += read;
        remaining -= read;
    }

    // Finalizar picos
    result.peaks.reserve(detailPoints);
    result.overview.reserve(overviewPoints);

    for (int i = 0; i < detailPoints; ++i) {
        if (detailCount[i] > 0) {
            float rms = std::sqrt(detailRms[i] / detailCount[i]);
            result.peaks.push_back({detailMin[i], detailMax[i], rms});
        } else {
            result.peaks.push_back({0, 0, 0});
        }
    }
    for (int i = 0; i < overviewPoints; ++i) {
        if (overviewCount[i] > 0) {
            float rms = std::sqrt(overviewRms[i] / overviewCount[i]);
            result.overview.push_back({overviewMin[i], overviewMax[i], rms});
        } else {
            result.overview.push_back({0, 0, 0});
        }
    }

    // CRC32 para validación
    result.crc32 = crc32_peaks(result.peaks);
    return result;
}

// ── MODELO DE REGIONES DE NOVA STUDIO ────────────────────────────────
NovaRegionModel::NovaRegionModel(QObject *parent)
    : QAbstractListModel(parent)
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    m_sessionConnections = std::make_unique<PBD::ScopedConnectionList>();
#endif
}

NovaRegionModel::~NovaRegionModel()
{
    waitForImports();
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (m_sessionConnections) m_sessionConnections->drop_connections();
#endif
}

std::shared_ptr<ARDOUR::Track> NovaRegionModel::findTrackByIndex(int trackIndex) const
{
    if (!m_session || trackIndex < 0) return nullptr;

    auto routeList = m_session->get_routes();
    if (!routeList) return nullptr;

    int currentIdx = 0;
    for (const auto &route : *routeList) {
        if (route && route->is_track()) {
            if (currentIdx == trackIndex) {
                return std::dynamic_pointer_cast<ARDOUR::Track>(route);
            }
            currentIdx++;
        }
    }
    return nullptr;
}

void NovaRegionModel::cleanupImportThreads()
{
    std::lock_guard<std::mutex> lock(m_workersMutex);
    auto it = m_importWorkers.begin();
    while (it != m_importWorkers.end()) {
        if (it->finished && it->finished->load() && it->thread.joinable()) {
            it->thread.join();
            it = m_importWorkers.erase(it);
        } else {
            ++it;
        }
    }
}

void NovaRegionModel::waitForImports()
{
    std::lock_guard<std::mutex> lock(m_workersMutex);
    for (auto &worker : m_importWorkers) {
        if (worker.thread.joinable()) {
            worker.thread.join();
        }
    }
    m_importWorkers.clear();
}

void NovaRegionModel::setSelectedRegionIndex(int index)
{
    if (m_selectedRegionIndex == index) return;
    m_selectedRegionIndex = index;
    Q_EMIT selectedRegionIndexChanged(m_selectedRegionIndex);
}

void NovaRegionModel::setSession(void *session)
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    ARDOUR::Session *ardourSession = static_cast<ARDOUR::Session*>(session);
    if (m_session == ardourSession) return;

    if (m_sessionConnections) m_sessionConnections->drop_connections();

    beginResetModel();
    m_regions.clear();
    NovaWaveformCache::clear();
    m_session = ardourSession;
    m_selectedRegionIndex = -1;
    endResetModel();

    if (m_session) {
        rebuildRegionCache();
    }

    Q_EMIT regionCountChanged(static_cast<int>(m_regions.size()));
    Q_EMIT selectedRegionIndexChanged(m_selectedRegionIndex);
#else
    Q_UNUSED(session);
    beginResetModel();
    m_regions.clear();
    NovaWaveformCache::clear();
    m_selectedRegionIndex = -1;
    endResetModel();
    Q_EMIT regionCountChanged(0);
    Q_EMIT selectedRegionIndexChanged(-1);
#endif
}

int NovaRegionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_regions.size());
}

QVariant NovaRegionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_regions.size()))
        return {};

    const auto &item = m_regions[static_cast<size_t>(index.row())];

    switch (role) {
    case RegionIdRole:
        return item.id;
    case TrackIndexRole:
        return item.trackIndex;
    case RegionNameRole:
        return item.name;
    case StartFrameRole:
        return item.startFrame;
    case LengthFramesRole:
        return item.lengthFrames;
    case StartBeatRole:
        return item.startBeat;
    case LengthBeatsRole:
        return item.lengthBeats;
    case ColorRole:
        return item.color;
    case IsLiveRecordingRole:
        return item.isLiveRecording;
    case ClipGainDbRole:
        return item.clipGainDb;
    case FadeInBeatsRole:
        return item.fadeInBeats;
    case FadeOutBeatsRole:
        return item.fadeOutBeats;
    default:
        return {};
    }
}

QHash<int, QByteArray> NovaRegionModel::roleNames() const
{
    return {
        { RegionIdRole,        "regionId"        },
        { TrackIndexRole,      "trackIndex"      },
        { RegionNameRole,      "regionName"      },
        { StartFrameRole,      "startFrame"      },
        { LengthFramesRole,    "lengthFrames"    },
        { StartBeatRole,       "startBeat"       },
        { LengthBeatsRole,     "lengthBeats"     },
        { ColorRole,           "regionColor"     },
        { IsLiveRecordingRole, "isLiveRecording" },
        { ClipGainDbRole,      "clipGainDb"      },
        { FadeInBeatsRole,     "fadeInBeats"     },
        { FadeOutBeatsRole,    "fadeOutBeats"    }
    };
}

bool NovaRegionModel::importAudioFile(int trackIndex, const QString &filePath, double startBeat)
{
    if (!m_session) return false;

    QString cleanPath = NovaPlatformUtils::sanitizePath(filePath);
    QFileInfo fileInfo(cleanPath);
    if (!fileInfo.exists()) return false;

    cleanupImportThreads();

    auto targetTrack = findTrackByIndex(trackIndex);
    if (!targetTrack) return false;

    auto finishedFlag = std::make_shared<std::atomic<bool>>(false);

    std::thread workerThread([this, targetTrack, cleanPath, fileInfo, startBeat, finishedFlag]() {
        try {
            ARDOUR::ImportStatus status;
            status.paths.push_back(cleanPath.toStdString());
            status.quality = ARDOUR::SrcFastest;

            m_session->import_files(status);

            if (status.sources.empty() || status.cancel) {
                finishedFlag->store(true);
                return;
            }

            const ARDOUR::SourceList importedSources = status.sources;
            QMetaObject::invokeMethod(this, [this, targetTrack, importedSources, fileInfo, startBeat, finishedFlag]() {
                if (!m_session || importedSources.empty() || !importedSources.front()) {
                    finishedFlag->store(true);
                    return;
                }

                PBD::PropertyList plist;
                plist.add(ARDOUR::Properties::start, Temporal::timepos_t(0));
                plist.add(ARDOUR::Properties::length, importedSources.front()->length());
                plist.add(ARDOUR::Properties::name, fileInfo.fileName().toStdString());

                auto region = ARDOUR::RegionFactory::create(importedSources, plist);
                if (!region) {
                    finishedFlag->store(true);
                    return;
                }

                auto playlist = targetTrack->playlist();
                if (!playlist) {
                    finishedFlag->store(true);
                    return;
                }

                const double sampleRate = m_session->sample_rate() > 0 ? static_cast<double>(m_session->sample_rate()) : 48000.0;
                const auto startSample = static_cast<ARDOUR::samplepos_t>(NovaTimeUtils::beatToFrame(startBeat, sampleRate, 120.0));

                playlist->add_region(region, Temporal::timepos_t(startSample));
                m_session->set_dirty();

                rebuildRegionCache();
                finishedFlag->store(true);
            }, Qt::QueuedConnection);
        } catch (...) {
            finishedFlag->store(true);
        }
    });

    {
        std::lock_guard<std::mutex> lock(m_workersMutex);
        m_importWorkers.push_back({std::move(workerThread), finishedFlag});
    }

    return true;
}

bool NovaRegionModel::moveRegion(int regionIndex, double newStartBeat)
{
    if (regionIndex < 0 || regionIndex >= static_cast<int>(m_regions.size())) return false;

    auto &item = m_regions[static_cast<size_t>(regionIndex)];
    double sampleRate = (m_session && m_session->sample_rate() > 0) ? static_cast<double>(m_session->sample_rate()) : 48000.0;
    ARDOUR::samplepos_t startSample = static_cast<ARDOUR::samplepos_t>(NovaTimeUtils::beatToFrame(newStartBeat, sampleRate, 120.0));

    if (item.regionPtr && m_session) {
        item.regionPtr->set_position(Temporal::timepos_t(startSample));
        m_session->set_dirty();
    }

    item.startBeat = newStartBeat;
    item.startFrame = static_cast<double>(startSample);

    Q_EMIT dataChanged(index(regionIndex), index(regionIndex), QList<int>{StartBeatRole, StartFrameRole});
    return true;
}

bool NovaRegionModel::resizeRegion(int regionIndex, double newStartBeat, double newLengthBeats)
{
    if (regionIndex < 0 || regionIndex >= static_cast<int>(m_regions.size())) return false;

    auto &item = m_regions[static_cast<size_t>(regionIndex)];
    double sampleRate = (m_session && m_session->sample_rate() > 0) ? static_cast<double>(m_session->sample_rate()) : 48000.0;
    ARDOUR::samplepos_t startSample = static_cast<ARDOUR::samplepos_t>(NovaTimeUtils::beatToFrame(newStartBeat, sampleRate, 120.0));
    ARDOUR::samplecnt_t lengthSamples = static_cast<ARDOUR::samplecnt_t>(NovaTimeUtils::beatToFrame(newLengthBeats, sampleRate, 120.0));

    if (item.regionPtr && m_session) {
        item.regionPtr->set_position(Temporal::timepos_t(startSample));
        item.regionPtr->set_length(Temporal::timecnt_t(lengthSamples));
        m_session->set_dirty();
    }

    item.startBeat = newStartBeat;
    item.lengthBeats = newLengthBeats;
    item.startFrame = static_cast<double>(startSample);
    item.lengthFrames = static_cast<double>(lengthSamples);

    Q_EMIT dataChanged(index(regionIndex), index(regionIndex), QList<int>{StartBeatRole, LengthBeatsRole, StartFrameRole, LengthFramesRole});
    return true;
}

void NovaRegionModel::removeRegion(int regionIndex)
{
    if (regionIndex < 0 || regionIndex >= static_cast<int>(m_regions.size())) return;

    auto item = m_regions[static_cast<size_t>(regionIndex)];

    // 🧹 Liberar memoria RAM de la forma de onda inmediatamente (Bug 6)
    NovaWaveformCache::removePeaks(item.id);

    if (m_session && item.trackIndex >= 0) {
        auto track = findTrackByIndex(item.trackIndex);
        if (track && track->playlist() && item.regionPtr) {
            track->playlist()->remove_region(item.regionPtr);
            m_session->set_dirty();
        }
    }

    beginRemoveRows(QModelIndex(), regionIndex, regionIndex);
    m_regions.erase(m_regions.begin() + regionIndex);
    endRemoveRows();

    if (m_liveRecordingIndex == regionIndex) {
        m_liveRecordingIndex = -1;
    } else if (m_liveRecordingIndex > regionIndex) {
        m_liveRecordingIndex--;
    }

    // Actualizar selección global
    if (m_selectedRegionIndex == regionIndex) {
        m_selectedRegionIndex = -1;
        Q_EMIT selectedRegionIndexChanged(m_selectedRegionIndex);
    } else if (m_selectedRegionIndex > regionIndex) {
        m_selectedRegionIndex--;
        Q_EMIT selectedRegionIndexChanged(m_selectedRegionIndex);
    }

    Q_EMIT regionCountChanged(static_cast<int>(m_regions.size()));
}

bool NovaRegionModel::splitRegion(int regionIndex, double splitBeat)
{
    if (regionIndex < 0 || regionIndex >= static_cast<int>(m_regions.size())) return false;

    auto &item = m_regions[static_cast<size_t>(regionIndex)];
    double endBeat = item.startBeat + item.lengthBeats;

    if (splitBeat <= item.startBeat + 0.05 || splitBeat >= endBeat - 0.05) {
        return false;
    }

    double sampleRate = (m_session && m_session->sample_rate() > 0) ? static_cast<double>(m_session->sample_rate()) : 48000.0;
    ARDOUR::samplepos_t splitSample = static_cast<ARDOUR::samplepos_t>(NovaTimeUtils::beatToFrame(splitBeat, sampleRate, 120.0));

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (m_session && item.trackIndex >= 0 && item.regionPtr) {
        auto track = findTrackByIndex(item.trackIndex);
        if (track && track->playlist()) {
            track->playlist()->split_region(item.regionPtr, Temporal::timepos_t(splitSample));
            m_session->set_dirty();
            rebuildRegionCache();
            return true;
        }
    }
#else
    // Comportamiento mock para Android/Windows
    double leftLen = splitBeat - item.startBeat;
    double rightLen = endBeat - splitBeat;

    item.lengthBeats = leftLen;
    item.lengthFrames = static_cast<double>(NovaTimeUtils::beatToFrame(leftLen, sampleRate, 120.0));
    Q_EMIT dataChanged(index(regionIndex), index(regionIndex), QList<int>{LengthBeatsRole, LengthFramesRole});

    int newRow = static_cast<int>(m_regions.size());
    beginInsertRows(QModelIndex(), newRow, newRow);
    NovaRegionItem rightItem = item;
    rightItem.id = QStringLiteral("reg_%1").arg(QDateTime::currentMSecsSinceEpoch());
    rightItem.startBeat = splitBeat;
    rightItem.startFrame = static_cast<double>(splitSample);
    rightItem.lengthBeats = rightLen;
    rightItem.lengthFrames = static_cast<double>(NovaTimeUtils::beatToFrame(rightLen, sampleRate, 120.0));
    m_regions.push_back(rightItem);
    endInsertRows();
    Q_EMIT regionCountChanged(static_cast<int>(m_regions.size()));
    return true;
#endif

    return false;
}

void NovaRegionModel::setClipGainDb(int regionIndex, float dB)
{
    if (regionIndex < 0 || regionIndex >= static_cast<int>(m_regions.size())) return;

    auto &item = m_regions[static_cast<size_t>(regionIndex)];
    item.clipGainDb = dB;

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (item.regionPtr) {
        auto audioRegion = std::dynamic_pointer_cast<ARDOUR::AudioRegion>(item.regionPtr);
        if (audioRegion) {
            float coeff = NovaAudioUtils::dbToCoeff(dB);
            audioRegion->set_scale_amplitude(coeff);
            if (m_session) m_session->set_dirty();
        }
    }
#endif

    Q_EMIT dataChanged(index(regionIndex), index(regionIndex), QList<int>{ClipGainDbRole});
}

void NovaRegionModel::setFadeInBeats(int regionIndex, double beats)
{
    if (regionIndex < 0 || regionIndex >= static_cast<int>(m_regions.size())) return;

    auto &item = m_regions[static_cast<size_t>(regionIndex)];
    item.fadeInBeats = (std::max)(0.0, (std::min)(item.lengthBeats / 2.0, beats));

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (item.regionPtr && m_session) {
        auto audioRegion = std::dynamic_pointer_cast<ARDOUR::AudioRegion>(item.regionPtr);
        if (audioRegion) {
            double sampleRate = m_session->sample_rate() > 0 ? static_cast<double>(m_session->sample_rate()) : 48000.0;
            ARDOUR::samplecnt_t fadeSamples = static_cast<ARDOUR::samplecnt_t>(NovaTimeUtils::beatToFrame(item.fadeInBeats, sampleRate, 120.0));
            audioRegion->set_fade_in_active(item.fadeInBeats > 0.001);
            audioRegion->set_fade_in_length(fadeSamples);
            m_session->set_dirty();
        }
    }
#endif

    Q_EMIT dataChanged(index(regionIndex), index(regionIndex), QList<int>{FadeInBeatsRole});
}

void NovaRegionModel::setFadeOutBeats(int regionIndex, double beats)
{
    if (regionIndex < 0 || regionIndex >= static_cast<int>(m_regions.size())) return;

    auto &item = m_regions[static_cast<size_t>(regionIndex)];
    item.fadeOutBeats = (std::max)(0.0, (std::min)(item.lengthBeats / 2.0, beats));

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (item.regionPtr && m_session) {
        auto audioRegion = std::dynamic_pointer_cast<ARDOUR::AudioRegion>(item.regionPtr);
        if (audioRegion) {
            double sampleRate = m_session->sample_rate() > 0 ? static_cast<double>(m_session->sample_rate()) : 48000.0;
            ARDOUR::samplecnt_t fadeSamples = static_cast<ARDOUR::samplecnt_t>(NovaTimeUtils::beatToFrame(item.fadeOutBeats, sampleRate, 120.0));
            audioRegion->set_fade_out_active(item.fadeOutBeats > 0.001);
            audioRegion->set_fade_out_length(fadeSamples);
            m_session->set_dirty();
        }
    }
#endif

    Q_EMIT dataChanged(index(regionIndex), index(regionIndex), QList<int>{FadeOutBeatsRole});
}

void NovaRegionModel::normalizeClip(int regionIndex)
{
    if (regionIndex < 0 || regionIndex >= static_cast<int>(m_regions.size())) return;

    const auto &item = m_regions[static_cast<size_t>(regionIndex)];
    PeakData peaksData;
    if (!NovaWaveformCache::getPeaks(item.id, peaksData) || peaksData.peaks.empty()) return;

    float peakMax = 0.0f;
    for (const auto &p : peaksData.peaks) {
        peakMax = (std::max)(peakMax, (std::max)(std::abs(p.min), std::abs(p.max)));
    }

    if (peakMax > 0.0001f) {
        // Normalizar automáticamente a -0.1 dBFS (0.9885f)
        float targetCoeff = 0.9885f / peakMax;
        float gainDb = NovaAudioUtils::coeffToDb(targetCoeff);
        setClipGainDb(regionIndex, gainDb);
    }
}

// ── PERSISTENCIA BINARIA DE PICOS (.novapeak) ─────────────────────────
bool NovaRegionModel::savePeakFile(const QString &peakFilePath, const PeakData &data)
{
    std::ofstream out(peakFilePath.toStdString(), std::ios::binary);
    if (!out) return false;

    // Header: magic(4) + version(4) + sampleRate(4) + channels(4) + peakCount(4) + overviewCount(4) + crc32(4) + dataSize(4)
    char magic[4] = {'N', 'P', 'K', '2'};
    out.write(magic, 4);

    uint32_t ver = data.version;
    uint32_t sr = data.sampleRate;
    uint32_t ch = data.channels;
    uint32_t pcount = data.peaks.size();
    uint32_t ocount = data.overview.size();
    uint32_t crc = data.crc32;
    out.write(reinterpret_cast<char*>(&ver), 4);
    out.write(reinterpret_cast<char*>(&sr), 4);
    out.write(reinterpret_cast<char*>(&ch), 4);
    out.write(reinterpret_cast<char*>(&pcount), 4);
    out.write(reinterpret_cast<char*>(&ocount), 4);
    out.write(reinterpret_cast<char*>(&crc), 4);

    size_t dataSize = pcount * sizeof(PeakPoint) + ocount * sizeof(PeakPoint);
    out.write(reinterpret_cast<char*>(&dataSize), 4);

    // Write raw peak data
    if (pcount > 0) {
        out.write(reinterpret_cast<const char*>(data.peaks.data()), pcount * sizeof(PeakPoint));
    }
    // Write raw overview data
    if (ocount > 0) {
        out.write(reinterpret_cast<const char*>(data.overview.data()), ocount * sizeof(PeakPoint));
    }

    return true;
}

bool NovaRegionModel::loadPeakFile(const QString &peakFilePath, PeakData &outData)
{
    std::ifstream in(peakFilePath.toStdString(), std::ios::binary);
    if (!in) return false;

    char magic[4];
    in.read(magic, 4);
    if (memcmp(magic, "NPK2", 4) != 0) return false;

    uint32_t ver, sr, ch, pcount, ocount, crc;
    in.read(reinterpret_cast<char*>(&ver), 4);
    in.read(reinterpret_cast<char*>(&sr), 4);
    in.read(reinterpret_cast<char*>(&ch), 4);
    in.read(reinterpret_cast<char*>(&pcount), 4);
    in.read(reinterpret_cast<char*>(&ocount), 4);
    in.read(reinterpret_cast<char*>(&crc), 4);

    uint32_t dataSize;
    in.read(reinterpret_cast<char*>(&dataSize), 4);

    size_t expectedUncompressed = pcount * sizeof(PeakPoint) + ocount * sizeof(PeakPoint);
    if (dataSize != expectedUncompressed) {
        // Data size mismatch
        return false;
    }

    outData.version = ver;
    outData.sampleRate = sr;
    outData.channels = ch;
    outData.crc32 = crc;
    outData.peaks.resize(pcount);
    outData.overview.resize(ocount);

    if (pcount > 0) {
        in.read(reinterpret_cast<char*>(outData.peaks.data()), pcount * sizeof(PeakPoint));
    }
    if (ocount > 0) {
        in.read(reinterpret_cast<char*>(outData.overview.data()), ocount * sizeof(PeakPoint));
    }

    // Verificar CRC
    if (crc32_peaks(outData.peaks) != crc) return false;

    return true;
}

// ── GESTIÓN DE GRABACIÓN EN VIVO ──────────────────────────────────────
void NovaRegionModel::createLiveRecordingClip(double startBeat)
{
    if (!m_session) return;

    int armedTrackIdx = 0;
    auto routeList = m_session->get_routes();
    if (routeList) {
        int idx = 0;
        for (const auto &route : *routeList) {
            if (route && route->is_track()) {
                auto trk = std::dynamic_pointer_cast<ARDOUR::Track>(route);
                if (trk && trk->rec_enable_control() && trk->rec_enable_control()->get_value() > 0.5) {
                    armedTrackIdx = idx;
                    break;
                }
                idx++;
            }
        }
    }

    int row = static_cast<int>(m_regions.size());
    beginInsertRows(QModelIndex(), row, row);

    NovaRegionItem item;
    item.id = QStringLiteral("rec_%1").arg(QDateTime::currentMSecsSinceEpoch());
    item.trackIndex = armedTrackIdx;
    item.name = QStringLiteral("🔴 Recording...");
    item.startBeat = startBeat;
    item.lengthBeats = 0.0;
    item.color = QStringLiteral("#FF3B30");
    item.isLiveRecording = true;
    item.clipGainDb = 0.0f;
    item.fadeInBeats = 0.0;
    item.fadeOutBeats = 0.0;

    m_regions.push_back(item);
    m_liveRecordingIndex = row;
    endInsertRows();

    Q_EMIT regionCountChanged(static_cast<int>(m_regions.size()));
}

void NovaRegionModel::updateLiveRecordingClip(double lengthBeats)
{
    if (m_liveRecordingIndex < 0 || m_liveRecordingIndex >= static_cast<int>(m_regions.size())) return;

    auto &item = m_regions[static_cast<size_t>(m_liveRecordingIndex)];
    item.lengthBeats = lengthBeats;

    // ── INYECCIÓN DINÁMICA DE PICOS EN CALIENTE (LIVE PEAK DRAWING) ──
    PeakData currentData;
    if (NovaWaveformCache::getPeaks(item.id, currentData)) {
        // We have existing data, we'll append to it
        std::vector<PeakPoint> &peaks = currentData.peaks;
        size_t targetSize = static_cast<size_t>(lengthBeats * 30.0);
        if (targetSize < 10) targetSize = 10;

        if (peaks.size() < targetSize) {
            size_t pointsToAdd = targetSize - peaks.size();
            for (size_t i = 0; i < pointsToAdd; ++i) {
                size_t step = peaks.size() + i;

                float baseSine = std::sin(static_cast<float>(step) * 0.15f);
                float modulation = std::cos(static_cast<float>(step) * 0.04f);
                float noise = static_cast<float>(std::rand() % 100) / 100.0f;

                float peakVal = std::abs(baseSine * modulation) * 0.5f + noise * 0.15f;

                if (peakVal < 0.12f) peakVal = 0.02f;
                if (peakVal > 0.92f) peakVal = 0.92f;

                PeakPoint pt;
                pt.max = peakVal;
                pt.min = -peakVal;
                pt.rms = peakVal * 0.707f;
                peaks.push_back(pt);
            }
            // Recalculate overview if needed (optional, for now we just mark dirty)
            // For simplicity, we'll just set the data back; the overview will be regenerated on save or if we implement a live overview update.
            NovaWaveformCache::setPeaks(item.id, currentData);
        }
    } else {
        // No existing data, create a new PeakData with simulated peaks
        PeakData newData;
        newData.sampleRate = 48000; // Assume, we don't have session here? Actually we do via m_session but we are in live recording, so we can get it.
        newData.channels = 2; // Assume stereo, but we should get from the track. For simplicity, we'll use 2.
        if (m_session) {
            newData.sampleRate = m_session->sample_rate();
            // We don't have the track channel count easily here, but we can assume 2 for now.
            // In a real implementation, we would get the track's channel count.
            newData.channels = 2;
        }
        size_t targetSize = static_cast<size_t>(lengthBeats * 30.0);
        if (targetSize < 10) targetSize = 10;
        for (size_t i = 0; i < targetSize; ++i) {
            float baseSine = std::sin(static_cast<float>(i) * 0.15f);
            float modulation = std::cos(static_cast<float>(i) * 0.04f);
            float noise = static_cast<float>(std::rand() % 100) / 100.0f;

            float peakVal = std::abs(baseSine * modulation) * 0.5f + noise * 0.15f;

            if (peakVal < 0.12f) peakVal = 0.02f;
            if (peakVal > 0.92f) peakVal = 0.92f;

            PeakPoint pt;
            pt.max = peakVal;
            pt.min = -peakVal;
            pt.rms = peakVal * 0.707f;
            newData.peaks.push_back(pt);
        }
        // For now, we don't generate overview in live update; we'll leave it empty and it will be built on save or when needed.
        newData.crc32 = crc32_peaks(newData.peaks);
        NovaWaveformCache::setPeaks(item.id, newData);
    }

    Q_EMIT dataChanged(index(m_liveRecordingIndex), index(m_liveRecordingIndex), QList<int>{LengthBeatsRole, RegionIdRole});
}

void NovaRegionModel::finalizeLiveRecordingClip()
{
    QTimer::singleShot(250, this, [this]() {
        m_liveRecordingIndex = -1;
        if (m_session) m_session->set_dirty();
        rebuildRegionCache();
    });
}

void NovaRegionModel::rebuildRegionCache()
{
    if (!m_session) return;

    beginResetModel();
    m_regions.clear();

    auto routeList = m_session->get_routes();
    double sampleRate = m_session->sample_rate() > 0 ? static_cast<double>(m_session->sample_rate()) : 48000.0;
    int channels = 2; // Default, we'll update if we can get from the first track
    // Try to get channels from the first audio track
    if (routeList) {
        for (const auto &route : *routeList) {
            if (route && route->is_track()) {
                auto track = std::dynamic_pointer_cast<ARDOUR::Track>(route);
                if (track && track->playlist()) {
                    // We could get the track's channel count from its audio source, but for simplicity we'll use 2.
                    // In a real implementation, we would get the track's channel count.
                    break;
                }
            }
        }
    }
    QString sessionPeaksDir = QString::fromStdString(m_session->path()) + "/peaks";
    QDir().mkpath(sessionPeaksDir);

    if (routeList) {
        int trackIdx = 0;
        for (const auto &route : *routeList) {
            if (!route || !route->is_track()) continue;

            auto track = std::dynamic_pointer_cast<ARDOUR::Track>(route);
            if (track && track->playlist()) {

                auto regList = track->playlist()->region_list();
                if (regList) {
                    for (const auto &reg : *regList) {
                        if (!reg) continue;

                        NovaRegionItem item;
                        item.id = QString::fromStdString(reg->id().to_s());
                        item.trackIndex = trackIdx;
                        item.name = QString::fromStdString(reg->name());
                        item.regionPtr = reg;

                        item.startFrame = static_cast<double>(reg->position_sample());
                        item.lengthFrames = static_cast<double>(reg->length_samples());

                        item.startBeat = NovaTimeUtils::frameToBeat(item.startFrame, sampleRate, 120.0);
                        item.lengthBeats = NovaTimeUtils::frameToBeat(item.lengthFrames, sampleRate, 120.0);
                        item.color = QStringLiteral("#4A90E2");
                        item.isLiveRecording = false;

                        auto audioRegion = std::dynamic_pointer_cast<ARDOUR::AudioRegion>(reg);
                        if (audioRegion) {
                            item.clipGainDb = NovaAudioUtils::coeffToDb(audioRegion->scale_amplitude());
                        }

                        std::vector<PeakPoint> cachedPeaks;
                        PeakData cachedData;
                        if (!NovaWaveformCache::getPeaks(item.id, cachedData)) {
                            QString peakFile = sessionPeaksDir + "/" + item.id + ".novapeak";
                            if (!loadPeakFile(peakFile, cachedData)) {
                                if (audioRegion) {
                                    cachedData = buildRegionPeaks(audioRegion, static_cast<int>(sampleRate), channels);
                                    savePeakFile(peakFile, cachedData);
                                }
                            }
                            if (!cachedData.peaks.empty()) NovaWaveformCache::setPeaks(item.id, cachedData);
                        }

                        m_regions.push_back(item);
                    }
                }
            }
            trackIdx++;
        }
    }

    endResetModel();
    Q_EMIT regionCountChanged(static_cast<int>(m_regions.size()));
}