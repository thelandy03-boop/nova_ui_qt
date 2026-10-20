#include "NovaTrackListModel.h"
#include "core/NovaLogging.h"
#include "core/NovaAudioUtils.h"
#include "core/platform/NovaAndroidStubs.h"

#include <QMetaObject>

#include <cmath>
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
#include "ardour/route.h"
#include "ardour/track.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/gain_control.h"
#include "ardour/mute_control.h"
#include "ardour/solo_control.h"
#include "ardour/automation_control.h"
#include "ardour/presentation_info.h"
#include "ardour/types.h"
#include "ardour/meter.h"
#include "ardour/chan_count.h"
#include "pbd/signals.h"

#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#pragma pop_macro("foreach")
#endif

// PIMPL implementation - class definition OUTSIDE macro protection
class NovaTrackListModelPrivate {
public:
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    ARDOUR::Session *session = nullptr;
    std::vector<std::shared_ptr<ARDOUR::Route>> routes;
    PBD::ScopedConnectionList sessionConnections;
    // Conexiones a las senales Changed de los controles de cada pista.
    // Se reconstruyen en cada syncWithArdour()/removeTrack().
    PBD::ScopedConnectionList controlConnections;
#else
    void *session = nullptr;
    std::vector<std::shared_ptr<ARDOUR::Route>> routes; // stub types on Android/Win
#endif
};

NovaTrackListModel::NovaTrackListModel(QObject *parent)
    : QAbstractListModel(parent)
    , d_ptr(std::make_unique<NovaTrackListModelPrivate>())
{
}

NovaTrackListModel::~NovaTrackListModel()
{
}

void NovaTrackListModel::setSession(void *session)
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    if (d_ptr->session == session) return;

    d_ptr->session = session;

    if (d_ptr->session) {
        syncWithArdour();
    } else {
        beginResetModel();
        d_ptr->routes.clear();
        endResetModel();
        Q_EMIT trackCountChanged(0);
    }
#else
    ARDOUR::Session *ardourSession = static_cast<ARDOUR::Session*>(session);
    
    if (d_ptr->session == ardourSession) return;

    d_ptr->sessionConnections.drop_connections();
    d_ptr->session = ardourSession;

    if (d_ptr->session) {
        // Las pistas pueden entrar en la sesion por muchos caminos (plantilla
        // del proyecto, carga de sesion, plugins, atajos...). El modelo solo se
        // resincronizaba desde addAudioTrack()/removeTrack(), asi que el
        // panel TCP se quedaba VACIO al abrir un proyecto y solo aparecia
        // al pulsar "+ Add Track". Con esta conexion a Session::RouteAdded
        // el panel se actualiza en cuanto la sesion gana una pista.
        ardourSession->RouteAdded.connect_same_thread(
            d_ptr->sessionConnections,
            [this](ARDOUR::RouteList &) { syncWithArdour(); });

        syncWithArdour();
    } else {
        beginResetModel();
        d_ptr->routes.clear();
        endResetModel();
        Q_EMIT trackCountChanged(0);
    }
#endif
}

void NovaTrackListModel::syncWithArdour()
{
    if (!d_ptr->session) return;

    beginResetModel();
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    // Las conexiones apuntan a pistas del vector anterior: soltarlas antes
    // de vaciarlo para no reaccionar a controles ya no listados.
    d_ptr->controlConnections.drop_connections();
#endif
    d_ptr->routes.clear();

#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    auto *stubSession = static_cast<ARDOUR::Session*>(d_ptr->session);
    auto routeList = stubSession->get_routes();
#else
    auto routeList = d_ptr->session->get_routes();
#endif
    if (routeList) {
        for (const auto &route : *routeList) {
            if (route && route->is_track()) {
                d_ptr->routes.push_back(route);
            }
        }
    }
    endResetModel();

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    connectControlSignals();
#endif

    Q_EMIT trackCountChanged(static_cast<int>(d_ptr->routes.size()));
}

