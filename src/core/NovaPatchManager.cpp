#include "NovaPatchManager.h"
#include "NovaLogging.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

// Magic bytes identificadores de un archivo .novapatch: "NPTC" (Nova PaTCh)
static const quint32 kNovaPatchMagic = 0x4E505443;
static const quint32 kNovaPatchCurrentVersion = 1;

bool NovaPatchManager::exportPatch(const QString &targetFilePath,
                                   const QString &projectName,
                                   const QString &author,
                                   const QString &notes,
                                   double bpm,
                                   const std::vector<NovaPatchTrackInfo> &tracks,
                                   QString *outError)
{
    if (targetFilePath.trimmed().isEmpty()) {
        if (outError) *outError = QStringLiteral("Ruta de archivo destino vacía.");
        return false;
    }

    // 1. Construir el objeto JSON de metadatos
    QJsonObject rootObj;
    rootObj[QStringLiteral("magic")] = QStringLiteral("NOVAPATCH");
    rootObj[QStringLiteral("version")] = static_cast<int>(kNovaPatchCurrentVersion);
    rootObj[QStringLiteral("projectName")] = projectName.isEmpty() ? QStringLiteral("Untitled") : projectName;
    rootObj[QStringLiteral("author")] = author.isEmpty() ? QStringLiteral("Colaborador") : author;
    rootObj[QStringLiteral("notes")] = notes;
    rootObj[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    rootObj[QStringLiteral("bpm")] = bpm > 0.0 ? bpm : 120.0;

    QJsonArray tracksArray;
    QMap<QString, QString> audioKeyToSourcePath;
    int audioCounter = 0;

    for (const auto &trk : tracks) {
        QJsonObject trackObj;
        trackObj[QStringLiteral("name")] = trk.name;
        trackObj[QStringLiteral("color")] = trk.color;
        trackObj[QStringLiteral("gainDb")] = trk.gainDb;
        trackObj[QStringLiteral("pan")] = trk.pan;

        QJsonArray clipsArray;
        for (const auto &clip : trk.clips) {
            QJsonObject clipObj;
            clipObj[QStringLiteral("name")] = clip.name;
            clipObj[QStringLiteral("startBeat")] = clip.startBeat;
            clipObj[QStringLiteral("lengthBeats")] = clip.lengthBeats;
            clipObj[QStringLiteral("clipGainDb")] = clip.clipGainDb;
            clipObj[QStringLiteral("fadeInBeats")] = clip.fadeInBeats;
            clipObj[QStringLiteral("fadeOutBeats")] = clip.fadeOutBeats;

            QString key = clip.audioKey;
            if (key.isEmpty()) {
                QString ext = QFileInfo(clip.originalAudioPath).suffix();
                if (ext.isEmpty()) ext = QStringLiteral("wav");
                key = QStringLiteral("audio_%1.%2").arg(audioCounter++).arg(ext);
            }
            clipObj[QStringLiteral("audioKey")] = key;
            clipsArray.append(clipObj);

            if (!clip.originalAudioPath.isEmpty() && QFile::exists(clip.originalAudioPath)) {
                audioKeyToSourcePath[key] = clip.originalAudioPath;
            }
        }
        trackObj[QStringLiteral("clips")] = clipsArray;
        tracksArray.append(trackObj);
    }
    rootObj[QStringLiteral("tracks")] = tracksArray;

    QJsonDocument doc(rootObj);
    QByteArray jsonBytes = doc.toJson(QJsonDocument::Compact);

    // 2. Escribir archivo contenedor binario
    QFile outFile(targetFilePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (outError) *outError = QStringLiteral("No se pudo abrir el archivo para escritura: %1").arg(outFile.errorString());
        return false;
    }

    QDataStream stream(&outFile);
    stream.setVersion(QDataStream::Qt_6_5);

    // Cabecera: Magic + Versión + Metadatos JSON
    stream << kNovaPatchMagic;
    stream << kNovaPatchCurrentVersion;
    stream << jsonBytes;

    // Bloque de audio empaquetado y comprimido
    quint32 fileCount = static_cast<quint32>(audioKeyToSourcePath.size());
    stream << fileCount;

    for (auto it = audioKeyToSourcePath.cbegin(); it != audioKeyToSourcePath.cend(); ++it) {
        const QString &key = it.key();
        const QString &sourcePath = it.value();

        QFile audioFile(sourcePath);
        if (!audioFile.open(QIODevice::ReadOnly)) {
            qCWarning(novaCore) << "⚠️ NovaPatch: no se pudo leer archivo de audio:" << sourcePath;
            // Escribir entrada vacía para mantener la consistencia
            stream << key;
            stream << QFileInfo(sourcePath).fileName();
            stream << static_cast<qint64>(0);
            stream << QByteArray();
            continue;
        }

        QByteArray rawAudio = audioFile.readAll();
        audioFile.close();

        qint64 uncompressedSize = rawAudio.size();
        QByteArray compressedAudio = qCompress(rawAudio, 6);

        stream << key;
        stream << QFileInfo(sourcePath).fileName();
        stream << uncompressedSize;
        stream << compressedAudio;

        qCDebug(novaCore) << "📦 NovaPatch: empaquetado" << key << "(" << uncompressedSize / 1024
                          << "KB comprimido a" << compressedAudio.size() / 1024 << "KB)";
    }

    outFile.close();
    qCDebug(novaCore) << "✅ NovaPatch: exportado exitosamente en:" << targetFilePath;
    return true;
}

bool NovaPatchManager::inspectPatch(const QString &patchFilePath,
                                    NovaPatchMetadata &outMetadata,
                                    qint64 &outTotalAudioBytes,
                                    QString *outError)
{
    outTotalAudioBytes = 0;
    outMetadata = NovaPatchMetadata{};

    QFile file(patchFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) *outError = QStringLiteral("No se pudo abrir el archivo: %1").arg(file.errorString());
        return false;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_5);

    quint32 magic = 0;
    quint32 version = 0;
    stream >> magic;
    stream >> version;

    if (magic != kNovaPatchMagic) {
        if (outError) *outError = QStringLiteral("El archivo no es un parche válido de NOVA Studio (.novapatch).");
        return false;
    }

    if (version > kNovaPatchCurrentVersion) {
        if (outError) *outError = QStringLiteral("Versión del parche no compatible (creado con una versión más reciente).");
        return false;
    }

    QByteArray jsonBytes;
    stream >> jsonBytes;

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        if (outError) *outError = QStringLiteral("Metadatos del parche corruptos: %1").arg(parseErr.errorString());
        return false;
    }

    QJsonObject rootObj = doc.object();
    outMetadata.version = rootObj.value(QStringLiteral("version")).toInt(1);
    outMetadata.projectName = rootObj.value(QStringLiteral("projectName")).toString();
    outMetadata.author = rootObj.value(QStringLiteral("author")).toString();
    outMetadata.notes = rootObj.value(QStringLiteral("notes")).toString();
    outMetadata.timestamp = rootObj.value(QStringLiteral("timestamp")).toString();
    outMetadata.bpm = rootObj.value(QStringLiteral("bpm")).toDouble(120.0);

    QJsonArray tracksArray = rootObj.value(QStringLiteral("tracks")).toArray();
    for (const auto &tVal : tracksArray) {
        QJsonObject tObj = tVal.toObject();
        NovaPatchTrackInfo trk;
        trk.name = tObj.value(QStringLiteral("name")).toString();
        trk.color = tObj.value(QStringLiteral("color")).toString(QStringLiteral("#4A90E2"));
        trk.gainDb = static_cast<float>(tObj.value(QStringLiteral("gainDb")).toDouble(0.0));
        trk.pan = static_cast<float>(tObj.value(QStringLiteral("pan")).toDouble(0.0));

        QJsonArray clipsArray = tObj.value(QStringLiteral("clips")).toArray();
        for (const auto &cVal : clipsArray) {
            QJsonObject cObj = cVal.toObject();
            NovaPatchClipInfo clip;
            clip.name = cObj.value(QStringLiteral("name")).toString();
            clip.startBeat = cObj.value(QStringLiteral("startBeat")).toDouble(0.0);
            clip.lengthBeats = cObj.value(QStringLiteral("lengthBeats")).toDouble(0.0);
            clip.clipGainDb = static_cast<float>(cObj.value(QStringLiteral("clipGainDb")).toDouble(0.0));
            clip.fadeInBeats = cObj.value(QStringLiteral("fadeInBeats")).toDouble(0.0);
            clip.fadeOutBeats = cObj.value(QStringLiteral("fadeOutBeats")).toDouble(0.0);
            clip.audioKey = cObj.value(QStringLiteral("audioKey")).toString();
            trk.clips.push_back(clip);
        }
        outMetadata.tracks.push_back(trk);
    }

    // Calcular tamaño total de audio sin descomprimir todo
    quint32 fileCount = 0;
    stream >> fileCount;

    for (quint32 i = 0; i < fileCount; ++i) {
        QString key;
        QString fileName;
        qint64 uncompressedSize = 0;
        QByteArray compressedAudio;

        stream >> key;
        stream >> fileName;
        stream >> uncompressedSize;
        stream >> compressedAudio;

        outTotalAudioBytes += uncompressedSize;
    }

    file.close();
    return true;
}

