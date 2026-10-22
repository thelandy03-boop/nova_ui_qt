#include "NovaAudioEngine.h"
#include "core/NovaLogging.h"
#include <QTimer>
#include "core/NovaAudioDeviceManager.h"
#include "core/NovaHardwareSanitizer.h"
#include "core/NovaProjectManager.h"
#include "core/NovaProjectPolicy.h"
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

NovaAudioEngine::NovaAudioEngine(QObject *parent)
    : QObject(parent)
{
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
#endif
}

NovaAudioEngine::~NovaAudioEngine()
{
    qCDebug(novaCore) << "Destruyendo NovaAudioEngine... Cierre seguro de recursos.";
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
#endif
}
void NovaAudioEngine::pause() {
    m_transport.stop();
#if defined(Q_OS_ANDROID)
    m_androidEngine.setPlaying(false);
#endif
}
void NovaAudioEngine::stop() {
    m_transport.stop();
#if defined(Q_OS_ANDROID)
    m_androidEngine.setPlaying(false);
    m_androidParadaPedida = true;
#endif
}
void NovaAudioEngine::togglePlay() { m_transport.togglePlay(); }
void NovaAudioEngine::toggleRecord() {
#if defined(Q_OS_ANDROID)
    // Fase 3 (grabación): en Android el grabador de Ardour (NovaRecordManager)
    // es un stub. Aquí se captura con el motor Oboe (input stream ya abierto
    // en initialize) y, al parar, se importa el WAV resultante al timeline.
    if (m_androidGrabando) {
        m_androidGrabando = false;
        m_androidEngine.setPlaying(false);
        m_transport.stop();
        Q_EMIT isRecordingChanged();
        // Dar tiempo al disk writer para volcar el último bloque.
        QTimer::singleShot(400, this, [this]() {
            importUltimaGrabacionAndroid();
        });
        qCDebug(novaCore) << "🎙️ [Android] Grabación detenida; importando captura.";
    } else {
        // Asegurar que el motor está listo (abre el input stream).
        if (!m_androidMotorListo) {
            m_androidMotorListo = androidPrepararMotor();
        }
        // Asegurar que hay al menos una pista para capturar.
        {
            auto session = m_sessionManager.session();
            if (session) {
                auto routes = session->get_routes();
                bool hasTrack = false;
                if (routes) {
                    for (const auto &r : *routes) {
                        if (r && r->is_track()) { hasTrack = true; break; }
                    }
                }
                if (!hasTrack) {
                    session->new_audio_track(2, 1, nullptr, 1, std::string("Audio 1"),
                                            ARDOUR::PresentationInfo::max_order, ARDOUR::Normal, true, false);
                }
            }
        }
        m_androidGrabando = true;
        m_androidEngine.setPlaying(true);
        m_transport.play();
        Q_EMIT isRecordingChanged();
        qCDebug(novaCore) << "🎙️ [Android] Grabación iniciada (captura de micrófono).";
    }
#else
    m_recorder.toggleRecord();
#endif
}
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
    if (path.isEmpty()) return false;
    m_androidParadaPedida = false;
    m_androidLoader.cargar(path);
    Q_EMIT androidImportingChanged(true);
    return true;
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
        return;
    }

    const PeakData &picos = m_androidLoader.picos();
    NovaDecodedAudio audio = m_androidLoader.tomarAudio();
    if (audio.channels <= 0 || audio.sampleRate <= 0) {
        qCWarning(novaCore) << "🔊 Android: no se ha podido decodificar el fichero";
        Q_EMIT androidImportFinished(false, QStringLiteral("No se pudo decodificar el fichero"));
        Q_EMIT androidImportingChanged(false);
        return;
    }

    m_androidDuracion = static_cast<double>(audio.frameCount()) / static_cast<double>(audio.sampleRate);

    // Fase 2c: crear una pista + region en la sesion stub para que el
    // timeline muestre el clip (y su onda). Si no hay pistas, se crea una.
    {
        auto session = m_sessionManager.session();
        if (session) {
            auto routeList = session->get_routes();
            bool hasTrack = false;
            if (routeList) {
                for (const auto &r : *routeList) {
                    if (r && r->is_track()) { hasTrack = true; break; }
                }
            }
            if (!hasTrack) {
                ARDOUR::AudioTrackList newTracks = session->new_audio_track(
                    2, 1, nullptr, 1, std::string("Audio 1"),
                    ARDOUR::PresentationInfo::max_order, ARDOUR::Normal, true, false);
                Q_UNUSED(newTracks);
            }
            // Buscar la primera pista y añadir la region.
            if (routeList) {
                for (const auto &r : *routeList) {
                    if (!r || !r->is_track()) continue;
                    auto track = std::dynamic_pointer_cast<ARDOUR::Track>(r);
                    if (!track || !track->playlist()) continue;
                    auto region = std::make_shared<ARDOUR::AudioRegion>();
                    region->set_length(ARDOUR::samplecnt_t(audio.frameCount()));
                    region->set_position(ARDOUR::Temporal::timepos_t(0));
                    track->playlist()->add_region(region, ARDOUR::Temporal::timepos_t(0));
                    session->set_dirty();
                    break;
                }
            }
            m_regionModel->rebuildRegionCache();
        }
    }

    // Publicar la onda bajo el id de la region (en Android el stub devuelve
    // "0" para todas las regiones). Fase 2d: publicar bajo el id real.
    {
        // Buscar el id de la region recien creada (la ultima del modelo).
        QString regionId = QStringLiteral("0");
        if (m_regionModel && m_regionModel->rowCount() > 0) {
            int last = m_regionModel->rowCount() - 1;
            QModelIndex idx = m_regionModel->index(last, 0);
            regionId = m_regionModel->data(idx, NovaRegionModel::RegionIdRole).toString();
            if (regionId.isEmpty()) regionId = QStringLiteral("0");
        }
        NovaWaveformCache::setPeaks(regionId, picos);
    }

    if (!androidPrepararMotor()) {
        qCWarning(novaCore) << "🔊 Android: motor no listo, se ve la onda pero no suena";
        Q_EMIT androidImportFinished(false, QStringLiteral("Motor de audio no listo"));
        Q_EMIT androidImportingChanged(false);
        return;
    }

    QString error;
    if (!m_androidEngine.loadDecoded(std::move(audio), &error)) {
        qCWarning(novaCore) << "🔊 Android: no se ha podido cargar el fichero en el motor:" << error;
        Q_EMIT androidImportFinished(false, error.isEmpty() ? QStringLiteral("No se pudo cargar el fichero") : error);
        Q_EMIT androidImportingChanged(false);
        return;
    }

    // Publicar la onda en la caché bajo el id "spike" (el mismo que usa
    // NovaWaveformItem en el timeline). Fase 2c: publicar bajo el id de la
    // región para soportar varias pistas.
    NovaWaveformCache::setPeaks(QStringLiteral("spike"), picos);
    m_androidVersionPicos++;

    // Arrancar la reproducción.
    m_androidEngine.setPlaying(true);
    m_transport.play();

    Q_EMIT androidImportFinished(true, QStringLiteral("Importado"));
    Q_EMIT androidImportingChanged(false);
}

void NovaAudioEngine::androidCargaFallida(const QString &error)
{
    qCWarning(novaCore) << "🔊 Android: carga fallida:" << error;
    Q_EMIT androidImportFinished(false, error);
    Q_EMIT androidImportingChanged(false);
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
    if (wavs.isEmpty()) return;

    // El mas reciente por fecha de modificacion.
    std::sort(wavs.begin(), wavs.end(), [](const QString &a, const QString &b) {
        return QFileInfo(a).lastModified() > QFileInfo(b).lastModified();
    });

    loadAndPlayAndroid(wavs.first());
}

#endif // Q_OS_ANDROID