void NovaTrackListModel::connectControlSignals()
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    // Mute, Solo y RecEnable son controles marcados con Controllable::RealTime
    // (ver mute_control.cc / solo_control.cc / record_enable_control.cc).
    // En AutomationControl::set_value() -> check_rt() eso significa que el
    // cambio NO se aplica: se encola para el hilo de audio y la funcion vuelve.
    // Consecuencia: el dataChanged que emitimos justo despues de set_value()
    // vuelve a leer el valor VIEJO, el binding de la tarjeta no cambia, y el
    // usuario percibe que hay que hacer "doble clic" (en realidad el primer
    // clic si cambia el audio; solo falla el refresco visual).
    //
    // Solucion: escuchar Changed, que se emite cuando el hilo de audio aplica
    // el valor de verdad, y reemitir dataChanged desde el hilo GUI.
    d_ptr->controlConnections.drop_connections();

    for (const auto &route : d_ptr->routes) {
        if (!route) continue;

        std::weak_ptr<ARDOUR::Route> weakRoute = route;

        auto connectControl = [this, weakRoute](const std::shared_ptr<ARDOUR::AutomationControl> &control,
                                                int role) {
            if (!control) return;

            // Changed puede emitirse desde el hilo de audio: solo encolamos
            // trabajo y tocamos el modelo en el hilo GUI.
            control->Changed.connect_same_thread(
                d_ptr->controlConnections,
                [this, weakRoute, role](bool, PBD::Controllable::GroupControlDisposition) {
                    QMetaObject::invokeMethod(this, [this, weakRoute, role]() {
                        const auto route = weakRoute.lock();
                        if (!route) return;
                        for (size_t row = 0; row < d_ptr->routes.size(); ++row) {
                            if (d_ptr->routes[row] == route) {
                                Q_EMIT dataChanged(index(static_cast<int>(row)),
                                                   index(static_cast<int>(row)),
                                                   QList<int>{role});
                                return;
                            }
                        }
                    }, Qt::QueuedConnection);
                });
        };

        connectControl(route->mute_control(), MuteRole);
        connectControl(route->solo_control(), SoloRole);

        if (auto track = std::dynamic_pointer_cast<ARDOUR::Track>(route)) {
            connectControl(track->rec_enable_control(), RecEnableRole);
        }
    }
#endif
}

int NovaTrackListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(d_ptr->routes.size());
}

QVariant NovaTrackListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(d_ptr->routes.size()))
        return {};

    const auto &route = d_ptr->routes[static_cast<size_t>(index.row())];
    if (!route) return {};

    switch (role) {
    case TrackIdRole:
        return QString::number(index.row() + 1);

    case TrackNameRole:
        return QString::fromStdString(route->name());

    case TrackTypeRole: {
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
        auto midiTrk = std::dynamic_pointer_cast<ARDOUR::MidiTrack>(route);
        return midiTrk ? QStringLiteral("midi") : QStringLiteral("audio");
#else
        return QStringLiteral("audio");
#endif
    }

    case GainRole: {
        auto gc = route->gain_control();
        if (gc) {
            float coeff = static_cast<float>(gc->get_value());
            return NovaAudioUtils::coeffToDb(coeff);
        }
        return 0.0f;
    }

    case MuteRole: {
        auto mc = route->mute_control();
        if (mc) {
            return mc->muted();
        }
        return false;
    }

    case SoloRole: {
        auto sc = route->solo_control();
        if (sc) {
            return sc->self_soloed();
        }
        return false;
    }

    case RecEnableRole: {
        auto track = std::dynamic_pointer_cast<ARDOUR::Track>(route);
        if (track) {
            auto rc = track->rec_enable_control();
            if (rc) {
                return (rc->get_value() > 0.5);
            }
        }
        return false;
    }

    case PanRole: {
        auto pc = route->pan_azimuth_control();
        if (pc) {
            float azimuth = static_cast<float>(pc->get_value());
            return (azimuth * 2.0f) - 1.0f;
        }
        return 0.0f;
    }

    case ColorRole:
        return QStringLiteral("#3498db");

    case PeakRole:
        return peakLevel(index.row());
    default:
        return {};
    }
}

