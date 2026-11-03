#include "NovaAudioEngine.h"
#include "core/NovaLogging.h"
#include <QCoreApplication>
#if defined(Q_OS_ANDROID)
#include "core/platform/NovaAndroidStubs.h"
#include <QPermissions>
#endif
#include <QTimer>
#include "core/NovaAudioDeviceManager.h"
#include "core/NovaHardwareSanitizer.h"
#include "core/NovaProjectManager.h"
#include "core/NovaProjectPolicy.h"
#include "core/NovaPatchManager.h"
#include "core/platform/NovaPlatformUtils.h"
#include "models/NovaTrackListModel.h"
#include "models/NovaRegionModel.h"
#include "views/NovaWaveformItem.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <cmath>
#include <algorithm>

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
#include "ardour/session_directory.h"
#include "ardour/route.h"
#include "ardour/meter.h"
#include "ardour/gain_control.h"

#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#pragma pop_macro("foreach")
#endif

struct _VSTState;
int vstfx_init(void*) { return 0; }
void vstfx_exit() {}
void vstfx_destroy_editor(_VSTState*) {}

#if defined(Q_OS_ANDROID)
#include <jni.h>
#include <QJniObject>

extern "C" {

JNIEXPORT void JNICALL
Java_com_novastudio_nova_AudioForegroundService_nativePauseAudio(JNIEnv*, jclass)
{
    if (NovaAudioEngine::instance()) {
        QMetaObject::invokeMethod(NovaAudioEngine::instance(), &NovaAudioEngine::pause, Qt::QueuedConnection);
    }
}

JNIEXPORT void JNICALL
Java_com_novastudio_nova_AudioForegroundService_nativePlayAudio(JNIEnv*, jclass)
{
    if (NovaAudioEngine::instance()) {
        QMetaObject::invokeMethod(NovaAudioEngine::instance(), &NovaAudioEngine::play, Qt::QueuedConnection);
    }
}

JNIEXPORT void JNICALL
Java_com_novastudio_nova_AudioForegroundService_nativeStopAudio(JNIEnv*, jclass)
{
    if (NovaAudioEngine::instance()) {
        QMetaObject::invokeMethod(NovaAudioEngine::instance(), &NovaAudioEngine::stop, Qt::QueuedConnection);
    }
}

JNIEXPORT jboolean JNICALL
Java_com_novastudio_nova_AudioForegroundService_nativeIsPlaying(JNIEnv*, jclass)
{
    return (NovaAudioEngine::instance() && NovaAudioEngine::instance()->isPlaying()) ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
#endif

static NovaAudioEngine* s_instance = nullptr;

NovaAudioEngine* NovaAudioEngine::instance()
{
    return s_instance;
}

void NovaAudioEngine::hapticFeedback()
{
    NovaPlatformUtils::hapticFeedback();
}

NovaAudioEngine::NovaAudioEngine(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    m_trackModel = new NovaTrackListModel(this);
    m_regionModel = new NovaRegionModel(this);

    // Conexión del transporte a la UI (60 FPS tick)
    connect(&m_transport, &NovaTransportController::isPlayingChanged, this, &NovaAudioEngine::isPlayingChanged);
    connect(&m_transport, &NovaTransportController::timecodeChanged, this, &NovaAudioEngine::timecodeChanged);
    connect(&m_transport, &NovaTransportController::bbtChanged, this, &NovaAudioEngine::bbtChanged);
    connect(&m_transport, &NovaTransportController::positionChanged, this, &NovaAudioEngine::positionChanged);
    connect(&m_transport, &NovaTransportController::positionChanged, this, &NovaAudioEngine::masterPeaksChanged); 
    connect(&m_transport, &NovaTransportController::bpmChanged, this, &NovaAudioEngine::bpmChanged);
    connect(&m_transport, &NovaTransportController::loopEnabledChanged, this, &NovaAudioEngine::loopEnabledChanged);
    connect(&m_transport, &NovaTransportController::loopRangeChanged, this, &NovaAudioEngine::loopRangeChanged);

    // Conectar el grabador de pistas
    connect(&m_recorder, &NovaRecordManager::isRecordingChanged, this, &NovaAudioEngine::isRecordingChanged);

#if defined(Q_OS_ANDROID)
    // Puente: el import en Android se hace via AudioEngine (Oboe), pero la UI
    // escucha las señales en NovaRegionModel (AudioEngine.regions).
    connect(this, &NovaAudioEngine::androidImportFinished,
            m_regionModel, &NovaRegionModel::importFinished);
    connect(this, &NovaAudioEngine::androidImportingChanged,
            m_regionModel, &NovaRegionModel::importingChanged);
#endif

#if defined(Q_OS_ANDROID)
    // Fase 2b: el cargador decodifica el fichero en un hilo aparte.
    connect(&m_androidLoader, &NovaSpikeLoader::cargaTerminada,
            this, &NovaAudioEngine::androidCargaTerminada);
    connect(&m_androidLoader, &NovaSpikeLoader::cargaFallida,
            this, &NovaAudioEngine::androidCargaFallida);

    // Auto-advance: cuando un clip se agota, pasar al siguiente.
    connect(&m_androidEngine, &NovaAndroidAudioEngine::playbackFinished,
            this, &NovaAudioEngine::androidAutoAdvance);

    // Feedback visual en vivo: actualizar el clip de grabación en tiempo real conforme avanza el cabezal
    connect(&m_transport, &NovaTransportController::positionChanged, this, [this]() {
        if (m_androidGrabando && m_regionModel && m_transport.isPlaying()) {
            double currentBeat = m_transport.currentBeat();
            double lengthBeats = (std::max)(0.0, currentBeat - m_androidGrabacionStartBeat);
            m_regionModel->updateLiveRecordingClip(lengthBeats);
        }
    });
#endif
}

NovaAudioEngine::~NovaAudioEngine()
{
    qCDebug(novaCore) << "Destruyendo NovaAudioEngine... Cierre seguro de recursos.";
    if (s_instance == this) s_instance = nullptr;
    if (m_regionModel) m_regionModel->waitForImports();
    m_transport.setSession(nullptr);
}

bool NovaAudioEngine::initEngine()
{
    // 🛡️ Sanitización preventiva de hardware al arrancar
    NovaHardwareSanitizer::sanitize();

    // Inicializar el subsistema del administrador de proyectos
    bool ok = m_sessionManager.initSession();
    if (!ok) {
        qCCritical(novaCore) << "Fallo fatal al arrancar m_sessionManager.";
        return false;
    }

    connectSessionSignals();

    // 🆕 Auto-crear proyecto temporal si no hay ninguno abierto.
    if (m_sessionManager.session() == nullptr) {
        const QString scratchDir = NovaProjectManager::scratchDirectory();
        qCDebug(novaCore) << "🆕 Auto-creando proyecto temporal en:" << scratchDir;
        newProjectInternal(QStringLiteral("Untitled"), scratchDir, /*isScratch=*/true);
    }

#if defined(Q_OS_ANDROID)
    // Fase 2b: motor de audio Oboe. Se prepara (initialize + start) de forma
    // lazy en la primera reproduccion, como hace el spike.
    qCDebug(novaCore) << "📱 Motor Android listo para cargar ficheros.";
#endif

    return true;
}

void NovaAudioEngine::connectSessionSignals()
{
    auto session = m_sessionManager.session();
    m_transport.setSession(session);
    m_recorder.setSession(session);
    m_recorder.setRegionModel(m_regionModel);
    m_recorder.setTransportController(&m_transport);
    m_trackModel->setSession(session);
    m_regionModel->setSession(session);
#if defined(Q_OS_ANDROID)
    m_androidEngine.setSession(static_cast<ARDOUR::Session*>(session));
#endif
    m_deviceManager.setEngine(m_sessionManager.engine());

    // Conectar el renderizador global de formas de onda
    NovaWaveformItem::setSession(session);

    // Notificar cambios del proyecto a la interfaz
    Q_EMIT currentProjectChanged();
    Q_EMIT isDirtyChanged();
    Q_EMIT tracksChanged();
    Q_EMIT regionsChanged();
    Q_EMIT recentProjectsChanged();
}

// Controles de Transporte
void NovaAudioEngine::play() {
    m_transport.play();
#if defined(Q_OS_ANDROID)
    // Fase 2b/3: si hay un fichero cargado, arranca la salida Oboe.
    if (m_androidEngine.tieneFicheroCargado()) {
        if (!m_androidMotorListo) {
            m_androidMotorListo = androidPrepararMotor();
        }
        if (m_androidMotorListo) {
            // Split real: si el playhead cae dentro de una region, se
            // reproduce solo esa porcion (rango). Si no, fichero completo.
            double curFrame = currentFrame();
            qint64 rangeStart = 0;
            qint64 rangeEnd = 0;
            if (m_regionModel) {
                for (int i = 0; i < m_regionModel->rowCount(); ++i) {
                    QModelIndex idx = m_regionModel->index(i, 0);
                    double rStart = m_regionModel->data(idx, NovaRegionModel::StartFrameRole).toDouble();
                    double rLen = m_regionModel->data(idx, NovaRegionModel::LengthFramesRole).toDouble();
                    if (curFrame >= rStart && curFrame < rStart + rLen && rLen > 0) {
                        rangeStart = static_cast<qint64>(rStart);
                        rangeEnd = static_cast<qint64>(rStart + rLen);
                        break;
                    }
                }
            }
            if (rangeEnd > rangeStart) {
                m_androidEngine.setPlaybackRange(rangeStart, rangeEnd);
                if (curFrame < rangeStart) curFrame = rangeStart;
                m_androidEngine.seekToFrame(static_cast<qint64>(curFrame));
            } else {
                m_androidEngine.resetPlaybackRange();
            }
            m_androidEngine.setPlaying(true);
        }
    }
    QJniObject::callStaticMethod<void>(
        "com/novastudio/nova/AudioForegroundService",
        "updateServicePlaybackState",
        "(Z)V",
        static_cast<jboolean>(true));
#endif
}
void NovaAudioEngine::pause() {
    m_transport.stop();
#if defined(Q_OS_ANDROID)
    m_androidEngine.setPlaying(false);
    QJniObject::callStaticMethod<void>(
        "com/novastudio/nova/AudioForegroundService",
        "updateServicePlaybackState",
        "(Z)V",
        static_cast<jboolean>(false));
#endif
}
void NovaAudioEngine::stop() {
    m_transport.stop();
#if defined(Q_OS_ANDROID)
    m_androidEngine.setPlaying(false);
    m_androidParadaPedida = true;
    QJniObject::callStaticMethod<void>(
        "com/novastudio/nova/AudioForegroundService",
        "updateServicePlaybackState",
        "(Z)V",
        static_cast<jboolean>(false));
#endif
}
void NovaAudioEngine::togglePlay() {
#if defined(Q_OS_ANDROID)
    // En Android el motor Oboe se arranca/para por separado del transporte
    // Ardour (que es un stub). Reutilizamos play()/stop() para no duplicar
    // la logica de rangos de reproduccion.
    if (isPlaying()) {
        stop();
    } else {
        play();
    }
#else
    m_transport.togglePlay();
#endif
}
void NovaAudioEngine::toggleRecord() {
#if defined(Q_OS_ANDROID)
    if (m_androidGrabando) {
        m_androidGrabando = false;
        m_androidEngine.setRecording(false);
        m_androidEngine.setPlaying(false);
        m_transport.stop();
        Q_EMIT isRecordingChanged();
        // Dar tiempo al disk writer para volcar el último bloque.
        QTimer::singleShot(400, this, [this]() {
            importUltimaGrabacionAndroid();
        });
        qCDebug(novaCore) << "🎙️ [Android] Grabación detenida; importando captura.";
    } else {
        // Comprobar / solicitar permiso de micrófono antes de capturar audio
        bool concedido = false;
        jboolean hasPerm = QJniObject::callStaticMethod<jboolean>(
            "com/novastudio/nova/MainActivity",
            "hasRecordAudioPermission");
        concedido = (hasPerm == JNI_TRUE);

        if (!concedido) {
            QMicrophonePermission micPerm;
            auto status = qApp->checkPermission(micPerm);
            if (status == Qt::PermissionStatus::Granted) {
                concedido = true;
            }
        }

        if (!concedido) {
            qCDebug(novaCore) << "🎙️ [Android] Solicitando permiso de micrófono al usuario...";
            QJniObject::callStaticMethod<void>(
                "com/novastudio/nova/MainActivity",
                "requestRecordAudioPermission");

            QMicrophonePermission micPerm;
            qApp->requestPermission(micPerm, this, [this](const QPermission &p) {
                if (p.status() == Qt::PermissionStatus::Granted) {
                    qCDebug(novaCore) << "🎙️ [Android] Permiso de micrófono concedido!";
                    iniciarGrabacionAndroid();
                } else {
                    qCWarning(novaCore) << "❌ [Android] Permiso de micrófono denegado.";
                }
            });
            return;
        }

        iniciarGrabacionAndroid();
    }
#else
    m_recorder.toggleRecord();
#endif
}

#if defined(Q_OS_ANDROID)
void NovaAudioEngine::iniciarGrabacionAndroid()
{
    if (m_androidGrabando) return;

    if (!m_androidMotorListo) {
        m_androidMotorListo = androidPrepararMotor();
    }
    // Asegurar que hay al menos una pista para capturar y asignar la sesión al motor
    int armedTrackIdx = 0;
    {
        auto *session = static_cast<ARDOUR::Session*>(m_sessionManager.session());
        if (session) {
            auto routes = session->get_routes();
            bool hasTrack = false;
            bool foundArmed = false;
            if (routes) {
                int idx = 0;
                for (const auto &r : *routes) {
                    if (r && r->is_track()) {
                        hasTrack = true;
                        auto trk = std::dynamic_pointer_cast<ARDOUR::Track>(r);
                        if (trk && trk->rec_enable_control() && trk->rec_enable_control()->get_value() > 0.5) {
                            armedTrackIdx = idx;
                            foundArmed = true;
                            break;
                        }
                        idx++;
                    }
                }
            }
            if (!hasTrack) {
                session->new_audio_track(2, 1, nullptr, 1, std::string("Audio 1"),
                                        ARDOUR::PresentationInfo::max_order, ARDOUR::Normal, true, false);
                armedTrackIdx = 0;
                foundArmed = true;
            } else if (!foundArmed) {
                // Si ninguna pista está armada, armar automáticamente la primera pista para que el usuario pueda grabar sin pasos extra
                armedTrackIdx = 0;
                for (const auto &r : *routes) {
                    if (r && r->is_track()) {
                        auto trk = std::dynamic_pointer_cast<ARDOUR::Track>(r);
                        if (trk && trk->rec_enable_control()) {
                            trk->rec_enable_control()->set_value(1.0, PBD::Controllable::NoGroup);
                        }
                        break;
                    }
                }
                Q_EMIT tracksChanged();
            }
            m_androidEngine.setSession(session);
        }
    }

    m_androidGrabacionTrackIndex = armedTrackIdx;
    m_androidGrabacionStartBeat = m_transport.currentBeat();

    m_androidGrabando = true;
    m_androidEngine.setRecording(true);
    m_androidEngine.setPlaying(true);
    m_transport.play();

    // Feedback visual en vivo: crear el clip rojo en la línea de tiempo en la pista y compás indicados
    if (m_regionModel) {
        m_regionModel->createLiveRecordingClip(m_androidGrabacionStartBeat, m_androidGrabacionTrackIndex);
    }

    Q_EMIT isRecordingChanged();
    qCDebug(novaCore) << "🎙️ [Android] Grabación iniciada en pista:" << m_androidGrabacionTrackIndex
                      << "beat:" << m_androidGrabacionStartBeat;
}
#endif
void NovaAudioEngine::locateFrame(double frame) { m_transport.locateFrame(frame); }
void NovaAudioEngine::locateBeat(double beat) { m_transport.locateBeat(beat); }
void NovaAudioEngine::setBpm(double bpm) { m_transport.setBpm(bpm); }
void NovaAudioEngine::rewind() { m_transport.rewind(); }

void NovaAudioEngine::setLoopEnabled(bool enabled) {
    m_transport.setLoopEnabled(enabled);
#if defined(Q_OS_ANDROID)
    m_androidEngine.setLoopEnabled(enabled);
#endif
}
void NovaAudioEngine::setLoopRange(double startBeat, double endBeat) {
    m_transport.setLoopRange(startBeat, endBeat);
#if defined(Q_OS_ANDROID)
    double sr = m_androidEngine.tasaSalidaReal() > 0 ? m_androidEngine.tasaSalidaReal() : 48000.0;
    double bpm = this->bpm() > 0 ? this->bpm() : 120.0;
    qint64 startFrame = static_cast<qint64>(NovaTimeUtils::beatToFrame(startBeat, sr, bpm));
    qint64 endFrame = static_cast<qint64>(NovaTimeUtils::beatToFrame(endBeat, sr, bpm));
    m_androidEngine.setLoopRangeFrames(startFrame, endFrame);
#endif
}
void NovaAudioEngine::toggleLoop() { m_transport.toggleLoop(); }

// ✂️ HERRAMIENTA SPLIT: Dividir clip en el punto exacto del Playhead
bool NovaAudioEngine::splitAtPlayhead(int targetRegionIndex)
{
    if (!m_regionModel) return false;

    double playheadBeat = m_transport.currentBeat();

    // 1. Si se le pasó un índice explícito, cortar esa región específica
    if (targetRegionIndex >= 0) {
        return m_regionModel->splitRegion(targetRegionIndex, playheadBeat);
    }

    // 2. Si no se especificó índice, buscar automáticamente la región bajo el Playhead
    bool anySplit = false;
    int count = m_regionModel->rowCount();

    for (int i = 0; i < count; ++i) {
        QModelIndex idx = m_regionModel->index(i, 0);
        double startBeat = m_regionModel->data(idx, NovaRegionModel::StartBeatRole).toDouble();
        double lengthBeats = m_regionModel->data(idx, NovaRegionModel::LengthBeatsRole).toDouble();
        double endBeat = startBeat + lengthBeats;

        if (playheadBeat > startBeat + 0.01 && playheadBeat < endBeat - 0.01) {
            qCDebug(novaCore) << "✂️ [Split] Cortando clip en índice" << i << "en beat:" << playheadBeat;
            if (m_regionModel->splitRegion(i, playheadBeat)) {
                anySplit = true;
                break; // Dividir el primer clip que coincida
            }
        }
    }

    if (anySplit) {
        Q_EMIT isDirtyChanged();
        Q_EMIT regionsChanged();
    } else {
        qCDebug(novaCore) << "✂️ [Split] No hay ningún clip posicionado sobre el Playhead en beat:" << playheadBeat;
    }

    return anySplit;
}

// 💾 GUARDAR EL PROYECTO ACTUAL (DESDE BOTÓN O CTRL + S)
bool NovaAudioEngine::saveProject()
{
    bool ok = NovaProjectManager::saveProject(m_sessionManager.session());
    if (ok) {
        Q_EMIT isDirtyChanged();
        Q_EMIT recentProjectsChanged();
    }
    return ok;
}

// 📂 CARGAR UN PROYECTO EXISTENTE DESDE DISCO
bool NovaAudioEngine::openProject(const QString &path)
{
    qCDebug(novaCore) << "🔊 Solicitada apertura del proyecto en:" << path;
    // Desacoplar los modelos ANTES de cerrar la sesión: los hilos de cálculo
    // de picos leen de la sesión y no pueden quedarse con una sesión ya
    // destruida (petaba al abrir/cerrar proyecto).
    m_trackModel->setSession(nullptr);
    m_regionModel->setSession(nullptr);
    m_sessionManager.closeCurrentSession();
    
    auto session = NovaProjectManager::openProject(m_sessionManager.engine(), path);
    m_sessionManager.setSession(session);
    
    if (session) {
        connectSessionSignals();
        return true;
    }
    return false;
}

// 🆕 CREAR UN NUEVO PROYECTO LIMPIO INDEPENDIENTE
bool NovaAudioEngine::newProject(const QString &name, const QString &parentDir)
{
    return newProjectInternal(name, parentDir, /*isScratch=*/false);
}

bool NovaAudioEngine::newProjectInternal(const QString &name, const QString &parentDir, bool isScratch)
{
    qCDebug(novaCore) << "🔊 Creando nuevo proyecto comercial:" << name << "en:" << parentDir;
    // Desacoplar los modelos antes de cerrar la sesión (ver openProject).
    m_trackModel->setSession(nullptr);
    m_regionModel->setSession(nullptr);
    m_sessionManager.closeCurrentSession();

    auto session = NovaProjectManager::createProject(m_sessionManager.engine(), name, parentDir, isScratch);

    if (!session && !isScratch) {
        // El nombre pedido esta ocupado por un proyecto guardado: se prueba
        // con el siguiente libre en vez de fallar en silencio.
        const QString uniqueName = NovaProjectPolicy::uniqueProjectName(parentDir, name);
        if (uniqueName != name) {
            qCDebug(novaCore) << "ℹ️ El nombre esta ocupado, se usara:" << uniqueName;
            session = NovaProjectManager::createProject(m_sessionManager.engine(), uniqueName, parentDir, isScratch);
        }
    }

    m_sessionManager.setSession(session);

    if (session) {
        connectSessionSignals();
        return true;
    }

    qCCritical(novaCore) << "❌ No se pudo crear el proyecto:" << name
                         << "(puede que ya exista un proyecto guardado con ese nombre)";
    return false;
}

// 🚪 CERRAR EL PROYECTO ACTIVO Y LIBERAR RECURSOS
void NovaAudioEngine::closeProject()
{
    qCDebug(novaCore) << "🚪 Liberando proyecto y retornando a la Start Screen...";
    m_transport.setSession(nullptr);
    m_recorder.setSession(nullptr);
    m_trackModel->setSession(nullptr);
    m_regionModel->setSession(nullptr);
#if defined(Q_OS_ANDROID)
    m_androidEngine.setSession(nullptr);
#endif
    m_sessionManager.closeCurrentSession();

    Q_EMIT currentProjectChanged();
    Q_EMIT isDirtyChanged();
    Q_EMIT tracksChanged();
    Q_EMIT regionsChanged();
    Q_EMIT recentProjectsChanged();
}

// 🛑 APAGADO COMPLETO DE LA APLICACIÓN (para "Descartar y Salir")
Q_INVOKABLE void NovaAudioEngine::quitApplication()
{
    qCDebug(novaCore) << "🛑 Apagado completo solicitado - cerrando motor de audio y saliendo...";
    
    // Detener transporte si está reproduciendo
    m_transport.stop();
    m_transport.setSession(nullptr);
    
    // Detener grabador
    m_recorder.setSession(nullptr);
    
    // Limpiar modelos
    m_trackModel->setSession(nullptr);
    m_regionModel->setSession(nullptr);
#if defined(Q_OS_ANDROID)
    m_androidEngine.setSession(nullptr);
#endif
    
    // Cerrar sesión y motor de audio
    m_sessionManager.closeCurrentSession();
    m_sessionManager.shutdownAudioEngine();
    
    // Forzar salida del event loop de Qt
    QCoreApplication::exit(0);
}

// 🗑️ DESCARTAR CAMBIOS Y ELIMINAR PROYECTO NO GUARDADO
void NovaAudioEngine::discardProject()
{
    qCDebug(novaCore) << "🗑️ Descartando proyecto activo sin guardar...";

    // 1) Decidir ANTES de cerrar: la sesion viva es quien tiene la ruta.
    //    Devuelve vacio si el proyecto esta guardado (no se borra nunca).
    const QString pathToDelete = NovaProjectManager::discardableProjectPath(m_sessionManager.session());

    // 2) Cerrar la sesion primero: mientras viva sigue escribiendo en la carpeta.
    closeProject();

    // 3) Y solo despues eliminar la carpeta, y solo si nunca fue guardada.
    NovaProjectManager::deleteUnsavedProjectFolder(pathToDelete);
}

// 🗑️ ELIMINAR PROYECTO DEL HISTORIAL DE RECIENTES
void NovaAudioEngine::removeRecentProject(const QString &path)
{
    NovaProjectManager::removeProjectFromRecent(path);
    Q_EMIT recentProjectsChanged();
}

bool NovaAudioEngine::isDirty() const
{
    return NovaProjectManager::isDirty(m_sessionManager.session());
}

QVariantList NovaAudioEngine::recentProjects() const
{
    return NovaProjectManager::getRecentProjects();
}

QString NovaAudioEngine::currentProjectName() const
{
    auto session = m_sessionManager.session();
    if (!session) return QStringLiteral("No Project");
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    return QString::fromStdString(static_cast<ARDOUR::Session*>(session)->name());
#else
    return QStringLiteral("No Project");
#endif
}

QString NovaAudioEngine::currentProjectPath() const
{
    auto session = m_sessionManager.session();
    if (!session) return QString();
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    return QString::fromStdString(static_cast<ARDOUR::Session*>(session)->path());
#else
    return QString::fromStdString(static_cast<ARDOUR::Session*>(session)->path());
#endif
}

float NovaAudioEngine::masterPeakLeft() const
{
#if defined(Q_OS_ANDROID)
    return m_androidEngine.peakLeft();
#elif defined(Q_OS_WIN)
    return 0.0f;
#else
    auto session = m_sessionManager.session();
    if (!session) return 0.0f;

    auto master = static_cast<ARDOUR::Session*>(session)->master_out();
    if (!master) return 0.0f;

    auto meter = master->peak_meter();
    if (!meter) return 0.0f;

    float val = meter->meter_level(0, ARDOUR::MeterPeak);
    if (std::isnan(val) || std::isinf(val) || val < 0.0f) return 0.0f;
    return val;
#endif
}

float NovaAudioEngine::masterPeakRight() const
{
#if defined(Q_OS_ANDROID)
    return m_androidEngine.peakRight();
#elif defined(Q_OS_WIN)
    return 0.0f;
#else
    auto session = m_sessionManager.session();
    if (!session) return 0.0f;

    auto master = static_cast<ARDOUR::Session*>(session)->master_out();
    if (!master) return 0.0f;

    auto meter = master->peak_meter();
    if (!meter) return 0.0f;

    float val = meter->meter_level(1, ARDOUR::MeterPeak);
    if (std::isnan(val) || std::isinf(val) || val < 0.0f) return 0.0f;
    return val;
#endif
}

float NovaAudioEngine::masterVolumeDb() const
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    return 0.0f;
#else
    auto session = m_sessionManager.session();
    if (!session) return 0.0f;

    auto master = static_cast<ARDOUR::Session*>(session)->master_out();
    if (!master) return 0.0f;

    auto gc = master->gain_control();
    if (!gc) return 0.0f;

    float coeff = static_cast<float>(gc->get_value());
    if (coeff <= 0.0000001f) return -192.0f;
    return 20.0f * std::log10(coeff);
#endif
}

void NovaAudioEngine::setMasterVolumeDb(float dB)
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    auto session = m_sessionManager.session();
    if (!session) return;

    auto master = static_cast<ARDOUR::Session*>(session)->master_out();
    if (!master) return;

    auto gc = master->gain_control();
    if (gc) {
        float coeff = (dB <= -192.0f) ? 0.0f : std::pow(10.0f, dB / 20.0f);
        gc->set_value(coeff, PBD::Controllable::NoGroup);
        Q_EMIT masterVolumeDbChanged();
    }
#else
    Q_UNUSED(dB);
#endif
}
#if defined(Q_OS_ANDROID)

