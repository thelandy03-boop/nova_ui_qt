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

// Cadencia del log de fotogramas: una vez por segundo.
constexpr qint64 kIntervaloLogMs = 1000;

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

    // El reloj del log de fotogramas.
    m_reloj.start();
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

    // Una carga nueva: el "Parar" anterior (si lo hubo) ya no aplica.
    m_paradaPedida = false;

    // El trabajo pesado (decodificar el fichero y recorrer todas sus
    // muestras para calcular los picos) se hace en un hilo aparte: en el
    // hilo de la interfaz la app se congelaría (el spec §8.4 lo exige:
    // "Se calcula en un hilo aparte, nunca en el hilo GUI").
    //
    // El fichero se decodifica UNA sola vez: el audio que devuelve el
    // cargador es el mismo que se pasa al motor (movido, sin copiar el PCM).
    qCDebug(novaCore) << "🔊 Spike: cargando" << path;
    m_cargador->cargar(path);
    actualizarEstado(QStringLiteral("cargando el fichero…"));
    return true;
}

void NovaSpikeController::alCargarTerminado()
{
    // 0. Si el usuario ha pulsado "Parar" mientras el fichero se
    //    decodificaba, el resultado se descarta: la carga es asíncrona, así
    //    que sin esta comprobación la reproducción arrancaría igualmente
    //    al terminar la carga y el "Parar" no habría servido de nada.
    if (m_paradaPedida) {
        // El audio decodificado se suelta (memoria): no se va a poner en
        // marcha.
        m_cargador->tomarAudio();
        actualizarEstado(QStringLiteral("parado"));
        return;
    }

    // 1. Los picos ya están calculados (en el hilo de trabajo). Se
    //    publican más abajo, cuando ya está claro que el fichero se va a
    //    poner en marcha: publicar la onda y luego fallar dejaría la
    //    pantalla mintiendo.
    const PeakData& picos = m_cargador->picos();

    // 2. El audio, ya decodificado. Se mueve al motor: las muestras no se
    //    copian (un fichero estéreo de cinco minutos son unos 115 MB).
    NovaDecodedAudio audio = m_cargador->tomarAudio();
    if (audio.channels <= 0 || audio.sampleRate <= 0) {
        limpiarOndaSiNoSuena();
        actualizarEstado(QStringLiteral("no se ha podido decodificar el fichero"));
        return;
    }

    // 3. Duración del fichero, para saber dónde acaba el cabezal.
    const double duracion = static_cast<double>(audio.frameCount())
                            / static_cast<double>(audio.sampleRate);

    // 4. Motor de audio: si no estaba listo se reintenta (a lo mejor ya se
    //    ha concedido el permiso). Sin motor no hay sonido, pero la onda
    //    que se acaba de calcular es real y se puede ver.
    if (!prepararMotor()) {
        publicarOnda(duracion);
        actualizarEstado(QStringLiteral("la onda está lista, pero el motor de audio no está listo: no suena"));
        return;
    }

    // 5. Cargar el audio en el motor y ponerlo en marcha. El motor NO
    //    decodifica: recibe el audio ya decodificado.
    QString error;
    if (!m_engine->loadDecoded(std::move(audio), &error)) {
        // El audio no se ha podido cargar: la onda que se acaba de
        // calcular describe un fichero que no está en marcha, así que no
        // se publica. Lo que hubiera en el motor se deja intacto (su onda
        // y su duración no se tocan).
        limpiarOndaSiNoSuena();
        actualizarEstado(error.isEmpty()
                             ? QStringLiteral("no se ha podido cargar el fichero en el motor de audio")
                             : error);
        return;
    }

    // 6. Publicar la onda (los picos y la duración) y avisar: el QML
    //    repinta la forma de onda con lo recién publicado (el renderizador
    //    solo lee la caché cuando se crea, así que sin este aviso la onda
    //    no aparecería hasta el siguiente clic).
    publicarOnda(duracion);

    // 7. Poner en marcha.
    m_engine->setPlaying(true);

    qCDebug(novaCore) << "🔊 Spike: reproduciendo" << picos.peaks.size()
                       << "picos";
    actualizarEstado(QStringLiteral("sonando"));
}

void NovaSpikeController::alCargarFallido(const QString& error)
{
    // Un fallo NO destruye lo que está sonando: si el motor tiene un
    // fichero cargado, su forma de onda y su duración se quedan (si no,
    // el cabezal desaparecería mientras el audio sigue sonando y la
    // posición avanzaría contra una duración de cero).
    limpiarOndaSiNoSuena();
    actualizarEstado(error.isEmpty()
                         ? QStringLiteral("no se ha podido cargar el fichero")
                         : error);
}

void NovaSpikeController::publicarOnda(double duracion)
{
    // Los picos van a la caché con el id "spike": es el mismo que pide la
    // página QML (NovaWaveformItem.regionId).
    NovaWaveformCache::setPeaks(QString::fromLatin1(kRegionId), m_cargador->picos());
    actualizarDuracion(duracion);

    // La versión cambia para que el QML repinte la forma de onda: el
    // renderizador solo vuelve a leer la caché cuando se crea.
    ++m_versionPicos;
    Q_EMIT versionPicosChanged();
}

void NovaSpikeController::limpiarOndaPublicada()
{
    NovaWaveformCache::removePeaks(QString::fromLatin1(kRegionId));
    actualizarDuracion(0.0);

    // La versión también cambia al borrar: el QML tiene que repintar la
    // forma de onda (que ahora está vacía). Si no, el renderizador
    // seguiría dibujando la onda anterior (la caché ya no la tiene, pero
    // el renderizador solo la relee cuando se crea).
    ++m_versionPicos;
    Q_EMIT versionPicosChanged();
}

void NovaSpikeController::limpiarOndaSiNoSuena()
{
    if (hayFicheroEnElMotor()) {
        return;
    }
    limpiarOndaPublicada();
}

bool NovaSpikeController::hayFicheroEnElMotor() const
{
    // Lo que decide es si el motor tiene un fichero cargado (el que se
    // está reproduciendo o el último que se cargó), no si el transporte
    // está en marcha: el transporte arranca con el motor, aunque no haya
    // ningún fichero cargado.
    return m_engine && m_engine->tieneFicheroCargado();
}

void NovaSpikeController::stop()
{
    // La carga es asíncrona: si el usuario pulsa "Parar" mientras el
    // fichero se está decodificando, la carga en marcha se tiene que
    // descartar al terminar (si no, la reproducción arrancaría igualmente
    // y el "Parar" no habría servido de nada).
    m_paradaPedida = true;

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

    // Una vez por segundo se dice cuántos fotogramas se han escrito: es el
    // criterio objetivo de la verificación (que los fotogramas escritos
    // crecen). El log se hace aquí, en el hilo de la interfaz: en el
    // callback de audio no se puede (allí no se reservan memoria ni se
    // construyen cadenas).
    const qint64 ahora = m_reloj.elapsed();
    if (ahora - m_ultimoLogMs >= kIntervaloLogMs) {
        m_ultimoLogMs = ahora;
        qCDebug(novaCore) << "🔊 Spike: fotogramas escritos"
                           << (m_engine ? m_engine->currentFrame() : 0)
                           << "posición" << posicion << "s";
    }

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
