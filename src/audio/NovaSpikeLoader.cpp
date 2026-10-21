#include "audio/NovaSpikeLoader.h"

#include "audio/NovaAudioDecoder.h"
#include "audio/NovaAudioPeaks.h"

#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>

#include <utility>

NovaSpikeLoader::NovaSpikeLoader(QObject* parent)
    : QObject(parent)
{
}

NovaSpikeLoader::~NovaSpikeLoader() = default;

void NovaSpikeLoader::cargar(const QString& path)
{
    m_audio.reset();
    m_picos = PeakData{};
    m_error.clear();
    m_cargado = false;
    m_hiloTrabajo = nullptr;

    // Cada carga tiene su número: si el usuario pide otro fichero mientras
    // este se está cargando, el resultado de este se descarta (así no se
    // publica una onda que no es la del fichero pedido).
    const quint64 generacion = ++m_generacion;

    // El cargador se lleva el trabajo con un puntero débil: si se destruye
    // mientras el trabajo está en marcha, el resultado se descarta (no se
    // avisa a un objeto que ya no existe).
    QPointer<NovaSpikeLoader> cargador(this);

    QThreadPool::globalInstance()->start([cargador, path, generacion]() {
        if (cargador.isNull()) {
            return;
        }

        // El hilo donde se hace el trabajo pesado: se anota para poder
        // comprobar que no es el hilo de la interfaz.
        QThread* hilo = QThread::currentThread();

        // 1. Decodificar. Es la ÚNICA decodificación del fichero: el audio
        //    que sale de aquí es el que se pasa al motor de audio.
        auto audio = std::make_shared<NovaDecodedAudio>();
        QString error;
        if (!decodeAudioFile(path, *audio, &error)) {
            if (cargador.isNull()) {
                return;
            }
            const QString mensaje = error.isEmpty()
                                        ? QStringLiteral("no se ha podido decodificar el fichero")
                                        : error;
            QMetaObject::invokeMethod(
                cargador,
                [cargador, mensaje, generacion]() {
                    if (cargador.isNull() || cargador->m_generacion != generacion) {
                        return;
                    }
                    cargador->m_error = mensaje;
                    cargador->m_cargado = false;
                    Q_EMIT cargador->cargaFallida(mensaje);
                },
                Qt::QueuedConnection);
            return;
        }

        // 2. Calcular los picos. También aquí, en el hilo de trabajo: es un
        //    recorrido por todas las muestras del fichero.
        const PeakData picos = buildPeakData(*audio, kPuntosDetalle, kPuntosVistaGeneral);

        // 3. Entregar el resultado en el hilo del cargador.
        if (cargador.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            cargador,
            [cargador, audio, picos, hilo, generacion]() {
                if (cargador.isNull() || cargador->m_generacion != generacion) {
                    return;
                }
                cargador->m_audio = audio;
                cargador->m_picos = picos;
                cargador->m_cargado = true;
                cargador->m_hiloTrabajo = hilo;
                Q_EMIT cargador->cargaTerminada();
            },
            Qt::QueuedConnection);
    });
}

NovaDecodedAudio NovaSpikeLoader::tomarAudio()
{
    if (!m_audio) {
        return NovaDecodedAudio{};
    }

    // El audio se mueve (las muestras no se copian) y el cargador se queda
    // sin él: el motor de audio es su nuevo dueño.
    NovaDecodedAudio audio = std::move(*m_audio);
    m_audio.reset();
    return audio;
}