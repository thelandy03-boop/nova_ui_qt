#include "audio/NovaWavReader.h"

#include <QByteArray>
#include <QFile>

#include <algorithm>
#include <cstring>
#include <vector>

namespace nova {
namespace {

// Formato de muestra de los ficheros WAV que se saben interpretar.
enum class FormatoMuestra {
    Entero,   // PCM de 16, 24 o 32 bits con signo
    Flotante, // IEEE float de 32 bits
};

// Datos del chunk 'fmt ' que hacen falta para interpretar las muestras.
struct FormatoWav {
    int canales = 0;
    int tasa = 0;
    int bitsPorMuestra = 0;
    FormatoMuestra formato = FormatoMuestra::Entero;
};

bool fallar(QString* error, const QString& mensaje)
{
    if (error) {
        *error = mensaje;
    }
    return false;
}

// Lectura de enteros de varios bytes en little endian (el orden de RIFF).
quint16 leerU16(const char* datos)
{
    const unsigned char* b = reinterpret_cast<const unsigned char*>(datos);
    return static_cast<quint16>(static_cast<quint16>(b[0]) | (static_cast<quint16>(b[1]) << 8));
}

quint32 leerU32(const char* datos)
{
    const unsigned char* b = reinterpret_cast<const unsigned char*>(datos);
    return static_cast<quint32>(b[0]) | (static_cast<quint32>(b[1]) << 8)
         | (static_cast<quint32>(b[2]) << 16) | (static_cast<quint32>(b[3]) << 24);
}

// Convierte un bloque de bytes crudos en muestras float. Se respeta el
// formato declarado: los 24 bits se assemblan a mano (no vale un memcpy) y
// se extensionan con el signo, y el entero de 32 bits se divide por 2^31.
void convertirBloque(const char* datos, qint64 bytes, const FormatoWav& formato,
                     std::vector<float>& muestras)
{
    switch (formato.bitsPorMuestra) {
    case 16:
        for (qint64 i = 0; i + 1 < bytes; i += 2) {
            const int bruto = static_cast<int>(leerU16(datos + i));
            const int valor = bruto >= 0x8000 ? bruto - 0x10000 : bruto;
            muestras.push_back(static_cast<float>(valor) / 32768.0f);
        }
        break;

    case 24:
        for (qint64 i = 0; i + 2 < bytes; i += 3) {
            const unsigned char* b = reinterpret_cast<const unsigned char*>(datos + i);
            const quint32 bruto = static_cast<quint32>(b[0])
                                | (static_cast<quint32>(b[1]) << 8)
                                | (static_cast<quint32>(b[2]) << 16);
            // Extension de signo: el bit 23 (el primero de los 24) dice si
            // el valor es negativo.
            const int64_t valor = (bruto & 0x800000u)
                                ? static_cast<int64_t>(bruto) - 0x1000000LL
                                : static_cast<int64_t>(bruto);
            muestras.push_back(static_cast<float>(valor) / 8388608.0f);
        }
        break;

    case 32:
        if (formato.formato == FormatoMuestra::Flotante) {
            for (qint64 i = 0; i + 3 < bytes; i += 4) {
                const quint32 bits = leerU32(datos + i);
                float valor = 0.0f;
                std::memcpy(&valor, &bits, sizeof(valor));
                muestras.push_back(valor);
            }
        } else {
            for (qint64 i = 0; i + 3 < bytes; i += 4) {
                const quint32 bruto = leerU32(datos + i);
                const int64_t valor = bruto >= 0x80000000u
                                    ? static_cast<int64_t>(bruto) - 0x100000000LL
                                    : static_cast<int64_t>(bruto);
                muestras.push_back(static_cast<float>(valor) / 2147483648.0f);
            }
        }
        break;

    default:
        break;
    }
}

// Interpreta el chunk 'fmt '. El formato puede venir directamente (PCM o
// coma flotante) o dentro de un WAVE_FORMAT_EXTENSIBLE, en cuyo caso manda
// el subformato real.
bool interpretarFormato(const QByteArray& bloque, FormatoWav& formato, QString* error)
{
    const char* d = bloque.constData();

    quint16 tag = leerU16(d);
    formato.canales = leerU16(d + 2);
    formato.tasa = static_cast<int>(leerU32(d + 4));
    const quint16 alineacion = leerU16(d + 12);
    formato.bitsPorMuestra = leerU16(d + 14);

    if (tag == 0xFFFE) { // WAVE_FORMAT_EXTENSIBLE
        if (bloque.size() < 40) {
            return fallar(error, QStringLiteral("el chunk 'fmt ' no tiene los 40 bytes que necesita WAVE_FORMAT_EXTENSIBLE"));
        }
        // Los dos primeros bytes del SubFormat son el formato real.
        tag = leerU16(d + 24);
    }

    switch (tag) {
    case 1: // WAVE_FORMAT_PCM
        formato.formato = FormatoMuestra::Entero;
        if (formato.bitsPorMuestra != 16 && formato.bitsPorMuestra != 24
            && formato.bitsPorMuestra != 32) {
            return fallar(error, QStringLiteral("el WAV usa %1 bits enteros por muestra, y solo se admiten 16, 24 o 32")
                          .arg(formato.bitsPorMuestra));
        }
        break;

    case 3: // WAVE_FORMAT_IEEE_FLOAT
        formato.formato = FormatoMuestra::Flotante;
        if (formato.bitsPorMuestra != 32) {
            return fallar(error, QStringLiteral("el WAV usa coma flotante de %1 bits, y solo se admiten 32")
                          .arg(formato.bitsPorMuestra));
        }
        break;

    default:
        return fallar(error, QStringLiteral("el WAV usa un formato de muestra no soportado (codigo %1): solo se admiten PCM y coma flotante de 32 bits")
                      .arg(tag));
    }

    if (formato.canales <= 0) {
        return fallar(error, QStringLiteral("el WAV declara %1 canales, que no es valido").arg(formato.canales));
    }
    if (formato.tasa <= 0) {
        return fallar(error, QStringLiteral("el WAV declara una tasa de muestreo de %1 Hz, que no es valida").arg(formato.tasa));
    }

    const int bytesPorFotograma = formato.canales * (formato.bitsPorMuestra / 8);
    if (alineacion != 0 && alineacion != bytesPorFotograma) {
        return fallar(error, QStringLiteral("el WAV es inconsistente: el bloque mide %1 bytes, pero %2 canales de %3 bits necesitan %4")
                      .arg(alineacion).arg(formato.canales).arg(formato.bitsPorMuestra).arg(bytesPorFotograma));
    }

    return true;
}

} // namespace

bool decodeWavFile(const QString& path, NovaDecodedAudio& out, QString* error)
{
    out = NovaDecodedAudio();

    QFile archivo(path);
    if (!archivo.open(QIODevice::ReadOnly)) {
        return fallar(error, QStringLiteral("no se puede abrir el fichero '%1': %2").arg(path, archivo.errorString()));
    }

    const QByteArray cabecera = archivo.read(12);
    if (cabecera.size() < 12) {
        return fallar(error, QStringLiteral("'%1' es demasiado corto para ser un WAV (faltan los 12 bytes de la cabecera RIFF)").arg(path));
    }
    if (std::memcmp(cabecera.constData(), "RIFF", 4) != 0) {
        return fallar(error, QStringLiteral("'%1' no es un fichero RIFF (la cabecera no empieza por 'RIFF')").arg(path));
    }
    if (std::memcmp(cabecera.constData() + 8, "WAVE", 4) != 0) {
        return fallar(error, QStringLiteral("'%1' no es un fichero WAVE (el tipo de RIFF no es 'WAVE')").arg(path));
    }

    FormatoWav formato;
    bool hayFormato = false;
    qint64 inicioDatos = -1;
    quint32 tamanoDatos = 0;

    // Se recorren los chunks buscando 'fmt ' y 'data'; el resto se salta.
    while (true) {
        const QByteArray id = archivo.read(4);
        if (id.size() < 4) {
            break; // se acabaron los chunks
        }
        const QByteArray tam = archivo.read(4);
        if (tam.size() < 4) {
            return fallar(error, QStringLiteral("'%1' esta truncado: la cabecera de un chunk esta incompleta").arg(path));
        }

        const quint32 bytes = leerU32(tam.constData());
        const qint64 inicioChunk = archivo.pos();

        if (id == "fmt ") {
            const QByteArray bloque = archivo.read(std::min<quint64>(bytes, 64));
            if (bloque.size() < 16) {
                return fallar(error, QStringLiteral("'%1' tiene el chunk 'fmt ' incompleto (%2 bytes)").arg(path).arg(bloque.size()));
            }
            if (!interpretarFormato(bloque, formato, error)) {
                return false;
            }
            hayFormato = true;
        } else if (id == "data") {
            inicioDatos = inicioChunk;
            tamanoDatos = bytes;
        }

        // Se salta el contenido del chunk. Los chunks impares llevan un byte
        // de relleno para que su tamano siga siendo par.
        if (!archivo.seek(inicioChunk + bytes + (bytes & 1))) {
            break;
        }
    }

    if (!hayFormato) {
        return fallar(error, QStringLiteral("'%1' no tiene el chunk 'fmt ' con el formato del audio").arg(path));
    }
    if (inicioDatos < 0) {
        return fallar(error, QStringLiteral("'%1' no tiene el chunk 'data' con las muestras").arg(path));
    }

    const qint64 bytesPorMuestra = formato.bitsPorMuestra / 8;
    const qint64 bytesPorFotograma = bytesPorMuestra * formato.canales;
    if (tamanoDatos % bytesPorFotograma != 0) {
        return fallar(error, QStringLiteral("'%1' es inconsistente: el chunk 'data' mide %2 bytes, que no es multiplo del fotograma (%3 bytes)")
                      .arg(path).arg(tamanoDatos).arg(bytesPorFotograma));
    }

    if (!archivo.seek(inicioDatos)) {
        return fallar(error, QStringLiteral("no se puede volver al inicio de los datos de '%1'").arg(path));
    }

    std::vector<float> muestras;
    muestras.reserve(static_cast<size_t>(tamanoDatos / bytesPorMuestra));

    QByteArray bloque;
    QByteArray sobra; // bytes de una muestra partida entre dos bloques
    quint32 leidos = 0;
    while (leidos < tamanoDatos) {
        const quint32 pedir = std::min<quint32>(tamanoDatos - leidos, 1024u * 1024u);
        bloque = archivo.read(pedir);
        if (bloque.isEmpty()) {
            break; // el fichero se ha acabado antes de tiempo
        }
        leidos += static_cast<quint32>(bloque.size());

        if (!sobra.isEmpty()) {
            bloque.prepend(sobra);
            sobra.clear();
        }
        const qint64 bytesUtiles = bloque.size() - (bloque.size() % bytesPorMuestra);
        sobra = bloque.mid(bytesUtiles);
        convertirBloque(bloque.constData(), bytesUtiles, formato, muestras);
    }

    if (leidos < tamanoDatos) {
        return fallar(error, QStringLiteral("'%1' esta truncado: el chunk 'data' declara %2 bytes pero solo hay %3")
                      .arg(path).arg(tamanoDatos).arg(leidos));
    }
    if (muestras.empty()) {
        return fallar(error, QStringLiteral("'%1' no contiene muestras de audio").arg(path));
    }

    out.sampleRate = formato.tasa;
    out.channels = formato.canales;
    out.samples = std::move(muestras);
    return true;
}

} // namespace nova
