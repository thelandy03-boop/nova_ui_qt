// Test del decodificador de audio.
//
// El motor de Nova no puede usar las fuentes de fichero de Ardour (ni
// libsndfile ni mpg123): en Android no existen. Por eso el motor lleva su
// propio decodificador (WAV a mano y MP3 con minimp3) y aqui se comprueba
// que funciona con ficheros reales: duracion y amplitud correctas, y errores
// claros cuando el fichero no existe, no es audio o esta truncado.
#include "audio/NovaAudioDecoder.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Ruta de los fixtures; la define CMake al compilar este test.
#ifndef NOVA_FIXTURES
#define NOVA_FIXTURES "tests/fixtures"
#endif

static int g_failures = 0;
static int g_checks = 0;

// Cada comprobacion lleva su nombre, para que la salida diga que se ha
// comprobado. Los nombres de las ocho comprobaciones del enunciado se
// respetan tal cual.
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

// Pico absoluto de la senal: sirve para comprobar que la amplitud se respeta.
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

// Pico de uno de los canales de una senal intercalada.
float picoCanal(const std::vector<float>& muestras, int canal, int canales)
{
    float pico = 0.0f;
    for (size_t i = static_cast<size_t>(canal); i < muestras.size(); i += static_cast<size_t>(canales)) {
        const float absoluto = std::fabs(muestras[i]);
        if (absoluto > pico) {
            pico = absoluto;
        }
    }
    return pico;
}

// Numero de cruces por cero de un canal: sirve para saber que frecuencia
// lleva cada canal de una senal intercalada.
int crucesPorCero(const std::vector<float>& muestras, int canal, int canales)
{
    int cruces = 0;
    bool anteriorNegativa = false;
    bool primero = true;
    for (size_t i = static_cast<size_t>(canal); i < muestras.size(); i += static_cast<size_t>(canales)) {
        const bool negativa = muestras[i] < 0.0f;
        if (!primero && negativa != anteriorNegativa) {
            ++cruces;
        }
        anteriorNegativa = negativa;
        primero = false;
    }
    return cruces;
}

// Escribe en un temporal los primeros bytes del MP3, para probar un fichero
// cortado por la mitad.
std::filesystem::path escribirTemporalMp3(size_t bytes)
{
    const std::filesystem::path destino =
        std::filesystem::temp_directory_path() / "nova_audio_truncado.mp3";

    std::ifstream entrada(rutaFixture("tono-1s-44100-mono.mp3").toStdString(),
                          std::ios::binary);
    std::vector<char> datos(bytes);
    if (entrada) {
        entrada.read(datos.data(), static_cast<std::streamsize>(bytes));
        datos.resize(static_cast<size_t>(entrada.gcount()));
    }

    std::ofstream salida(destino.string(), std::ios::binary | std::ios::trunc);
    if (salida && !datos.empty()) {
        salida.write(datos.data(), static_cast<std::streamsize>(datos.size()));
    }
    salida.close();

    return destino;
}

void anadirU16(std::vector<unsigned char>& bytes, unsigned int valor)
{
    bytes.push_back(static_cast<unsigned char>(valor & 0xFFu));
    bytes.push_back(static_cast<unsigned char>((valor >> 8) & 0xFFu));
}

void anadirU32(std::vector<unsigned char>& bytes, unsigned int valor)
{
    bytes.push_back(static_cast<unsigned char>(valor & 0xFFu));
    bytes.push_back(static_cast<unsigned char>((valor >> 8) & 0xFFu));
    bytes.push_back(static_cast<unsigned char>((valor >> 16) & 0xFFu));
    bytes.push_back(static_cast<unsigned char>((valor >> 24) & 0xFFu));
}

// Escribe un WAV ya montado en disco (corrigiendo el tamano del RIFF).
void escribirWav(const std::filesystem::path& destino,
                 std::vector<unsigned char>& fichero)
{
    const unsigned int tamano = static_cast<unsigned int>(fichero.size() - 8);
    fichero[4] = static_cast<unsigned char>(tamano & 0xFFu);
    fichero[5] = static_cast<unsigned char>((tamano >> 8) & 0xFFu);
    fichero[6] = static_cast<unsigned char>((tamano >> 16) & 0xFFu);
    fichero[7] = static_cast<unsigned char>((tamano >> 24) & 0xFFu);

    std::ofstream salida(destino.string(), std::ios::binary | std::ios::trunc);
    if (salida) {
        salida.write(reinterpret_cast<const char*>(fichero.data()),
                     static_cast<std::streamsize>(fichero.size()));
    }
    salida.close();
}

