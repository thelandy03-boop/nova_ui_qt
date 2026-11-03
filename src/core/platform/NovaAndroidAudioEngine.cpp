#include "NovaAndroidAudioEngine.h"
#include "audio/dsp/NovaDspChain.h"

#if defined(Q_OS_ANDROID)
#include <oboe/Oboe.h>

#define LOG_TAG "NovaAndroidAudio"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define LOGD(...) 
#define LOGI(...)
#define LOGE(...)
#define LOGW(...)
#endif

// ARDOUR includes for Android (using stubs)
#include "NovaAndroidStubs.h"

#include "core/NovaLogging.h"

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <QDir>

// ============================================================================
// WAV Header (RIFF IEEE Float 32-bit)
// ============================================================================
#pragma pack(push, 1)
struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 3; // IEEE Float
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize;
};
#pragma pack(pop)

// ============================================================================
// RingBuffer Implementation
// ============================================================================

NovaAndroidAudioEngine::RingBuffer::RingBuffer(size_t capacityFrames, int channels)
    : m_buffer(capacityFrames * channels)
    , m_capacityFrames(capacityFrames)
    , m_channels(channels)
{
}

NovaAndroidAudioEngine::RingBuffer::~RingBuffer() = default;

bool NovaAndroidAudioEngine::RingBuffer::write(const float* input, size_t frames) {
    if (frames == 0) return true;
    
    const size_t samples = frames * m_channels;
    size_t available = availableWrite();
    if (samples > available) {
        // Overrun - drop oldest data or return false
        // LOGE removed from realtime path - use atomic flag instead
        m_overrun.store(true, std::memory_order_relaxed);
        return false;
    }

    size_t writePos = m_writePos.load(std::memory_order_relaxed);
    size_t firstChunk = std::min(samples, m_buffer.size() - writePos);
    
    std::copy(input, input + firstChunk, m_buffer.begin() + writePos);
    
    if (firstChunk < samples) {
        std::copy(input + firstChunk, input + samples, m_buffer.begin());
    }
    
    m_writePos.store((writePos + samples) % m_buffer.size(), std::memory_order_release);
    return true;
}

size_t NovaAndroidAudioEngine::RingBuffer::read(float* output, size_t frames) {
    if (frames == 0) return 0;
    
    const size_t samples = frames * m_channels;
    size_t available = availableRead();
    size_t toRead = std::min(samples, available);
    
    if (toRead == 0) {
        m_underrun.store(true, std::memory_order_relaxed);
        return 0;
    }

    size_t readPos = m_readPos.load(std::memory_order_relaxed);
    size_t firstChunk = std::min(toRead, m_buffer.size() - readPos);
    
    std::copy(m_buffer.begin() + readPos, m_buffer.begin() + readPos + firstChunk, output);
    
    if (firstChunk < toRead) {
        std::copy(m_buffer.begin(), m_buffer.begin() + (toRead - firstChunk), output + firstChunk);
    }
    
    m_readPos.store((readPos + toRead) % m_buffer.size(), std::memory_order_release);
    return toRead / m_channels;
}

size_t NovaAndroidAudioEngine::RingBuffer::availableRead() const {
    size_t writePos = m_writePos.load(std::memory_order_acquire);
    size_t readPos = m_readPos.load(std::memory_order_relaxed);
    
    if (writePos >= readPos) {
        return writePos - readPos;
    } else {
        return m_buffer.size() - (readPos - writePos);
    }
}

size_t NovaAndroidAudioEngine::RingBuffer::availableWrite() const {
    return m_buffer.size() - availableRead() - 1; // -1 to distinguish full/empty
}

void NovaAndroidAudioEngine::RingBuffer::clear() {
    m_writePos.store(0, std::memory_order_release);
    m_readPos.store(0, std::memory_order_release);
    std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
}

// ============================================================================
// NovaAndroidAudioEngine Implementation
// ============================================================================

NovaAndroidAudioEngine::NovaAndroidAudioEngine(QObject* parent)
    : QObject(parent)
{
#if defined(Q_OS_ANDROID)
    crearCallbacks();
#endif
}

void NovaAndroidAudioEngine::crearCallbacks() {
#if defined(Q_OS_ANDROID)
    // Los callbacks se liberan en shutdown(); si el motor se reinicializa
    // hay que volver a crearlos, o el stream se abriría sin callback y
    // saldría mudo.
    if (!m_inputCallback) {
        m_inputCallback = std::make_unique<InputStreamCallback>(this);
    }
    if (!m_outputCallback) {
        m_outputCallback = std::make_unique<OutputStreamCallback>(this);
    }
#endif
}

NovaAndroidAudioEngine::~NovaAndroidAudioEngine() {
    shutdown();
}

