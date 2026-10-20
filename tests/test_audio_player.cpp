// Test del reproductor de audio y del remuestreo.
//
// El reproductor es la pieza que en el móvil alimentará el callback de Oboe:
// toma el audio ya decodificado, lo remuestrea a la tasa de salida (48 kHz en
// el móvil) y entrega fotogramas intercalados listos para escribir en el buffer
// del stream. Aquí se comprueba lo que de verdad importa:
//
//   - la duración se calcula con la tasa de salida vigente, no con la del
//     fichero (el motor pide 48 kHz aunque el fichero sea de 44,1 kHz);
//   - renderFrames() avanza la posición y entrega audio de verdad, no silencio;
//   - al agotarse el fichero, renderFrames() devuelve 0 y la posición no pasa
//     de la duración: es lo que evita que el callback de Oboe lea fuera del
//     buffer (el overrun que rompe el audio en el móvil).
#include "audio/NovaAudioDecoder.h"
#include "audio/NovaAudioPlayer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// Ruta de los fixtures; la define CMake al compilar este test.
#ifndef NOVA_FIXTURES
#define NOVA_FIXTURES "tests/fixtures"
#endif

static int g_failures = 0;
static int g_checks = 0;

// Cada comprobación lleva su nombre, para que la salida diga qué se ha
// comprobado. Los nombres de las siete comprobaciones del enunciado se
// respetan tal cual; las tres que se añaden comprueban cosas que el
// enunciado enuncia (el error de load(), la posición fraccionaria que
// evita los clics y el remuestreo por canal) pero que no verificaba.
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

// Decodifica el WAV del fixture (1 s de mono a 44,1 kHz).
bool decodificarFixture(NovaDecodedAudio& audio, QString* error)
{
    return decodeAudioFile(rutaFixture("tono-1s-44100-mono.wav"), audio, error);
}

// Pico absoluto de un buffer de muestras: sirve para comprobar que el audio
// escrito no es silencio.
float picoAbsoluto(const std::vector<float>& muestras)
{
    float pico = 0.0f;
    for (float m : muestras) {
        const float absoluto = std::fabs(m);
        if (absoluto > pico) {
            pico = absoluto;
        }
    }
    return pico;
}

// Fotogramas de salida que le tocan a un fichero de `fotogramas` muestras
// cuando la salida va a `tasaSalida` Hz.
qint64 fotogramasDeSalida(qint64 fotogramas, int tasaEntrada, int tasaSalida)
{
    const double duracion = static_cast<double>(fotogramas)
                          * static_cast<double>(tasaSalida)
                          / static_cast<double>(tasaEntrada);
    return static_cast<qint64>(std::llround(duracion));
}

// Número de cruces por cero de un canal de una señal intercalada: sirve para
// saber qué frecuencia lleva cada canal.
int crucesPorCero(const std::vector<float>& muestras, int canal, int canales)
{
    int cruces = 0;
    bool anteriorNegativa = false;
    bool primero = true;
    for (size_t i = static_cast<size_t>(canal); i < muestras.size();
         i += static_cast<size_t>(canales)) {
        const bool negativa = muestras[i] < 0.0f;
        if (!primero && negativa != anteriorNegativa) {
            ++cruces;
        }
        anteriorNegativa = negativa;
        primero = false;
    }
    return cruces;
}

} // namespace

