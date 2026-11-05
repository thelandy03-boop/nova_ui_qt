#include "NovaTrackListModel.h"
#include "core/NovaLogging.h"
#include "core/NovaAudioUtils.h"
#include "core/platform/NovaAndroidStubs.h"
#include "audio/dsp/NovaDspChain.h"
#include "audio/dsp/NovaBiquadEq.h"
#include "audio/dsp/NovaCompressor.h"
#include "audio/dsp/NovaStereoDelay.h"
#include "audio/dsp/NovaReverb.h"
#include "audio/dsp/jsfx/NovaJsfxManager.h"

#include <QMetaObject>
#include <QDateTime>

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
#include "ardour/plugin_manager.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin.h"
#include "ardour/processor.h"
#include "pbd/signals.h"

#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#pragma pop_macro("foreach")
#endif

// PIMPL implementation - class definition OUTSIDE macro protection
class NovaTrackListModelPrivate {
public:
    struct PluginStubInfo {
        QString id;
        QString name;
        QString category;
        bool bypassed = false;
        QVariantList parameters;
        std::shared_ptr<NovaDspEffect> dspEffect;
    };
    QMap<int, QList<PluginStubInfo>> stubPlugins;
    QMap<int, std::shared_ptr<NovaDspChain>> trackDspChains;

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

QVariantList NovaTrackListModel::trackPlugins(int trackRow) const
{
    QVariantList result;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (trackRow >= 0 && trackRow < static_cast<int>(d_ptr->routes.size())) {
        auto route = d_ptr->routes[static_cast<size_t>(trackRow)];
        if (route) {
            int pluginIdx = 0;
            route->foreach_processor([&](std::weak_ptr<ARDOUR::Processor> wp) {
                auto proc = wp.lock();
                if (!proc) return;
                auto pi = std::dynamic_pointer_cast<ARDOUR::PluginInsert>(proc);
                if (pi && pi->plugin()) {
                    QVariantMap map;
                    map[QStringLiteral("index")] = pluginIdx++;
                    map[QStringLiteral("name")] = QString::fromStdString(pi->plugin()->name());
                    map[QStringLiteral("uniqueId")] = QString::fromStdString(pi->plugin()->unique_id());
                    map[QStringLiteral("bypassed")] = !pi->enabled();

                    QString nameLower = map[QStringLiteral("name")].toString().toLower();
                    if (nameLower.contains(QStringLiteral("eq")) || nameLower.contains(QStringLiteral("equal"))) {
                        map[QStringLiteral("category")] = QStringLiteral("EQ");
                    } else if (nameLower.contains(QStringLiteral("comp")) || nameLower.contains(QStringLiteral("limit"))) {
                        map[QStringLiteral("category")] = QStringLiteral("DINÁMICA");
                    } else if (nameLower.contains(QStringLiteral("reverb")) || nameLower.contains(QStringLiteral("room"))) {
                        map[QStringLiteral("category")] = QStringLiteral("ESPACIO");
                    } else if (nameLower.contains(QStringLiteral("delay")) || nameLower.contains(QStringLiteral("echo"))) {
                        map[QStringLiteral("category")] = QStringLiteral("DELAY");
                    } else {
                        map[QStringLiteral("category")] = QStringLiteral("EFECTO");
                    }

                    QVariantList params;
                    uint32_t count = pi->plugin()->parameter_count();
                    uint32_t maxParams = std::min<uint32_t>(count, 8);
                    for (uint32_t i = 0; i < maxParams; ++i) {
                        QVariantMap pmap;
                        pmap[QStringLiteral("index")] = static_cast<int>(i);
                        pmap[QStringLiteral("name")] = QString::fromStdString(pi->plugin()->parameter_label(i));
                        pmap[QStringLiteral("value")] = static_cast<double>(pi->plugin()->get_parameter(i));
                        pmap[QStringLiteral("min")] = 0.0;
                        pmap[QStringLiteral("max")] = 1.0;
                        pmap[QStringLiteral("unit")] = QStringLiteral("");
                        params.append(pmap);
                    }
                    map[QStringLiteral("parameters")] = params;
                    result.append(map);
                }
            });
        }
    }
    if (!result.isEmpty()) return result;
#endif

    if (d_ptr->stubPlugins.contains(trackRow)) {
        int idx = 0;
        for (const auto &p : d_ptr->stubPlugins[trackRow]) {
            QVariantMap map;
            map[QStringLiteral("index")] = idx++;
            map[QStringLiteral("name")] = p.name;
            map[QStringLiteral("uniqueId")] = p.id;
            map[QStringLiteral("category")] = p.category;
            map[QStringLiteral("bypassed")] = p.bypassed;
            map[QStringLiteral("parameters")] = p.parameters;
            result.append(map);
        }
    }
    return result;
}

QVariantList NovaTrackListModel::availablePlugins() const
{
    QVariantList list;

    auto addPlug = [&](const QString &id, const QString &name, const QString &cat, const QString &desc) {
        QVariantMap map;
        map[QStringLiteral("id")] = id;
        map[QStringLiteral("name")] = name;
        map[QStringLiteral("category")] = cat;
        map[QStringLiteral("description")] = desc;
        list.append(map);
    };

    addPlug(QStringLiteral("urn:ardour:a-eq"), QStringLiteral("ACE EQ"), QStringLiteral("EQ"),
            QStringLiteral("Ecualizador paramétrico oficial de 4 bandas con filtros paso alto y bajo"));
    addPlug(QStringLiteral("urn:ardour:a-comp"), QStringLiteral("ACE Compressor"), QStringLiteral("DINÁMICA"),
            QStringLiteral("Compresor analógico transparente con control de ataque, relajación y rodilla"));
    addPlug(QStringLiteral("urn:ardour:a-delay"), QStringLiteral("ACE Delay"), QStringLiteral("DELAY"),
            QStringLiteral("Efecto de retardo digital estéreo con sincronización al tempo del proyecto"));
    addPlug(QStringLiteral("urn:ardour:a-reverb"), QStringLiteral("ACE Reverb"), QStringLiteral("ESPACIO"),
            QStringLiteral("Procesador de reverberación espacial de sala y estudio de alta fidelidad"));

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    try {
        ARDOUR::PluginManager &pm = ARDOUR::PluginManager::instance();
        for (const auto &info : pm.lv2_plugin_info()) {
            if (!info) continue;
            QString uri = QString::fromStdString(info->unique_id);
            if (uri.startsWith(QStringLiteral("urn:ardour:"))) continue;
            QString name = QString::fromStdString(info->name);
            QString cat = QString::fromStdString(info->category);
            if (cat.isEmpty()) cat = QStringLiteral("LV2");
            addPlug(uri, name, cat, QString::fromStdString(info->creator));
        }
    } catch (...) {}
#endif

    for (const auto& jsfx : NovaJsfxManager::instance().availableJsfxPlugins()) {
        addPlug(jsfx.id, jsfx.name, jsfx.category, jsfx.description);
    }

    return list;
}

bool NovaTrackListModel::addPluginToTrack(int trackRow, const QString &pluginId)
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (d_ptr->session && trackRow >= 0 && trackRow < static_cast<int>(d_ptr->routes.size())) {
        auto route = d_ptr->routes[static_cast<size_t>(trackRow)];
        if (route) {
            ARDOUR::PluginManager &pm = ARDOUR::PluginManager::instance();
            std::shared_ptr<ARDOUR::PluginInfo> targetInfo;

            for (const auto &info : pm.lv2_plugin_info()) {
                if (info && (info->unique_id == pluginId.toStdString() || info->name == pluginId.toStdString())) {
                    targetInfo = info;
                    break;
                }
            }

            if (targetInfo) {
                ARDOUR::PluginPtr plug = targetInfo->load(*d_ptr->session);
                if (plug) {
                    std::shared_ptr<ARDOUR::PluginInsert> insert(
                        new ARDOUR::PluginInsert(*d_ptr->session, *route, plug));
                    route->add_processor(insert, ARDOUR::PostFader);
                    Q_EMIT trackPluginsChanged(trackRow);
                    return true;
                }
            }
        }
    }
#endif

