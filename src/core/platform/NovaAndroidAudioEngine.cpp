#include "NovaAndroidAudioEngine.h"

#if defined(Q_OS_ANDROID)
#include <oboe/Oboe.h>

#define LOG_TAG "NovaAndroidAudio"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define LOGD(...) 
#define LOGE(...)
#define LOGW(...)
#endif

// ARDOUR includes for Android (using stubs)
#include "NovaAndroidStubs.h"

#include "audio/NovaAudioDecoder.h"
#include "core/NovaLogging.h"

#include <cmath>
#include <algorithm>
#include <cstdint>

// ============================================================================
// WAV Header (RIFF IEEE Float 32-bit)
// ============================================================================
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
    // El micro NO es imprescindible para reproducir: si no se puede abrir
    // (permiso sin conceder, dispositivo ocupado...) se sigue sin grabar,
    // pero la salida de audio -que es lo que usa el spike- queda abierta.
    //
    // Antes esto cerraba la salida y devolvía false, y en el móvil el
    // permiso de micro se pide DESPUÉS de arrancar la app: el motor se
    // quedaba muerto en el primer arranque y no había forma de que volviera.
    oboe::AudioStreamBuilder inputBuilder;
    inputBuilder.setDirection(oboe::Direction::Input);
    inputBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    inputBuilder.setSharingMode(oboe::SharingMode::Exclusive);
    inputBuilder.setFormat(oboe::AudioFormat::Float);
    inputBuilder.setChannelCount(2);
    inputBuilder.setSampleRate(sampleRate);
    inputBuilder.setFramesPerCallback(m_framesPerBurst);
    inputBuilder.setCallback(m_inputCallback.get());

    oboe::Result result = inputBuilder.openStream(m_inputStream);
    if (result != oboe::Result::OK) {
        // Oboe deja el stream a null cuando no puede abrirlo.
        LOGW("⚠️ No se pudo abrir la entrada de audio (micro): se sigue sin grabar (%s)",
             oboe::convertToText(result));
        m_inputStream.reset();
    } else {
        LOGD("Input stream opened: %d Hz, %d frames/burst, %d channels",
             m_inputStream->getSampleRate(), m_inputStream->getFramesPerBurst(), m_inputStream->getChannelCount());

        // Initialize ring buffers
        crearBuffersEntrada(m_inputStream->getChannelCount());
    }

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
    // (use-after-free). Por eso loadFile() cierra el stream antes de llamar
    // aquí, y lo arranca después.
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
    // Grabar solo es posible si hay micro.
    m_isRecording = (m_inputStream != nullptr);
    m_currentFrame = 0;

    Q_EMIT playbackStateChanged(true);
    Q_EMIT recordingStateChanged(m_isRecording);
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
    m_isRecording.store(enabled);
    Q_EMIT recordingStateChanged(enabled);
}

void NovaAndroidAudioEngine::setPlaying(bool enabled) {
    m_isPlaying.store(enabled);
#if defined(Q_OS_ANDROID)
    // El fichero cargado sigue al transporte del motor: al ponerlo en
    // marcha arranca el reproductor, y al pararlo se detiene.
    if (enabled) {
        m_player.play();
        // El stream de salida tiene que estar en marcha para que suene.
        if (m_outputStream) {
            const oboe::StreamState estado = m_outputStream->getState();
            if (estado != oboe::StreamState::Started && estado != oboe::StreamState::Starting) {
                m_outputStream->requestStart();
            }
        }
    } else {
        m_player.stop();
    }
#endif
    Q_EMIT playbackStateChanged(enabled);
}