int main()
{
    std::printf("== Reproductor de audio ==\n");

    // ------------------------------------------------------------------
    // 1. El WAV del fixture se carga y la duración se calcula con la tasa
    //    de salida por defecto (48 kHz): 44.100 × (48000/44100) = 48.000
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        const bool decodificado = decodificarFixture(audio, &error);

        NovaAudioPlayer reproductor;
        const bool cargado = reproductor.load(audio, &error);
        const qint64 duracion = reproductor.durationFrames();

        CHECK_NAMED("carga_acepta_un_fichero_decodificado",
                    decodificado && cargado && error.isEmpty()
                        && duracion == fotogramasDeSalida(audio.frameCount(), 44100, 48000)
                        && duracion >= 47520 && duracion <= 48480,
                    "el WAV del fixture debe cargarse y durar 48.000 fotogramas a 48 kHz (+/- 1 %)");
    }

    // ------------------------------------------------------------------
    // 2. Tras play(), renderFrames() escribe los frames pedidos, avanza la
    //    posición y entrega audio de verdad (no silencio)
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        decodificarFixture(audio, &error);

        NovaAudioPlayer reproductor;
        reproductor.load(audio, &error);
        reproductor.play();

        std::vector<float> buffer(512, 0.0f);
        const int escritos = reproductor.renderFrames(buffer.data(), 512);

        CHECK_NAMED("render_escribe_frames_y_avanza_la_posicion",
                    escritos == 512 && reproductor.positionFrames() == 512
                        && picoAbsoluto(buffer) > 0.01f,
                    "tras play(), renderFrames debe escribir 512 fotogramas, avanzar la posición "
                    "y no escribir silencio");
    }

    // ------------------------------------------------------------------
    // 3. Al agotarse el fichero: la última llamada devuelve 0 y la posición
    //    no pasa de la duración (el overrun que rompe el audio en el móvil)
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        decodificarFixture(audio, &error);

        NovaAudioPlayer reproductor;
        reproductor.load(audio, &error);
        reproductor.play();

        const qint64 objetivo = reproductor.durationFrames() + 1000;
        std::vector<float> buffer(512, 0.0f);
        qint64 escritos = 0;
        int ultimaLlamada = -1;
        while (escritos < objetivo) {
            const int n = reproductor.renderFrames(buffer.data(), 512);
            ultimaLlamada = n;
            escritos += n;
            if (n == 0) {
                break;
            }
        }

        CHECK_NAMED("render_tras_el_fin_no_lee_fuera",
                    ultimaLlamada == 0 && escritos == reproductor.durationFrames()
                        && reproductor.positionFrames() <= reproductor.durationFrames()
                        && !reproductor.isPlaying(),
                    "al agotarse el fichero, renderFrames debe devolver 0 y la posición no debe "
                    "pasar de la duración");
    }

    // ------------------------------------------------------------------
    // 4. La duración en segundos no cambia al cambiar la tasa de salida;
    //    solo cambia el número de fotogramas (remuestreo, no recorte)
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        decodificarFixture(audio, &error);

        NovaAudioPlayer reproductor;
        reproductor.load(audio, &error);

        const double duracion48 = reproductor.durationSeconds();
        const qint64 fotogramas48 = reproductor.durationFrames();

        reproductor.setOutputSampleRate(44100);
        const double duracion44 = reproductor.durationSeconds();
        const qint64 fotogramas44 = reproductor.durationFrames();

        CHECK_NAMED("remuestreo_cambia_la_duracion_esperada",
                    std::fabs(duracion48 - 1.0) <= 0.01 && std::fabs(duracion44 - 1.0) <= 0.01
                        && fotogramas48 == fotogramasDeSalida(audio.frameCount(), 44100, 48000)
                        && fotogramas44 == fotogramasDeSalida(audio.frameCount(), 44100, 44100),
                    "la duración debe ser ~1 s tanto a 48 kHz como a 44,1 kHz; solo cambia el "
                    "número de fotogramas");
    }

    // ------------------------------------------------------------------
    // 5. Un fichero más corto que un bloque: se entrega lo que hay y no se
    //    escribe nada más en el buffer del llamante
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        audio.sampleRate = 48000;
        audio.channels = 1;
        audio.samples.assign(10, 0.5f);

        NovaAudioPlayer reproductor;
        QString error;
        reproductor.load(audio, &error);
        reproductor.play();

        std::vector<float> buffer(512, -7.0f);
        const int escritos = reproductor.renderFrames(buffer.data(), 512);

        bool intacto = true;
        for (size_t i = static_cast<size_t>(escritos); i < buffer.size(); ++i) {
            if (buffer[i] != -7.0f) {
                intacto = false;
            }
        }

        CHECK_NAMED("fichero_mas_corto_que_un_bloque",
                    escritos == 10 && reproductor.positionFrames() == 10
                        && reproductor.durationFrames() == 10 && intacto,
                    "un fichero de 10 fotogramas debe escribir 10 fotogramas y no tocar el resto "
                    "del buffer");
    }

    // ------------------------------------------------------------------
    // 6. stop() detiene la reproducción
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        decodificarFixture(audio, &error);

        NovaAudioPlayer reproductor;
        reproductor.load(audio, &error);
        reproductor.play();

        std::vector<float> buffer(512, 0.0f);
        const int antes = reproductor.renderFrames(buffer.data(), 512);
        reproductor.stop();
        const int despues = reproductor.renderFrames(buffer.data(), 512);

        CHECK_NAMED("stop_para_la_reproduccion",
                    antes == 512 && despues == 0 && !reproductor.isPlaying(),
                    "tras stop(), renderFrames debe devolver 0 y el reproductor no debe estar sonando");
    }

    // ------------------------------------------------------------------
    // 7. Sin load() no se escribe nada
    // ------------------------------------------------------------------
    {
        NovaAudioPlayer reproductor;

        std::vector<float> buffer(512, -7.0f);
        const int escritos = reproductor.renderFrames(buffer.data(), 512);

        bool intacto = true;
        for (float m : buffer) {
            if (m != -7.0f) {
                intacto = false;
            }
        }

        CHECK_NAMED("sin_cargar_no_escribe_nada", escritos == 0 && intacto,
                    "sin load(), renderFrames debe devolver 0 y no escribir en el buffer");
    }

    // ------------------------------------------------------------------
    // 8. Un audio que no tiene canales ni tasa de muestreo no se puede
    //    reproducir: load() falla con un error claro
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio; // sin decodificar: 0 canales y 0 Hz

        NovaAudioPlayer reproductor;
        QString error;
        const bool cargado = reproductor.load(audio, &error);

        CHECK_NAMED("carga_rechaza_un_audio_invalido", !cargado && !error.isEmpty(),
                    "un audio sin canales ni tasa de muestreo no se puede cargar: load() debe "
                    "fallar con un error claro");
    }

    // ------------------------------------------------------------------
    // 9. La posición fraccionaria del remuestreador se conserva entre
    //    llamadas: el audio es el mismo se pida en un bloque o en muchos
    //    (si se perdiera, aparecerían clics en las uniones)
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        decodificarFixture(audio, &error);

        NovaAudioPlayer enUnBloque;
        enUnBloque.load(audio, &error);
        enUnBloque.play();
        std::vector<float> todo(48000, 0.0f);
        const int total = enUnBloque.renderFrames(todo.data(), 48000);

        NovaAudioPlayer enBloques;
        enBloques.load(audio, &error);
        enBloques.play();
        std::vector<float> trozo(64, 0.0f);
        std::vector<float> partido;
        while (true) {
            const int n = enBloques.renderFrames(trozo.data(), 64);
            partido.insert(partido.end(), trozo.begin(), trozo.begin() + n);
            if (n == 0) {
                break;
            }
        }

        float maxDiferencia = 0.0f;
        const size_t comunes = std::min(todo.size(), partido.size());
        for (size_t i = 0; i < comunes; ++i) {
            maxDiferencia = std::max(maxDiferencia, std::fabs(todo[i] - partido[i]));
        }

        CHECK_NAMED("el_remuestreo_no_tiene_discontinuidades",
                    total == 48000 && partido.size() == static_cast<size_t>(total)
                        && maxDiferencia < 0.000001f,
                    "el audio debe ser el mismo se pida en un bloque o en muchos: la posición "
                    "fraccionaria del remuestreador debe conservarse entre llamadas");
    }

    // ------------------------------------------------------------------
    // 10. En un fichero estéreo los canales se remuestrean por separado: el
    //     izquierdo sigue a 440 Hz y el derecho a 880 Hz (si se mezclaran, los
    //     canales se irían desplazando uno respecto del otro)
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        decodeAudioFile(rutaFixture("tono-1s-44100-estereo.mp3"), audio, &error);

        NovaAudioPlayer reproductor;
        reproductor.load(audio, &error);
        reproductor.play();

        std::vector<float> salida(48000 * 2, 0.0f);
        const int escritos = reproductor.renderFrames(salida.data(), 48000);
        salida.resize(static_cast<size_t>(escritos) * 2);

        const int crucesIzquierdo = crucesPorCero(salida, 0, 2);
        const int crucesDerecho = crucesPorCero(salida, 1, 2);

        CHECK_NAMED("los_canales_no_se_mezclan",
                    audio.channels == 2 && escritos == 48000
                        && crucesIzquierdo > 800 && crucesIzquierdo < 960
                        && crucesDerecho > 1600 && crucesDerecho < 1900,
                    "tras remuestrear a 48 kHz, el canal izquierdo debe seguir a 440 Hz y el "
                    "derecho a 880 Hz");
    }

    std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
