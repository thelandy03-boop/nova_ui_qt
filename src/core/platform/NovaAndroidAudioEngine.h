#ifndef NOVAAUDIOENGINE_ANDROID_H
#define NOVAAUDIOENGINE_ANDROID_H

#include <QObject>
#include <QString>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <QTimer>

// El audio ya decodificado que el motor reproduce (lo recibe movido: el
// fichero se decodifica una sola vez y el PCM no se copia).
#include "audio/NovaDecodedAudio.h"

#if defined(Q_OS_ANDROID)
#include <oboe/Oboe.h>
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>

// El reproductor del fichero cargado: lo sirve el callback de Oboe.
#include "audio/NovaAudioPlayer.h"

#define LOG_TAG "NovaAndroidAudio"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define LOGD(...) 
#define LOGE(...)
#define LOGW(...)
#endif

// Forward declarations
namespace ARDOUR {
    class Session;
    class AudioEngine;
    class Route;
    class Track;
    class AudioTrack;
    class MidiTrack;
    class Playlist;
    class Region;
    class AudioRegion;
    class Source;
    using RouteList = std::vector<std::shared_ptr<Route>>;
    using AudioTrackList = std::vector<std::shared_ptr<AudioTrack>>;
    using samplepos_t = long long;
    using samplecnt_t = long long;
    using Sample = float;
}

class NovaAndroidAudioEngine : public QObject {
    Q_OBJECT

public:
    explicit NovaAndroidAudioEngine(QObject* parent = nullptr);
    ~NovaAndroidAudioEngine() override;

    // Audio engine lifecycle
    bool initialize(int sampleRate = 48000, int framesPerBurst = 0);
    void shutdown();
    bool start();
    void stop();

    // Session management
    void setSession(ARDOUR::Session* session);
    ARDOUR::Session* session() const { return m_session; }

    // Audio device management
    bool openInputStream(int channelCount = 2);
    bool openOutputStream(int channelCount = 2);
    void closeStreams();

    // Reproducción de un fichero (spike): deja el audio listo para que el
    // callback de Oboe lo sirva por la salida.
    //
    // El motor NO decodifica: recibe el audio ya decodificado (así el
    // fichero se decodifica una sola vez y el PCM no se copia).
    //
    // El stream de salida se abre con los canales del fichero: el
    // reproductor escribe muestras intercaladas con esos canales, y un
    // fichero mono en un stream estéreo sonaría mal.
    //
    // @param audio Audio ya decodificado (se mueve: el motor es su dueño).
    // @param error Mensaje con el motivo del fallo (puede ser nullptr).
    // @return true si el audio se ha cargado y se puede reproducir.
    bool loadDecoded(NovaDecodedAudio&& audio, QString* error);

    // Recording/Playback control
    void setRecording(bool enabled);
    void setPlaying(bool enabled);
    bool isRecording() const { return m_isRecording.load(); }
    bool isPlaying() const { return m_isPlaying.load(); }

    /**
     * true si el motor tiene un fichero cargado (el del reproductor).
     *
     * No es lo mismo que `isPlaying()`: el transporte está en marcha desde
     * que el motor arranca, aunque no haya ningún fichero cargado. Para
     * saber si la forma de onda que hay en pantalla describe algo real
     * (y no se puede borrar) hace falta esta pregunta: ¿hay fichero?
     */
    bool tieneFicheroCargado() const;

    // Ring buffer access for tracks
    class RingBuffer {
    public:
        explicit RingBuffer(size_t capacityFrames, int channels);
        ~RingBuffer();

        // Write from audio callback (producer)
        bool write(const float* input, size_t frames);
        
        // Read from consumer (track disk writer)
        size_t read(float* output, size_t frames);
        
        // Query
        size_t availableRead() const;
        size_t availableWrite() const;
        void clear();
        
        // For monitoring
        size_t capacity() const { return m_buffer.size(); }
        int channels() const { return m_channels; }

    private:
        friend class NovaAndroidAudioEngine;
        std::vector<float> m_buffer;
        const size_t m_capacityFrames;
        const int m_channels;
        std::atomic<size_t> m_writePos{0};
        std::atomic<size_t> m_readPos{0};
        std::atomic<bool> m_overrun{false};
        std::atomic<bool> m_underrun{false};
    };

    // Per-track ring buffers
    RingBuffer* getInputBuffer(int trackIndex);
    RingBuffer* getOutputBuffer(int trackIndex);

    // Transport info
    int64_t currentFrame() const { return m_currentFrame.load(); }

    void setLoopEnabled(bool enabled) { m_loopEnabled.store(enabled, std::memory_order_relaxed); }
    bool loopEnabled() const { return m_loopEnabled.load(std::memory_order_relaxed); }

    // Picos del último bloque escrito (VU meter). 0..1 aprox.
    float peakLeft() const { return m_peakLeft.load(std::memory_order_relaxed); }
    float peakRight() const { return m_peakRight.load(std::memory_order_relaxed); }
    double currentTimeSeconds() const
    {
        return static_cast<double>(m_currentFrame.load())
             / static_cast<double>(tasaSalidaReal());
    }
    void setSampleRate(int rate) { m_sampleRate = rate; }

    /** Tasa pedida al dispositivo (48.000 Hz en el móvil). */
    int sampleRate() const { return m_sampleRate; }