    NovaTrackListModelPrivate::PluginStubInfo stub;
    stub.id = pluginId;
    std::shared_ptr<NovaDspEffect> effect;

    // 1. Intentar cargar mediante NovaJsfxManager (soporta ACE EQ, ACE Comp, ACE Delay, ACE Reverb y plugins JSFX)
    auto jsfx = NovaJsfxManager::instance().createEffect(pluginId);
    if (jsfx) {
        stub.name = jsfx->name();
        stub.category = jsfx->category();
        for (const auto& sl : jsfx->sliders()) {
            QVariantMap pmap;
            pmap[QStringLiteral("index")] = sl.index;
            pmap[QStringLiteral("name")] = sl.name;
            pmap[QStringLiteral("value")] = sl.defaultValue;
            pmap[QStringLiteral("min")] = sl.minValue;
            pmap[QStringLiteral("max")] = sl.maxValue;
            pmap[QStringLiteral("unit")] = sl.unit;
            stub.parameters.append(pmap);
        }
        effect = jsfx;
    } else if (pluginId.contains(QStringLiteral("a-eq"))) {
        stub.name = QStringLiteral("ACE EQ");
        stub.category = QStringLiteral("EQ");
        stub.parameters = {
            QVariantMap{{QStringLiteral("index"), 0}, {QStringLiteral("name"), QStringLiteral("Graves 80Hz")}, {QStringLiteral("value"), 0.0}, {QStringLiteral("min"), -12.0}, {QStringLiteral("max"), 12.0}, {QStringLiteral("unit"), QStringLiteral("dB")}},
            QVariantMap{{QStringLiteral("index"), 1}, {QStringLiteral("name"), QStringLiteral("Medios 1kHz")}, {QStringLiteral("value"), 0.0}, {QStringLiteral("min"), -12.0}, {QStringLiteral("max"), 12.0}, {QStringLiteral("unit"), QStringLiteral("dB")}},
            QVariantMap{{QStringLiteral("index"), 2}, {QStringLiteral("name"), QStringLiteral("Agudos 10kHz")}, {QStringLiteral("value"), 0.0}, {QStringLiteral("min"), -12.0}, {QStringLiteral("max"), 12.0}, {QStringLiteral("unit"), QStringLiteral("dB")}}
        };
        effect = std::make_shared<NovaBiquadEq>();
    } else if (pluginId.contains(QStringLiteral("a-comp"))) {
        stub.name = QStringLiteral("ACE Compressor");
        stub.category = QStringLiteral("DINÁMICA");
        stub.parameters = {
            QVariantMap{{QStringLiteral("index"), 0}, {QStringLiteral("name"), QStringLiteral("Umbral")}, {QStringLiteral("value"), -12.0}, {QStringLiteral("min"), -40.0}, {QStringLiteral("max"), 0.0}, {QStringLiteral("unit"), QStringLiteral("dB")}},
            QVariantMap{{QStringLiteral("index"), 1}, {QStringLiteral("name"), QStringLiteral("Ratio")}, {QStringLiteral("value"), 3.0}, {QStringLiteral("min"), 1.0}, {QStringLiteral("max"), 20.0}, {QStringLiteral("unit"), QStringLiteral(":1")}},
            QVariantMap{{QStringLiteral("index"), 2}, {QStringLiteral("name"), QStringLiteral("Ganancia")}, {QStringLiteral("value"), 2.0}, {QStringLiteral("min"), 0.0}, {QStringLiteral("max"), 20.0}, {QStringLiteral("unit"), QStringLiteral("dB")}}
        };
        effect = std::make_shared<NovaCompressor>();
    } else if (pluginId.contains(QStringLiteral("a-delay"))) {
        stub.name = QStringLiteral("ACE Delay");
        stub.category = QStringLiteral("DELAY");
        stub.parameters = {
            QVariantMap{{QStringLiteral("index"), 0}, {QStringLiteral("name"), QStringLiteral("Tiempo")}, {QStringLiteral("value"), 250.0}, {QStringLiteral("min"), 10.0}, {QStringLiteral("max"), 1000.0}, {QStringLiteral("unit"), QStringLiteral("ms")}},
            QVariantMap{{QStringLiteral("index"), 1}, {QStringLiteral("name"), QStringLiteral("Feedback")}, {QStringLiteral("value"), 30.0}, {QStringLiteral("min"), 0.0}, {QStringLiteral("max"), 100.0}, {QStringLiteral("unit"), QStringLiteral("%")}},
            QVariantMap{{QStringLiteral("index"), 2}, {QStringLiteral("name"), QStringLiteral("Mezcla")}, {QStringLiteral("value"), 25.0}, {QStringLiteral("min"), 0.0}, {QStringLiteral("max"), 100.0}, {QStringLiteral("unit"), QStringLiteral("%")}}
        };
        effect = std::make_shared<NovaStereoDelay>();
    } else if (pluginId.contains(QStringLiteral("a-reverb"))) {
        stub.name = QStringLiteral("ACE Reverb");
        stub.category = QStringLiteral("ESPACIO");
        stub.parameters = {
            QVariantMap{{QStringLiteral("index"), 0}, {QStringLiteral("name"), QStringLiteral("Tamaño Sala")}, {QStringLiteral("value"), 45.0}, {QStringLiteral("min"), 0.0}, {QStringLiteral("max"), 100.0}, {QStringLiteral("unit"), QStringLiteral("%")}},
            QVariantMap{{QStringLiteral("index"), 1}, {QStringLiteral("name"), QStringLiteral("Mezcla Wet")}, {QStringLiteral("value"), 20.0}, {QStringLiteral("min"), 0.0}, {QStringLiteral("max"), 100.0}, {QStringLiteral("unit"), QStringLiteral("%")}}
        };
        effect = std::make_shared<NovaReverb>();
    } else {
        stub.name = pluginId;
        stub.category = QStringLiteral("EFECTO");
    }