bool NovaAndroidAudioEngine::loadFile(const QString& path, QString* error) {
#if defined(Q_OS_ANDROID)
    NovaDecodedAudio audio;
    if (!decodeAudioFile(path, audio, error)) {
        return false;
    }

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

    // Con el stream cerrado se puede tocar el reproductor sin carrera.
    if (!m_player.load(audio, error)) {
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
    // reproductor entrega 48 kHz, el audio suena grave y lento. Al
    // reabrir el stream (más abajo) la tasa real se vuelve a aplicar, por
    // si el driver la ha cambiado.
    m_player.setOutputSampleRate(tasaSalidaReal());
    m_canalesFichero = audio.channels;

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
        if (!abrirStreamSalida(audio.channels, error)) {
            // Se deja el motor como estaba, con los canales anteriores. El
            // fichero no sonará (su callback ve que los canales no
            // coinciden), pero el motor sigue vivo.
            QString errorRestaurar;
            if (!restaurarStreamSalida(canalesAnteriores, estabaEnMarcha, &errorRestaurar)) {
                anadirAvisoDeSalidaCerrada(errorRestaurar);
            }
            return false;
        }

        if (m_outputStream->getChannelCount() != audio.channels) {
            // El driver ha abierto el stream con otros canales: el audio
            // intercalado del fichero no cabría en el buffer del driver, así
            // que el fichero no se puede reproducir con este stream.
            const QString errorCanales = QStringLiteral(
                "la salida de audio se ha abierto con %1 canales y el fichero tiene %2")
                .arg(m_outputStream->getChannelCount())
                .arg(audio.channels);
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
        m_canalSalida = audio.channels;
    }

    qCDebug(novaCore) << "🔊 Spike: cargado" << path << audio.sampleRate << "Hz,"
                       << audio.channels << "canales," << audio.frameCount() << "muestras";
    return true;
#else
    Q_UNUSED(path);
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
    
    // Create directory if needed
    std::string cmd = "mkdir -p " + interchangeDir;
    system(cmd.c_str());
    
    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm* tm = localtime(&time_t);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm);
    
    std::string filename = interchangeDir + "/" + track->name() + "_" + timestamp + ".wav";
    
    // Open file for appending (or create new)
    FILE* file = fopen(filename.c_str(), "ab");
    if (!file) {
        LOGE("Failed to open WAV file for writing: %s", filename.c_str());
        return;
    }
    
    // Check if file is new (write header)
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    bool isNewFile = (fileSize == 0);
    
    size_t totalSamples = frames * channels;
    size_t dataBytes = totalSamples * sizeof(float);
    
    if (isNewFile) {
        // Write WAV header
        WavHeader header;
        header.sampleRate = m_sampleRate;
        header.channels = channels;
        header.bitsPerSample = 32; // float
        header.byteRate = header.sampleRate * header.channels * (header.bitsPerSample / 8);
        header.blockAlign = header.channels * (header.bitsPerSample / 8);
        header.dataSize = dataBytes;
        header.chunkSize = 36 + header.dataSize;
        
        fwrite(&header, sizeof(WavHeader), 1, file);
    } else {
        // Update existing header - need to update dataSize and chunkSize
        // Read current header
        fseek(file, 0, SEEK_SET);
        WavHeader existingHeader;
        fread(&existingHeader, sizeof(WavHeader), 1, file);
        
        // Update sizes
        existingHeader.dataSize += dataBytes;
        existingHeader.chunkSize = 36 + existingHeader.dataSize;
        
        // Write updated header
        fseek(file, 0, SEEK_SET);
        fwrite(&existingHeader, sizeof(WavHeader), 1, file);
        
        // Seek to end for appending
        fseek(file, 0, SEEK_END);
    }
    
    // Write audio data (32-bit IEEE float PCM)
    fwrite(samples, sizeof(float), totalSamples, file);
    
    fclose(file);
    
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
    
    float* inputBuffer = static_cast<float*>(audioData);
    int channels = stream->getChannelCount();
    
    // Write to all armed track input buffers
    for (int i = 0; i < kMaxTracks; ++i) { // max tracks
        auto* ringBuf = m_engine->getInputBuffer(i);
        if (ringBuf) {
            ringBuf->write(inputBuffer, numFrames);
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
        }
    }
    
    // Update frame counter
    // El contador avanza con los fotogramas realmente escritos: los del
    // fichero (el playhead marca su posición) o los que trae el ring
    // buffer. Así el playhead nunca se adelanta al audio.
    if (emitidos > 0) {
        m_engine->m_currentFrame.fetch_add(emitidos, std::memory_order_relaxed);
    }
    
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