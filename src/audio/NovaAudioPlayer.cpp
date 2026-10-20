#include "audio/NovaAudioPlayer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

bool NovaAudioPlayer::load(const NovaDecodedAudio& audio, QString* error)
{
    if (error) {
        error->clear();
    }

    if (audio.channels <= 0 || audio.sampleRate <= 0) {
        if (error) {
            *error = QStringLiteral("el audio no tiene canales ni tasa de muestreo válidos");
        }
        return false;
    }

    m_audio = audio;
    m_canales = audio.channels;
    m_tasaEntrada = audio.sampleRate;
    m_fotogramasEntrada = audio.frameCount();

    m_fotogramasAlimentados = 0;
    m_fotogramasPosicion = 0;
    m_sonando = false;
    m_cargado = true;

    m_remuestreador.setRates(m_tasaEntrada, m_tasaSalida, m_canales);
    recalcularDuracion();

    return true;
}

void NovaAudioPlayer::setOutputSampleRate(int sampleRate)
{
    if (sampleRate <= 0) {
        return;
    }

    m_tasaSalida = sampleRate;
    if (!m_cargado) {
        return;
    }

    // El audio ya emitido no vale a otra tasa: se vuelve al principio y se
    // reinicia el remuestreador (setRates() ya lo deja limpio).
    m_fotogramasAlimentados = 0;
    m_fotogramasPosicion = 0;
    m_remuestreador.setRates(m_tasaEntrada, m_tasaSalida, m_canales);
    recalcularDuracion();
}

void NovaAudioPlayer::play()
{
    if (!m_cargado) {
        return;
    }
    m_sonando = true;
}

void NovaAudioPlayer::stop()
{
    m_sonando = false;
}

bool NovaAudioPlayer::isPlaying() const
{
    return m_sonando;
}

qint64 NovaAudioPlayer::positionFrames() const
{
    return m_fotogramasPosicion;
}

qint64 NovaAudioPlayer::durationFrames() const
{
    return m_fotogramasDuracion;
}

double NovaAudioPlayer::durationSeconds() const
{
    if (m_tasaSalida <= 0) {
        return 0.0;
    }
    return static_cast<double>(m_fotogramasDuracion) / static_cast<double>(m_tasaSalida);
}

void NovaAudioPlayer::reset()
{
    m_fotogramasAlimentados = 0;
    m_fotogramasPosicion = 0;
    m_remuestreador.reset();
}

int NovaAudioPlayer::renderFrames(float* interleavedOut, int frames)
{
    if (!m_cargado || !m_sonando || interleavedOut == nullptr || frames <= 0) {
        return 0;
    }

    const qint64 restantes = m_fotogramasDuracion - m_fotogramasPosicion;
    if (restantes <= 0) {
        // El fichero se ha agotado.
        m_sonando = false;
        return 0;
    }
    const int pedidos = static_cast<int>(std::min<qint64>(frames, restantes));

    int escritos = 0;
    while (escritos < pedidos) {
        // Al remuestreador se le pasa justo el trozo de entrada que necesita
        // para los fotogramas que faltan: ni uno más (el buffer del
        // remuestreador guardaría audio que no se va a usar) ni uno menos
        // (el remuestreador no podría interpolar).
        const qint64 ultimo = m_fotogramasPosicion + pedidos - 1;
        const qint64 necesario =
            static_cast<qint64>(std::floor(static_cast<double>(ultimo) * ratioEntradaPorSalida())) + 3;

        qint64 aAlimentar = 0;
        if (m_fotogramasAlimentados < necesario) {
            aAlimentar = std::min(necesario - m_fotogramasAlimentados,
                                   m_fotogramasEntrada - m_fotogramasAlimentados);
            // El remuestreador trabaja con un número de fotogramas de tipo
            // int: si el fichero fuera enorme, se alimenta en varios trozos.
            aAlimentar = std::min<qint64>(aAlimentar, std::numeric_limits<int>::max());
        }

        if (aAlimentar > 0) {
            const float* entrada = m_audio.samples.data()
                                   + static_cast<size_t>(m_fotogramasAlimentados)
                                         * static_cast<size_t>(m_canales);
            const int producidos = m_remuestreador.process(
                entrada, static_cast<int>(aAlimentar),
                interleavedOut + static_cast<size_t>(escritos) * static_cast<size_t>(m_canales),
                pedidos - escritos);
            m_fotogramasAlimentados += aAlimentar;
            escritos += producidos;
            continue;
        }

        const int producidos = m_remuestreador.process(
            nullptr, 0,
            interleavedOut + static_cast<size_t>(escritos) * static_cast<size_t>(m_canales),
            pedidos - escritos);
        escritos += producidos;
        if (producidos == 0) {
            // Ya no queda entrada que convertir.
            break;
        }
    }

    if (escritos < pedidos && m_fotogramasEntrada > 0) {
        // Al final del fichero no hay muestra siguiente con la que
        // interpolar: se mantiene el último fotograma para no cortar el
        // audio a mitad (el final de un fichero no tiene continuación).
        const int faltan = pedidos - escritos;
        const float* ultimoFotograma = m_audio.samples.data()
                                       + static_cast<size_t>(m_fotogramasEntrada - 1)
                                             * static_cast<size_t>(m_canales);
        for (int i = 0; i < faltan; ++i) {
            std::memcpy(interleavedOut + static_cast<size_t>(escritos + i) * static_cast<size_t>(m_canales),
                        ultimoFotograma,
                        static_cast<size_t>(m_canales) * sizeof(float));
        }
        escritos += faltan;
    }

    m_fotogramasPosicion += escritos;
    if (m_fotogramasPosicion >= m_fotogramasDuracion) {
        m_sonando = false;
    }

    return escritos;
}

void NovaAudioPlayer::recalcularDuracion()
{
    if (m_tasaEntrada <= 0 || m_tasaSalida <= 0) {
        m_fotogramasDuracion = 0;
        return;
    }

    const double duracion = static_cast<double>(m_fotogramasEntrada)
                          * static_cast<double>(m_tasaSalida)
                          / static_cast<double>(m_tasaEntrada);
    m_fotogramasDuracion = static_cast<qint64>(std::llround(duracion));
}

double NovaAudioPlayer::ratioEntradaPorSalida() const
{
    if (m_tasaEntrada <= 0 || m_tasaSalida <= 0) {
        return 1.0;
    }
    return static_cast<double>(m_tasaEntrada) / static_cast<double>(m_tasaSalida);
}