bool NovaAudioEngine::loadAndPlayAndroid(const QString &path)
{
    return importAudioAndroid(-1, path, 0.0);
}

bool NovaAudioEngine::importAudioAndroid(int trackIndex, const QString &path, double startBeat)
{
    if (path.isEmpty()) return false;
    m_pendingImportTrackIndex = trackIndex;
    m_pendingImportStartBeat = startBeat;
    m_pendingImportFileName = QFileInfo(path).fileName();
    m_androidParadaPedida = false;
    m_androidLoader.cargar(path);
    Q_EMIT androidImportingChanged(true);
    return true;
}

void NovaAudioEngine::reverseAudioAndroid()
{
    m_androidEngine.reverseCurrentAudio();
}

void NovaAudioEngine::setPitchSemitonesAndroid(int semitones)
{
    m_androidEngine.setPitchSemitones(semitones);
}

void NovaAudioEngine::requestStoragePermission()
{
#if defined(Q_OS_ANDROID)
    QJniObject::callStaticMethod<void>(
        "com/novastudio/nova/MainActivity",
        "requestStorageAudioPermission");
#endif
}

void NovaAudioEngine::openAppSettings()
{
#if defined(Q_OS_ANDROID)
    QJniObject::callStaticMethod<void>(
        "com/novastudio/nova/MainActivity",
        "openAppSettings");
#endif
}

