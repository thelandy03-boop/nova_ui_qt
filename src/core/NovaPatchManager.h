#pragma once

#include <QString>
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QVariantMap>
#include <vector>

// Forward declarations para no contaminar el header público con tipos de Ardour
namespace ARDOUR {
    class Session;
}
class NovaTrackListModel;
class NovaRegionModel;

struct NovaPatchClipInfo {
    QString name;
    double startBeat = 0.0;
    double lengthBeats = 0.0;
    float clipGainDb = 0.0f;
    double fadeInBeats = 0.0;
    double fadeOutBeats = 0.0;
    QString audioKey;          // Identificador interno en el paquete (ej: audio_0.wav)
    QString originalAudioPath; // Ruta física en disco antes de empaquetar
};

struct NovaPatchPluginInfo {
    QString id;
    QString name;
    QString category;
    QString script;             // Fuente JSFX (necesaria para plugins de usuario)
    bool bypassed = false;
    std::vector<float> sliders; // Valores de slider1..N en orden
    QByteArray state;           // Blob de @serialize (float32 little-endian)
};

struct NovaPatchTrackInfo {
    QString name;
    QString color = QStringLiteral("#4A90E2");
    float gainDb = 0.0f;
    float pan = 0.0f;
    std::vector<NovaPatchClipInfo> clips;
    std::vector<NovaPatchPluginInfo> plugins;
};

struct NovaPatchMetadata {
    int version = 1;
    QString projectName;
    QString author;
    QString notes;
    QString timestamp;
    double bpm = 120.0;
    std::vector<NovaPatchTrackInfo> tracks;
};

class NovaPatchManager {
public:
    // 📦 Exporta un archivo de parche (.novapatch) conteniendo pistas, clips y audios empaquetados
    static bool exportPatch(const QString &targetFilePath,
                            const QString &projectName,
                            const QString &author,
                            const QString &notes,
                            double bpm,
                            const std::vector<NovaPatchTrackInfo> &tracks,
                            QString *outError = nullptr);

    // 🔍 Inspecciona un archivo .novapatch y extrae sus metadatos sin descomprimir el audio
    static bool inspectPatch(const QString &patchFilePath,
                             NovaPatchMetadata &outMetadata,
                             qint64 &outTotalAudioBytes,
                             QString *outError = nullptr);

    // 📥 Extrae los archivos de audio de un .novapatch hacia un directorio de destino
    // Devuelve un mapa asociando audioKey -> ruta física extraída en disco
    static bool extractAudioFiles(const QString &patchFilePath,
                                  const QString &destinationDir,
                                  QMap<QString, QString> &outExtractedFiles,
                                  QString *outError = nullptr);

    // Helper para convertir NovaPatchMetadata a QVariantMap amigable con QML
    static QVariantMap metadataToVariant(const NovaPatchMetadata &meta, qint64 totalAudioBytes);
};