// Escribe un WAV de prueba (un seno de 440 Hz con pico 0,5) con el formato
// de muestra que se le pida, para comprobar los tipos que admite el lector.
std::filesystem::path escribirWavTemporal(int tag, int bits, bool extensible)
{
    const std::filesystem::path destino =
        std::filesystem::temp_directory_path() / "nova_audio_wav_temporal.wav";

    const int muestras = 4410; // 0,1 s
    std::vector<unsigned char> datos;
    for (int i = 0; i < muestras; ++i) {
        const double valor = 0.5 * std::sin(2.0 * 3.14159265358979 * 440.0 * i / 44100.0);
        if (bits == 24) {
            const int v = static_cast<int>(valor * 8388608.0);
            datos.push_back(static_cast<unsigned char>(v & 0xFF));
            datos.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
            datos.push_back(static_cast<unsigned char>((v >> 16) & 0xFF));
        } else if (tag == 3) {
            const float v = static_cast<float>(valor);
            unsigned char crudos[4];
            std::memcpy(crudos, &v, sizeof(crudos));
            datos.insert(datos.end(), crudos, crudos + sizeof(crudos));
        } else {
            const int v = static_cast<int>(valor * 32767.0);
            datos.push_back(static_cast<unsigned char>(v & 0xFF));
            datos.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
        }
    }

    std::vector<unsigned char> fichero;
    fichero.insert(fichero.end(), "RIFF", "RIFF" + 4);
    anadirU32(fichero, 0); // se rellena al final
    fichero.insert(fichero.end(), "WAVE", "WAVE" + 4);
    fichero.insert(fichero.end(), "fmt ", "fmt " + 4);
    if (extensible) {
        anadirU32(fichero, 40);
        anadirU16(fichero, 0xFFFE);
    } else {
        anadirU32(fichero, 16);
        anadirU16(fichero, static_cast<unsigned int>(tag));
    }
    anadirU16(fichero, 1);                        // canales
    anadirU32(fichero, 44100);                    // tasa
    anadirU32(fichero, 44100u * static_cast<unsigned int>(bits) / 8u); // bytes por segundo
    anadirU16(fichero, static_cast<unsigned int>(bits) / 8u);         // alineacion del bloque
    anadirU16(fichero, static_cast<unsigned int>(bits));             // bits por muestra
    if (extensible) {
        anadirU16(fichero, 22);  // tamano de la extension
        anadirU16(fichero, static_cast<unsigned int>(bits)); // bits validos
        anadirU32(fichero, 3);   // mascara de canales
        anadirU16(fichero, static_cast<unsigned int>(tag)); // subformato real
        anadirU16(fichero, 0);
        const unsigned char cola[] = {0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00,
                                      0xAA, 0x00, 0x38, 0x9B, 0x71};
        fichero.insert(fichero.end(), cola, cola + sizeof(cola));
    }
    fichero.insert(fichero.end(), "data", "data" + 4);
    anadirU32(fichero, static_cast<unsigned int>(datos.size()));
    fichero.insert(fichero.end(), datos.begin(), datos.end());

    escribirWav(destino, fichero);
    return destino;
}

// Escribe un WAV cuya cabecera declara un chunk 'data' mucho mas grande que
// el fichero. Es un fichero roto: el motor no debe fiarse de ese tamano
// (reservar memoria para el lo mataria) sino fallar con un error.
std::filesystem::path escribirWavConTamanoMentiroso()
{
    const std::filesystem::path destino =
        std::filesystem::temp_directory_path() / "nova_audio_wav_mentiroso.wav";

    std::vector<unsigned char> fichero;
    fichero.insert(fichero.end(), "RIFF", "RIFF" + 4);
    anadirU32(fichero, 36);
    fichero.insert(fichero.end(), "WAVE", "WAVE" + 4);
    fichero.insert(fichero.end(), "fmt ", "fmt " + 4);
    anadirU32(fichero, 16);
    anadirU16(fichero, 1);   // PCM
    anadirU16(fichero, 1);   // canales
    anadirU32(fichero, 44100);
    anadirU32(fichero, 88200);
    anadirU16(fichero, 2);   // alineacion del bloque
    anadirU16(fichero, 16);  // bits por muestra
    fichero.insert(fichero.end(), "data", "data" + 4);
    anadirU32(fichero, 0x7FFFF000u); // casi 2 GiB de datos que no existen

    escribirWav(destino, fichero);
    return destino;
}

} // namespace

