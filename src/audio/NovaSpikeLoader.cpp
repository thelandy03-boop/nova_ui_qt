#include "audio/NovaSpikeLoader.h"

#include "audio/NovaAudioDecoder.h"
#include "audio/NovaAudioPeaks.h"

#include <QMetaObject>
#include <QThreadPool>

#include <utility>

NovaSpikeLoader::NovaSpikeLoader(QObject* parent)
    : QObject(parent)
    , m_estado(std::make_shared<EstadoEntrega>())
{
}

NovaSpikeLoader::~NovaSpikeLoader()
{
    // A partir de aquí el trabajo que esté en marcha ya no puede entregar
    // nada. Se marca bajo el candado del estado: si el hilo de trabajo
    // está entregando el resultado en este preciso momento, este
    // destructor espera a que termine (así no se publica nada en un
    // objeto que ya no existe) y, si no, el trabajo se descarta al llegar
    // a la entrega.
    std::lock_guard<std::mutex> lock(m_estado->mutex);
    m_estado->vivo.store(false);
}

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

    // El estado de la entrega se comparte con el hilo de trabajo: vive en
    // un puntero compartido, así que sigue vivo aunque este cargador se
    // destruya (el hilo de trabajo solo lo usa para saber si puede
    // entregar el resultado).
    const std::shared_ptr<EstadoEntrega> estado = m_estado;

    // Puntero plano al cargador: el hilo de trabajo NO puede mirar un
    // `QPointer` (el cargador se puede destruir en el hilo de la interfaz
    // mientras el trabajo está en marcha, y leer ese puntero débil desde
    // otro hilo es una carrera que puede acabar en un acceso a memoria
    // liberada). Este puntero solo se usa bajo el candado del estado, que
    // garantiza que el cargador sigue vivo mientras se entrega el
    // resultado: el destructor marca `vivo = false` bajo ese mismo
    // candado, así que la entrega se hace con el cargador vivo o no se
    // hace.
    NovaSpikeLoader* cargador = this;

    QThreadPool::globalInstance()->start([estado, cargador, path, generacion]() {
        // Si el cargador ya no existe, no hay a quién entregar nada: no se
        // pierde tiempo decodificando un fichero que no se va a usar.
        // La comprobación va BAJO EL CANDADO, como las demás: así el hilo
        // de trabajo y el destructor nunca se pisan al mirar el estado.
        {
            std::lock_guard<std::mutex> lock(estado->mutex);
            if (!estado->vivo.load(std::memory_order_acquire)) {
                return;
            }
        }

        // El hilo donde se hace el trabajo pesado: se anota para poder
        // comprobar que no es el hilo de la interfaz.
        QThread* hilo = QThread::currentThread();

        // 1. Decodificar. Es la ÚNICA decodificación del fichero: el audio
        //    que sale de aquí es el que se pasa al motor de audio.
        auto audio = std::make_shared<NovaDecodedAudio>();
        QString error;
        if (!decodeAudioFile(path, *audio, &error)) {
            const QString mensaje = error.isEmpty()
                                        ? QStringLiteral("no se ha podido decodificar el fichero")
                                        : error;
            // La entrega se hace bajo el candado del estado: si el
            // cargador se ha destruido mientras se decodificaba, el
            // resultado se descarta (no se publica nada en un objeto que
            // ya no existe).
            std::lock_guard<std::mutex> lock(estado->mutex);
            if (!estado->vivo.load(std::memory_order_acquire)) {
                return;
            }
            QMetaObject::invokeMethod(
                cargador,
                [cargador, mensaje, generacion]() {
                    if (cargador->m_generacion != generacion) {
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

        // 3. Entregar el resultado en el hilo del cargador (bajo el
        //    candado del estado, como arriba).
        {
            std::lock_guard<std::mutex> lock(estado->mutex);
            if (!estado->vivo.load(std::memory_order_acquire)) {
                return;
            }
            QMetaObject::invokeMethod(
                cargador,
                [cargador, audio, picos, hilo, generacion]() {
                    if (cargador->m_generacion != generacion) {
                        return;
                    }
                    cargador->m_audio = audio;
                    cargador->m_picos = picos;
                    cargador->m_cargado = true;
                    cargador->m_hiloTrabajo = hilo;
                    Q_EMIT cargador->cargaTerminada();
                },
                Qt::QueuedConnection);
        }
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