    if (effect) {
        stub.dspEffect = effect;
        if (!d_ptr->trackDspChains.contains(trackRow)) {
            d_ptr->trackDspChains[trackRow] = std::make_shared<NovaDspChain>();
        }
        d_ptr->trackDspChains[trackRow]->addEffect(effect);
    }

    d_ptr->stubPlugins[trackRow].append(stub);
    Q_EMIT trackPluginsChanged(trackRow);
    return true;
}

bool NovaTrackListModel::removePluginFromTrack(int trackRow, int pluginIndex)
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (trackRow >= 0 && trackRow < static_cast<int>(d_ptr->routes.size())) {
        auto route = d_ptr->routes[static_cast<size_t>(trackRow)];
        if (route) {
            int currentIdx = 0;
            std::shared_ptr<ARDOUR::PluginInsert> targetPi;
            route->foreach_processor([&](std::weak_ptr<ARDOUR::Processor> wp) {
                auto proc = wp.lock();
                if (!proc) return;
                auto pi = std::dynamic_pointer_cast<ARDOUR::PluginInsert>(proc);
                if (pi && pi->plugin()) {
                    if (currentIdx == pluginIndex) {
                        targetPi = pi;
                    }
                    currentIdx++;
                }
            });
            if (targetPi) {
                route->remove_processor(targetPi);
                Q_EMIT trackPluginsChanged(trackRow);
                return true;
            }
        }
    }