QHash<int, QByteArray> NovaTrackListModel::roleNames() const
{
    return {
        { TrackIdRole,     "trackId"     },
        { TrackNameRole,   "trackName"   },
        { TrackTypeRole,   "trackType"   },
        { GainRole,        "gain"        },
        { MuteRole,        "mute"        },
        { SoloRole,        "solo"        },
        { RecEnableRole,   "recEnable"   },
        { PanRole,         "pan"         },
        { ColorRole,       "trackColor"  },
        { PeakRole,        "trackPeak"   }
    };
}

void NovaTrackListModel::addAudioTrack(const QString &name)
{
    if (!d_ptr->session) return;
    qCDebug(novaModel) << "➕ Creando nueva pista de audio nativa:" << name;

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    try {
        d_ptr->session->new_audio_track(
            2,
            2,
            std::shared_ptr<ARDOUR::RouteGroup>(),
            1,
            name.toStdString(),
            ARDOUR::PresentationInfo::max_order,
            ARDOUR::Normal
        );
        d_ptr->session->set_dirty();
    } catch (const std::exception &e) {
        qCWarning(novaModel) << "❌ Error crítico al instanciar pista de audio nativa:" << e.what();
        Q_EMIT trackAddFailed(QString::fromStdString(e.what()));
        return;
    }
#else
    auto *stubSession = static_cast<ARDOUR::Session*>(d_ptr->session);
    stubSession->new_audio_track(
        2, 2, std::shared_ptr<ARDOUR::RouteGroup>(), 1, name.toStdString(), ARDOUR::PresentationInfo::max_order, ARDOUR::Normal
    );
    stubSession->set_dirty();
#endif

    // La vista se sincroniza con un reset completo del modelo.
    // No se envuelve en beginInsertRows/endInsertRows: syncWithArdour() ya
    // emite beginResetModel/endResetModel, y anidar un reset dentro de una
    // insercion viola el contrato de QAbstractItemModel. El repetidor de QML
    // creaba un delegate de mas por cada pista anadida (N+1 delegates para N
    // filas), duplicando la ultima tarjeta en pantalla.
    syncWithArdour();
}

void NovaTrackListModel::addMidiTrack(const QString &name)
{
    if (!d_ptr->session) return;
    qCDebug(novaModel) << "➕ Creando nueva pista MIDI nativa:" << name;

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    try {
        ARDOUR::DataType midiType(ARDOUR::DataType::MIDI);
        ARDOUR::DataType audioType(ARDOUR::DataType::AUDIO);
        ARDOUR::ChanCount input(midiType, 1);
        ARDOUR::ChanCount output(audioType, 2);

        d_ptr->session->new_midi_track(
            input,
            output,
            false,
            std::shared_ptr<ARDOUR::PluginInfo>(),
            nullptr,
            std::shared_ptr<ARDOUR::RouteGroup>(),
            1,
            name.toStdString(),
            ARDOUR::PresentationInfo::max_order,
            ARDOUR::Normal,
            true,
            false
        );
        d_ptr->session->set_dirty();
    } catch (const std::exception &e) {
        qCWarning(novaModel) << "❌ Error crítico al instanciar pista MIDI nativa:" << e.what();
        Q_EMIT trackAddFailed(QString::fromStdString(e.what()));
    }
#else
    static_cast<ARDOUR::Session*>(d_ptr->session)->set_dirty();
#endif

    syncWithArdour();
}

void NovaTrackListModel::removeTrack(int row)
{
    if (row < 0 || row >= static_cast<int>(d_ptr->routes.size()) || !d_ptr->session) return;

    auto routeToRemove = d_ptr->routes[static_cast<size_t>(row)];
    if (!routeToRemove) return;

    qCDebug(novaModel) << "🗑️ Eliminando pista nativa en fila:" << row << "Nombre:" << QString::fromStdString(routeToRemove->name());

    beginRemoveRows(QModelIndex(), row, row);
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    auto *stubSession = static_cast<ARDOUR::Session*>(d_ptr->session);
    stubSession->remove_route(routeToRemove);
    stubSession->set_dirty();
#else
    d_ptr->session->remove_route(routeToRemove);
    d_ptr->session->set_dirty();
#endif
    d_ptr->routes.erase(d_ptr->routes.begin() + row);
    endRemoveRows();

    // Reconstruir conexiones: la pista eliminada ya no debe notificar.
    connectControlSignals();

    Q_EMIT trackCountChanged(static_cast<int>(d_ptr->routes.size()));
}