bool NovaAndroidAudioEngine::initialize(int sampleRate, int framesPerBurst) {
#if defined(Q_OS_ANDROID)
    m_sampleRate = sampleRate;

    // Si el motor ya estaba inicializado, sus streams pueden estar en
    // marcha: el reproductor no se puede tocar con su callback corriendo,
    // así que primero se cierra la salida.
    closeStreams();

    // El hilo del grabador solo puede haber uno: si el motor ya estaba
    // inicializado, el anterior se para antes de relanzarlo.
    if (m_diskWriterThread.joinable()) {
        m_diskWriterRunning = false;
        m_diskWriterCV.notify_all();
        m_diskWriterThread.join();
    }

    // Los callbacks se liberan en shutdown(); si el motor se reinicializa
    // hay que volver a crearlos, o el stream se abriría sin callback y
    // saldría mudo.
    crearCallbacks();

    // El reproductor entrega el audio a la tasa del stream (48.000 Hz en el
    // móvil), así que remuestrea el fichero si viene a otra tasa. La tasa
    // real (la que dé el dispositivo al abrir el stream) se aplica después,
    // en abrirStreamSalida().
    m_player.setOutputSampleRate(sampleRate);

    // Get optimal frames per burst from Oboe
    if (framesPerBurst == 0) {
        // We'll query this after opening streams
        m_framesPerBurst = 256; // default fallback
    } else {
        m_framesPerBurst = framesPerBurst;
    }

    // Open output stream first (lower latency requirement)
    // El stream se abre con los canales del fichero cargado (si ya hay uno);
    // si no hay fichero, con 2 por defecto.
    QString error;
    if (!abrirStreamSalida(m_canalSalida, &error)) {
        LOGE("No se pudo inicializar la salida de audio: %s", qPrintable(error));
        Q_EMIT audioInitialized(false);
        return false;
    }

    // Si ya hay un fichero cargado (se cargó antes de inicializar), el
    // stream tiene que tener sus canales: si el driver abre el stream con
    // otros, el audio intercalado del fichero no cabría en el buffer del
    // driver.
    const int canalesFichero = m_canalesFichero.load();
    if (canalesFichero != 0 && m_outputStream->getChannelCount() != canalesFichero) {
        const QString errorCanales = QStringLiteral(
            "la salida de audio se ha abierto con %1 canales y el fichero cargado tiene %2")
            .arg(m_outputStream->getChannelCount())
            .arg(canalesFichero);
        LOGE("%s", qPrintable(errorCanales));
        closeStreams();
        Q_EMIT audioInitialized(false);
        return false;
    }

    // ── Entrada de audio (micro): OPCIONAL ───────────────────────────────
    abrirStreamEntrada();

    crearBuffersSalida(m_outputStream->getChannelCount());

    // Start disk writer thread
    m_diskWriterRunning = true;
    m_diskWriterThread = std::thread(&NovaAndroidAudioEngine::diskWriterLoop, this);

    Q_EMIT audioInitialized(true);
    LOGD("Audio engine initialized successfully");
    return true;

#else
    Q_UNUSED(sampleRate);
    Q_UNUSED(framesPerBurst);
    Q_EMIT audioInitialized(false);
    return false;
#endif
}

bool NovaAndroidAudioEngine::abrirStreamSalida(int canales, QString* error) {
#if defined(Q_OS_ANDROID)
    // Note: Oboe builder setters return AudioStreamBuilder* (pointer), hence -> chaining
    oboe::AudioStreamBuilder outputBuilder;
    outputBuilder.setDirection(oboe::Direction::Output);
    outputBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    outputBuilder.setSharingMode(oboe::SharingMode::Exclusive);
    outputBuilder.setFormat(oboe::AudioFormat::Float);
    outputBuilder.setChannelCount(canales);
    outputBuilder.setSampleRate(m_sampleRate);
    outputBuilder.setCallback(m_outputCallback.get());

    std::shared_ptr<oboe::AudioStream> stream;
    const oboe::Result result = outputBuilder.openStream(stream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open output stream: %s", oboe::convertToText(result));
        if (error) {
            *error = QStringLiteral("no se pudo abrir la salida de audio con %1 canales: %2")
                         .arg(canales)
                         .arg(QString::fromUtf8(oboe::convertToText(result)));
        }
        return false;
    }

    m_outputStream = stream;
    // Quedan los canales con los que el driver ha abierto el stream, que
    // pueden no ser los pedidos: son los que gobiernan el buffer del
    // callback y los ring buffers.
    m_canalSalida = m_outputStream->getChannelCount();
    m_framesPerBurst = std::max(m_outputStream->getFramesPerBurst(), 64);

    // La tasa con la que el driver ha abierto el stream es la que manda:
    // puede no ser la que se le ha pedido (44,1 kHz es habitual en muchos
    // móviles). El reproductor tiene que entregar el audio a ESA tasa: si
    // entrega 48 kHz a un HAL que consume 44,1 kHz, el audio suena ~8,8 %
    // lento y más de un semitono grave.
    //
    // INVARIANTE: aquí el stream de salida está parado (todas las aperturas
    // se hacen con el stream cerrado), así que se puede tocar el reproductor
    // sin carrera con su callback.
    const int tasaReal = m_outputStream->getSampleRate();
    m_tasaSalidaReal.store(tasaReal > 0 ? tasaReal : m_sampleRate);
    m_player.setOutputSampleRate(m_tasaSalidaReal.load());

    LOGD("Output stream opened: %d Hz (pedida %d Hz), %d frames/burst, %d channels",
         m_tasaSalidaReal.load(), m_sampleRate, m_framesPerBurst,
         m_outputStream->getChannelCount());

    return true;
#else
    Q_UNUSED(canales);
    Q_UNUSED(error);
    return false;
#endif
}

void NovaAndroidAudioEngine::crearBuffersEntrada(int canales) {
    // INVARIANTE: los ring buffers de entrada solo se rehacen con el stream
    // de entrada PARADO (no arrancado), por el mismo motivo que los de
    // salida (el callback usa un puntero que se escapa del mutex).
    const size_t bufferCapacity = static_cast<size_t>(m_framesPerBurst) * 4; // 4 bursts worth

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_inputBuffers.clear();
    for (int i = 0; i < kMaxTracks; ++i) {
        m_inputBuffers.push_back(std::make_unique<RingBuffer>(bufferCapacity, canales));
    }
}

void NovaAndroidAudioEngine::crearBuffersSalida(int canales) {
    // INVARIANTE: los ring buffers de salida solo se rehacen con el stream de
    // salida PARADO (no arrancado). El callback de salida usa un puntero
    // suelto a un ring buffer que se escapa del mutex: si se rehacen los
    // buffers con el stream en marcha, ese puntero podría quedar colgando
    // (use-after-free). Por eso loadDecoded() cierra el stream antes de
    // llamar a esta función, y lo arranca después.
    const size_t bufferCapacity = static_cast<size_t>(m_framesPerBurst) * 4; // 4 bursts worth

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_outputBuffers.clear();
    for (int i = 0; i < kMaxTracks; ++i) {
        m_outputBuffers.push_back(std::make_unique<RingBuffer>(bufferCapacity, canales));
    }
}

