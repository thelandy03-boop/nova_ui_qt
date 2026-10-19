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

        if (d_ptr->engine->start() != 0) {
            qCCritical(novaCore) << "Error al arrancar el motor de audio.";
            return false;
        }

        qCDebug(novaCore) << "🔊 Conectado exitosamente al servidor PipeWire-JACK!";
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