int main()
{
    std::printf("== Decodificador de audio ==\n");

    // ------------------------------------------------------------------
    // 1. El WAV se decodifica entero: 1 s de mono a 44,1 kHz
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        const bool ok = decodeAudioFile(rutaFixture("tono-1s-44100-mono.wav"),
                                         audio, &error);

        CHECK_NAMED("wav_se_decodifica_completo",
                    ok && error.isEmpty() && audio.sampleRate == 44100
                        && audio.channels == 1 && audio.frameCount() >= 44090
                        && audio.frameCount() <= 44110,
                    "el WAV debe decodificarse entero: 44100 Hz, mono, 44100 fotogramas y sin error");
    }

    // ------------------------------------------------------------------
    // 2. El WAV conserva la amplitud: el fixture es una senal de pico 0,5
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        decodeAudioFile(rutaFixture("tono-1s-44100-mono.wav"), audio, &error);

        const float pico = picoAbsoluto(audio.samples);
        CHECK_NAMED("wav_preserva_la_amplitud", pico >= 0.45f && pico <= 0.55f,
                    "el pico del WAV debe estar entre 0,45 y 0,55");
    }

    // ------------------------------------------------------------------
    // 3. Los demas tipos de muestra del WAV tambien se decodifican
    //    (PCM de 24 bits, coma flotante de 32 y WAVE_FORMAT_EXTENSIBLE)
    // ------------------------------------------------------------------
    {
        struct Variante {
            const char* nombre;
            int tag;
            int bits;
            bool extensible;
        };
        const Variante variantes[] = {
            {"wav_pcm_de_24_bits", 1, 24, false},
            {"wav_float_de_32_bits", 3, 32, false},
            {"wav_extensible", 1, 16, true},
        };

        for (const Variante& variante : variantes) {
            const std::filesystem::path fichero = escribirWavTemporal(
                variante.tag, variante.bits, variante.extensible);

            NovaDecodedAudio audio;
            QString error;
            const bool ok = decodeAudioFile(QString::fromStdString(fichero.string()),
                                             audio, &error);
            const float pico = picoAbsoluto(audio.samples);

            CHECK_NAMED(variante.nombre,
                        ok && audio.frameCount() == 4410 && pico >= 0.45f && pico <= 0.55f,
                        "el WAV de prueba debe decodificarse con todos sus fotogramas y su amplitud");

            std::filesystem::remove(fichero);
        }
    }

    // ------------------------------------------------------------------
    // 4. El MP3 se decodifica entero: 1 s de mono a 44,1 kHz
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        const bool ok = decodeAudioFile(rutaFixture("tono-1s-44100-mono.mp3"),
                                         audio, &error);

        CHECK_NAMED("mp3_se_decodifica_completo",
                    ok && error.isEmpty() && audio.sampleRate == 44100 && audio.channels == 1,
                    "el MP3 debe decodificarse entero: 44100 Hz, mono y sin error");
    }

    // ------------------------------------------------------------------
    // 5. El tag ID3v2 no cuenta como audio y la duracion es la real
    //    (el +/- 2 % absorbe el retardo y el relleno del codificador)
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        decodeAudioFile(rutaFixture("tono-1s-44100-mono.mp3"), audio, &error);

        const qint64 fotogramas = audio.frameCount();
        CHECK_NAMED("mp3_con_tag_id3_da_duracion_real",
                    fotogramas >= 43218 && fotogramas <= 44982,
                    "el MP3 debe durar lo mismo que el WAV (44100 +/- 2 %), sin contar el tag ID3");
    }

    // ------------------------------------------------------------------
    // 6. El MP3 conserva la amplitud (el codificador con losses puede quedarse
    //    algo corto, por eso el margen es mayor que en el WAV)
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        decodeAudioFile(rutaFixture("tono-1s-44100-mono.mp3"), audio, &error);

        const float pico = picoAbsoluto(audio.samples);
        CHECK_NAMED("mp3_preserva_la_amplitud", pico >= 0.40f && pico <= 0.55f,
                    "el pico del MP3 debe estar entre 0,40 y 0,55");
    }

    // ------------------------------------------------------------------
    // 7. Un MP3 en estereo: los dos canales van intercalados y cada uno
    //    lleva su propia senal (izquierdo a 440 Hz, derecho a 880 Hz)
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        const bool ok = decodeAudioFile(rutaFixture("tono-1s-44100-estereo.mp3"),
                                         audio, &error);

        CHECK_NAMED("mp3_estereo_se_decodifica_completo",
                    ok && audio.channels == 2 && audio.frameCount() >= 43218
                        && audio.frameCount() <= 44982,
                    "el MP3 en estereo debe decodificarse entero: 2 canales y 44100 fotogramas (+/- 2 %)");
        CHECK_NAMED("mp3_estereo_conserva_la_amplitud",
                    picoCanal(audio.samples, 0, 2) >= 0.40f && picoCanal(audio.samples, 0, 2) <= 0.55f
                        && picoCanal(audio.samples, 1, 2) >= 0.30f && picoCanal(audio.samples, 1, 2) <= 0.45f,
                    "cada canal del MP3 en estereo debe conservar su amplitud");

        // 440 Hz y 880 Hz en 1 s son 880 y 1760 cruces por cero.
        const int crucesIzquierdo = crucesPorCero(audio.samples, 0, 2);
        const int crucesDerecho = crucesPorCero(audio.samples, 1, 2);
        CHECK_NAMED("mp3_estereo_intercala_los_canales",
                    crucesIzquierdo > 700 && crucesIzquierdo < 1050
                        && crucesDerecho > 1400 && crucesDerecho < 2100,
                    "los canales deben ir intercalados: 880 cruces en el izquierdo y 1760 en el derecho");
    }

    // ------------------------------------------------------------------
    // 8. Un fichero que no existe: error claro, nunca exito
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        const bool ok = decodeAudioFile(rutaFixture("no-existe-este-fichero.wav"),
                                         audio, &error);

        CHECK_NAMED("archivo_inexistente_devuelve_error", !ok && !error.isEmpty(),
                    "un fichero inexistente no puede decodificarse y debe explicar el error");
    }

    // ------------------------------------------------------------------
    // 9. Un fichero que no es audio: error claro, nunca exito
    // ------------------------------------------------------------------
    {
        NovaDecodedAudio audio;
        QString error;
        const bool ok = decodeAudioFile(rutaFixture("no-es-audio.txt"),
                                         audio, &error);

        CHECK_NAMED("archivo_no_audio_devuelve_error", !ok && !error.isEmpty(),
                    "un fichero de texto no puede decodificarse como audio y debe explicar el error");
    }

    // ------------------------------------------------------------------
    // 10. Un MP3 truncado: no debe colgarse ni leer fuera. Un DAW prefiere un
    //     error claro a un audio a medias, asi que se exige error, no un
    //     exito con menos fotogramas.
    // ------------------------------------------------------------------
    {
        const std::filesystem::path truncado = escribirTemporalMp3(1024);

        NovaDecodedAudio audio;
        QString error;
        const bool ok = decodeAudioFile(QString::fromStdString(truncado.string()),
                                         audio, &error);

        CHECK_NAMED("mp3_truncado_no_provoca_crash",
                    !ok && !error.isEmpty() && !(ok && audio.frameCount() == 0),
                    "un MP3 truncado debe devolver error, no un audio a medias, y nunca exito sin fotogramas");

        std::filesystem::remove(truncado);
    }

    // ------------------------------------------------------------------
    // 11. Un MP3 cortado justo en un borde de frame: tampoco hay bytes
    //     sueltos al final, pero el fichero esta incompleto y se nota
    // ------------------------------------------------------------------
    {
        // 722 bytes = la etiqueta ID3 (97) + el frame Xing (208) + 2 frames
        // de audio: el corte cae justo en un borde de frame.
        const std::filesystem::path truncado = escribirTemporalMp3(722);

        NovaDecodedAudio audio;
        QString error;
        const bool ok = decodeAudioFile(QString::fromStdString(truncado.string()),
                                         audio, &error);

        CHECK_NAMED("mp3_truncado_en_borde_de_frame_devuelve_error",
                    !ok && !error.isEmpty(),
                    "un MP3 cortado en un borde de frame debe devolver error, no un audio a medias");

        std::filesystem::remove(truncado);
    }

    // ------------------------------------------------------------------
    // 12. Un WAV que declara mas datos de los que tiene: error, no la muerte
    //     del proceso por pedir memoria para un tamano inventado
    // ------------------------------------------------------------------
    {
        const std::filesystem::path fichero = escribirWavConTamanoMentiroso();

        NovaDecodedAudio audio;
        QString error;
        const bool ok = decodeAudioFile(QString::fromStdString(fichero.string()),
                                         audio, &error);

        CHECK_NAMED("wav_con_tamano_de_datos_mentiroso_devuelve_error",
                    !ok && !error.isEmpty(),
                    "un WAV que declara mas datos de los que tiene debe fallar con un error, no reventar");

        std::filesystem::remove(fichero);
    }

    std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