bool NovaAudioEngine::androidPrepararMotor()
{
    if (m_androidMotorListo) return true;
    if (!m_androidEngine.initialize(48000)) {
        qCWarning(novaCore) << "🔊 Android: el motor no se ha podido inicializar";
        return false;
    }
    if (!m_androidEngine.start()) {
        qCWarning(novaCore) << "🔊 Android: el motor no se ha podido arrancar";
        return false;
    }
    if (!m_androidEngine.openOutputStream(2)) {
        qCWarning(novaCore) << "🔊 Android: la salida no tiene dos canales";
    }
    m_androidMotorListo = true;
    return true;
}

void NovaAudioEngine::androidCargaTerminada()
{
    if (m_androidParadaPedida) {
        m_androidLoader.tomarAudio();
        Q_EMIT androidImportFinished(false, QStringLiteral("Parado"));
        Q_EMIT androidImportingChanged(false);
        if (m_regionModel) {
            Q_EMIT m_regionModel->importFinished(false, QStringLiteral("Parado"));
            Q_EMIT m_regionModel->importingChanged(false);
        }
        return;
    }

    const PeakData &picos = m_androidLoader.picos();
    NovaDecodedAudio audio = m_androidLoader.tomarAudio();
    if (audio.channels <= 0 || audio.sampleRate <= 0) {
        qCWarning(novaCore) << "🔊 Android: no se ha podido decodificar el fichero";
        Q_EMIT androidImportFinished(false, QStringLiteral("No se pudo decodificar el fichero"));
        Q_EMIT androidImportingChanged(false);
        if (m_regionModel) {
            Q_EMIT m_regionModel->importFinished(false, QStringLiteral("No se pudo decodificar el fichero"));
            Q_EMIT m_regionModel->importingChanged(false);
        }
        return;
    }

    m_androidDuracion = static_cast<double>(audio.frameCount()) / static_cast<double>(audio.sampleRate);

    // Crear la región en la pista seleccionada dentro de la sesión stub de Android
    std::string newRegionIdStr = "0";
    {
        auto *session = static_cast<ARDOUR::Session*>(m_sessionManager.session());
        if (session) {
            auto routeList = session->get_routes();
            bool hasTrack = false;
            if (routeList) {
                for (const auto &r : *routeList) {
                    if (r && r->is_track()) { hasTrack = true; break; }
                }
            }
            if (!hasTrack) {
                session->new_audio_track(
                    2, 1, nullptr, 1, std::string("Audio 1"),
                    ARDOUR::PresentationInfo::max_order, ARDOUR::Normal, true, false);
            }

            routeList = session->get_routes();
            std::shared_ptr<ARDOUR::Track> targetTrack;
            if (routeList) {
                int curIdx = 0;
                for (const auto &r : *routeList) {
                    if (!r || !r->is_track()) continue;
                    if (curIdx == m_pendingImportTrackIndex) {
                        targetTrack = std::dynamic_pointer_cast<ARDOUR::Track>(r);
                        break;
                    }
                    curIdx++;
                }
                if (!targetTrack) {
                    for (const auto &r : *routeList) {
                        if (r && r->is_track()) {
                            targetTrack = std::dynamic_pointer_cast<ARDOUR::Track>(r);
                            break;
                        }
                    }
                }
            }

            if (targetTrack && targetTrack->playlist()) {
                auto region = std::make_shared<ARDOUR::AudioRegion>();
                region->set_name(m_pendingImportFileName.isEmpty() ? "Audio" : m_pendingImportFileName.toStdString());
                region->set_length(Temporal::timecnt_t(audio.frameCount()));

                double sRate = session->sample_rate() > 0 ? session->sample_rate() : 48000.0;
                auto startSample = static_cast<ARDOUR::samplepos_t>(
                    NovaTimeUtils::beatToFrame(m_pendingImportStartBeat, sRate, 120.0));
                region->set_position(Temporal::timepos_t(startSample));

                targetTrack->playlist()->add_region(region, Temporal::timepos_t(startSample));
                session->set_dirty();
                newRegionIdStr = region->id().to_s();
            }
        }
    }

    // Publicar la forma de onda en la caché de picos ANTES de reconstruir el modelo
    const QString qRegionId = QString::fromStdString(newRegionIdStr);
    NovaWaveformCache::setPeaks(qRegionId, picos);
    NovaWaveformCache::setPeaks(QStringLiteral("spike"), picos);
    m_androidVersionPicos++;

    // Guardar también en disco si hay carpeta de sesión
    if (m_sessionManager.session()) {
        auto *session = static_cast<ARDOUR::Session*>(m_sessionManager.session());
        QString peaksDir = QString::fromStdString(session->path()) + "/peaks";
        QDir().mkpath(peaksDir);
        QString peakFile = peaksDir + "/" + qRegionId + ".novapeak";
        NovaRegionModel::savePeakFile(peakFile, picos);
    }

    // Reconstruir la caché de regiones con los picos ya listos
    if (m_regionModel) {
        m_regionModel->rebuildRegionCache();
    }

    if (!androidPrepararMotor()) {
        qCWarning(novaCore) << "🔊 Android: motor no listo, se ve la onda pero no suena";
        Q_EMIT androidImportFinished(false, QStringLiteral("Motor de audio no listo"));
        Q_EMIT androidImportingChanged(false);
        if (m_regionModel) {
            Q_EMIT m_regionModel->importFinished(false, QStringLiteral("Motor de audio no listo"));
            Q_EMIT m_regionModel->importingChanged(false);
        }
        return;
    }

    QString error;
    if (!m_androidEngine.loadDecoded(std::move(audio), &error)) {
        qCWarning(novaCore) << "🔊 Android: no se ha podido cargar el fichero en el motor:" << error;
        Q_EMIT androidImportFinished(false, error.isEmpty() ? QStringLiteral("No se pudo cargar el fichero") : error);
        Q_EMIT androidImportingChanged(false);
        if (m_regionModel) {
            Q_EMIT m_regionModel->importFinished(false, error.isEmpty() ? QStringLiteral("No se pudo cargar el fichero") : error);
            Q_EMIT m_regionModel->importingChanged(false);
        }
        return;
    }

    // Arrancar la reproducción
    m_androidEngine.setPlaying(true);
    m_transport.play();

    Q_EMIT androidImportFinished(true, QStringLiteral("%1 importado").arg(m_pendingImportFileName));
    Q_EMIT androidImportingChanged(false);
    if (m_regionModel) {
        Q_EMIT m_regionModel->importFinished(true, QStringLiteral("%1 importado").arg(m_pendingImportFileName));
        Q_EMIT m_regionModel->importingChanged(false);
    }
}

