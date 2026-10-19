#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QtGlobal>
#include <vector>
#include <memory>
#include <mutex>
#include <map>
#include <thread>
#include <atomic>
#include "../core/NovaPeakData.h"

// Forward declarations - NO ARDOUR/PBD includes in public header
namespace ARDOUR {
    class Session;
    class Track;
    class Playlist;
    class Region;
    class AudioRegion;
    class Source;
    using SourceList = std::vector<std::shared_ptr<Source>>;
}
namespace PBD {
    class ScopedConnectionList;
}

struct NovaRegionItem {
    QString id;
    int trackIndex = 0;
    QString name;
    double startFrame = 0.0;
    double lengthFrames = 0.0;
    double startBeat = 0.0;
    double lengthBeats = 0.0;
    QString color = QStringLiteral("#4A90E2");
    bool isLiveRecording = false;

    // Propiedades de Ganancia y Fundidos (Fades)
    float clipGainDb = 0.0f;
    double fadeInBeats = 0.0;
    double fadeOutBeats = 0.0;

    std::shared_ptr<ARDOUR::Region> regionPtr = nullptr;
};

// PeakPoint and PeakData now defined in core/NovaPeakData.h

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

class NovaRegionModel : public QAbstractListModel
{
    Q_OBJECT

    // 🎯 Propiedad de Selección Global del Clip Activo en el Timeline
    Q_PROPERTY(int selectedRegionIndex READ selectedRegionIndex WRITE setSelectedRegionIndex NOTIFY selectedRegionIndexChanged)

public:
    enum RegionRoles : int {
        RegionIdRole = Qt::UserRole + 1,
        TrackIndexRole,
        RegionNameRole,
        StartFrameRole,
        LengthFramesRole,
        StartBeatRole,
        LengthBeatsRole,
        ColorRole,
        IsLiveRecordingRole,
        ClipGainDbRole,
        FadeInBeatsRole,
        FadeOutBeatsRole
    };
    Q_ENUM(RegionRoles)

    explicit NovaRegionModel(QObject *parent = nullptr);
    ~NovaRegionModel() override;

    void setSession(void *session);
    void waitForImports();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Selección de clips
    int selectedRegionIndex() const { return m_selectedRegionIndex; }
    void setSelectedRegionIndex(int index);

    Q_INVOKABLE bool importAudioFile(int trackIndex, const QString &filePath, double startBeat);
    Q_INVOKABLE bool moveRegion(int regionIndex, double newStartBeat);
    Q_INVOKABLE bool resizeRegion(int regionIndex, double newStartBeat, double newLengthBeats);
    Q_INVOKABLE void removeRegion(int regionIndex);

    // ✂️ Herramienta Split (Cortar Clips)
    Q_INVOKABLE bool splitRegion(int regionIndex, double splitBeat);

    // Métodos de Clip Gain, Fades y Normalización
    Q_INVOKABLE void setClipGainDb(int regionIndex, float dB);
    Q_INVOKABLE void setFadeInBeats(int regionIndex, double beats);
    Q_INVOKABLE void setFadeOutBeats(int regionIndex, double beats);
    Q_INVOKABLE void normalizeClip(int regionIndex); // ⚡ Auto-Gain 1-Toque

    // Control de Grabación en Vivo
    void createLiveRecordingClip(double startBeat);
    void updateLiveRecordingClip(double lengthBeats);
    void finalizeLiveRecordingClip();

    // Persistencia y Manejo de Peaks en Disco (.novapeak)
    static bool savePeakFile(const QString &peakFilePath, const PeakData &data);
    static bool loadPeakFile(const QString &peakFilePath, PeakData &outData);

signals:
    void regionCountChanged(int count);
    void selectedRegionIndexChanged(int index);

private:
    void rebuildRegionCache();
    std::shared_ptr<ARDOUR::Track> findTrackByIndex(int trackIndex) const; // 🧹 Clean Code (CC-2)
    void cleanupImportThreads(); // 🧹 Limpieza automática de hilos terminados (Bug 3)

    struct ImportWorker {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> finished;
    };

    // ⚠️ ARDOUR::Session* es seguro aquí: forward-declared, se castea en el .cpp
    ARDOUR::Session *m_session = nullptr;
    std::vector<NovaRegionItem> m_regions;
    std::unique_ptr<PBD::ScopedConnectionList> m_sessionConnections; // 🛡️ unique_ptr evita incluir pbd/signals.h aquí
    std::vector<ImportWorker> m_importWorkers;
    std::mutex m_workersMutex;

    int m_liveRecordingIndex = -1;
    int m_selectedRegionIndex = -1;
};