void NovaAndroidAudioEngine::shutdown() {
#if defined(Q_OS_ANDROID)
    stop();
    
    m_diskWriterRunning = false;
    m_diskWriterCV.notify_all();
    if (m_diskWriterThread.joinable()) {
        m_diskWriterThread.join();
    }

    // Oboe exige cerrar los streams ANTES de destruir el motor (y sus
    // callbacks): si no, crash al salir. No reordenar estas tres líneas.
    closeStreams();
    m_outputCallback.reset();
    m_inputCallback.reset();

    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_inputBuffers.clear();
        m_outputBuffers.clear();
    }
    
    m_session = nullptr;

    qCDebug(novaCore) << "🛑 Spike: streams cerrados";
#endif
}

bool NovaAndroidAudioEngine::start() {
#if defined(Q_OS_ANDROID)
    // Solo hace falta la salida de audio: la entrada (micro) es opcional, y
    // sin ella el motor reproduce igual (lo que no puede es grabar).
    if (!m_outputStream) return false;

    // El reproductor no se puede tocar con el stream de salida en marcha: su
    // callback puede estar leyendo el reproductor a la vez (el reproductor no
    // es seguro entre hilos). Por eso el stream se para de forma síncrona
    // ANTES de tocar el reproductor, y se arranca después.
    const oboe::StreamState estado = m_outputStream->getState();
    if (estado == oboe::StreamState::Started || estado == oboe::StreamState::Starting) {
        const oboe::Result parada = m_outputStream->stop();
        if (parada != oboe::Result::OK) {
            LOGE("Failed to stop output stream: %s", oboe::convertToText(parada));
            return false;
        }
    }

    // Si ya hay un fichero cargado, el transporte lo pone en marcha desde el
    // principio: el reproductor y el contador del motor van a la par.
    m_player.reset();
    m_player.play();

    oboe::Result result = m_outputStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start output stream: %s", oboe::convertToText(result));
        return false;
    }

    // La entrada se arranca solo si existe: si no, se sigue sin grabar pero
    // la salida (y el spike) siguen funcionando.
    if (m_inputStream) {
        const oboe::Result arranqueEntrada = m_inputStream->requestStart();
        if (arranqueEntrada != oboe::Result::OK) {
            LOGW("⚠️ No se pudo arrancar la entrada de audio (micro): se sigue sin grabar (%s)",
                 oboe::convertToText(arranqueEntrada));
            m_inputStream.reset();
        }
    }

    m_isPlaying = true;
    m_isRecording = false;
    m_currentFrame = 0;

    Q_EMIT playbackStateChanged(true);
    Q_EMIT recordingStateChanged(false);
    return true;
#else
    return false;
#endif
}

void NovaAndroidAudioEngine::stop() {
#if defined(Q_OS_ANDROID)
    m_isPlaying = false;
    m_isRecording = false;
    m_player.stop();
    
    // stop() para el transporte, no libera el dispositivo: los streams se
    // quedan abiertos (pero parados) para poder volver a arrancar. El
    // dispositivo se cierra en closeStreams(), que es lo que hace shutdown()
    // antes de destruir el motor.
    if (m_outputStream) {
        m_outputStream->requestStop();
    }
    
    if (m_inputStream) {
        m_inputStream->requestStop();
    }
    
    Q_EMIT playbackStateChanged(false);
    Q_EMIT recordingStateChanged(false);
#endif
}

void NovaAndroidAudioEngine::setSession(ARDOUR::Session* session) {
    m_session = session;
}

bool NovaAndroidAudioEngine::abrirStreamEntrada() {
#if defined(Q_OS_ANDROID)
    if (m_inputStream) return true;
    if (!m_inputCallback) return false;

    struct ConfigEntrada {
        oboe::SharingMode modoCompartido;
        oboe::AudioFormat formato;
        int canales;
        int sampleRate;
        const char* descripcion;
    };

    const int tasaPreferida = (m_sampleRate > 0 ? m_sampleRate : 48000);

    // Cascada de resiliencia: desde máxima fidelidad DAW (Exclusive Float Estéreo)
    // hasta compatibilidad universal (Shared I16 Mono con tasa nativa) admitida por cualquier móvil.
    const std::vector<ConfigEntrada> configs = {
        { oboe::SharingMode::Exclusive, oboe::AudioFormat::Float, 2, tasaPreferida, "Exclusive Float 2ch" },
        { oboe::SharingMode::Exclusive, oboe::AudioFormat::Float, 1, tasaPreferida, "Exclusive Float 1ch" },
        { oboe::SharingMode::Exclusive, oboe::AudioFormat::I16,   2, tasaPreferida, "Exclusive I16 2ch" },
        { oboe::SharingMode::Exclusive, oboe::AudioFormat::I16,   1, tasaPreferida, "Exclusive I16 1ch" },
        { oboe::SharingMode::Shared,    oboe::AudioFormat::Float, 2, tasaPreferida, "Shared Float 2ch" },
        { oboe::SharingMode::Shared,    oboe::AudioFormat::Float, 1, tasaPreferida, "Shared Float 1ch" },
        { oboe::SharingMode::Shared,    oboe::AudioFormat::I16,   2, tasaPreferida, "Shared I16 2ch" },
        { oboe::SharingMode::Shared,    oboe::AudioFormat::I16,   1, tasaPreferida, "Shared I16 1ch" },
        // Fallbacks con tasa nativa del hardware (0 = unspecified para que el driver elija su tasa óptima):
        { oboe::SharingMode::Shared,    oboe::AudioFormat::I16,   1, 0,             "Shared I16 Mono Nativo" },
        { oboe::SharingMode::Shared,    oboe::AudioFormat::I16,   2, 0,             "Shared I16 Estéreo Nativo" },
        { oboe::SharingMode::Shared,    oboe::AudioFormat::Float, 1, 0,             "Shared Float Mono Nativo" }
    };

    oboe::Result ultimoResultado = oboe::Result::OK;
    for (const auto &cfg : configs) {
        oboe::AudioStreamBuilder inputBuilder;
        inputBuilder.setDirection(oboe::Direction::Input);
        inputBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        inputBuilder.setSharingMode(cfg.modoCompartido);
        inputBuilder.setFormat(cfg.formato);
        inputBuilder.setChannelCount(cfg.canales);
        if (cfg.sampleRate > 0) {
            inputBuilder.setSampleRate(cfg.sampleRate);
        }
        inputBuilder.setInputPreset(oboe::InputPreset::Generic);
        inputBuilder.setFramesPerCallback(m_framesPerBurst > 0 ? m_framesPerBurst : 256);
        inputBuilder.setCallback(m_inputCallback.get());

        ultimoResultado = inputBuilder.openStream(m_inputStream);
        if (ultimoResultado == oboe::Result::OK && m_inputStream) {
            LOGI("🎙️ [Oboe] Entrada de audio (micro) abierta con éxito (%s): %d Hz, %d canales, formato %s",
                 cfg.descripcion,
                 m_inputStream->getSampleRate(),
                 m_inputStream->getChannelCount(),
                 oboe::convertToText(m_inputStream->getFormat()));
            break;
        } else {
            LOGD("ℹ️ Oboe probando fallback de micro: no soportó %s (%s)",
                 cfg.descripcion, oboe::convertToText(ultimoResultado));
            m_inputStream.reset();
        }
    }

    if (!m_inputStream) {
        LOGW("⚠️ No se pudo abrir la entrada de audio tras probar todas las opciones: %s",
             oboe::convertToText(ultimoResultado));
        return false;
    }

    crearBuffersEntrada(m_inputStream->getChannelCount());
    return true;
#else
    return false;
#endif
}