void NovaAudioEngine::androidCargaFallida(const QString &error)
{
    qCWarning(novaCore) << "🔊 Android: carga fallida:" << error;
    Q_EMIT androidImportFinished(false, error);
    Q_EMIT androidImportingChanged(false);
    if (m_regionModel) {
        Q_EMIT m_regionModel->importFinished(false, error);
        Q_EMIT m_regionModel->importingChanged(false);
    }
}

void NovaAudioEngine::androidAutoAdvance()
{
#if defined(Q_OS_ANDROID)
    if (!m_androidEngine.tieneFicheroCargado()) return;

    // Buscar la siguiente region (por startFrame) tras la actual.
    double curFrame = currentFrame();
    int nextIdx = -1;
    double bestStart = -1;
    for (int i = 0; i < (m_regionModel ? m_regionModel->rowCount() : 0); ++i) {
        QModelIndex idx = m_regionModel->index(i, 0);
        double rStart = m_regionModel->data(idx, NovaRegionModel::StartFrameRole).toDouble();
        double rLen = m_regionModel->data(idx, NovaRegionModel::LengthFramesRole).toDouble();
        if (rLen <= 0) continue;
        if (rStart > curFrame + 1.0) {
            if (bestStart < 0 || rStart < bestStart) {
                bestStart = rStart;
                nextIdx = i;
            }
        }
    }

    if (nextIdx >= 0) {
        QModelIndex idx = m_regionModel->index(nextIdx, 0);
        double rStart = m_regionModel->data(idx, NovaRegionModel::StartFrameRole).toDouble();
        double rLen = m_regionModel->data(idx, NovaRegionModel::LengthFramesRole).toDouble();
        m_androidEngine.setPlaybackRange(static_cast<qint64>(rStart),
                                        static_cast<qint64>(rStart + rLen));
        m_androidEngine.seekToFrame(static_cast<qint64>(rStart));
        m_androidEngine.setPlaying(true);
        m_transport.play();
        qCDebug(novaCore) << "⏭️ [Android] Auto-advance a región" << nextIdx;
    } else {
        // No hay más regiones: parar.
        m_androidEngine.setPlaying(false);
        m_transport.stop();
        qCDebug(novaCore) << "⏹️ [Android] Fin de la reproducción.";
    }
#endif
}