bool NovaPatchManager::extractAudioFiles(const QString &patchFilePath,
                                         const QString &destinationDir,
                                         QMap<QString, QString> &outExtractedFiles,
                                         QString *outError)
{
    outExtractedFiles.clear();

    QFile file(patchFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) *outError = QStringLiteral("No se pudo abrir el parche: %1").arg(file.errorString());
        return false;
    }

    QDir().mkpath(destinationDir);

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_5);

    quint32 magic = 0;
    quint32 version = 0;
    stream >> magic;
    stream >> version;

    if (magic != kNovaPatchMagic) {
        if (outError) *outError = QStringLiteral("Formato no válido de parche.");
        return false;
    }

    QByteArray jsonBytes;
    stream >> jsonBytes; // Saltar metadatos

    quint32 fileCount = 0;
    stream >> fileCount;

    for (quint32 i = 0; i < fileCount; ++i) {
        QString key;
        QString originalFileName;
        qint64 uncompressedSize = 0;
        QByteArray compressedAudio;

        stream >> key;
        stream >> originalFileName;
        stream >> uncompressedSize;
        stream >> compressedAudio;

        if (compressedAudio.isEmpty()) continue;

        QByteArray rawAudio = qUncompress(compressedAudio);
        if (rawAudio.isEmpty() && uncompressedSize > 0) {
            qCWarning(novaCore) << "⚠️ NovaPatch: fallo al descomprimir audio:" << key;
            continue;
        }

        // Generar nombre de destino seguro
        QString destFileName = originalFileName.isEmpty() ? key : originalFileName;
        QString destPath = destinationDir + QStringLiteral("/") + destFileName;

        // Si ya existe un archivo con ese nombre, agregar sufijo numérico
        if (QFile::exists(destPath)) {
            QFileInfo fi(destPath);
            destPath = destinationDir + QStringLiteral("/") + fi.completeBaseName()
                       + QStringLiteral("_collab_") + QString::number(QDateTime::currentMSecsSinceEpoch())
                       + QStringLiteral(".") + fi.suffix();
        }

        QFile extractedFile(destPath);
        if (extractedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            extractedFile.write(rawAudio);
            extractedFile.close();
            outExtractedFiles[key] = destPath;
            qCDebug(novaCore) << "📥 NovaPatch: audio extraído a:" << destPath;
        } else {
            qCCritical(novaCore) << "❌ NovaPatch: no se pudo escribir archivo extraído:" << destPath;
        }
    }

    file.close();
    return true;
}

