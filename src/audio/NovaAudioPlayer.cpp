#include "audio/NovaAudioPlayer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

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

    // El reproductor guarda su propia copia del audio, así que el llamante
    // puede destruir el suyo.
    m_audio = audio;
    finalizarCarga();

    return true;
}

bool NovaAudioPlayer::load(NovaDecodedAudio&& audio, QString* error)
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

    // El audio se mueve: las muestras no se copian (el PCM de un fichero
    // estéreo de cinco minutos son unos 115 MB).
    m_audio = std::move(audio);
    finalizarCarga();

    return true;
}

void NovaAudioPlayer::finalizarCarga()
{
    m_canales = m_audio.channels;
    m_tasaEntrada = m_audio.sampleRate;
    m_fotogramasEntrada = m_audio.frameCount();

    m_fotogramasAlimentados = 0;
    m_fotogramasPosicion = 0;
    m_sonando = false;
    m_cargado = true;

    m_remuestreador.setRates(m_tasaEntrada, m_tasaSalida, m_canales);
    recalcularDuracion();
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

void NovaAudioPlayer::seekToFrame(qint64 outputFrame)
{
    if (!m_cargado || m_fotogramasDuracion <= 0) {
        return;
    }
    if (outputFrame < 0) outputFrame = 0;
    if (outputFrame > m_fotogramasDuracion) outputFrame = m_fotogramasDuracion;

    m_fotogramasPosicion = outputFrame;
    // Se vuelve a alimentar desde el fotograma de entrada equivalente,
    // dejando un margen para que el remuestreador interpole.
    const double ratio = ratioEntradaPorSalida();
    qint64 alimentados = static_cast<qint64>(std::floor(static_cast<double>(outputFrame) * ratio));
    if (alimentados < 0) alimentados = 0;
    if (alimentados > m_fotogramasEntrada) alimentados = m_fotogramasEntrada;
    m_fotogramasAlimentados = alimentados;
    m_remuestreador.reset();
}

void NovaAudioPlayer::setPlaybackRange(qint64 startFrame, qint64 endFrame)
{
    if (startFrame < 0) startFrame = 0;
    if (endFrame < 0) endFrame = 0;
    if (endFrame > 0 && endFrame <= startFrame) {
        // Rango invalido: se ignora (se mantiene el anterior).
        return;
    }
    m_rangoInicio = startFrame;
    m_rangoFin = endFrame;

    // Si la posición actual queda fuera del nuevo rango, se recoloca.
    const qint64 effEnd = (m_rangoFin > 0) ? m_rangoFin : m_fotogramasDuracion;
    if (m_fotogramasPosicion < m_rangoInicio) {
        m_fotogramasPosicion = m_rangoInicio;
        const double ratio = ratioEntradaPorSalida();
        qint64 alimentados = static_cast<qint64>(std::floor(static_cast<double>(m_rangoInicio) * ratio));
        if (alimentados < 0) alimentados = 0;
        if (alimentados > m_fotogramasEntrada) alimentados = m_fotogramasEntrada;
        m_fotogramasAlimentados = alimentados;
        m_remuestreador.reset();
    } else if (m_fotogramasPosicion > effEnd) {
        m_fotogramasPosicion = effEnd;
    }
}

int NovaAudioPlayer::renderFrames(float* interleavedOut, int frames)
{
    if (!m_cargado || !m_sonando || interleavedOut == nullptr || frames <= 0) {
        return 0;
    }

    // Rango efectivo de reproducción.
    const qint64 effEnd = (m_rangoFin > 0 && m_rangoFin <= m_fotogramasDuracion)
                              ? m_rangoFin : m_fotogramasDuracion;
    if (m_fotogramasPosicion < m_rangoInicio) {
        m_fotogramasPosicion = m_rangoInicio;
    }
    const qint64 restantes = effEnd - m_fotogramasPosicion;
    if (restantes <= 0) {
        // El rango se ha agotado.
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
    const qint64 effEnd2 = (m_rangoFin > 0 && m_rangoFin <= m_fotogramasDuracion)
                               ? m_rangoFin : m_fotogramasDuracion;
    if (m_fotogramasPosicion >= effEnd2) {
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
