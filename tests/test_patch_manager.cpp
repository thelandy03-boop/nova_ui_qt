// Test del gestor de parches de colaboración local (.novapatch).
//
// Comprueba que un paquete de cambios pueda exportar pistas, clips y audios
// comprimidos, que se pueda inspeccionar su contenido sin descomprimir y
// que la extracción recupere los datos de audio intactos.

#include "core/NovaPatchManager.h"
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <cstdio>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK_NAMED(nombre, cond, msg)                                          \
    do {                                                                        \
        ++g_checks;                                                             \
        if (cond) {                                                             \
            std::printf("  [ok] %s\n", (nombre));                               \
        } else {                                                                \
            ++g_failures;                                                       \
            std::printf("  [FALLO] %s: %s  (%s:%d)\n", (nombre), (msg),         \
                        __FILE__, __LINE__);                                    \
        }                                                                       \
    } while (0)

int main()
{
    std::printf("== Tests de NovaPatchManager ==\n");

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        std::printf("Error: no se pudo crear directorio temporal para las pruebas.\n");
        return 1;
    }

    // 1. Crear un archivo de audio simulado
    QString fakeAudioPath = tempDir.path() + QStringLiteral("/take1.wav");
    QByteArray fakePcmData;
    fakePcmData.resize(1024 * 64); // 64 KB de datos simulados
    for (int i = 0; i < fakePcmData.size(); ++i) {
        fakePcmData[i] = static_cast<char>(i % 256);
    }
    {
        QFile f(fakeAudioPath);
        f.open(QIODevice::WriteOnly);
        f.write(fakePcmData);
        f.close();
    }

    // 2. Preparar metadatos y pistas
    NovaPatchTrackInfo track;
    track.name = QStringLiteral("Solo de Guitarra");
    track.color = QStringLiteral("#FF5500");
    track.gainDb = -2.5f;
    track.pan = 0.25f;

    NovaPatchClipInfo clip;
    clip.name = QStringLiteral("guitar_take_1");
    clip.startBeat = 16.0;
    clip.lengthBeats = 8.0;
    clip.clipGainDb = 0.0f;
    clip.fadeInBeats = 0.5;
    clip.fadeOutBeats = 1.0;
    clip.originalAudioPath = fakeAudioPath;
    track.clips.push_back(clip);

    std::vector<NovaPatchTrackInfo> tracks;
    tracks.push_back(track);

    QString patchPath = tempDir.path() + QStringLiteral("/colaboracion.novapatch");

    // 3. Probar exportación
    QString exportErr;
    bool exportOk = NovaPatchManager::exportPatch(patchPath,
                                                 QStringLiteral("MiCancion"),
                                                 QStringLiteral("Landy"),
                                                 QStringLiteral("Añadí solo compás 16"),
                                                 128.0,
                                                 tracks,
                                                 &exportErr);

    CHECK_NAMED("exporta_parche_correctamente", exportOk, exportErr.toUtf8().constData());
    CHECK_NAMED("archivo_parche_existe", QFile::exists(patchPath), "El archivo .novapatch no fue creado");

    // 4. Probar inspección sin extraer
    NovaPatchMetadata meta;
    qint64 totalAudioBytes = 0;
    QString inspectErr;
    bool inspectOk = NovaPatchManager::inspectPatch(patchPath, meta, totalAudioBytes, &inspectErr);

    CHECK_NAMED("inspecciona_parche_con_exito", inspectOk, inspectErr.toUtf8().constData());
    CHECK_NAMED("autor_coincide", meta.author == QStringLiteral("Landy"), "El autor no coincide");
    CHECK_NAMED("notas_coinciden", meta.notes == QStringLiteral("Añadí solo compás 16"), "Las notas no coinciden");
    CHECK_NAMED("bpm_coincide", meta.bpm == 128.0, "El BPM no coincide");
    CHECK_NAMED("cantidad_pistas_coincide", meta.tracks.size() == 1, "Número incorrecto de pistas");
    if (!meta.tracks.empty()) {
        CHECK_NAMED("nombre_pista_coincide", meta.tracks[0].name == QStringLiteral("Solo de Guitarra"), "Nombre de pista incorrecto");
        CHECK_NAMED("cantidad_clips_coincide", meta.tracks[0].clips.size() == 1, "Número incorrecto de clips");
        if (!meta.tracks[0].clips.empty()) {
            CHECK_NAMED("start_beat_coincide", meta.tracks[0].clips[0].startBeat == 16.0, "StartBeat del clip incorrecto");
            CHECK_NAMED("duracion_beats_coincide", meta.tracks[0].clips[0].lengthBeats == 8.0, "LengthBeats incorrecto");
        }
    }
    CHECK_NAMED("tamano_audio_coincide", totalAudioBytes == fakePcmData.size(), "Tamaño de audio informado incorrecto");

    // 5. Probar extracción de audio
    QString extractDir = tempDir.path() + QStringLiteral("/extracted_audio");
    QMap<QString, QString> extractedMap;
    QString extractErr;
    bool extractOk = NovaPatchManager::extractAudioFiles(patchPath, extractDir, extractedMap, &extractErr);

    CHECK_NAMED("extrae_audio_con_exito", extractOk, extractErr.toUtf8().constData());
    CHECK_NAMED("extrae_exactamente_un_archivo", extractedMap.size() == 1, "Cantidad de archivos extraídos incorrecta");

    if (!extractedMap.isEmpty()) {
        QString extractedPath = extractedMap.first();
        CHECK_NAMED("archivo_extraido_existe", QFile::exists(extractedPath), "El archivo extraído no existe en disco");

        QFile extractedF(extractedPath);
        extractedF.open(QIODevice::ReadOnly);
        QByteArray extractedBytes = extractedF.readAll();
        extractedF.close();

        CHECK_NAMED("audio_extraido_identico_al_original", extractedBytes == fakePcmData, "El contenido del audio extraído difiere del original");
    }

    // 6. Probar rechazo de archivo corrupto
    QString corruptPath = tempDir.path() + QStringLiteral("/corrupt.novapatch");
    {
        QFile fc(corruptPath);
        fc.open(QIODevice::WriteOnly);
        fc.write("ESTO NO ES UN PARCHE");
        fc.close();
    }
    NovaPatchMetadata badMeta;
    qint64 badBytes = 0;
    QString badErr;
    bool corruptOk = NovaPatchManager::inspectPatch(corruptPath, badMeta, badBytes, &badErr);
    CHECK_NAMED("rechaza_archivo_corrupto", !corruptOk, "No rechazó archivo inválido");

    std::printf("\nResultado: %d comprobaciones, %d fallos.\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
