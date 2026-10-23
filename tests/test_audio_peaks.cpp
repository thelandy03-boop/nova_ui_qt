// Test del cálculo de picos a partir del PCM ya decodificado.
//
// El renderizador de la forma de onda no sabe leer audio: solo sabe leer
// picos (min, max y rms) que alguien publica en la caché de formas de onda.
// Este test comprueba que buildPeakData() saca esos picos del audio
// decodificado: que la amplitud se respeta, que el silencio da ceros y que
// el silencio del principio no se confunde con el resto del audio.
#include "audio/NovaAudioPeaks.h"
#include "audio/NovaAudioDecoder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <future>
#include <thread>
#include <vector>

// Ruta de los fixtures; la define CMake al compilar este test.
#ifndef NOVA_FIXTURES
#define NOVA_FIXTURES "tests/fixtures"
#endif

static int g_failures = 0;
static int g_checks = 0;

// Cada comprobación lleva su nombre, para que la salida diga que se ha
// comprobado.
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

// Pico absoluto de los picos calculados: el valor más alto (en valor
// absoluto) que tiene el audio según los extremos de cada bin.
float picoDeLosPicos(const std::vector<PeakPoint>& picos)
{
    float pico = 0.0f;
    for (const PeakPoint& p : picos) {
        pico = std::max(pico, std::fabs(p.min));
        pico = std::max(pico, std::fabs(p.max));
    }
    return pico;
}

// ¿Todos los picos son exactamente cero?
bool todosCero(const std::vector<PeakPoint>& picos)
{
    if (picos.empty()) {
        return false;
    }
    for (const PeakPoint& p : picos) {
        if (p.min != 0.0f || p.max != 0.0f || p.rms != 0.0f) {
            return false;
        }
    }
    return true;
}

// Número de bins vacíos (sin muestras: sus picos valen 0, 0, 0).
//
// Un bin sin muestras es un bin que el cálculo no ha rellenado: con el paso
// en fotogramas, en un fichero estéreo (muestras intercaladas) la mitad de
// los bins se quedan vacíos.
int binsEnCero(const std::vector<PeakPoint>& picos)
{
    int vacios = 0;
    for (const PeakPoint& p : picos) {
        if (p.min == 0.0f && p.max == 0.0f && p.rms == 0.0f) {
            ++vacios;
        }
    }
    return vacios;
}

// Audio de 1 s con medio segundo de silencio y después un tono de 440 Hz
// con pico 0,5.
NovaDecodedAudio silencioYTono()
{
    NovaDecodedAudio audio;
    audio.sampleRate = 44100;
    audio.channels = 1;
    audio.samples.resize(44100);
    for (size_t i = 0; i < audio.samples.size(); ++i) {
        const double t = static_cast<double>(i) / 44100.0;
        audio.samples[i] = (t < 0.5)
            ? 0.0f
            : static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979 * 440.0 * t));
    }
    return audio;
}

} // namespace

