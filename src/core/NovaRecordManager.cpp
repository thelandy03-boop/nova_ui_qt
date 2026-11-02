#include "NovaRecordManager.h"
#include "NovaLogging.h"
#include "../models/NovaRegionModel.h"
#include "NovaTransportController.h"
#include <QDebug>
#include <QTimer>
#include <algorithm>
#include <memory>

// Include ARDOUR headers BEFORE private class definition so types are complete
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
#include "ardour/track.h"
#include "ardour/audio_track.h"
#include "ardour/route.h"
#include "ardour/playlist.h"
#include "ardour/region.h"
#include "pbd/signals.h"

#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#pragma pop_macro("foreach")
#endif

// PIMPL implementation - class definition OUTSIDE macro protection
class NovaRecordManagerPrivate {
public:
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    ARDOUR::Session *session = nullptr;
    std::vector<int> armedTrackIndices;
    bool captureFinished = false;
    bool wasRecording = false;
#else
    void *session = nullptr;
    std::vector<int> armedTrackIndices;
    bool captureFinished = false;
    bool wasRecording = false;
#endif
};

NovaRecordManager::NovaRecordManager(QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<NovaRecordManagerPrivate>())
{
}

NovaRecordManager::~NovaRecordManager() = default;

void NovaRecordManager::setSession(void *session)
{
    ARDOUR::Session *ardourSession = static_cast<ARDOUR::Session*>(session);
    d_ptr->session = ardourSession;
}

void NovaRecordManager::setRegionModel(NovaRegionModel *regionModel)
{
    m_regionModel = regionModel;
}

void NovaRecordManager::setTransportController(NovaTransportController *transport)
{
    m_transport = transport;
    if (m_transport) {
        connect(m_transport, &NovaTransportController::positionChanged,
                this, &NovaRecordManager::onPositionChanged);
        connect(m_transport, &NovaTransportController::transportStoppedWasRecording,
                this, &NovaRecordManager::onTransportStopped);
    }
}

void NovaRecordManager::toggleRecord()
{
    if (!d_ptr->session) {
        qCWarning(novaRecord) << "Sin sesión activa.";
        return;
    }

#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    const bool alreadyRecording = m_isRecording || m_liveRegionActive;

    if (alreadyRecording) {
        if (m_transport) {
            m_transport->stop();
        } else {
            onTransportStopped();
        }
        qCDebug(novaRecord) << "Grabación detenida limpiamente (STOP Stub).";
        return;
    }

    m_isRecording = true;
    m_liveRegionActive = false;

    if (m_transport) {
        m_transport->play();
    }

    Q_EMIT isRecordingChanged();
    qCDebug(novaRecord) << "Grabación en vivo iniciada (ROLL Stub).";
#else
    const bool alreadyRecording = m_isRecording || (d_ptr->session && d_ptr->session->get_record_enabled()) || m_liveRegionActive;

    if (alreadyRecording) {
        // Al parar, delegamos en el transporte. El slot onTransportStopped
        // se encarga de deshabilitar la grabación en Ardour y esperar a que el
        // hilo Butler escriba las regiones antes de finalizar.
        if (m_transport) {
            m_transport->stop();
        } else {
            onTransportStopped();
        }
        qCDebug(novaRecord) << "Grabación detenida limpiamente (STOP).";
        return;
    }

    // === INICIAR GRABACIÓN REAL ===
    bool anyArmed = false;
    d_ptr->armedTrackIndices.clear();

    if (auto routes = d_ptr->session->get_routes()) {
        int trackIdx = 0;
        for (const auto &route : *routes) {
            if (!route || !route->is_track()) {
                continue;
            }
            auto track = std::dynamic_pointer_cast<ARDOUR::Track>(route);
            if (!track) {
                continue;
            }
            auto rec = track->rec_enable_control();
            if (rec && rec->get_value() != 0.0) {
                anyArmed = true;
                d_ptr->armedTrackIndices.push_back(trackIdx);
                qCDebug(novaRecord) << "Pista armada detectada:" << QString::fromStdString(track->name()) << "índice:" << trackIdx;
            }
            trackIdx++;
        }

        // Auto-arm primera pista si ninguna armada
        if (!anyArmed) {
            trackIdx = 0;
            for (const auto &route : *routes) {
                if (!route || !route->is_track()) {
                    continue;
                }
                auto track = std::dynamic_pointer_cast<ARDOUR::Track>(route);
                if (!track) {
                    continue;
                }
                auto rec = track->rec_enable_control();
                if (rec) {
                    rec->set_value(1.0, PBD::Controllable::NoGroup);  // 1.0 = armed
                    anyArmed = true;
                    d_ptr->armedTrackIndices.push_back(trackIdx);
                    d_ptr->session->set_dirty();
                    qCDebug(novaRecord) << "Auto-arm en pista:" << QString::fromStdString(track->name()) << "índice:" << trackIdx;
                    break;
                }
                trackIdx++;
            }
        }

        // Si no había ninguna pista en la sesión, auto-crear una pista de audio
        if (!anyArmed && trackIdx == 0) {
            try {
                auto newTracks = d_ptr->session->new_audio_track(
                    2, 2, nullptr, 1, std::string("Audio 1"),
                    ARDOUR::PresentationInfo::max_order, ARDOUR::Normal, true, false
                );
                if (!newTracks.empty() && newTracks.front()) {
                    auto rec = newTracks.front()->rec_enable_control();
                    if (rec) {
                        rec->set_value(1.0, PBD::Controllable::NoGroup);
                    }
                    anyArmed = true;
                    d_ptr->armedTrackIndices.push_back(0);
                    d_ptr->session->set_dirty();
                    qCDebug(novaRecord) << "Auto-creada y armada pista Audio 1 para grabación.";
                }
            } catch (const std::exception &e) {
                qCWarning(novaRecord) << "Error al auto-crear pista de audio:" << e.what();
            }
        }
    }

    if (!anyArmed) {
        qCWarning(novaRecord) << "No hay pistas disponibles para grabar.";
        return;
    }

    // Guardar beat de inicio para crear región visual
    m_recordStartBeat = m_transport ? m_transport->currentBeat() : 0.0;
    m_liveRegionActive = false;
    d_ptr->captureFinished = false;

    // Habilitar grabación en la sesión (esto activa DiskWriter en pistas armadas)
    d_ptr->session->maybe_enable_record();
    m_isRecording = true;

    if (m_transport) {
        m_transport->play();
    }

    Q_EMIT isRecordingChanged();
    qCDebug(novaRecord) << "Grabación REAL iniciada en" << d_ptr->armedTrackIndices.size() << "pista(s), beat inicio:" << m_recordStartBeat;
#endif
}

