// Test del cargador del fichero del spike.
//
// El cargador decodifica el fichero y le calcula los picos **en un hilo
// aparte** (el spec §8.4 lo exige: "Se calcula en un hilo aparte, nunca en
// el hilo GUI") y el fichero se decodifica **una sola vez**: el audio que
// entrega es el mismo que se pasa al motor de audio, movido (sin copiar el
// PCM).
#include "audio/NovaAudioPlayer.h"
#include "audio/NovaSpikeLoader.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QThreadPool>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <memory>
#include <utility>

// Ruta de los fixtures; la define CMake al compilar este test.
#ifndef NOVA_FIXTURES
#define NOVA_FIXTURES "tests/fixtures"
#endif

static int g_failures = 0;
static int g_checks = 0;

#define CHECK_NAMED(nombre, cond, msg)                                          \
    do {                                                                        \
        ++g_checks;                                                             \
        if (cond) {                                                             \
            std::printf("  [ok] %s\n", (nombre));                               \
        } else {                                                                \
            ++g_failures;                                                       \
            std::printf("  [FALLO] %s: %s  (%s:%d)\n", (nombre), (msg),         \
                        __FILE__, __LINE__);                                    \
        }                                                                       \
    } while (0)

namespace {

QString rutaFixture(const char* nombre)
{
    return QString::fromLatin1(NOVA_FIXTURES) + QLatin1Char('/')
         + QString::fromLatin1(nombre);
}

// Pico absoluto de los picos de detalle: sirve para comprobar que los picos
// son los del audio cargado.
float picoAbsoluto(const PeakData& picos)
{
    float pico = 0.0f;
    for (const PeakPoint& punto : picos.peaks) {
        pico = std::max(pico, std::fabs(punto.min));
        pico = std::max(pico, std::fabs(punto.max));
    }
    return pico;
}

// Espera a que el cargador termine (con un límite de tiempo, para que un
// fallo no deje el test colgado).
bool esperarCarga(NovaSpikeLoader& cargador, bool& fallido, QString& error)
{
    QEventLoop bucle;
    bool terminado = false;

    QObject::connect(&cargador, &NovaSpikeLoader::cargaTerminada, &bucle, [&]() {
        terminado = true;
        bucle.quit();
    });
    QObject::connect(&cargador, &NovaSpikeLoader::cargaFallida, &bucle,
                     [&](const QString& mensaje) {
                         fallido = true;
                         error = mensaje;
                         bucle.quit();
                     });

    QTimer::singleShot(20000, &bucle, &QEventLoop::quit);

    cargador.cargar(rutaFixture("tono-1s-44100-mono.wav"));
    bucle.exec();

    return terminado;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    std::printf("== Cargador del spike ==\n");

    // ------------------------------------------------------------------
    // 1. La carga termina con el audio decodificado y los picos calculados
    // ------------------------------------------------------------------
    NovaSpikeLoader cargador;
    bool fallido = false;
    QString error;
    const bool terminado = esperarCarga(cargador, fallido, error);

    CHECK_NAMED("la_carga_termina_con_el_audio_y_los_picos",
                terminado && !fallido && cargador.cargado()
                    && cargador.audio() != nullptr
                    && !cargador.picos().peaks.empty()
                    && !cargador.picos().overview.empty(),
                "cargar el WAV del fixture tiene que terminar con el audio decodificado y los "
                "picos calculados (detalle y vista general)");

    // ------------------------------------------------------------------
    // 2. El trabajo pesado se hace en un hilo aparte, nunca en el hilo de
    //    la interfaz (spec §8.4)
    // ------------------------------------------------------------------
    CHECK_NAMED("los_picos_se_calculan_fuera_del_hilo_de_la_interfaz",
                cargador.hiloTrabajo() != nullptr
                    && cargador.hiloTrabajo() != QThread::currentThread(),
                "decodificar y calcular los picos se tiene que hacer en un hilo de trabajo, "
                "no en el hilo de la interfaz");

    // ------------------------------------------------------------------
    // 3. Los picos son los del audio cargado (el fixture es un tono de
    //    amplitud 0,5)
    // ------------------------------------------------------------------
    CHECK_NAMED("los_picos_valen_para_el_audio_cargado",
                picoAbsoluto(cargador.picos()) > 0.45f
                    && picoAbsoluto(cargador.picos()) < 0.55f,
                "el pico de los picos debe ser la amplitud del audio (0,5)");

    // ------------------------------------------------------------------
    // 4. El audio se entrega al motor **movido**: no se copia el PCM (un
    //    fichero estéreo de cinco minutos son unos 115 MB)
    // ------------------------------------------------------------------
    NovaDecodedAudio audio = cargador.tomarAudio();
    const bool tomado = (cargador.audio() == nullptr) && !audio.samples.empty()
                        && audio.sampleRate == 44100 && audio.channels == 1;

    NovaAudioPlayer reproductor;
    QString errorReproductor;
    const bool cargado = reproductor.load(std::move(audio), &errorReproductor);

    CHECK_NAMED("el_audio_se_pasa_al_reproductor_sin_copiar",
                tomado && cargado && errorReproductor.isEmpty()
                    && audio.samples.empty() && reproductor.channels() == 1
                    && reproductor.durationFrames() > 0,
                "el audio se tiene que mover al reproductor (las muestras no se copian: tras el "
                "movimiento el origen queda vacío)");

    // ------------------------------------------------------------------
    // 5. Un fichero que no existe: la carga falla con un error legible
    // ------------------------------------------------------------------
    {
        NovaSpikeLoader cargadorFallo;
        QEventLoop bucle;
        bool fallo = false;
        QString mensaje;

        QObject::connect(&cargadorFallo, &NovaSpikeLoader::cargaFallida, &bucle,
                         [&](const QString& texto) {
                             fallo = true;
                             mensaje = texto;
                             bucle.quit();
                         });
        QTimer::singleShot(20000, &bucle, &QEventLoop::quit);

        cargadorFallo.cargar(QStringLiteral("/no/existe/este/fichero.mp3"));
        bucle.exec();

        CHECK_NAMED("un_fichero_inexistente_falla_con_error",
                    fallo && !mensaje.isEmpty() && !cargadorFallo.cargado(),
                    "un fichero que no existe tiene que fallar la carga con un mensaje legible");
    }

    // ------------------------------------------------------------------
    // 6. Si se pide otro fichero mientras el primero se está cargando, el
    //    resultado del primero se descarta: el estado tiene que ser el del
    //    último fichero pedido (no una onda que no es la del fichero).
    // ------------------------------------------------------------------
    {
        NovaSpikeLoader cargador;
        QEventLoop bucle;
        int terminadas = 0;

        QObject::connect(&cargador, &NovaSpikeLoader::cargaTerminada, &bucle, [&]() {
            ++terminadas;
            bucle.quit();
        });
        QObject::connect(&cargador, &NovaSpikeLoader::cargaFallida, &bucle,
                         [&]() { bucle.quit(); });
        QTimer::singleShot(20000, &bucle, &QEventLoop::quit);

        // El primero se pide y, sin esperar a que termine, el segundo: el
        // resultado del primero se tiene que descartar.
        cargador.cargar(rutaFixture("tono-1s-44100-mono.wav"));
        cargador.cargar(rutaFixture("tono-1s-44100-estereo.mp3"));
        bucle.exec();

        const std::shared_ptr<NovaDecodedAudio>& audio = cargador.audio();
        CHECK_NAMED("el_resultado_de_una_carga_antigua_se_descarta",
                    terminadas == 1 && cargador.cargado() && audio
                        && audio->channels == 2,
                    "al pedir un segundo fichero, el resultado del primero se tiene que descartar: "
                    "solo se publica el del último fichero pedido");
    }

    // ------------------------------------------------------------------
    // 7. Destruir el cargador mientras la carga está en marcha: el
    //    resultado se descarta y no puede haber acceso a memoria liberada.
    //
    //    El cargador se destruye en el hilo de la interfaz mientras el
    //    hilo de trabajo sigue decodificando: el trabajo no puede mirar
    //    el puntero del cargador (si lo mirara, leería memoria ya
    //    liberada). Con ASan, un acceso así se vería como un
    //    "heap-use-after-free".
    //
    //    El cargador se destruye en momentos distintos a propósito: unas
    //    veces nada más empezar (el hilo de trabajo todavía no ha tocado
    //    el puntero) y otras cuando ya está decodificando (el momento
    //    peligroso: el trabajo está a punto de entregar el resultado).
    // ------------------------------------------------------------------
    {
        for (int intento = 0; intento < 40; ++intento) {
            std::unique_ptr<NovaSpikeLoader> cargador =
                std::make_unique<NovaSpikeLoader>();
            cargador->cargar(rutaFixture("tono-1s-44100-mono.wav"));
            if (intento % 2 == 1) {
                // Se deja que el hilo de trabajo empiece a decodificar
                // antes de destruir el cargador.
                QThread::msleep(static_cast<unsigned long>(intento % 5));
            }
            // Se destruye el cargador con la carga en marcha: el hilo de
            // trabajo sigue, y su resultado ya no tiene a quién entregar.
            cargador.reset();
        }
        // Los hilos de trabajo tienen que terminar (el resultado se
        // descarta: el cargador ya no existe). waitForDone() devuelve
        // false si se agota el tiempo: eso significaría que el candado
        // del estado de entrega se ha quedado pillado.
        const bool terminado = QThreadPool::globalInstance()->waitForDone(30000);

        CHECK_NAMED("destruir_el_cargador_en_marcha_no_toca_memoria_liberada",
                    terminado,
                    "destruir el cargador mientras la carga está en marcha no puede dar acceso a "
                    "memoria liberada (el hilo de trabajo no toca el puntero del cargador) ni "
                    "dejar pillado el candado del estado de entrega");
    }

    // ------------------------------------------------------------------
    // 8. Después de destruir cargadores con la carga en marcha, un
    //    cargador nuevo tiene que funcionar igual: el estado de entrega
    //    es de cada cargador (uno destruido no puede fastidiar a los
    //    siguientes).
    // ------------------------------------------------------------------
    {
        NovaSpikeLoader cargador;
        bool fallido = false;
        QString error;
        const bool terminado = esperarCarga(cargador, fallido, error);

        CHECK_NAMED("el_cargador_sigue_funcionando_despues_de_destruir_otros",
                    terminado && !fallido && cargador.cargado()
                        && !cargador.picos().peaks.empty(),
                    "un cargador nuevo tiene que cargar con normalidad después de que se hayan "
                    "destruido otros con la carga en marcha");
    }

    std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