bool NovaAndroidAudioEngine::openInputStream(int channelCount) {
#if defined(Q_OS_ANDROID)
    return m_inputStream != nullptr && m_inputStream->getChannelCount() >= channelCount;
#else
    Q_UNUSED(channelCount);
    return false;
#endif
}

bool NovaAndroidAudioEngine::openOutputStream(int channelCount) {
#if defined(Q_OS_ANDROID)
    return m_outputStream != nullptr && m_outputStream->getChannelCount() >= channelCount;
#else
    Q_UNUSED(channelCount);
    return false;
#endif
}

void NovaAndroidAudioEngine::closeStreams() {
#if defined(Q_OS_ANDROID)
    if (m_outputStream) {
        m_outputStream->close();
        m_outputStream.reset();
    }
    if (m_inputStream) {
        m_inputStream->close();
        m_inputStream.reset();
    }
#endif
}

void NovaAndroidAudioEngine::setRecording(bool enabled) {
    if (enabled) {
        limpiarPicosEnVivo();
        {
            std::lock_guard<std::mutex> lock(m_livePeaksMutex);
            m_livePeaks.reserve(16384);
        }
        m_ultimaTomaWavPath.clear();
        m_isRecording.store(true);
#if defined(Q_OS_ANDROID)
        if (!m_inputStream) {
            abrirStreamEntrada();
        }
        if (m_inputStream) {
            const oboe::StreamState st = m_inputStream->getState();
            if (st != oboe::StreamState::Started && st != oboe::StreamState::Starting) {
                oboe::Result res = m_inputStream->requestStart();
                LOGI("🎙️ [Oboe] inputStream requestStart resultado: %s", oboe::convertToText(res));
            }
        }
#endif
        {
            std::lock_guard<std::mutex> lock(m_recordMutex);
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            struct tm tmBuf;
            struct tm* tm = localtime_r(&time_t, &tmBuf);
            char timestamp[64];
            strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm);
            m_tomaGrabacionTimestamp = timestamp;
            LOGI("🎙️ [Oboe] Nueva toma iniciada: %s", timestamp);
        }
    } else {
        if (!m_isRecording.load()) return;
        m_isRecording.store(false);
#if defined(Q_OS_ANDROID)
        if (m_inputStream) {
            const oboe::StreamState st = m_inputStream->getState();
            if (st == oboe::StreamState::Started || st == oboe::StreamState::Starting) {
                m_inputStream->requestStop();
                LOGI("🎙️ [Oboe] inputStream detenido tras finalizar grabación");
            }
        }
        // Volcar inmediatamente cualquier buffer restante en m_inputBuffers antes de cerrar la toma
        if (m_session) {
            auto routes = m_session->get_routes();
            if (routes) {
                int trackIdx = 0;
                for (const auto& route : *routes) {
                    if (route && route->is_track()) {
                        writeTrackToDisk(trackIdx);
                        trackIdx++;
                    }
                }
            }
        }
#endif
    }
    Q_EMIT recordingStateChanged(enabled);
}

void NovaAndroidAudioEngine::setPlaying(bool enabled) {
    m_isPlaying.store(enabled);
#if defined(Q_OS_ANDROID)
    if (enabled) {
        m_exhaustionNotified.store(false, std::memory_order_relaxed);
        m_player.play();
        // El stream de salida tiene que estar en marcha para que suene.
        if (m_outputStream) {
            const oboe::StreamState estado = m_outputStream->getState();
            if (estado != oboe::StreamState::Started && estado != oboe::StreamState::Starting) {
                m_outputStream->requestStart();
            }
        }
        // Si estamos en modo grabación, asegurar que el stream de entrada esté corriendo
        if (m_isRecording.load() && m_inputStream) {
            const oboe::StreamState estadoEntrada = m_inputStream->getState();
            if (estadoEntrada != oboe::StreamState::Started && estadoEntrada != oboe::StreamState::Starting) {
                oboe::Result res = m_inputStream->requestStart();
                LOGI("🎙️ [Oboe] inputStream requestStart en setPlaying: %s", oboe::convertToText(res));
            }
        }
    } else {
        m_player.stop();
    }
#endif
    Q_EMIT playbackStateChanged(enabled);
}

void NovaAndroidAudioEngine::setDspChain(NovaDspChain* chain) {
#if defined(Q_OS_ANDROID)
    m_player.setDspChain(chain);
#endif
    m_activeDspChain.store(chain, std::memory_order_release);
}

NovaDspChain* NovaAndroidAudioEngine::dspChain() const {
    return m_activeDspChain.load(std::memory_order_acquire);
}