void NovaAudioEngine::importUltimaGrabacionAndroid()
{
    // Busca el WAV mas reciente en la carpeta interchange del proyecto.
    QString dir = currentProjectPath();
    if (dir.isEmpty()) return;

    QDir interchange(dir + QStringLiteral("/interchange"));
    if (!interchange.exists()) return;

    QStringList wavs;
    // Recorrer subcarpetas (una por pista).
    QDirIterator it(interchange.absolutePath(), QStringList() << QStringLiteral("*.wav"),
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        wavs.append(it.next());
    }
    if (wavs.isEmpty()) {
        qCWarning(novaCore) << "⚠️ [Android] No se encontró ninguna toma WAV grabada en:" << interchange.absolutePath();
        if (m_regionModel) {
            m_regionModel->finalizeLiveRecordingClip();
        }
        return;
    }

    // El mas reciente por fecha de modificacion.
    std::sort(wavs.begin(), wavs.end(), [](const QString &a, const QString &b) {
        return QFileInfo(a).lastModified() > QFileInfo(b).lastModified();
    });

    QString wavPath = wavs.first();
    qCDebug(novaCore) << "🎙️ [Android] Importando toma grabada:" << wavPath
                      << "en pista:" << m_androidGrabacionTrackIndex
                      << "beat:" << m_androidGrabacionStartBeat;

    if (m_regionModel) {
        m_regionModel->finalizeLiveRecordingClip();
    }

    importAudioAndroid(m_androidGrabacionTrackIndex, wavPath, m_androidGrabacionStartBeat);
}

