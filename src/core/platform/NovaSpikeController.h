#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

// El motor de audio de Android (Oboe). Solo se declara aquí: el controlador
// es el único que lo instancia, y solo existe en Android.
class NovaAndroidAudioEngine;

// El cargador del fichero: lo decodifica y le calcula los picos en un hilo
// aparte (nunca en el hilo de la interfaz).
class NovaSpikeLoader;

/**
 * Controlador del spike de audio en Android.
 *
 * Es el dueño del motor de audio (nadie más lo instancia en Android: la
 * sesión usa el stub de Ardour) y el que publica la forma de onda:
 *
 *  - `loadAndPlay()` pide el fichero al cargador, que lo decodifica y le
 *    calcula los picos **en un hilo aparte** (el spec §8.4 lo exige: los
 *    picos nunca se calculan en el hilo de la interfaz). Cuando termina,
 *    los picos se publican en la caché de formas de onda bajo el id "spike"
 *    (el mismo que pide la página QML), el audio ya decodificado se pasa al
 *    motor (movido, sin copiar) y el fichero se pone en marcha.
 *  - `positionSeconds()` dice dónde está el cabezal. La posición la lleva
 *    el motor (los fotogramas que ha escrito de verdad); el temporizador
 *    solo refresca la lectura.
 */
class NovaSpikeController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double positionSeconds READ positionSeconds NOTIFY positionChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY statusChanged)

public:
    explicit NovaSpikeController(QObject* parent = nullptr);
    ~NovaSpikeController() override;

    /**
     * Pide cargar y reproducir un fichero.
     *
     * El trabajo pesado (decodificar y calcular los picos) se hace en un
     * hilo aparte, así que esta función no bloquea: devuelve enseguida y el
     * estado va cambiando según avance (el texto del estado dice en qué
     * punto está).
     *
     * @param path Ruta del fichero a reproducir.
     * @return true si la carga se ha pedido; false si no hay motor de audio.
     */
    Q_INVOKABLE bool loadAndPlay(const QString& path);

    /** Para la reproducción. */
    Q_INVOKABLE void stop();

    /** Posición del cabezal, en segundos. */
    double positionSeconds() const;

    /** Texto que cuenta qué está pasando (o por qué ha fallado). */
    QString status() const { return m_status; }

    /** Duración del fichero cargado, en segundos (0 si no hay ninguno). */
    double durationSeconds() const { return m_durationSeconds; }

signals:
    /** La posición del cabezal ha cambiado. */
    void positionChanged();

    /** El estado (o la duración) han cambiado. */
    void statusChanged();

private slots:
    /** El fichero se ha decodificado y sus picos están calculados. */
    void alCargarTerminado();

    /** La carga ha fallado (el motivo va en el argumento). */
    void alCargarFallido(const QString& error);

private:
    /**
     * Deja el motor de audio listo para reproducir (initialize + start).
     *
     * Se reintenta en cada carga: al arrancar, el permiso de micro se pide
     * después de crear la interfaz, y el motor puede quedarse sin salida
     * hasta que se conceda.
     *
     * @return true si el motor está listo para reproducir.
     */
    bool prepararMotor();

    /** Quita de la caché la onda publicada y pone la duración a cero. */
    void limpiarOndaPublicada();

    /** Refresca la posición del cabezal (la del motor, no la del reloj). */
    void actualizarPosicion();

    /** Cambia el texto de estado y avisa. */
    void actualizarEstado(const QString& texto);

    /** Cambia la duración del fichero y avisa. */
    void actualizarDuracion(double segundos);

    QString m_status;
    double m_durationSeconds = 0.0;

    // true si el motor de audio se ha podido inicializar (sin motor no hay
    // sonido, y el estado lo dice).
    bool m_motorListo = false;

    // Última posición avisada: solo se emite positionChanged cuando cambia.
    double m_ultimaPosicion = 0.0;

    // Reloj de refresco de la posición (el motor lleva la posición real).
    QTimer m_temporizador;

    // El cargador del fichero (decodifica y calcula los picos en un hilo
    // aparte).
    std::unique_ptr<NovaSpikeLoader> m_cargador;

    // El motor de audio: lo crea y lo destruye el controlador.
    std::unique_ptr<NovaAndroidAudioEngine> m_engine;
};
