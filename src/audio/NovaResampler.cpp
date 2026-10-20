#include "audio/NovaResampler.h"

#include <cmath>

namespace {

// Tamaño con el que se reserva el buffer de entrada. Es suficiente para los
// bloques típicos del callback de audio (unos cientos de fotogramas) y evita
// que el remuestreador reserve memoria mientras suena el audio.
constexpr size_t kCapacidadReservada = 16384;

} // namespace

void NovaResampler::setRates(int inRate, int outRate, int channels)
{
    m_tasaEntrada = inRate;
    m_tasaSalida = outRate;
    m_canales = channels > 0 ? channels : 1;
    m_ratio = (m_tasaEntrada > 0 && m_tasaSalida > 0)
                  ? static_cast<double>(m_tasaEntrada) / static_cast<double>(m_tasaSalida)
                  : 1.0;

    // Las muestras que hubiera en el buffer ya no valen para las tasas
    // nuevas, así que se empieza de cero.
    reset();
    m_buffer.reserve(kCapacidadReservada);
}

void NovaResampler::reset()
{
    m_buffer.clear();
    m_inicio = 0;
    m_pos = 0.0;
}

int NovaResampler::process(const float* in, int inFrames, float* out, int outCapacity)
{
    if (m_tasaEntrada <= 0 || m_tasaSalida <= 0 || m_canales <= 0) {
        return 0;
    }

    if (in != nullptr && inFrames > 0) {
        m_buffer.insert(m_buffer.end(), in,
                        in + static_cast<size_t>(inFrames) * static_cast<size_t>(m_canales));
    }

    if (out == nullptr || outCapacity <= 0) {
        return 0;
    }

    const qint64 pendientes =
        static_cast<qint64>((m_buffer.size() - m_inicio) / static_cast<size_t>(m_canales));

    int escritos = 0;
    while (escritos < outCapacity) {
        const qint64 indice = static_cast<qint64>(std::floor(m_pos));
        if (indice + 1 >= pendientes) {
            // No hay muestra siguiente con la que interpolar: el audio se ha
            // agotado (o todavía no ha llegado entero).
            break;
        }

        const double frac = m_pos - static_cast<double>(indice);
        const float* anterior =
            m_buffer.data() + m_inicio + static_cast<size_t>(indice) * static_cast<size_t>(m_canales);
        const float* siguiente = anterior + m_canales;
        float* destino = out + static_cast<size_t>(escritos) * static_cast<size_t>(m_canales);

        for (int canal = 0; canal < m_canales; ++canal) {
            destino[canal] = anterior[canal]
                             + (siguiente[canal] - anterior[canal]) * static_cast<float>(frac);
        }

        ++escritos;
        m_pos += m_ratio;
    }

    // Se descarta lo ya convertido. Se conserva el fotograma entero donde cae
    // la posición, porque hace falta como muestra anterior para la siguiente
    // interpolación (si no, aparecería un clic en la unión de dos bloques).
    const qint64 consumido = static_cast<qint64>(std::floor(m_pos));
    if (consumido > 0) {
        m_inicio += static_cast<size_t>(consumido) * static_cast<size_t>(m_canales);
        m_pos -= static_cast<double>(consumido);
    }
    compactar();

    return escritos;
}

void NovaResampler::compactar()
{
    // Las muestras ya convertidas se quedan en el vector hasta que ocupan
    // una parte apreciable de él; entonces se quitan de golpe, para no
    // mover memoria en cada llamada.
    if (m_inicio >= kCapacidadReservada && m_inicio * 2 >= m_buffer.size()) {
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(m_inicio));
        m_inicio = 0;
    }
}