void NovaTrackListModel::setGain(int row, float dB)
{
    if (row < 0 || row >= static_cast<int>(d_ptr->routes.size()) || !d_ptr->session) return;

    const auto &route = d_ptr->routes[static_cast<size_t>(row)];
    if (route) {
        auto gc = route->gain_control();
        if (gc) {
            double coeff = NovaAudioUtils::dbToCoeff(dB);
            gc->set_value(coeff, PBD::Controllable::NoGroup);
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
            static_cast<ARDOUR::Session*>(d_ptr->session)->set_dirty();
#else
            d_ptr->session->set_dirty();
#endif
        }
    }
    Q_EMIT dataChanged(index(row), index(row), QList<int>{GainRole});
}

void NovaTrackListModel::setMute(int row, bool muted)
{
    if (row < 0 || row >= static_cast<int>(d_ptr->routes.size()) || !d_ptr->session) return;

    const auto &route = d_ptr->routes[static_cast<size_t>(row)];
    if (route) {
        auto mc = route->mute_control();
        if (mc) {
            mc->set_value(muted ? 1.0 : 0.0, PBD::Controllable::NoGroup);
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
            static_cast<ARDOUR::Session*>(d_ptr->session)->set_dirty();
#else
            d_ptr->session->set_dirty();
#endif
        }
    }
    // Refresco inmediato (util sobre todo si el control no esta en modo
    // RealTime). Para mute/solo/rec el refresco definitivo llega desde
    // connectControlSignals() cuando el hilo de audio aplica el cambio.
    Q_EMIT dataChanged(index(row), index(row), QList<int>{MuteRole});
}

void NovaTrackListModel::setSolo(int row, bool soloed)
{
    if (row < 0 || row >= static_cast<int>(d_ptr->routes.size()) || !d_ptr->session) return;

    const auto &route = d_ptr->routes[static_cast<size_t>(row)];
    if (route) {
        auto sc = route->solo_control();
        if (sc) {
            sc->set_value(soloed ? 1.0 : 0.0, PBD::Controllable::NoGroup);
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
            static_cast<ARDOUR::Session*>(d_ptr->session)->set_dirty();
#else
            d_ptr->session->set_dirty();
#endif
        }
    }
    Q_EMIT dataChanged(index(row), index(row), QList<int>{SoloRole});
}

void NovaTrackListModel::setRecEnable(int row, bool enabled)
{
    if (row < 0 || row >= static_cast<int>(d_ptr->routes.size()) || !d_ptr->session) return;

    const auto &route = d_ptr->routes[static_cast<size_t>(row)];
    if (route) {
        auto track = std::dynamic_pointer_cast<ARDOUR::Track>(route);
        if (track) {
            auto rc = track->rec_enable_control();
            if (rc) {
                rc->set_value(enabled ? 1.0 : 0.0, PBD::Controllable::NoGroup);
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
                static_cast<ARDOUR::Session*>(d_ptr->session)->set_dirty();
#else
                d_ptr->session->set_dirty();
#endif
            }
        }
    }
    Q_EMIT dataChanged(index(row), index(row), QList<int>{RecEnableRole});
}

void NovaTrackListModel::setPan(int row, float pan)
{
    if (row < 0 || row >= static_cast<int>(d_ptr->routes.size()) || !d_ptr->session) return;

    const auto &route = d_ptr->routes[static_cast<size_t>(row)];
    if (route) {
        auto pc = route->pan_azimuth_control();
        if (pc) {
            double azimuth = (pan + 1.0f) / 2.0f;
            pc->set_value(azimuth, PBD::Controllable::NoGroup);
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
            static_cast<ARDOUR::Session*>(d_ptr->session)->set_dirty();
#else
            d_ptr->session->set_dirty();
#endif
        }
    }
    Q_EMIT dataChanged(index(row), index(row), QList<int>{PanRole});
}

float NovaTrackListModel::peakLevel(int row) const
{
    if (row < 0 || row >= static_cast<int>(d_ptr->routes.size()) || !d_ptr->session) return 0.0f;
    return 0.0f;
}