#endif // Q_OS_ANDROID

// ── COLABORACIÓN LOCAL: PAQUETES DE CAMBIOS (.novapatch) ─────────────

bool NovaAudioEngine::exportPatch(const QString &targetFilePath,
                                  const QString &notes,
                                  const QString &author,
                                  const QVariantList &trackIndices)
{
    qCDebug(novaCore) << "📦 Exportando parche colaborativo a:" << targetFilePath;

    std::vector<NovaPatchTrackInfo> patchTracks;
    QString projName = currentProjectName();
    double currentBpm = bpm();

    // Obtener la carpeta de audio de la sesión
    QString soundDir;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    void *sess = m_sessionManager.session();
    if (sess) {
        ARDOUR::Session *ardourSession = static_cast<ARDOUR::Session*>(sess);
        soundDir = QString::fromStdString(ardourSession->session_directory().sound_path());
    }
#endif
    if (soundDir.isEmpty()) {
        soundDir = currentProjectPath() + QStringLiteral("/audio");
        if (!QDir(soundDir).exists()) {
            soundDir = currentProjectPath() + QStringLiteral("/interchange");
        }
    }

    // Filtrar pistas seleccionadas
    QSet<int> includedIndices;
    if (trackIndices.isEmpty()) {
        int total = m_trackModel ? m_trackModel->rowCount() : 0;
        for (int i = 0; i < total; ++i) includedIndices.insert(i);
    } else {
        for (const auto &v : trackIndices) includedIndices.insert(v.toInt());
    }

    int totalTracks = m_trackModel ? m_trackModel->rowCount() : 0;
    int totalRegions = m_regionModel ? m_regionModel->rowCount() : 0;

    for (int tIdx = 0; tIdx < totalTracks; ++tIdx) {
        if (!includedIndices.contains(tIdx)) continue;

        QModelIndex trkIdx = m_trackModel->index(tIdx, 0);
        QString trkName = m_trackModel->data(trkIdx, NovaTrackListModel::TrackNameRole).toString();
        QString trkColor = m_trackModel->data(trkIdx, NovaTrackListModel::ColorRole).toString();
        float trkGain = m_trackModel->data(trkIdx, NovaTrackListModel::GainRole).toFloat();
        float trkPan = m_trackModel->data(trkIdx, NovaTrackListModel::PanRole).toFloat();

        NovaPatchTrackInfo patchTrack;
        patchTrack.name = trkName;
        patchTrack.color = trkColor;
        patchTrack.gainDb = trkGain;
        patchTrack.pan = trkPan;

        // Buscar regiones asociadas a esta pista
        for (int rIdx = 0; rIdx < totalRegions; ++rIdx) {
            QModelIndex regIdx = m_regionModel->index(rIdx, 0);
            int regionTrack = m_regionModel->data(regIdx, NovaRegionModel::TrackIndexRole).toInt();
            if (regionTrack != tIdx) continue;

            NovaPatchClipInfo clip;
            clip.name = m_regionModel->data(regIdx, NovaRegionModel::RegionNameRole).toString();
            clip.startBeat = m_regionModel->data(regIdx, NovaRegionModel::StartBeatRole).toDouble();
            clip.lengthBeats = m_regionModel->data(regIdx, NovaRegionModel::LengthBeatsRole).toDouble();
            clip.clipGainDb = m_regionModel->data(regIdx, NovaRegionModel::ClipGainDbRole).toFloat();
            clip.fadeInBeats = m_regionModel->data(regIdx, NovaRegionModel::FadeInBeatsRole).toDouble();
            clip.fadeOutBeats = m_regionModel->data(regIdx, NovaRegionModel::FadeOutBeatsRole).toDouble();

            // Localizar archivo físico de audio
            QString possiblePath;
            QString candidate1 = soundDir + QStringLiteral("/") + clip.name;
            QString candidate2 = soundDir + QStringLiteral("/") + clip.name + QStringLiteral(".wav");
            QString candidate3 = soundDir + QStringLiteral("/") + clip.name + QStringLiteral(".mp3");

            if (QFile::exists(candidate1)) possiblePath = candidate1;
            else if (QFile::exists(candidate2)) possiblePath = candidate2;
            else if (QFile::exists(candidate3)) possiblePath = candidate3;
            else {
                QDirIterator it(currentProjectPath(), QStringList() << (clip.name + QStringLiteral("*")), QDir::Files, QDirIterator::Subdirectories);
                if (it.hasNext()) possiblePath = it.next();
            }

            clip.originalAudioPath = possiblePath;
            patchTrack.clips.push_back(clip);
        }

        if (!patchTrack.clips.empty() || !trackIndices.isEmpty()) {
            patchTracks.push_back(patchTrack);
        }
    }

    QString errorStr;
    bool ok = NovaPatchManager::exportPatch(targetFilePath, projName, author, notes, currentBpm, patchTracks, &errorStr);
    if (ok) {
        Q_EMIT patchExportFinished(true, QStringLiteral("Parche exportado exitosamente en: %1").arg(targetFilePath));
    } else {
        Q_EMIT patchExportFinished(false, errorStr);
    }
    return ok;
}