#endif

    if (d_ptr->stubPlugins.contains(trackRow) &&
        pluginIndex >= 0 && pluginIndex < d_ptr->stubPlugins[trackRow].size()) {
        d_ptr->stubPlugins[trackRow].removeAt(pluginIndex);
        if (d_ptr->trackDspChains.contains(trackRow)) {
            d_ptr->trackDspChains[trackRow]->removeEffect(pluginIndex);
        }
        Q_EMIT trackPluginsChanged(trackRow);
        return true;
    }
    return false;
}

bool NovaTrackListModel::setPluginBypassed(int trackRow, int pluginIndex, bool bypassed)
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (trackRow >= 0 && trackRow < static_cast<int>(d_ptr->routes.size())) {
        auto route = d_ptr->routes[static_cast<size_t>(trackRow)];
        if (route) {
            int currentIdx = 0;
            std::shared_ptr<ARDOUR::PluginInsert> targetPi;
            route->foreach_processor([&](std::weak_ptr<ARDOUR::Processor> wp) {
                auto proc = wp.lock();
                if (!proc) return;
                auto pi = std::dynamic_pointer_cast<ARDOUR::PluginInsert>(proc);
                if (pi && pi->plugin()) {
                    if (currentIdx == pluginIndex) {
                        targetPi = pi;
                    }
                    currentIdx++;
                }
            });
            if (targetPi) {
                targetPi->enable(!bypassed);
                Q_EMIT trackPluginsChanged(trackRow);
                return true;
            }
        }
    }
