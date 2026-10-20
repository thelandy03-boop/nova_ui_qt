#include "NovaSpikeController.h"

#if defined(Q_OS_ANDROID)

#include "NovaAndroidAudioEngine.h"

#include "audio/NovaAudioDecoder.h"
#include "audio/NovaAudioPeaks.h"
#include "core/NovaLogging.h"
#include "models/NovaRegionModel.h"

namespace {

// Id de la región donde se publica la forma de onda: tiene que ser
// EXACTAMENTE el mismo que pide la página QML (NovaWaveformItem.regionId).
const char* const kRegionId = "spike";

// Puntos de la forma de onda: el detalle (zoom máximo) y la vista general.
constexpr int kPuntosDetalle = 2048;
constexpr int kPuntosVistaGeneral = 256;

// Cadencia con la que se refresca la posición del cabezal.
constexpr int kIntervaloPosicionMs = 50;

// Tasa con la que arranca el motor de audio.
constexpr int kTasaMotor = 48000;

} // namespace

NovaSpikeController::NovaSpikeController(QObject* parent)
    : QObject(parent)
{
    // El controlador es el dueño del motor de audio: en Android no lo
    // instancia nadie más (la sesión de Ardour usa su stub).
    m_engine = std::make_unique<NovaAndroidAudioEngine>();
    if (!m_engine->initialize(kTasaMotor)) {
        // Sin motor no hay sonido: el estado lo dice, para que se vea por
        // que no suena en vez de parecer que el fichero está mal.
        qCWarning(novaCore) << "🔊 Spike: el motor de audio no se ha podido inicializar";
        actualizarEstado(QStringLiteral("el motor de audio no se ha podido inicializar"));
    } else {
        m_motorListo = true;
        m_engine->start();
        if (!m_engine->openOutputStream(2)) {
            // Aviso, no error: hay dispositivos que abren la salida con un
            // solo canal y siguen sonando.
            qCWarning(novaCore) << "🔊 Spike: la salida de audio no tiene dos canales";
        }
        actualizarEstado(QStringLiteral("listo"));
    }

    // El reloj no decide la posición: solo avisa de que hay que volver a
    // leerla del motor.
    m_temporizador.setInterval(kIntervaloPosicionMs);
    connect(&m_temporizador, &QTimer::timeout,
            this, &NovaSpikeController::actualizarPosicion);
    m_temporizador.start();
}

NovaSpikeController::~NovaSpikeController()
{
    // Oboe exige cerrar los streams ANTES de destruir el motor: si no, crash
    // al salir. Por eso se apaga el motor aquí y no se deja que se destruya
    // solo.
    if (m_engine) {
        m_engine->shutdown();
    }
}

bool NovaSpikeController::loadAndPlay(const QString& path)
{
    if (!m_engine || !m_motorListo) {
        actualizarEstado(QStringLiteral("el motor de audio no está disponible"));
        return false;
    }

    // 1. Decodificar: sin audio no hay ni picos ni sonido.
    NovaDecodedAudio audio;
    QString error;
    if (!decodeAudioFile(path, audio, &error)) {
        actualizarEstado(error.isEmpty()
                             ? QStringLiteral("no se ha podido decodificar el fichero")
                             : error);
        return false;
    }

    // 2. Calcular los picos y publicarlos: el renderizador de la forma de
    //    onda los lee de la caché por el id de región ("spike").
    const PeakData picos = buildPeakData(audio, kPuntosDetalle, kPuntosVistaGeneral);
    NovaWaveformCache::setPeaks(QString::fromLatin1(kRegionId), picos);

    // 3. Duración del fichero, para saber dónde acaba el cabezal.
    actualizarDuracion(audio.sampleRate > 0
                            ? static_cast<double>(audio.frameCount()) / audio.sampleRate
                            : 0.0);

    // 4. Cargar el fichero en el motor y ponerlo en marcha.
    if (!m_engine->loadFile(path, &error)) {
        actualizarEstado(error.isEmpty()
                             ? QStringLiteral("no se ha podido cargar el fichero en el motor de audio")
                             : error);
        return false;
    }
    m_engine->setPlaying(true);

    qCDebug(novaCore) << "🔊 Spike: reproduciendo" << path
                       << picos.peaks.size() << "picos";
    actualizarEstado(QStringLiteral("sonando"));
    return true;
}

void NovaSpikeController::stop()
{
    if (m_engine) {
        m_engine->setPlaying(false);
    }
    actualizarEstado(QStringLiteral("parado"));
}

double NovaSpikeController::positionSeconds() const
{
    if (!m_engine) {
        return 0.0;
    }
    // La posición la lleva el motor: son los fotogramas que ha escrito de
    // verdad, no los que lleva el reloj.
    const int tasa = m_engine->sampleRate();
    if (tasa <= 0) {
        return 0.0;
    }
    return static_cast<double>(m_engine->currentFrame()) / tasa;
}

void NovaSpikeController::actualizarPosicion()
{
    Q_EMIT positionChanged();
}

void NovaSpikeController::actualizarEstado(const QString& texto)
{
    if (m_status == texto) {
        return;
    }
    m_status = texto;
    Q_EMIT statusChanged();
}

void NovaSpikeController::actualizarDuracion(double segundos)
{
    if (m_durationSeconds == segundos) {
        return;
    }
    m_durationSeconds = segundos;
    // La duración se avisa con la misma señal que el estado.
    Q_EMIT statusChanged();
}

#endif // Q_OS_ANDROID
