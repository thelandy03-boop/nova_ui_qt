#include "NovaSessionManager.h"
#include "NovaLogging.h"
#include "NovaProjectManager.h"
#include "platform/NovaPlatformUtils.h"
#include "platform/NovaAndroidStubs.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <thread>
#include <chrono>
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

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/audio_backend.h"
#include "ardour/session.h"
#include "ardour/session_event.h"
#include "ardour/plugin_manager.h"
#include "ardour/rc_configuration.h"
#include "ardour/transport_master_manager.h"
#include "pbd/stateful.h"

#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#pragma pop_macro("foreach")
#endif

// PIMPL implementation - class definition OUTSIDE macro protection
class NovaSessionManagerPrivate {
public:
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    ARDOUR::AudioEngine *engine = nullptr;
    ARDOUR::Session *session = nullptr;
#else
    void *engine = nullptr;
    void *session = nullptr;
#endif
};

NovaSessionManager::NovaSessionManager(QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<NovaSessionManagerPrivate>())
{
}

NovaSessionManager::~NovaSessionManager()
{
    closeCurrentSession();
}

bool NovaSessionManager::initSession()
{
    if (d_ptr->engine) return true;

    qCDebug(novaCore) << "Inicializando subsistemas PBD y Ardour Core...";

    try {
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
        // ARDOUR headers already included at top of file

        if (!ARDOUR::init(true, nullptr, true)) {
            qCCritical(novaCore) << "Error al inicializar subsistemas globales ARDOUR::init";
            return false;
        }

        // 🛡️ EVITAR CRASH DE ARRANQUE (use-after-free del TransportMaster)
        //
        // AudioEngine::start() llama a ARDOUR::init_post_engine() ->
        // TransportMasterManager::restart(). Si la configuracion de Ardour no
        // guarda el estado de "TransportMasters" (este proyecto no crea
        // ~/.config/ardour/ardour.conf), restart() cae en la rama
        // set_default_configuration() -> clear(), que DESTRUYE todos los
        // TransportMaster. Para entonces el backend JACK ya ha pedido conectar
        // sus puertos y el hilo de JACK puede estar dentro de
        // TransportMaster::connection_handler() sobre un objeto ya liberado:
        // vptr corrupto -> salto a direccion invalida -> SIGSEGV al arrancar.
        //
        // Publicamos el estado actual de los transport masters en la
        // configuracion en memoria: asi restart() usa la rama que NO destruye
        // nada y los objetos sobreviven a los callbacks del hilo de audio.
        if (ARDOUR::Config && !ARDOUR::Config->transport_master_state()) {
            XMLNode &tm_state = ARDOUR::TransportMasterManager::instance().get_state ();
            XMLNode *root = new XMLNode ("Ardour");
            root->add_child_nocopy (tm_state);
            ARDOUR::Config->set_state (*root, PBD::Stateful::current_state_version);
            delete root;
            qCDebug(novaCore) << "🛡️ Estado de transport masters publicado en memoria (evita clear() destructivo al arrancar).";
        }

        if (!ARDOUR::SessionEvent::has_per_thread_pool()) {
            ARDOUR::SessionEvent::create_per_thread_pool("GUI", 4096);
        }

        ARDOUR::PluginManager::instance().refresh(true);

        if (!d_ptr->engine) {
            d_ptr->engine = ARDOUR::AudioEngine::create();
        }

        if (!d_ptr->engine) {
            qCCritical(novaCore) << "Error fatal: No se pudo instanciar ARDOUR::AudioEngine.";
            return false;
        }

        d_ptr->engine->set_backend("JACK/Pipewire", "NOVA STUDIO", "");

        int retries = 0;
        while (!d_ptr->engine->current_backend() && retries < 5) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            d_ptr->engine->set_backend("JACK/Pipewire", "NOVA STUDIO", "");
            retries++;
        }

        auto backend = d_ptr->engine->current_backend();
        if (!backend) {
            qCCritical(novaCore) << "❌ Error fatal: El backend de audio JACK/Pipewire no respondió.";
            return false;
        }

        backend->set_sample_rate(48000.0f);
        backend->set_buffer_size(1024);

        // 🔍 Verificación: los transport masters deben SOBREVIVIR al arranque.
        // Si engine->start() los destruye y recrea (clear()), el hilo de JACK
        // puede estar usando el objeto liberado -> use-after-free -> crash.
        const size_t tm_before = ARDOUR::TransportMasterManager::instance().transport_masters().size();
        const std::shared_ptr<ARDOUR::TransportMaster> tm_current_before =
            ARDOUR::TransportMasterManager::instance().current();

        if (d_ptr->engine->start() != 0) {
            qCCritical(novaCore) << "Error al arrancar el motor de audio.";
            return false;
        }

        const size_t tm_after = ARDOUR::TransportMasterManager::instance().transport_masters().size();
        const bool tm_intactos = (tm_current_before ==
                                   ARDOUR::TransportMasterManager::instance().current());

        qCDebug(novaCore) << "🔊 Conectado exitosamente al servidor PipeWire-JACK!"
                          << "| transport masters:" << tm_before << "->" << tm_after
                          << "| mismo objeto:" << (tm_intactos ? "sí" : "NO (hubo clear() → riesgo de crash)");
#else
        qCDebug(novaCore) << "📱 Inicializando subsistema C++ para Android/Windows...";
        if (!d_ptr->engine) {
            d_ptr->engine = ARDOUR::AudioEngine::create();
        }
#endif
        m_engine = d_ptr->engine;
        return true;

    } catch (const std::exception &e) {
        qCCritical(novaCore) << "Excepción al inicializar el gestor de sesiones:" << e.what();
    }

    return false;
}

void NovaSessionManager::closeCurrentSession()
{
    if (d_ptr->session) {
        qCDebug(novaCore) << "🚪 Cerrando sesión activa de Ardour...";
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
        delete static_cast<ARDOUR::Session*>(d_ptr->session);
#endif
        d_ptr->session = nullptr;
        m_session = nullptr;
    }
}

void NovaSessionManager::shutdownAudioEngine()
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    if (d_ptr->engine) {
        qCDebug(novaCore) << "🛑 Deteniendo motor de audio Ardour...";
        static_cast<ARDOUR::AudioEngine*>(d_ptr->engine)->stop();
        delete static_cast<ARDOUR::AudioEngine*>(d_ptr->engine);
        d_ptr->engine = nullptr;
        m_engine = nullptr;
    }
    // ARDOUR::cleanup() may not exist - comment out for now
    // ARDOUR::cleanup();
#endif
}