#endif

    if (d_ptr->stubPlugins.contains(trackRow) &&
        pluginIndex >= 0 && pluginIndex < d_ptr->stubPlugins[trackRow].size()) {
        d_ptr->stubPlugins[trackRow][pluginIndex].bypassed = bypassed;
        if (d_ptr->stubPlugins[trackRow][pluginIndex].dspEffect) {
            d_ptr->stubPlugins[trackRow][pluginIndex].dspEffect->setBypassed(bypassed);
        }
        Q_EMIT trackPluginsChanged(trackRow);
        return true;
    }
    return false;
}

bool NovaTrackListModel::setTrackPluginsBypassed(int trackRow, bool bypassed)
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (trackRow >= 0 && trackRow < static_cast<int>(d_ptr->routes.size())) {
        auto route = d_ptr->routes[static_cast<size_t>(trackRow)];
        if (route) {
            route->foreach_processor([&](std::weak_ptr<ARDOUR::Processor> wp) {
                auto proc = wp.lock();
                if (!proc) return;
                auto pi = std::dynamic_pointer_cast<ARDOUR::PluginInsert>(proc);
                if (pi && pi->plugin()) {
                    pi->enable(!bypassed);
                }
            });
            Q_EMIT trackPluginsChanged(trackRow);
            return true;
        }
    }
#endif

    if (d_ptr->stubPlugins.contains(trackRow)) {
        for (auto &p : d_ptr->stubPlugins[trackRow]) {
            p.bypassed = bypassed;
            if (p.dspEffect) {
                p.dspEffect->setBypassed(bypassed);
            }
        }
        Q_EMIT trackPluginsChanged(trackRow);
        return true;
    }
    return false;
}

bool NovaTrackListModel::isTrackPluginsBypassed(int trackRow) const
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (trackRow >= 0 && trackRow < static_cast<int>(d_ptr->routes.size())) {
        auto route = d_ptr->routes[static_cast<size_t>(trackRow)];
        if (route) {
            bool hasPlugin = false;
            bool allBypassed = true;
            route->foreach_processor([&](std::weak_ptr<ARDOUR::Processor> wp) {
                auto proc = wp.lock();
                if (!proc) return;
                auto pi = std::dynamic_pointer_cast<ARDOUR::PluginInsert>(proc);
                if (pi && pi->plugin()) {
                    hasPlugin = true;
                    if (pi->enabled()) allBypassed = false;
                }
            });
            if (hasPlugin) return allBypassed;
        }
    }
#endif

    if (d_ptr->stubPlugins.contains(trackRow) && !d_ptr->stubPlugins[trackRow].isEmpty()) {
        for (const auto &p : d_ptr->stubPlugins[trackRow]) {
            if (!p.bypassed) return false;
        }
        return true;
    }
    return false;
}

bool NovaTrackListModel::setPluginParameter(int trackRow, int pluginIndex, int paramIndex, float value)
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (trackRow >= 0 && trackRow < static_cast<int>(d_ptr->routes.size())) {
        auto route = d_ptr->routes[static_cast<size_t>(trackRow)];
        if (route) {
            int currentIdx = 0;
            bool ok = false;
            route->foreach_processor([&](std::weak_ptr<ARDOUR::Processor> wp) {
                auto proc = wp.lock();
                if (!proc) return;
                auto pi = std::dynamic_pointer_cast<ARDOUR::PluginInsert>(proc);
                if (pi && pi->plugin()) {
                    if (currentIdx == pluginIndex) {
                        auto ac = pi->automation_control(Evoral::Parameter(ARDOUR::PluginAutomation, 0, paramIndex));
                        if (ac) {
                            ac->set_value(value, PBD::Controllable::NoGroup);
                            ok = true;
                        }
                    }
                    currentIdx++;
                }
            });
            if (ok) return true;
        }
    }
