#include "audio/NovaMp3Decoder.h"

#include <QByteArray>
#include <QFile>

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

// minimp3 no incluye las cabeceras estandar (esta pensada para incluirse
// dentro de un namespace), asi que se incluyen antes que ella.
#include <stdint.h>

// Los simbolos de minimp3 se renombran al incluirla: Ardour tambien usa
// esta cabecera y exporta esos mismos simbolos desde libardour.so. Si el
// ejecutable de Nova los definiera con el mismo nombre, el enlazador
// dinamico haria que las llamadas internas de Ardour resolvieran a la copia
// de Nova (y al reves) y los dos decodificadores se pisarian. El fichero
// vendorizado no se modifica: el renombrado se hace con macros aqui.
#define mp3dec_init nova_mp3dec_init
#define mp3dec_decode_frame nova_mp3dec_decode_frame
#define mp3dec_f32_to_s16 nova_mp3dec_f32_to_s16

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NONSTANDARD_BUT_LOGICAL
#include "../../third_party/minimp3/minimp3.h"

namespace nova {
namespace {

// Retardo que mete el decodificador de MP3 por el solape de sus MDCT, en
// muestras. La etiqueta LAME solo dice el retardo del codificador, asi que
// el total que hay que quitar del principio es la suma de los dos.
const int kRetardoDecodificador = 529;

// Tamano de la etiqueta ID3v1 que algunos codificadores dejan al final.
const int kTamanoId3v1 = 128;

// Retardo y relleno que declara la etiqueta LAME, en muestras.
struct DatosGapless {
    bool hayDatos = false;
    int retardo = 0; // muestras de retardo del codificador
    int relleno = 0; // muestras de relleno al final
};

bool fallar(QString* error, const QString& mensaje)
{
    if (error) {
        *error = mensaje;
    }
    return false;
}

// Los campos de la cabecera Xing/Info van en big endian (al reves que los
// chunks de un WAV, que son little endian).
quint32 leerU32Be(const char* datos)
{
    const unsigned char* b = reinterpret_cast<const unsigned char*>(datos);
    return (static_cast<quint32>(b[0]) << 24) | (static_cast<quint32>(b[1]) << 16)
         | (static_cast<quint32>(b[2]) << 8) | static_cast<quint32>(b[3]);
}

// Devuelve la posicion de la cabecera Xing/Info dentro del frame, o -1 si el
// frame no la lleva. La cabecera va detras de la cabecera MPEG y de la
// informacion lateral, cuyo tamano depende de la version de MPEG y del
// numero de canales.
int posicionCabeceraXing(const char* frame, int bytes)
{
    if (bytes < 4) {
        return -1;
    }
    const unsigned char b0 = static_cast<unsigned char>(frame[0]);
    const unsigned char b1 = static_cast<unsigned char>(frame[1]);
    const unsigned char b3 = static_cast<unsigned char>(frame[3]);
    if (b0 != 0xFF || (b1 & 0xE0) != 0xE0) {
        return -1; // no es una cabecera MPEG
    }
    if (((b1 >> 1) & 0x03) != 0x01) {
        return -1; // no es Layer III
    }

    const int version = (b1 >> 3) & 0x03; // 3 = MPEG1, 2 = MPEG2, 0 = MPEG2.5
    const int modo = (b3 >> 6) & 0x03;    // 3 = mono
    const int lateral = (version == 3) ? ((modo == 3) ? 17 : 32)  // MPEG1
                                       : ((modo == 3) ? 9 : 17);  // MPEG2 y MPEG2.5

    const int posicion = 4 + lateral;
    if (posicion + 8 > bytes) {
        return -1;
    }
    if (std::memcmp(frame + posicion, "Xing", 4) != 0
        && std::memcmp(frame + posicion, "Info", 4) != 0) {
        return -1;
    }
    return posicion;
}

// Lee el retardo y el relleno de la etiqueta LAME que cuelga de la cabecera
// Xing/Info. Devuelve false si no hay una etiqueta LAME utilizable.
bool leerGaplessLame(const char* frame, int bytes, int posicionXing, DatosGapless& datos)
{
    const quint32 banderas = leerU32Be(frame + posicionXing + 4);
    int p = posicionXing + 8;
    if (banderas & 0x01) {
        p += 4;   // numero de frames
    }
    if (banderas & 0x02) {
        p += 4;   // numero de bytes
    }
    if (banderas & 0x04) {
        p += 100; // tabla de contenido
    }
    if (banderas & 0x08) {
        p += 4;   // escala VBR
    }

    // A partir de aqui va la version del codificador (9 bytes). El retardo
    // del codificador y el relleno final estan 21 bytes mas adelante.
    if (p + 24 > bytes) {
        return false;
    }
    const bool esLame = std::memcmp(frame + p, "LAME", 4) == 0
                     || std::memcmp(frame + p, "Lavf", 4) == 0
                     || std::memcmp(frame + p, "Lavc", 4) == 0;
    if (!esLame) {
        return false;
    }

    const int retardo = (static_cast<unsigned char>(frame[p + 21]) << 4)
                      | (static_cast<unsigned char>(frame[p + 22]) >> 4);
    const int relleno = ((static_cast<unsigned char>(frame[p + 22]) & 0x0F) << 8)
                      | static_cast<unsigned char>(frame[p + 23]);
    if (retardo > 3000 || relleno > 3000) {
        return false; // valores sin sentido: no es una etiqueta LAME valida
    }

    datos.hayDatos = true;
    datos.retardo = retardo;
    datos.relleno = relleno;
    return true;
}

// Bytes sueltos al final del fichero. Se toleran solo si son relleno de ceros
// o una etiqueta conocida; cualquier otra cosa significa que el fichero esta
// cortado por la mitad.
bool sonBytesFinalesBenignos(const char* datos, qint64 bytes)
{
    if (bytes <= 0) {
        return true;
    }
    if (bytes >= 3 && (std::memcmp(datos, "TAG", 3) == 0 || std::memcmp(datos, "ID3", 3) == 0)) {
        return true; // etiqueta ID3v1 o ID3v2 suelta al final
    }
    for (qint64 i = 0; i < bytes; ++i) {
        if (datos[i] != '\0') {
            return false;
        }
    }
    return true; // relleno de ceros
}

} // namespace

bool decodeMp3File(const QString& path, NovaDecodedAudio& out, QString* error)
{
    out = NovaDecodedAudio();

    QFile archivo(path);
    if (!archivo.open(QIODevice::ReadOnly)) {
        return fallar(error, QStringLiteral("no se puede abrir el fichero '%1': %2").arg(path, archivo.errorString()));
    }

    const QByteArray contenido = archivo.readAll();
    if (contenido.size() < 4) {
        return fallar(error, QStringLiteral("'%1' es demasiado corto para ser un MP3").arg(path));
    }
    if (contenido.size() > std::numeric_limits<int>::max()) {
        return fallar(error, QStringLiteral("'%1' es demasiado grande para decodificarse de una vez").arg(path));
    }

    const char* bytes = contenido.constData();
    qint64 fin = contenido.size();

    // La etiqueta ID3v1 del final no es audio. Se quita para que el ultimo
    // frame se pueda decodificar: minimp3 solo da por bueno un frame si ve
    // que el siguiente tambien lo es, y al final ya no hay siguiente.
    if (fin >= kTamanoId3v1 && std::memcmp(bytes + fin - kTamanoId3v1, "TAG", 3) == 0) {
        fin -= kTamanoId3v1;
    }

    mp3dec_t decodificador;
    nova_mp3dec_init(&decodificador);

    std::vector<float> muestras;
    DatosGapless gapless;
    int canales = 0;
    int tasa = 0;
    bool primero = true;

    const char* puntero = bytes;
    qint64 restante = fin;

    while (restante > 0) {
        float pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
        mp3dec_frame_info_t info;
        const int muestrasPorCanal = nova_mp3dec_decode_frame(
            &decodificador, reinterpret_cast<const uint8_t*>(puntero),
            static_cast<int>(restante), pcm, &info);
        if (muestrasPorCanal <= 0) {
            break; // no queda ningun frame completo
        }

        const char* frame = puntero + info.frame_offset;
        const int bytesFrame = info.frame_bytes - info.frame_offset;
        bool esCabecera = false;

        if (primero) {
            primero = false;
            canales = info.channels;
            tasa = info.hz;
            // El primer frame puede ser la cabecera Xing/Info: es metadatos,
            // no audio, asi que sus muestras se descartan (pero se leen sus
            // datos de retardo y relleno).
            const int posicionXing = posicionCabeceraXing(frame, bytesFrame);
            if (posicionXing >= 0) {
                esCabecera = true;
                leerGaplessLame(frame, bytesFrame, posicionXing, gapless);
            }
        } else if (info.channels != canales || info.hz != tasa) {
            return fallar(error, QStringLiteral("'%1' cambia de formato a mitad del flujo (%2 Hz y %3 canales -> %4 Hz y %5 canales)")
                          .arg(path).arg(tasa).arg(canales).arg(info.hz).arg(info.channels));
        }

        if (!esCabecera) {
            const size_t total = static_cast<size_t>(muestrasPorCanal)
                               * static_cast<size_t>(info.channels);
            muestras.insert(muestras.end(), pcm, pcm + total);
        }

        puntero += info.frame_bytes;
        restante -= info.frame_bytes;
    }

    if (restante > 0 && !sonBytesFinalesBenignos(puntero, restante)) {
        return fallar(error, QStringLiteral("'%1' esta truncado: los %2 bytes del final no forman un frame completo")
                      .arg(path).arg(restante));
    }
    if (muestras.empty()) {
        return fallar(error, QStringLiteral("'%1' no contiene frames de audio validos").arg(path));
    }

    // Sin recorte, la pista suena con un trozo de silencio al principio
    // (el retardo del codificador mas el del decodificador) y con relleno
    // al final. La etiqueta LAME dice cuanto sobra en cada lado. El retardo
    // y el relleno se cuentan por canal, y las muestras estan intercaladas.
    qint64 inicio = 0;
    qint64 final = static_cast<qint64>(muestras.size());
    if (gapless.hayDatos) {
        const qint64 porCanal = canales > 0 ? canales : 1;
        const qint64 retardo = (gapless.retardo + kRetardoDecodificador) * porCanal;
        const qint64 relleno = std::max<qint64>(0, gapless.relleno - kRetardoDecodificador) * porCanal;
        inicio = std::min<qint64>(retardo, final);
        final = std::max(inicio, final - relleno);
    }
    if (final - inicio <= 0) {
        return fallar(error, QStringLiteral("'%1' no contiene audio utilizable despues de quitar el retardo y el relleno").arg(path));
    }

    out.sampleRate = tasa;
    out.channels = canales;
    out.samples.assign(muestras.begin() + static_cast<std::ptrdiff_t>(inicio),
                        muestras.begin() + static_cast<std::ptrdiff_t>(final));
    return true;
}

} // namespace nova