QVariantMap NovaPatchManager::metadataToVariant(const NovaPatchMetadata &meta, qint64 totalAudioBytes)
{
    QVariantMap map;
    map[QStringLiteral("valid")] = true;
    map[QStringLiteral("version")] = meta.version;
    map[QStringLiteral("projectName")] = meta.projectName;
    map[QStringLiteral("author")] = meta.author;
    map[QStringLiteral("notes")] = meta.notes;
    map[QStringLiteral("timestamp")] = meta.timestamp;
    map[QStringLiteral("bpm")] = meta.bpm;
    map[QStringLiteral("totalAudioBytes")] = totalAudioBytes;
    map[QStringLiteral("trackCount")] = static_cast<int>(meta.tracks.size());

    int totalClips = 0;
    QVariantList tracksList;
    for (const auto &trk : meta.tracks) {
        QVariantMap tMap;
        tMap[QStringLiteral("name")] = trk.name;
        tMap[QStringLiteral("color")] = trk.color;
        tMap[QStringLiteral("gainDb")] = trk.gainDb;
        tMap[QStringLiteral("pan")] = trk.pan;
        tMap[QStringLiteral("clipCount")] = static_cast<int>(trk.clips.size());
        totalClips += static_cast<int>(trk.clips.size());

        QVariantList clipsList;
        for (const auto &c : trk.clips) {
            QVariantMap cMap;
            cMap[QStringLiteral("name")] = c.name;
            cMap[QStringLiteral("startBeat")] = c.startBeat;
            cMap[QStringLiteral("lengthBeats")] = c.lengthBeats;
            cMap[QStringLiteral("clipGainDb")] = c.clipGainDb;
            clipsList.append(cMap);
        }
        tMap[QStringLiteral("clips")] = clipsList;
        tracksList.append(tMap);
    }

    map[QStringLiteral("clipCount")] = totalClips;
    map[QStringLiteral("tracks")] = tracksList;
    return map;
}