#endif

    if (d_ptr->stubPlugins.contains(trackRow) &&
        pluginIndex >= 0 && pluginIndex < d_ptr->stubPlugins[trackRow].size()) {
        auto &params = d_ptr->stubPlugins[trackRow][pluginIndex].parameters;
        if (paramIndex >= 0 && paramIndex < params.size()) {
            QVariantMap m = params[paramIndex].toMap();
            m[QStringLiteral("value")] = value;
            params[paramIndex] = m;
            if (d_ptr->stubPlugins[trackRow][pluginIndex].dspEffect) {
                d_ptr->stubPlugins[trackRow][pluginIndex].dspEffect->setParameter(paramIndex, value);
            }
            return true;
        }
    }
    return false;
}

NovaDspChain* NovaTrackListModel::dspChainForTrack(int trackRow) const
{
    if (d_ptr->trackDspChains.contains(trackRow)) {
        return d_ptr->trackDspChains[trackRow].get();
    }
    return nullptr;
}

bool NovaTrackListModel::importJsfxScript(const QString &filePath)
{
    QString err;
    if (NovaJsfxManager::instance().registerUserScript(filePath, &err)) {
        return true;
    }
    qCWarning(novaCore) << "Error al importar script JSFX:" << err;
    return false;
}

bool NovaTrackListModel::isJsfxPlugin(int trackRow, int pluginIndex) const
{
    if (d_ptr->stubPlugins.contains(trackRow) &&
        pluginIndex >= 0 && pluginIndex < d_ptr->stubPlugins[trackRow].size()) {
        return true; // Todos los plugins reales de NOVA Studio disponen de código editable
    }
    return false;
}

QString NovaTrackListModel::getPluginScript(int trackRow, int pluginIndex) const
{
    if (d_ptr->stubPlugins.contains(trackRow) &&
        pluginIndex >= 0 && pluginIndex < d_ptr->stubPlugins[trackRow].size()) {
        const auto &p = d_ptr->stubPlugins[trackRow][pluginIndex];
        if (p.dspEffect) {
            auto jsfx = std::dynamic_pointer_cast<NovaJsfxEffect>(p.dspEffect);
            if (jsfx) {
                return jsfx->scriptSource();
            }
        }
        // Si no es un NovaJsfxEffect directo, obtener el script oficial de NovaJsfxManager
        QString script = NovaJsfxManager::instance().getScriptContent(p.id);
        if (script.isEmpty()) {
            script = NovaJsfxManager::instance().getScriptContent(p.name);
        }
        return script;
    }
    return QString();
}

QString NovaTrackListModel::updatePluginScript(int trackRow, int pluginIndex, const QString &sourceCode)
{
    if (!d_ptr->stubPlugins.contains(trackRow) ||
        pluginIndex < 0 || pluginIndex >= d_ptr->stubPlugins[trackRow].size()) {
        return QStringLiteral("Índice de plugin o pista inválido");
    }

    auto &stub = d_ptr->stubPlugins[trackRow][pluginIndex];
    auto jsfx = std::dynamic_pointer_cast<NovaJsfxEffect>(stub.dspEffect);

    if (jsfx) {
        QString err;
        if (!jsfx->recompile(sourceCode, &err)) {
            return err.isEmpty() ? QStringLiteral("Error de compilación desconocido") : err;
        }
    } else {
        auto newJsfx = std::make_shared<NovaJsfxEffect>();
        QString err;
        if (!newJsfx->loadScript(sourceCode, &err)) {
            return err.isEmpty() ? QStringLiteral("Error de compilación desconocido") : err;
        }
        stub.dspEffect = newJsfx;
        if (d_ptr->trackDspChains.contains(trackRow)) {
            d_ptr->trackDspChains[trackRow]->replaceEffect(pluginIndex, newJsfx);
        }
        jsfx = newJsfx;
    }

    // Actualizar nombre y sliders en los metadatos de la UI
    stub.name = jsfx->name();
    stub.parameters.clear();
    for (const auto& sl : jsfx->sliders()) {
        QVariantMap pmap;
        pmap[QStringLiteral("index")] = sl.index;
        pmap[QStringLiteral("name")] = sl.name;
        pmap[QStringLiteral("value")] = sl.defaultValue;
        pmap[QStringLiteral("min")] = sl.minValue;
        pmap[QStringLiteral("max")] = sl.maxValue;
        pmap[QStringLiteral("unit")] = sl.unit;
        stub.parameters.append(pmap);
    }

    Q_EMIT trackPluginsChanged(trackRow);
    return QString(); // Éxito
}

