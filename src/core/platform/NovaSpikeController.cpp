#include "NovaSpikeController.h"

#if defined(Q_OS_ANDROID)

#include "NovaAndroidAudioEngine.h"

#include "audio/NovaSpikeLoader.h"
#include "core/NovaLogging.h"
#include "models/NovaRegionModel.h"

namespace {

// Id de la región donde se publica la forma de onda: tiene que ser
// EXACTAMENTE el mismo que pide la página QML (NovaWaveformItem.regionId).
const char* const kRegionId = "spike";

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

    // El cargador decodifica el fichero y calcula los picos en un hilo
    // aparte: en el hilo de la interfaz la app se congelaría (spec §8.4).
    // No tiene padre: lo destruye este controlador (el unique_ptr).
    m_cargador = std::make_unique<NovaSpikeLoader>();
    connect(m_cargador.get(), &NovaSpikeLoader::cargaTerminada,
            this, &NovaSpikeController::alCargarTerminado);
    connect(m_cargador.get(), &NovaSpikeLoader::cargaFallida,
            this, &NovaSpikeController::alCargarFallido);

    if (prepararMotor()) {
        actualizarEstado(QStringLiteral("listo"));
    } else {
        // Sin motor no hay sonido, pero los picos de un fichero se pueden
        // calcular igual: el estado lo dice para que no parezca un fallo del
        // fichero.
        actualizarEstado(QStringLiteral("el motor de audio no está listo: se puede ver la onda, pero no suena"));
    }

    // El reloj no decide la posición: solo avisa de que hay que volver a
    // leerla del motor.
    m_temporizador.setInterval(kIntervaloPosicionMs);
    connect(&m_temporizador, &QTimer::timeout,
            this, &NovaSpikeController::actualizarPosicion);
    m_temporizador.start();
}

bool NovaSpikeController::prepararMotor()
{
    if (!m_engine) {
        return false;
    }
    if (m_motorListo) {
        return true;
    }

    // Se reintenta en cada carga: al arrancar, el permiso de micro se pide
    // después de crear la interfaz, y el motor puede quedarse sin salida
    // hasta que se conceda. Con el reintento, el primer toque de "Reproducir"
    // ya funciona.
    if (!m_engine->initialize(kTasaMotor)) {
        qCWarning(novaCore) << "🔊 Spike: el motor de audio no se ha podido inicializar";
        return false;
    }
    if (!m_engine->start()) {
        qCWarning(novaCore) << "🔊 Spike: el motor de audio no se ha podido arrancar";
        return false;
    }
    if (!m_engine->openOutputStream(2)) {
        // Aviso, no error: hay dispositivos que abren la salida con un
        // solo canal y siguen sonando.
        qCWarning(novaCore) << "🔊 Spike: la salida de audio no tiene dos canales";
    }

    m_motorListo = true;
    return true;
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
    if (!m_engine) {
        actualizarEstado(QStringLiteral("el motor de audio no está disponible"));
        return false;
    }

    // El trabajo pesado (decodificar el fichero y recorrer todas sus
    // muestras para calcular los picos) se hace en un hilo aparte: en el
    // hilo de la interfaz la app se congelaría (el spec §8.4 lo exige:
    // "Se calcula en un hilo aparte, nunca en el hilo GUI").
    //
    // El fichero se decodifica UNA sola vez: el audio que devuelve el
    // cargador es el mismo que se pasa al motor (movido, sin copiar el PCM).
    m_cargador->cargar(path);
    actualizarEstado(QStringLiteral("cargando el fichero…"));
    return true;
}

void NovaSpikeController::alCargarTerminado()
{
    // 1. Los picos ya están calculados: se publican y el renderizador de la
    //    forma de onda los coge (los pide por el id de región "spike").
    NovaWaveformCache::setPeaks(QString::fromLatin1(kRegionId), m_cargador->picos());

    // 2. El audio, ya decodificado. Se mueve al motor: las muestras no se
    //    copian (un fichero estéreo de cinco minutos son unos 115 MB).
    NovaDecodedAudio audio = m_cargador->tomarAudio();
    if (audio.channels <= 0 || audio.sampleRate <= 0) {
        limpiarOndaPublicada();
        actualizarEstado(QStringLiteral("no se ha podido decodificar el fichero"));
        return;
    }

    // 3. Duración del fichero, para saber dónde acaba el cabezal.
    actualizarDuracion(static_cast<double>(audio.frameCount())
                        / static_cast<double>(audio.sampleRate));

    // 4. Motor de audio: si no estaba listo se reintenta (a lo mejor ya se ha
    //    concedido el permiso). Sin motor no hay sonido, pero la onda que se
    //    acaba de publicar es real y se puede ver.
    if (!prepararMotor()) {
        actualizarEstado(QStringLiteral("la onda está lista, pero el motor de audio no está listo: no suena"));
        return;
    }

    // 5. Cargar el audio en el motor y ponerlo en marcha. El motor NO
    //    decodifica: recibe el audio ya decodificado.
    QString error;
    if (!m_engine->loadDecoded(std::move(audio), &error)) {
        // El audio no se ha podido cargar: la onda y la duración que
        // acabamos de publicar describen un fichero que no está en marcha,
        // así que se quitan (si no, el cabezal se mediría contra un
        // fichero que no suena).
        limpiarOndaPublicada();
        actualizarEstado(error.isEmpty()
                             ? QStringLiteral("no se ha podido cargar el fichero en el motor de audio")
                             : error);
        return;
    }
    m_engine->setPlaying(true);

    qCDebug(novaCore) << "🔊 Spike: reproduciendo" << m_cargador->picos().peaks.size()
                       << "picos";
    actualizarEstado(QStringLiteral("sonando"));
}

void NovaSpikeController::alCargarFallido(const QString& error)
{
    limpiarOndaPublicada();
    actualizarEstado(error.isEmpty()
                         ? QStringLiteral("no se ha podido cargar el fichero")
                         : error);
}

void NovaSpikeController::limpiarOndaPublicada()
{
    NovaWaveformCache::removePeaks(QString::fromLatin1(kRegionId));
    actualizarDuracion(0.0);
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
    //
    // Se mide con la tasa REAL del stream de salida (la que dio el
    // dispositivo), no con la pedida: si el móvil abre el stream a 44,1 kHz,
    // los fotogramas son 44,1 mil por segundo.
    const int tasa = m_engine->tasaSalidaReal();
    if (tasa <= 0) {
        return 0.0;
    }
    return static_cast<double>(m_engine->currentFrame()) / tasa;
}

void NovaSpikeController::actualizarPosicion()
{
    const double posicion = positionSeconds();

    // Solo se avisa cuando la posición cambia de verdad: si no está sonando
    // nada, el reloj no tiene nada que contar (y el QML no se refresca 20
    // veces por segundo sin motivo).
    if (posicion == m_ultimaPosicion) {
        return;
    }
    m_ultimaPosicion = posicion;
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
