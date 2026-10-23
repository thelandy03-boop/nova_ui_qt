#pragma once

#include "audio/NovaDecodedAudio.h"
#include "core/NovaPeakData.h"

#include <QObject>
#include <QString>
#include <QThread>

#include <atomic>
#include <memory>
#include <mutex>

/**
 * Carga el fichero de audio del spike: lo decodifica y le calcula los picos
 * **en un hilo aparte**, nunca en el hilo de la interfaz.
 *
 * Decodificar un fichero y recorrer todas sus muestras para sacar los picos
 * son operaciones lentas (un fichero de cinco minutos son millones de
 * muestras): si se hicieran en el hilo de la interfaz, la app se congelaría
 * mientras se carga. El spec lo dice explícitamente (§8.4): *"Se calcula en
 * un hilo aparte, nunca en el hilo GUI"*.
 *
 * El fichero se decodifica **una sola vez**: el audio que sale de aquí es el
 * mismo que después se pasa al motor de audio, movido (sin copiar el PCM).
 */
class NovaSpikeLoader : public QObject
{
    Q_OBJECT

public:
    explicit NovaSpikeLoader(QObject* parent = nullptr);
    ~NovaSpikeLoader() override;

    /**
     * Pide cargar un fichero.
     *
     * El trabajo (decodificar y calcular los picos) se hace en un hilo de
     * trabajo; al terminar se emite `cargaTerminada()` o `cargaFallida()`.
     * Esta función no bloquea: vuelve enseguida.
     *
     * @param path Ruta del fichero a cargar.
     */
    void cargar(const QString& path);

    /** true si la última carga ha terminado bien. */
    bool cargado() const { return m_cargado; }
    /** El audio decodificado de la última carga (nulo si no se ha cargado). */
    const std::shared_ptr<NovaDecodedAudio>& audio() const { return m_audio; }

    /**
     * El audio decodificado, para pasarlo al motor de audio.
     *
     * El audio se **mueve** (las muestras no se copian: el PCM de un
     * fichero estéreo de cinco minutos son unos 115 MB) y el cargador se
     * queda sin él: el motor de audio es su nuevo dueño.
     */
    NovaDecodedAudio tomarAudio();

    /** Los picos de la última carga. */
    const PeakData& picos() const { return m_picos; }

    /** El motivo del fallo de la última carga (vacío si no ha fallado). */
    QString error() const { return m_error; }

    /**
     * El hilo en el que se ha hecho el trabajo de la última carga.
     *
     * Sirve para comprobar que el trabajo pesado NO se hace en el hilo de la
     * interfaz (ver `tests/test_audio_loader.cpp`).
     */
    QThread* hiloTrabajo() const { return m_hiloTrabajo; }

    /**
     * Número de carga en curso.
     *
     * Si el usuario pide otro fichero mientras el primero se está
     * cargando, el resultado del primero se descarta (así no se publica una
     * onda que no es la del fichero pedido).
     */
    quint64 generacion() const { return m_generacion; }

signals:
    /** La carga ha terminado bien: el audio y los picos están disponibles. */
    void cargaTerminada();

    /** La carga ha fallado: `error()` dice por qué. */
    void cargaFallida(const QString& error);

private:
    /**
     * Estado de la entrega del resultado, compartido con el hilo de
     * trabajo.
     *
     * El hilo de trabajo **no puede** mirar un puntero débil de este
     * objeto (un `QPointer`): el cargador se puede destruir en el hilo de
     * la interfaz mientras el trabajo está en marcha, y leer ese puntero
     * desde otro hilo es una carrera que puede acabar en un acceso a
     * memoria ya liberada.
     *
     * En su lugar, el trabajo comparte este estado (que vive en un puntero
     * compartido, así que sobrevive al cargador) y consulta `vivo` **bajo
     * el candado** justo antes de entregar el resultado. El destructor
     * marca `vivo = false` bajo ese mismo candado: así, o el trabajo
     * entrega el resultado mientras el cargador sigue vivo, o no entrega
     * nada.
     *
     * Invariante: el estado compartido **solo** se mira y se toca bajo su
     * candado (el hilo de trabajo y el destructor son los dos que lo
     * usan).
     */
    struct EstadoEntrega {
        std::mutex mutex;
        std::atomic<bool> vivo{true};
    };

    /** El audio de la última carga (se mueve al motor, sin copiar). */
    std::shared_ptr<NovaDecodedAudio> m_audio;

    /** Los picos de la última carga. */
    PeakData m_picos;

    /** El motivo del fallo de la última carga. */
    QString m_error;

    /** true si la última carga ha terminado bien. */
    bool m_cargado = false;

    /** El hilo en el que se hizo el trabajo de la última carga. */
    QThread* m_hiloTrabajo = nullptr;

    /**
     * Número de carga: cada `cargar()` incrementa este número, y el
     * resultado de una carga que ya no es la última se descarta.
     */
    quint64 m_generacion = 0;

    /**
     * Estado de la entrega, compartido con los hilos de trabajo.
     *
     * Vive en un puntero compartido: los hilos de trabajo lo tienen
     * mientras trabajan, aunque este cargador se destruya.
     */
    std::shared_ptr<EstadoEntrega> m_estado;
};