bool NovaTrackListModel::createCustomJsfxPlugin(int trackRow, const QString &name, const QString &sourceCode)
{
    if (trackRow < 0) {
        if (rowCount() == 0) {
            addAudioTrack(QStringLiteral("Audio 1"));
            trackRow = 0;
        } else {
            trackRow = 0;
        }
    }

    auto jsfx = std::make_shared<NovaJsfxEffect>(sourceCode, name);
    QString err;
    if (!jsfx->recompile(sourceCode, &err)) {
        qCWarning(novaCore) << "Error al compilar nuevo script JSFX:" << err;
        return false;
    }

    NovaTrackListModelPrivate::PluginStubInfo stub;
    stub.id = QStringLiteral("jsfx:user_%1").arg(QDateTime::currentMSecsSinceEpoch());
    stub.name = jsfx->name().isEmpty() ? name : jsfx->name();
    stub.category = QStringLiteral("JSFX");

    for (const auto& sl : jsfx->sliders()) {
        QVariantMap pmap;
        pmap[QStringLiteral("index")] = sl.index;
        pmap[QStringLiteral("name")] = sl.name;
        pmap[QStringLiteral("value")] = sl.defaultValue;
        pmap[QStringLiteral("min")] = sl.minValue;
        pmap[QStringLiteral("max")] = sl.maxValue;
        pmap[QStringLiteral("unit")] = sl.unit;
        stub.parameters.append(pmap);
    }

    stub.dspEffect = jsfx;

    if (!d_ptr->trackDspChains.contains(trackRow)) {
        d_ptr->trackDspChains[trackRow] = std::make_shared<NovaDspChain>();
    }
    d_ptr->trackDspChains[trackRow]->addEffect(jsfx);
    d_ptr->stubPlugins[trackRow].append(stub);

    Q_EMIT trackPluginsChanged(trackRow);
    return true;
}

QVariantList NovaTrackListModel::exportPluginsSnapshot(int trackRow) const
{
    QVariantList resultado;

    const auto it = d_ptr->stubPlugins.constFind(trackRow);
    if (it == d_ptr->stubPlugins.constEnd()) {
        return resultado;
    }

    for (const auto &plugin : *it) {
        QVariantMap mapa;
        mapa[QStringLiteral("id")] = plugin.id;
        mapa[QStringLiteral("name")] = plugin.name;
        mapa[QStringLiteral("category")] = plugin.category;
        mapa[QStringLiteral("bypassed")] = plugin.bypassed;

        QVariantList sliders;
        for (const QVariant &parametro : plugin.parameters) {
            sliders.append(parametro.toMap().value(QStringLiteral("value")).toFloat());
        }
        mapa[QStringLiteral("sliders")] = sliders;

        if (auto jsfx = std::dynamic_pointer_cast<NovaJsfxEffect>(plugin.dspEffect)) {
            mapa[QStringLiteral("script")] = jsfx->scriptSource();
            mapa[QStringLiteral("state")] = jsfx->saveState();
        } else {
            mapa[QStringLiteral("script")] = QString();
            mapa[QStringLiteral("state")] = QByteArray();
        }

        resultado.append(mapa);
    }

    return resultado;
}