    /**
     * Tasa real del stream de salida: la que el dispositivo ha dado de
     * verdad, que puede no ser la que se le ha pedido (44,1 kHz es lo
     * habitual en muchos móviles).
     *
     * Es la que gobierna el reproductor (que remuestrea a esa tasa) y la
     * posición del cabezal.
     */
    int tasaSalidaReal() const
    {
        const int real = m_tasaSalidaReal.load();
        return real > 0 ? real : m_sampleRate;
    }

signals:
    void audioInitialized(bool success);
    void recordingStateChanged(bool recording);
    void playbackStateChanged(bool playing);
    void bufferUnderrun();
    void bufferOverrun();

private:
#if defined(Q_OS_ANDROID)
    // Oboe stream callbacks
    class InputStreamCallback : public oboe::AudioStreamCallback {
    public:
        InputStreamCallback(NovaAndroidAudioEngine* engine) : m_engine(engine) {}
        oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) override;
        void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;
    private:
        NovaAndroidAudioEngine* m_engine;
    };

    class OutputStreamCallback : public oboe::AudioStreamCallback {
    public:
        OutputStreamCallback(NovaAndroidAudioEngine* engine) : m_engine(engine) {}
        oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) override;
        void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;
    private:
        NovaAndroidAudioEngine* m_engine;
    };

    std::shared_ptr<oboe::AudioStream> m_inputStream;
    std::shared_ptr<oboe::AudioStream> m_outputStream;
    std::unique_ptr<InputStreamCallback> m_inputCallback;
    std::unique_ptr<OutputStreamCallback> m_outputCallback;

    // Reproductor del fichero cargado: el callback de salida le pide los
    // fotogramas y los escribe en el buffer del driver.
    NovaAudioPlayer m_player;

    // Canales con los que se abrió el stream de salida (los del fichero
    // cargado, o 2 por defecto si todavía no hay ninguno).
    int m_canalSalida = 2;

    // Canales del fichero cargado (0 si no hay fichero). Lo lee el callback
    // de salida: es atómico porque lo cambia el hilo de la interfaz y lo
    // lee el hilo de audio.
    std::atomic<int> m_canalesFichero{0};
#endif

    ARDOUR::Session* m_session = nullptr;
    std::atomic<bool> m_isRecording{false};
    std::atomic<bool> m_isPlaying{false};
    std::atomic<int64_t> m_currentFrame{0};
    std::atomic<bool> m_inputStreamError{false};
    std::atomic<bool> m_outputStreamError{false};
    int m_sampleRate = 48000;

    // Tasa real del stream de salida (la que dio el dispositivo). 0 si
    // todavía no se ha abierto ningún stream. La lee el hilo de la
    // interfaz; es atómica por si el motor se consulta desde otro hilo.
    std::atomic<int> m_tasaSalidaReal{0};

    std::atomic<bool> m_loopEnabled{false};

    // Picos del último bloque de salida (para VU meters).
    std::atomic<float> m_peakLeft{0.0f};
    std::atomic<float> m_peakRight{0.0f};

    int m_framesPerBurst = 0;

    // Per-track ring buffers
    std::vector<std::unique_ptr<RingBuffer>> m_inputBuffers;
    std::vector<std::unique_ptr<RingBuffer>> m_outputBuffers;
    mutable std::mutex m_bufferMutex;

    // Disk writing thread
    std::thread m_diskWriterThread;
    std::atomic<bool> m_diskWriterRunning{false};
    std::condition_variable m_diskWriterCV;
    std::mutex m_diskWriterMutex;

    void diskWriterLoop();
    void writeTrackToDisk(int trackIndex);
    void writeWavFile(
        std::shared_ptr<ARDOUR::Track> track,
        const float* samples,
        size_t frames,
        int channels);

    // Crea los ring buffers de entrada con los canales del stream de
    // entrada (los usa el grabador).
    void crearBuffersEntrada(int canales);

    // Crea los ring buffers de salida con los canales del stream: si no
    // coinciden, al leer un bloque se escribirían más muestras de las que
    // caben en el buffer del driver.
    void crearBuffersSalida(int canales);

    // Abre (o reabre) el stream de salida con el número de canales dado.
    bool abrirStreamSalida(int canales, QString* error);

    // Vuelve a abrir el stream de salida con los canales que tenía y lo
    // deja como estaba (arrancado o parado). Se usa cuando una operación
    // falla a mitad y el motor tiene que quedarse como estaba.
    //
    // @param canales        Canales con los que se reabre el stream.
    // @param estabaEnMarcha true si el stream estaba arrancado antes.
    // @param error          Mensaje si no se ha podido reabrir (puede ser
    //                       nullptr).
    // @return true si el stream se ha reabierto; false si se ha quedado
    //         cerrado (el motor se queda sin salida de audio).
    bool restaurarStreamSalida(int canales, bool estabaEnMarcha, QString* error);

    // Crea los callbacks de Oboe si no existen (shutdown() los libera, y un
    // initialize() posterior tiene que volver a crearlos).
    void crearCallbacks();

    // Número máximo de tracks con ring buffer propio.
    static constexpr int kMaxTracks = 32;
};

#endif // NOVAAUDIOENGINE_ANDROID_H