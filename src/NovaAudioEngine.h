#ifndef NOVAAUDIOENGINE_H
#define NOVAAUDIOENGINE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include "core/NovaSessionManager.h"
#include "core/NovaTransportController.h"
#include "core/NovaRecordManager.h"
#include "core/NovaAudioDeviceManager.h"
#include "core/NovaTimeUtils.h"
#include "models/NovaTrackListModel.h"
#include "models/NovaRegionModel.h"

#if defined(Q_OS_ANDROID)
#include "core/platform/NovaAndroidAudioEngine.h"
#include "audio/NovaSpikeLoader.h"
#endif

class NovaAudioEngine : public QObject
{
    Q_OBJECT

    // ── PROPIEDADES EXPUESTAS A QML ──
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
    Q_PROPERTY(QString timecode READ timecode NOTIFY timecodeChanged)
    Q_PROPERTY(QString bbt READ bbt NOTIFY bbtChanged)
    Q_PROPERTY(double currentFrame READ currentFrame NOTIFY positionChanged)
    Q_PROPERTY(double currentBeat READ currentBeat NOTIFY positionChanged)
    Q_PROPERTY(double bpm READ bpm WRITE setBpm NOTIFY bpmChanged)
    
    Q_PROPERTY(NovaTrackListModel* tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(NovaRegionModel* regions READ regions NOTIFY regionsChanged)
    Q_PROPERTY(NovaAudioDeviceManager* deviceManager READ deviceManager CONSTANT)

    Q_PROPERTY(bool loopEnabled READ loopEnabled WRITE setLoopEnabled NOTIFY loopEnabledChanged)
    Q_PROPERTY(double loopStartBeat READ loopStartBeat NOTIFY loopRangeChanged)
    Q_PROPERTY(double loopEndBeat READ loopEndBeat NOTIFY loopRangeChanged)

    // VU METER MASTER GLOBAL (FL STUDIO STYLE)
    Q_PROPERTY(float masterPeakLeft READ masterPeakLeft NOTIFY masterPeaksChanged)
    Q_PROPERTY(float masterPeakRight READ masterPeakRight NOTIFY masterPeaksChanged)
    Q_PROPERTY(float masterVolumeDb READ masterVolumeDb WRITE setMasterVolumeDb NOTIFY masterVolumeDbChanged)

    // 💾 GESTIÓN DE PROYECTOS Y SESIONES
    Q_PROPERTY(bool isDirty READ isDirty NOTIFY isDirtyChanged)
    Q_PROPERTY(QVariantList recentProjects READ recentProjects NOTIFY recentProjectsChanged)
    Q_PROPERTY(QString currentProjectName READ currentProjectName NOTIFY currentProjectChanged)
    Q_PROPERTY(QString currentProjectPath READ currentProjectPath NOTIFY currentProjectChanged)

public:
    explicit NovaAudioEngine(QObject *parent = nullptr);
    ~NovaAudioEngine() override;

    Q_INVOKABLE bool initEngine();

#if defined(Q_OS_ANDROID)
    // Fase 2b: carga (decodifica) y reproduce un fichero en Android.
    Q_INVOKABLE bool loadAndPlayAndroid(const QString &path);
#endif

    // Controles de Transporte
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void toggleRecord();
    Q_INVOKABLE void locateFrame(double frame);
    Q_INVOKABLE void locateBeat(double beat);
    Q_INVOKABLE void setBpm(double bpm);
    Q_INVOKABLE void rewind();

    // Bucle (Loop)
    Q_INVOKABLE void setLoopEnabled(bool enabled);
    Q_INVOKABLE void setLoopRange(double startBeat, double endBeat);
    Q_INVOKABLE void toggleLoop();

    // ✂️ Herramienta Split (Cortar Clip en el Cabezal / Playhead)
    Q_INVOKABLE bool splitAtPlayhead(int targetRegionIndex = -1);

    // Helpers de Tiempo y Píxeles
    Q_INVOKABLE double beatToPixel(double beat, double barWidth = 80.0) const { return NovaTimeUtils::beatToPixel(beat, barWidth); }
    Q_INVOKABLE double pixelToBeat(double pixelX, double barWidth = 80.0) const { return NovaTimeUtils::pixelToBeat(pixelX, barWidth); }

    // Métodos CRUD de Proyectos
    Q_INVOKABLE bool saveProject();
    Q_INVOKABLE bool openProject(const QString &path);
    Q_INVOKABLE bool newProject(const QString &name, const QString &parentDir);
    Q_INVOKABLE void closeProject();
    Q_INVOKABLE void discardProject(); // 🗑️ Descartar cambios y eliminar si nunca fue guardado
    Q_INVOKABLE void quitApplication(); // 🛑 Apagado completo y salida de la app
    Q_INVOKABLE void removeRecentProject(const QString &path);

    // Getters de Estado
    bool isPlaying() const {
#if defined(Q_OS_ANDROID)
        return m_androidEngine.isPlaying() || m_transport.isPlaying();
#else
        return m_transport.isPlaying();
#endif
    }
    bool isRecording() const { return m_recorder.isRecording(); }
    QString timecode() const {
#if defined(Q_OS_ANDROID)
        if (m_androidEngine.tieneFicheroCargado()) {
            double secs = m_androidEngine.currentTimeSeconds();
            int total = static_cast<int>(secs);
            int mins = total / 60;
            int secsR = total % 60;
            int ms = static_cast<int>((secs - total) * 100.0);
            return QString("%1:%2:%3").arg(mins,2,10,QChar('0')).arg(secsR,2,10,QChar('0')).arg(ms,2,10,QChar('0'));
        }
#endif
        return m_transport.timecode();
    }
    QString bbt() const { return m_transport.bbt(); }
    double currentFrame() const {
#if defined(Q_OS_ANDROID)
        if (m_androidEngine.tieneFicheroCargado()) {
            return static_cast<double>(m_androidEngine.currentFrame());
        }
#endif
        return m_transport.currentFrame();
    }
    double currentBeat() const {
#if defined(Q_OS_ANDROID)
        if (m_androidEngine.tieneFicheroCargado()) {
            double sr = m_androidEngine.tasaSalidaReal() > 0 ? m_androidEngine.tasaSalidaReal() : 48000.0;
            double secs = static_cast<double>(m_androidEngine.currentFrame()) / sr;
            double bpm = bpm() > 0 ? bpm() : 120.0;
            return secs * (bpm / 60.0);
        }
#endif
        return m_transport.currentBeat();
    }
    double bpm() const { return m_transport.bpm(); }
    
    NovaTrackListModel* tracks() const { return m_trackModel; }
    NovaRegionModel* regions() const { return m_regionModel; }
    NovaAudioDeviceManager* deviceManager() { return &m_deviceManager; }

    bool loopEnabled() const { return m_transport.loopEnabled(); }
    double loopStartBeat() const { return m_transport.loopStartBeat(); }
    double loopEndBeat() const { return m_transport.loopEndBeat(); }

    float masterPeakLeft() const;
    float masterPeakRight() const;
    float masterVolumeDb() const;
    Q_INVOKABLE void setMasterVolumeDb(float dB);

    // Getters de Sesión / Proyectos
    bool isDirty() const;
    QVariantList recentProjects() const;
    QString currentProjectName() const;
    QString currentProjectPath() const;

signals:
    void isPlayingChanged();
    void isRecordingChanged();
    void timecodeChanged();
    void bbtChanged();
    void positionChanged();
    void bpmChanged();
    void tracksChanged();
    void regionsChanged();
    void loopEnabledChanged();
    void loopRangeChanged();
    void masterPeaksChanged();
    void masterVolumeDbChanged();

    // Señales de guardado y carga
    void isDirtyChanged();
    void recentProjectsChanged();
    void currentProjectChanged();

#if defined(Q_OS_ANDROID)
    // Puente para NovaRegionModel::importAudioFile en Android.
    void androidImportFinished(bool success, const QString &message);
    void androidImportingChanged(bool importing);
#endif

private:
    NovaSessionManager m_sessionManager;
    NovaTransportController m_transport;
    NovaRecordManager m_recorder;
    NovaAudioDeviceManager m_deviceManager;

    NovaTrackListModel *m_trackModel = nullptr;
    NovaRegionModel *m_regionModel = nullptr;

#if defined(Q_OS_ANDROID)
    // Fase 2b: motor de audio Oboe + cargador (decodifica en hilo aparte).
    // El controlador es el dueño: nadie más los instancia en Android.
    NovaAndroidAudioEngine m_androidEngine;
    NovaSpikeLoader m_androidLoader;
    bool m_androidMotorListo = false;
    bool m_androidParadaPedida = false;
    double m_androidDuracion = 0.0;
    int m_androidVersionPicos = 0;
#endif

    void connectSessionSignals();
    bool newProjectInternal(const QString &name, const QString &parentDir, bool isScratch);

#if defined(Q_OS_ANDROID)
    // Fase 2b: callbacks del cargador (decodifica en hilo aparte).
    void androidCargaTerminada();
    void androidCargaFallida(const QString &error);
    bool androidPrepararMotor();
#endif
};

#endif // NOVAAUDIOENGINE_H