bool NovaTrackListModel::restorePluginsSnapshot(int trackRow, const QVariantList &plugins)
{
    if (trackRow < 0) {
        return false;
    }

    // La instantanea sustituye la cadena completa de la pista. Se llama solo
    // sobre pistas nuevas o existentes sin plugins, asi que no se pierde
    // ningun plugin local.
    if (!d_ptr->trackDspChains.contains(trackRow)) {
        d_ptr->trackDspChains[trackRow] = std::make_shared<NovaDspChain>();
    } else {
        d_ptr->trackDspChains[trackRow]->clear();
    }
    d_ptr->stubPlugins[trackRow].clear();

    for (const QVariant &valor : plugins) {
        const QVariantMap mapa = valor.toMap();
        const QString id = mapa.value(QStringLiteral("id")).toString();
        const QString script = mapa.value(QStringLiteral("script")).toString();
        const bool bypassed = mapa.value(QStringLiteral("bypassed")).toBool();
        const QByteArray estado = mapa.value(QStringLiteral("state")).toByteArray();

        const QVariantList sliders = mapa.value(QStringLiteral("sliders")).toList();

        std::shared_ptr<NovaJsfxEffect> jsfx;
        if (!script.isEmpty()) {
            jsfx = std::make_shared<NovaJsfxEffect>();
            QString error;
            if (!jsfx->loadScript(script, &error)) {
                qCWarning(novaCore) << "No se pudo restaurar el plugin JSFX del parche:"
                                    << mapa.value(QStringLiteral("name")).toString()
                                    << error;
                jsfx.reset();
            }
        } else if (!id.isEmpty()) {
            jsfx = NovaJsfxManager::instance().createEffect(id);
        }

        if (!jsfx) {
            qCWarning(novaCore) << "Plugin del parche no disponible:" << id
                                << mapa.value(QStringLiteral("name")).toString();
            continue;
        }

        // Orden de restauracion: sliders primero, luego @init + @serialize
        // (lectura) + @slider dentro de loadState.
        const size_t limite = std::min<size_t>(sliders.size(), jsfx->sliders().size());
        for (size_t i = 0; i < limite; ++i) {
            jsfx->setParameter(static_cast<int>(i), sliders[static_cast<int>(i)].toFloat());
        }
        if (!estado.isEmpty()) {
            jsfx->loadState(estado);
        }
        jsfx->setBypassed(bypassed);

        NovaTrackListModelPrivate::PluginStubInfo stub;
        stub.id = id;
        stub.name = jsfx->name();
        stub.category = mapa.value(QStringLiteral("category")).toString();
        if (stub.category.isEmpty()) {
            stub.category = jsfx->category();
        }
        stub.bypassed = bypassed;
        stub.dspEffect = jsfx;

        for (const auto &sl : jsfx->sliders()) {
            QVariantMap pmap;
            pmap[QStringLiteral("index")] = sl.index;
            pmap[QStringLiteral("name")] = sl.name;
            pmap[QStringLiteral("value")] = jsfx->getParameter(sl.index);
            pmap[QStringLiteral("min")] = sl.minValue;
            pmap[QStringLiteral("max")] = sl.maxValue;
            pmap[QStringLiteral("unit")] = sl.unit;
            stub.parameters.append(pmap);
        }

        d_ptr->trackDspChains[trackRow]->addEffect(jsfx);
        d_ptr->stubPlugins[trackRow].append(stub);
    }

    Q_EMIT trackPluginsChanged(trackRow);
    return true;
}

QString NovaTrackListModel::compileJsfxScript(const QString &sourceCode)
{
    NovaJsfxEffect effect;
    QString err;
    if (!effect.loadScript(sourceCode, &err)) {
        return err.isEmpty() ? QStringLiteral("Error de sintaxis en script JSFX") : err;
    }
    return QString(); // Compilación exitosa sin errores
}

QVariantList NovaTrackListModel::parseJsfxSliders(const QString &sourceCode) const
{
    // Solo cabecera: la UI debe poder pintar los controles mientras se
    // escribe, aunque el cuerpo del script todavia no compile.
    const auto metadata = NovaJsfxEffect::parseMetadata(sourceCode);
    QVariantList list;
    for (const auto &sl : metadata.sliders) {
        QVariantMap map;
        map[QStringLiteral("index")] = sl.index;
        map[QStringLiteral("sliderNum")] = sl.sliderNum;
        map[QStringLiteral("name")] = sl.name;
        map[QStringLiteral("value")] = sl.defaultValue;
        map[QStringLiteral("min")] = sl.minValue;
        map[QStringLiteral("max")] = sl.maxValue;
        map[QStringLiteral("step")] = sl.stepValue;
        map[QStringLiteral("unit")] = sl.unit;
        list.append(map);
    }
    return list;
}

QString NovaTrackListModel::getJsfxDescription(const QString &sourceCode) const
{
    return NovaJsfxEffect::parseMetadata(sourceCode).name;
}