int main()
{
    std::printf("== Cálculo de picos desde el PCM ==\n");

    // ------------------------------------------------------------------
    // 1. El tono del fixture: los picos tienen su amplitud y hay datos de
    //    detalle y de vista general
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        const bool ok = decodeAudioFile(rutaFixture("tono-1s-44100-mono.wav"),
                                         audio, &error);

        const PeakData data = buildPeakData(audio, 64, 8);
        const float pico = picoDeLosPicos(data.peaks);

        CHECK_NAMED("picos_de_un_tono_coinciden_con_la_amplitud",
                    ok && !data.peaks.empty() && !data.overview.empty()
                        && data.version == PeakData().version
                        && pico >= 0.45f && pico <= 0.55f,
                    "los picos del tono deben llevar su amplitud (0,45 a 0,55) y "
                    "llenar el detalle y la vista general");
    }

    // ------------------------------------------------------------------
    // 2. Un audio de silencio: todos los picos valen cero (ni un -FLT_MAX
    //    sin rellenar ni un resto de ruido)
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio silencio;
        silencio.sampleRate = 44100;
        silencio.channels = 1;
        silencio.samples.assign(44100, 0.0f);

        const PeakData data = buildPeakData(silencio, 64, 8);

        CHECK_NAMED("picos_de_un_silencio_son_cero",
                    todosCero(data.peaks) && todosCero(data.overview),
                    "un audio de ceros debe dar picos con min, max y rms a cero");
    }

    // ------------------------------------------------------------------
    // 3. Un audio que empieza con medio segundo de silencio: el primer
    //    pico es ese silencio y los siguientes ya tienen el tono
    // ------------------------------------------------------------------
    {
        const NovaDecodedAudio audio = silencioYTono();
        const PeakData data = buildPeakData(audio, 64, 8);

        bool hayTonoDespues = false;
        for (size_t i = 1; i < data.peaks.size(); ++i) {
            if (data.peaks[i].max > 0.4f) {
                hayTonoDespues = true;
            }
        }

        CHECK_NAMED("el_primer_pico_no_es_el_silencio_inicial",
                    !data.peaks.empty() && data.peaks[0].max == 0.0f && hayTonoDespues,
                    "el primer pico debe ser el silencio inicial (max 0) y después "
                    "debe aparecer el tono (max mayor que 0,4)");
    }

    // ------------------------------------------------------------------
    // 4. Un fichero estéreo: las muestras van intercaladas (L R L R ...), así
    //    que el paso de los bins se cuenta en MUESTRAS. Con el paso en
    //    fotogramas solo se llenaría la mitad de los bins y la otra mitad
    //    saldría como silencio.
    //
    //    El cálculo va en un hilo con reloj: con el paso equivocado el
    //    helper de picos se queda en un bucle infinito (el índice del bin se
    //    recorta, el del trozo no, y el trozo nunca avanza), y el test se
    //    quedaría colgado en vez de fallar.
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        const bool ok = decodeAudioFile(rutaFixture("tono-1s-44100-estereo.mp3"),
                                         audio, &error);

        auto promesa = std::make_shared<std::promise<PeakData>>();
        std::future<PeakData> resultado = promesa->get_future();
        std::thread([promesa, audio]() {
            promesa->set_value(buildPeakData(audio, 64, 8));
        }).detach();

        bool calculoTerminado = false;
        PeakData data;
        if (resultado.wait_for(std::chrono::seconds(10)) == std::future_status::ready) {
            data = resultado.get();
            calculoTerminado = true;
        }

        const int vacios = binsEnCero(data.peaks);
        const float pico = picoDeLosPicos(data.peaks);

        CHECK_NAMED("picos_de_un_estereo_llenan_todos_los_bins",
                    ok && audio.channels == 2 && calculoTerminado && !data.peaks.empty()
                        && vacios == 0 && pico >= 0.45f && pico <= 0.55f,
                    "en un fichero estéreo todos los bins deben tener muestras "
                    "(con el paso en fotogramas solo se llenaría la mitad y el "
                    "cálculo se queda colgado)");
    }

    // ------------------------------------------------------------------
    // 5. La resolución de los picos se adapta a la duración: un clip corto
    //    no necesita 32.000 puntos, pero un tema largo sí para que al hacer
    //    zoom no se vean escalones. Los topes evitan disparar la memoria.
    // ------------------------------------------------------------------
    {
        CHECK_NAMED("la_resolucion_de_picos_se_adapta_a_la_duracion",
                    novaDetailPeakPoints(1.0) == 8192
                        && novaDetailPeakPoints(60.0) == 10800
                        && novaDetailPeakPoints(600.0) == 32768
                        && novaOverviewPeakPoints(8192) == 512
                        && novaOverviewPeakPoints(32768) == 2048,
                    "el detalle debe tener un mínimo de 8.192 puntos, 180 puntos "
                    "por segundo y un máximo de 32.768; la vista general, entre "
                    "512 y 2.048 puntos");
    }

    std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