bool NovaAndroidAudioEngine::tieneFicheroCargado() const {
#if defined(Q_OS_ANDROID)
    // El reproductor guarda los canales del fichero cargado (0 si no hay
    // ninguno): eso dice si hay algo que pueda seguir sonando.
    return m_player.channels() > 0;
#else
    return false;
#endif
}

bool NovaAndroidAudioEngine::loadDecoded(NovaDecodedAudio&& audio, QString* error) {
    m_exhaustionNotified.store(false, std::memory_order_relaxed);
#if defined(Q_OS_ANDROID)
    // El reproductor no es seguro entre hilos (el llamante lo usa desde el
    // hilo de audio), así que el stream de salida se cierra mientras se
    // cambia el audio: mientras está cerrado, su callback no corre y no hay
    // carrera con el hilo de la interfaz.
    const bool habiaStream = (m_outputStream != nullptr);
    const bool estabaEnMarcha =
        habiaStream && (m_outputStream->getState() == oboe::StreamState::Started);
    // Canales con los que estaba abierto el stream, para poder volver a él
    // si algo falla por el camino.
    const int canalesAnteriores = m_canalSalida;

    // Si al restaurar el stream tampoco se puede, el llamante tiene que
    // enterarse de que la salida de audio se ha quedado cerrada.
    const auto anadirAvisoDeSalidaCerrada = [error](const QString& errorRestaurar) {
        if (error && !errorRestaurar.isEmpty()) {
            *error = QStringLiteral("%1. %2").arg(*error).arg(errorRestaurar);
        }
    };

    if (habiaStream) {
        m_outputStream->requestStop();
        m_outputStream->close();
        m_outputStream.reset();
    }

    // Con el stream cerrado se puede tocar el reproductor sin carrera. El
    // audio se mueve: el reproductor es su nuevo dueño (sin copias del PCM).
    if (!m_player.load(std::move(audio), error)) {
        if (habiaStream) {
            QString errorRestaurar;
            if (!restaurarStreamSalida(canalesAnteriores, estabaEnMarcha, &errorRestaurar)) {
                anadirAvisoDeSalidaCerrada(errorRestaurar);
            }
        }
        return false;
    }
    // El reproductor remuestrea a la tasa REAL del stream de salida, no a
    // la pedida: si el dispositivo ha abierto el stream a 44,1 kHz y el
    // reproductor entrega 48 kHz, el audio suena grave y lento. Al reabrir
    // el stream (más abajo) la tasa real se vuelve a aplicar, por si el
    // driver la ha cambiado.
    m_player.setOutputSampleRate(tasaSalidaReal());
    m_canalesFichero = m_player.channels();

    // Un fichero nuevo empieza en el principio: el cabezal de reproducción
    // vuelve a cero con el.
    m_currentFrame = 0;

    // Si el transporte ya está en marcha, el fichero suena desde el
    // principio; si no, espera a que lo pongan en marcha.
    if (m_isPlaying.load()) {
        m_player.play();
    }

    if (habiaStream) {
        // El stream se abre con los canales del fichero: el reproductor
        // escribe las muestras intercaladas con esos canales, y un fichero
        // mono en un stream estéreo sonaría mal.
        if (!abrirStreamSalida(m_player.channels(), error)) {
            // Se deja el motor como estaba, con los canales anteriores. El
            // fichero no sonará (su callback ve que los canales no
            // coinciden), pero el motor sigue vivo.
            QString errorRestaurar;
            if (!restaurarStreamSalida(canalesAnteriores, estabaEnMarcha, &errorRestaurar)) {
                anadirAvisoDeSalidaCerrada(errorRestaurar);
            }
            return false;
        }

        if (m_outputStream->getChannelCount() != m_player.channels()) {
            // El driver ha abierto el stream con otros canales: el audio
            // intercalado del fichero no cabría en el buffer del driver, así
            // que el fichero no se puede reproducir con este stream.
            const QString errorCanales = QStringLiteral(
                "la salida de audio se ha abierto con %1 canales y el fichero tiene %2")
                .arg(m_outputStream->getChannelCount())
                .arg(m_player.channels());
            LOGE("%s", qPrintable(errorCanales));
            if (error) {
                *error = errorCanales;
            }
            QString errorRestaurar;
            if (!restaurarStreamSalida(canalesAnteriores, estabaEnMarcha, &errorRestaurar)) {
                anadirAvisoDeSalidaCerrada(errorRestaurar);
            }
            return false;
        }

        // Los ring buffers de salida se recrean con los canales nuevos.
        crearBuffersSalida(m_outputStream->getChannelCount());

        if (estabaEnMarcha) {
            m_outputStream->requestStart();
        }
    } else {
        // El motor todavía no está inicializado: initialize() abrirá el
        // stream con estos canales.
        m_canalSalida = m_player.channels();
    }

    qCDebug(novaCore) << "🔊 Spike: cargado" << m_player.channels() << "canales,"
                       << m_player.durationSeconds() << "s a"
                       << m_player.outputSampleRate() << "Hz (tasa real del stream)";
    return true;
#else
    Q_UNUSED(audio);
    Q_UNUSED(error);
    return false;
#endif
}

bool NovaAndroidAudioEngine::restaurarStreamSalida(int canales, bool estabaEnMarcha, QString* error) {
#if defined(Q_OS_ANDROID)
    // Vuelve a abrir el stream de salida con los canales que tenía y lo
    // deja como estaba (arrancado o parado).
    QString errorAlRestaurar;
    if (!abrirStreamSalida(canales, &errorAlRestaurar)) {
        LOGE("No se pudo reabrir el stream de salida: %s", qPrintable(errorAlRestaurar));
        if (error) {
            *error = QStringLiteral("no se pudo reabrir la salida de audio con %1 canales: %2. "
                                    "La salida de audio se ha quedado cerrada.")
                         .arg(canales)
                         .arg(errorAlRestaurar);
        }
        // Sin stream de salida el motor no puede arrancar ni sonar: no tiene
        // sentido seguir fingiendo que está en marcha.
        m_isPlaying = false;
        return false;
    }

    crearBuffersSalida(m_outputStream->getChannelCount());

    if (estabaEnMarcha) {
        m_outputStream->requestStart();
    }
    return true;
#else
    Q_UNUSED(canales);
    Q_UNUSED(estabaEnMarcha);
    Q_UNUSED(error);
    return false;
#endif
}