void NovaRecordManager::onPositionChanged()
{
    if (!d_ptr->session || !m_isRecording || !m_transport) return;
    if (!m_transport->isPlaying()) return;

    double currentBeat = m_transport->currentBeat();

    if (!m_liveRegionActive) {
        m_recordStartBeat = currentBeat;
        m_liveRegionActive = true;

        if (m_regionModel) {
            m_regionModel->createLiveRecordingClip(m_recordStartBeat);
            qCDebug(novaRecord) << "Clip visual de grabación creado en beat:" << m_recordStartBeat;
        }
    } else if (m_regionModel) {
        double lengthBeats = (std::max)(0.0, currentBeat - m_recordStartBeat);
        m_regionModel->updateLiveRecordingClip(lengthBeats);
    }
}

void NovaRecordManager::onTransportStopped()
{
    if (!m_liveRegionActive && !m_isRecording) return;

    qCDebug(novaRecord) << "Transporte detenido. Finalizando grabación...";
    m_liveRegionActive = false;
    m_isRecording = false;
    Q_EMIT isRecordingChanged();

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    // Asegurar que Ardour deshabilite grabación
    if (d_ptr->session && d_ptr->session->get_record_enabled()) {
        d_ptr->session->disable_record(false, false);  // RecordAction::Stop, RecordAction::PunchOut
    }
    
    // Esperar un poco a que Ardour termine de escribir a disco y cree las regiones en las playlists
    QTimer::singleShot(600, this, [this]() {
        finalizeRecording();
    });
#else
    if (m_regionModel) {
        m_regionModel->finalizeLiveRecordingClip();
    }
#endif
}

void NovaRecordManager::finalizeRecording()
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    qCDebug(novaRecord) << "Finalizando grabación - verificando regiones creadas por Ardour...";
    
    // Forzar rebuild de regiones desde la sesión (Ardour ya debería haber creado las regiones en las playlists)
    if (m_regionModel) {
        m_regionModel->setSession(d_ptr->session); // Esto llama rebuildRegionCache() internamente
        qCDebug(novaRecord) << "Regiones totales tras rebuild:" << m_regionModel->rowCount();
    }
    
    d_ptr->armedTrackIndices.clear();
    d_ptr->captureFinished = false;
    d_ptr->wasRecording = false;
#endif

    if (m_regionModel) {
        m_regionModel->finalizeLiveRecordingClip();
    }

    Q_EMIT isRecordingChanged();
    qCDebug(novaRecord) << "Grabación finalizada completamente.";
}