QVariantMap NovaAudioEngine::inspectPatch(const QString &patchFilePath)
{
    NovaPatchMetadata meta;
    qint64 totalBytes = 0;
    QString err;
    if (!NovaPatchManager::inspectPatch(patchFilePath, meta, totalBytes, &err)) {
        QVariantMap errMap;
        errMap[QStringLiteral("valid")] = false;
        errMap[QStringLiteral("error")] = err;
        return errMap;
    }
    return NovaPatchManager::metadataToVariant(meta, totalBytes);
}

bool NovaAudioEngine::applyPatch(const QString &patchFilePath, bool createNewTracks)
{
    qCDebug(novaCore) << "📥 Aplicando parche colaborativo desde:" << patchFilePath;

    NovaPatchMetadata meta;
    qint64 totalAudioBytes = 0;
    QString err;
    if (!NovaPatchManager::inspectPatch(patchFilePath, meta, totalAudioBytes, &err)) {
        qCCritical(novaCore) << "❌ Error al inspeccionar parche:" << err;
        Q_EMIT patchApplyFinished(false, err);
        return false;
    }

    // Carpeta destino del audio
    QString soundDir;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    void *sess = m_sessionManager.session();
    if (sess) {
        ARDOUR::Session *ardourSession = static_cast<ARDOUR::Session*>(sess);
        soundDir = QString::fromStdString(ardourSession->session_directory().sound_path());
    }
#endif
    if (soundDir.isEmpty()) {
        soundDir = currentProjectPath() + QStringLiteral("/audio");
        QDir().mkpath(soundDir);
    }

    QMap<QString, QString> extractedFiles;
    if (!NovaPatchManager::extractAudioFiles(patchFilePath, soundDir, extractedFiles, &err)) {
        qCCritical(novaCore) << "❌ Error al extraer audio del parche:" << err;
        Q_EMIT patchApplyFinished(false, err);
        return false;
    }

    int tracksImported = 0;
    int clipsImported = 0;

    for (const auto &trk : meta.tracks) {
        int targetTrackIdx = -1;

        if (!createNewTracks && m_trackModel) {
            for (int i = 0; i < m_trackModel->rowCount(); ++i) {
                QString existingName = m_trackModel->data(m_trackModel->index(i, 0), NovaTrackListModel::TrackNameRole).toString();
                if (existingName.compare(trk.name, Qt::CaseInsensitive) == 0) {
                    targetTrackIdx = i;
                    break;
                }
            }
        }

        if (targetTrackIdx < 0 && m_trackModel) {
            QString newTrkName = trk.name;
            if (!meta.author.isEmpty() && meta.author != QStringLiteral("Colaborador")) {
                newTrkName = QStringLiteral("%1 (%2)").arg(trk.name, meta.author);
            }
            m_trackModel->addAudioTrack(newTrkName);
            targetTrackIdx = m_trackModel->rowCount() - 1;
            tracksImported++;
        }

        if (targetTrackIdx >= 0 && m_regionModel) {
            for (const auto &clip : trk.clips) {
                QString extractedAudioPath = extractedFiles.value(clip.audioKey);
                if (extractedAudioPath.isEmpty() || !QFile::exists(extractedAudioPath)) {
                    qCWarning(novaCore) << "⚠️ Clip sin audio en el parche:" << clip.name;
                    continue;
                }

                if (m_regionModel->importAudioFile(targetTrackIdx, extractedAudioPath, clip.startBeat)) {
                    clipsImported++;
                }
            }
        }
    }

    QString successMsg = QStringLiteral("Parche de %1 aplicado: %2 pistas y %3 clips importados.")
                             .arg(meta.author.isEmpty() ? QStringLiteral("colaborador") : meta.author)
                             .arg(tracksImported)
                             .arg(clipsImported);
    qCDebug(novaCore) << "✅" << successMsg;
    Q_EMIT patchApplyFinished(true, successMsg);
    return true;
}