NovaAndroidAudioEngine::RingBuffer* NovaAndroidAudioEngine::getInputBuffer(int trackIndex) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    if (trackIndex >= 0 && trackIndex < static_cast<int>(m_inputBuffers.size())) {
        return m_inputBuffers[trackIndex].get();
    }
    return nullptr;
}

NovaAndroidAudioEngine::RingBuffer* NovaAndroidAudioEngine::getOutputBuffer(int trackIndex) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    if (trackIndex >= 0 && trackIndex < static_cast<int>(m_outputBuffers.size())) {
        return m_outputBuffers[trackIndex].get();
    }
    return nullptr;
}

void NovaAndroidAudioEngine::diskWriterLoop() {
#if defined(Q_OS_ANDROID)
    while (m_diskWriterRunning) {
        std::unique_lock<std::mutex> lock(m_diskWriterMutex);
        m_diskWriterCV.wait_for(lock, std::chrono::milliseconds(100), [this]() {
            return !m_diskWriterRunning;
        });
        
        if (!m_diskWriterRunning) break;
        
        // Write each track's buffer to disk
        if (m_session) {
            auto routes = m_session->get_routes();
            if (routes) {
                int trackIdx = 0;
                for (const auto& route : *routes) {
                    if (route && route->is_track()) {
                        writeTrackToDisk(trackIdx);
                        trackIdx++;
                    }
                }
            }
        }
    }
#endif
}

void NovaAndroidAudioEngine::writeTrackToDisk(int trackIndex) {
#if defined(Q_OS_ANDROID)
    auto* inputBuf = getInputBuffer(trackIndex);
    auto* outputBuf = getOutputBuffer(trackIndex);
    
    if (!inputBuf && !outputBuf) return;
    
    // Monitor buffer levels
    if (inputBuf) {
        size_t avail = inputBuf->availableRead();
        if (avail > inputBuf->capacity() * 0.9) {
            Q_EMIT bufferOverrun();
        }
    }
    if (outputBuf) {
        size_t avail = outputBuf->availableRead();
        if (avail > outputBuf->capacity() * 0.9) {
            Q_EMIT bufferUnderrun();
        }
    }
    
    // Only write input buffers (recorded audio) to disk
    if (!inputBuf) return;
    
    size_t avail = inputBuf->availableRead();
    if (avail == 0) return;
    
    // Get track info from session
    if (!m_session) return;
    auto routes = m_session->get_routes();
    if (!routes) return;
    
    int trackIdx = 0;
    std::shared_ptr<ARDOUR::Track> targetTrack = nullptr;
    for (const auto& route : *routes) {
        if (route && route->is_track()) {
            if (trackIdx == trackIndex) {
                targetTrack = std::dynamic_pointer_cast<ARDOUR::Track>(route);
                break;
            }
            trackIdx++;
        }
    }
    
    if (!targetTrack) return;
    
    // Read available samples from ring buffer
    size_t samplesToRead = avail / inputBuf->channels();
    std::vector<float> buffer(samplesToRead * inputBuf->channels());
    size_t framesRead = inputBuf->read(buffer.data(), samplesToRead);
    
    if (framesRead == 0) return;
    
    // Write to WAV file in project interchange directory
    writeWavFile(targetTrack, buffer.data(), framesRead, inputBuf->channels());
#else
    Q_UNUSED(trackIndex);
#endif
}

void NovaAndroidAudioEngine::writeWavFile(
    std::shared_ptr<ARDOUR::Track> track,
    const float* samples,
    size_t frames,
    int channels) {
#if defined(Q_OS_ANDROID)
    if (!m_session) return;
    
    // Create interchange directory
    std::string sessionPath = m_session->path();
    std::string interchangeDir = sessionPath + "/interchange/" + track->name();

    // Crear el directorio con Qt (sin llamar a system()) y comprobar el resultado.
    QDir().mkpath(QString::fromStdString(interchangeDir));
    if (!QDir(QString::fromStdString(interchangeDir)).exists()) {
        LOGE("No se pudo crear el directorio de interchange: %s", interchangeDir.c_str());
        return;
    }

    // Obtener el timestamp de la toma actual para mantener el mismo fichero durante toda la grabación
    std::string timestamp;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        timestamp = m_tomaGrabacionTimestamp;
    }
    if (timestamp.empty()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        struct tm tmBuf;
        struct tm* tm = localtime_r(&time_t, &tmBuf);
        char tsBuf[64];
        strftime(tsBuf, sizeof(tsBuf), "%Y%m%d_%H%M%S", tm);
        timestamp = tsBuf;
    }
    
    std::string filename = interchangeDir + "/" + track->name() + "_" + timestamp + ".wav";
    m_ultimaTomaWavPath = QString::fromStdString(filename);
    
    size_t totalSamples = frames * channels;
    size_t dataBytes = totalSamples * sizeof(float);
    int sampleRate = m_inputStream ? m_inputStream->getSampleRate() : (m_sampleRate > 0 ? m_sampleRate : 48000);

    bool fileExists = QFileInfo::exists(QString::fromStdString(filename));
    FILE* file = nullptr;

    if (!fileExists) {
        // Archivo nuevo: abrir con "wb" y escribir cabecera inicial más muestras
        file = fopen(filename.c_str(), "wb");
        if (!file) {
            LOGE("Failed to create WAV file: %s", filename.c_str());
            return;
        }

        WavHeader header;
        header.sampleRate = sampleRate;
        header.channels = channels;
        header.bitsPerSample = 32; // IEEE Float
        header.byteRate = header.sampleRate * header.channels * (header.bitsPerSample / 8);
        header.blockAlign = header.channels * (header.bitsPerSample / 8);
        header.dataSize = static_cast<uint32_t>(dataBytes);
        header.chunkSize = 36 + header.dataSize;
        
        if (fwrite(&header, sizeof(WavHeader), 1, file) != 1) {
            LOGE("Error al escribir la cabecera WAV inicial");
        }
        if (fwrite(samples, sizeof(float), totalSamples, file) != totalSamples) {
            LOGE("Error al escribir los datos de audio iniciales");
        }
        fclose(file);
    } else {
        // Archivo existente: abrir con "r+b" para permitir posicionamiento arbitrario
        file = fopen(filename.c_str(), "r+b");
        if (!file) {
            LOGE("Failed to open WAV file for updating: %s", filename.c_str());
            return;
        }

        // 1. Ir al final y agregar las nuevas muestras de audio
        fseek(file, 0, SEEK_END);
        if (fwrite(samples, sizeof(float), totalSamples, file) != totalSamples) {
            LOGE("Error al escribir los datos de audio en el fichero existente");
        }
        long totalFileSize = ftell(file);

        // 2. Rebobinar al inicio y actualizar dataSize y chunkSize de la cabecera
        fseek(file, 0, SEEK_SET);
        WavHeader header;
        if (fread(&header, sizeof(WavHeader), 1, file) == 1) {
            if (totalFileSize > static_cast<long>(sizeof(WavHeader))) {
                header.dataSize = static_cast<uint32_t>(totalFileSize - sizeof(WavHeader));
                header.chunkSize = 36 + header.dataSize;
                fseek(file, 0, SEEK_SET);
                if (fwrite(&header, sizeof(WavHeader), 1, file) != 1) {
                    LOGE("Error al actualizar la cabecera WAV en el fichero");
                }
            }
        }
        fclose(file);
    }
    
    LOGD("Wrote %zu frames to %s", frames, filename.c_str());
#else
    Q_UNUSED(track);
    Q_UNUSED(samples);
    Q_UNUSED(frames);
    Q_UNUSED(channels);
#endif
}

// ============================================================================
// Oboe Callbacks
// ============================================================================

#if defined(Q_OS_ANDROID)
oboe::DataCallbackResult NovaAndroidAudioEngine::InputStreamCallback::onAudioReady(
    oboe::AudioStream* stream, void* audioData, int32_t numFrames) {
    
    if (numFrames <= 0 || !audioData) {
        return oboe::DataCallbackResult::Continue;
    }

    const int channels = stream->getChannelCount();
    const oboe::AudioFormat format = stream->getFormat();
    
    const float* floatInput = nullptr;

    if (format == oboe::AudioFormat::Float) {
        floatInput = static_cast<const float*>(audioData);
    } else if (format == oboe::AudioFormat::I16) {
        const int16_t* i16Data = static_cast<const int16_t*>(audioData);
        const size_t totalSamples = static_cast<size_t>(numFrames) * static_cast<size_t>(channels);
        if (m_conversionBuffer.size() < totalSamples) {
            m_conversionBuffer.resize(totalSamples);
        }
        for (size_t i = 0; i < totalSamples; ++i) {
            m_conversionBuffer[i] = static_cast<float>(i16Data[i]) / 32768.0f;
        }
        floatInput = m_conversionBuffer.data();
    } else {
        return oboe::DataCallbackResult::Continue;
    }

    if (!m_engine->isRecording()) {
        return oboe::DataCallbackResult::Continue;
    }

    // Calcular picos en tiempo real del micrófono físico para la forma de onda
    float maxV = 0.0f;
    float minV = 0.0f;
    double sumSq = 0.0;
    const size_t totalSamples = static_cast<size_t>(numFrames) * static_cast<size_t>(channels);
    for (size_t s = 0; s < totalSamples; ++s) {
        float val = floatInput[s];
        if (val > maxV) maxV = val;
        if (val < minV) minV = val;
        sumSq += static_cast<double>(val) * static_cast<double>(val);
    }
    float rmsV = (totalSamples > 0) ? static_cast<float>(std::sqrt(sumSq / totalSamples)) : 0.0f;
    m_engine->registrarPicoEnVivo(minV, maxV, rmsV);

    // Write to all armed track input buffers
    for (int i = 0; i < kMaxTracks; ++i) { // max tracks
        auto* ringBuf = m_engine->getInputBuffer(i);
        if (ringBuf) {
            ringBuf->write(floatInput, numFrames);
        }
    }
    
    return oboe::DataCallbackResult::Continue;
}

void NovaAndroidAudioEngine::InputStreamCallback::onErrorAfterClose(
    oboe::AudioStream* stream, oboe::Result error) {
    LOGE("Input stream error: %s", oboe::convertToText(error));
    m_engine->Q_EMIT bufferOverrun();
}

oboe::DataCallbackResult NovaAndroidAudioEngine::OutputStreamCallback::onAudioReady(
    oboe::AudioStream* stream, void* audioData, int32_t numFrames) {
    
    float* outputBuffer = static_cast<float*>(audioData);
    int channels = stream->getChannelCount();
    const int muestras = numFrames * channels;

    if (numFrames <= 0 || channels <= 0) {
        return oboe::DataCallbackResult::Continue;
    }
    
    // Clear output buffer first
    std::fill(outputBuffer, outputBuffer + muestras, 0.0f);
    
    int emitidos = 0;
    
    if (m_engine->m_player.isPlaying()) {
        if (m_engine->m_canalesFichero.load(std::memory_order_relaxed) == channels) {
            // El reproductor entrega el fichero ya remuestreado a la tasa del
            // stream, con los canales del fichero (que son los del stream).
            int escritos = m_engine->m_player.renderFrames(outputBuffer, numFrames);
            if (escritos < 0) {
                escritos = 0;
            }
            if (escritos > numFrames) {
                escritos = numFrames;
            }
            // renderFrames() deja intacto el resto del buffer: la cola se
            // rellena con ceros, o el driver emitiría muestras sin
            // inicializar en el último bloque del fichero.
            if (escritos < numFrames) {
                std::fill(outputBuffer + static_cast<size_t>(escritos) * static_cast<size_t>(channels),
                          outputBuffer + muestras, 0.0f);
            }
            emitidos = escritos;
        }
        // Si los canales del fichero no son los del stream, el bloque sale
        // mudo: escribir el audio intercalado del fichero desbordaría el
        // buffer del driver.
    } else {
        // Read from track output buffers and mix
        // For simplicity, we'll read from track 0 output buffer
        // Real implementation would mix all tracks with gain/pan
        // (se conserva para el mixer de la Fase 1)
        auto* track0Output = m_engine->getOutputBuffer(0);
        if (track0Output && track0Output->channels() == channels) {
            emitidos = static_cast<int>(track0Output->read(outputBuffer, numFrames));
            NovaDspChain* dsp = m_engine->m_activeDspChain.load(std::memory_order_relaxed);
            if (dsp && emitidos > 0) {
                dsp->process(outputBuffer, emitidos, channels, stream->getSampleRate());
            }
        }
    }
    
    // Loop (Fase 3): si el fichero se ha agotado y el loop está activo,
    // se vuelve al principio y se sigue reproduciendo.
    if (emitidos == 0 && m_engine->m_player.durationFrames() > 0
        && m_engine->m_loopEnabled.load(std::memory_order_relaxed)) {
        m_engine->m_player.reset();
        m_engine->m_player.play();
        m_engine->m_currentFrame.store(0, std::memory_order_relaxed);
        int reintentados = m_engine->m_player.renderFrames(outputBuffer, numFrames);
        if (reintentados < 0) reintentados = 0;
        if (reintentados > numFrames) reintentados = numFrames;
        if (reintentados < numFrames) {
            std::fill(outputBuffer + static_cast<size_t>(reintentados) * static_cast<size_t>(channels),
                      outputBuffer + muestras, 0.0f);
        }
        emitidos = reintentados;
    }

    // Update frame counter
    if (emitidos > 0) {
        m_engine->m_currentFrame.fetch_add(emitidos, std::memory_order_relaxed);
    }

    // Loop por rango (Fase 3): si el playhead supera el final del loop,
    // se vuelve al inicio del rango.
    bool rangoLoopActivo = false;
    if (m_engine->m_loopEnabled.load(std::memory_order_relaxed)
        && m_engine->hasLoopRange()) {
        const qint64 endFrame = m_engine->loopEndFrame();
        const qint64 startFrame = m_engine->loopStartFrame();
        if (m_engine->m_currentFrame.load(std::memory_order_relaxed) >= endFrame) {
            m_engine->m_player.seekToFrame(startFrame);
            m_engine->m_currentFrame.store(startFrame, std::memory_order_relaxed);
            rangoLoopActivo = true;
        }
    }

    // Auto-advance: si el reproductor se ha agotado (sin loop activo que
    // haya manejado el caso), se avisa una sola vez para que el hilo de la
    // interfaz pueda pasar a la siguiente region.
    if (emitidos == 0 && !m_engine->m_player.isPlaying()
        && m_engine->m_player.durationFrames() > 0
        && !rangoLoopActivo
        && !m_engine->m_loopEnabled.load(std::memory_order_relaxed)) {
        if (!m_engine->m_exhaustionNotified.load(std::memory_order_relaxed)) {
            m_engine->m_exhaustionNotified.store(true, std::memory_order_relaxed);
            QMetaObject::invokeMethod(m_engine, "playbackFinished", Qt::QueuedConnection);
        }
    }

    // Picos del bloque escrito (VU meter). Se calculan sobre el buffer de
    // salida real (lo que suena), canal L y R.
    float pL = 0.0f, pR = 0.0f;
    for (int i = 0; i < numFrames; ++i) {
        int idx = i * channels;
        float aL = std::fabs(outputBuffer[idx]);
        if (aL > pL) pL = aL;
        if (channels > 1) {
            float aR = std::fabs(outputBuffer[idx + 1]);
            if (aR > pR) pR = aR;
        } else {
            pR = pL;
        }
    }
    m_engine->m_peakLeft.store(pL, std::memory_order_relaxed);
    m_engine->m_peakRight.store(pR, std::memory_order_relaxed);

    return oboe::DataCallbackResult::Continue;
}

void NovaAndroidAudioEngine::OutputStreamCallback::onErrorAfterClose(
    oboe::AudioStream* stream, oboe::Result error) {
    LOGE("Output stream error: %s", oboe::convertToText(error));
    // Si hay que reabrir el stream, se hace desde el hilo de la interfaz
    // (aquí no: este callback corre en el hilo de audio y reabrir un stream
    // ahí no es seguro). Toda reapertura pasa por abrirStreamSalida(), que
    // vuelve a aplicar la tasa real del dispositivo al reproductor.
    m_engine->Q_EMIT bufferUnderrun();
}
#endif // Q_OS_ANDROID

void NovaAndroidAudioEngine::registrarPicoEnVivo(float minV, float maxV, float rmsV)
{
    // Usar try_to_lock: el hilo de audio en tiempo real NUNCA debe bloquearse
    // si el hilo de UI de QML está leyendo los picos en ese milisegundo exacto.
    std::unique_lock<std::mutex> lock(m_livePeaksMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return;
    }
    PeakPoint pt;
    pt.min = minV;
    pt.max = maxV;
    pt.rms = rmsV;
    m_livePeaks.push_back(pt);
}

void NovaAndroidAudioEngine::obtenerPicosEnVivo(PeakData &outData)
{
    std::lock_guard<std::mutex> lock(m_livePeaksMutex);
    outData.peaks = m_livePeaks;
    outData.sampleRate = m_sampleRate > 0 ? m_sampleRate : 48000;
    outData.channels = 2;
    outData.version = 3;
}

void NovaAndroidAudioEngine::limpiarPicosEnVivo()
{
    std::lock_guard<std::mutex> lock(m_livePeaksMutex);
    m_livePeaks.clear();
    m_livePeaks.shrink_to_fit();
}

QString NovaAndroidAudioEngine::ultimaTomaWavPath() const
{
    return m_ultimaTomaWavPath;
}