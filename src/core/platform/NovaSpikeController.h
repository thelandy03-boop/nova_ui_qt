#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

// El motor de audio de Android (Oboe). Solo se declara aquí: el controlador
// es el único que lo instancia, y solo existe en Android.
class NovaAndroidAudioEngine;

/**
 * Controlador del spike de audio en Android.
 *
 * Es el dueño del motor de audio (nadie más lo instancia en Android: la
 * sesión usa el stub de Ardour) y el que publica la forma de onda:
 *
 *  - `loadAndPlay()` decodifica el fichero, calcula sus picos, los publica
 *    en la caché de formas de onda bajo el id "spike" (el mismo que pide la
 *    página QML) y pone el fichero en marcha.
 *  - `positionSeconds()` dice dónde está el cabezal. La posición la lleva el
 *    motor (los fotogramas que ha escrito de verdad); el temporizador solo
 *    refresca la lectura.
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
     * Decodifica el fichero, calcula sus picos, los publica y lo reproduce.
     *
     * @param path Ruta del fichero a reproducir.
     * @return true solo si el fichero se ha podido cargar y reproducir.
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

private:
    /** Refresca la posición del cabezal (la del motor, no la del reloj). */
    void actualizarPosicion();

    /** Cambia el texto de estado y avisa. */
    void actualizarEstado(const QString& texto);

    /** Cambia la duración del fichero y avisa (con la misma señal). */
    void actualizarDuracion(double segundos);

    QString m_status;
    double m_durationSeconds = 0.0;

    // true si el motor de audio se ha podido inicializar (sin motor no hay
    // sonido, y el estado lo dice).
    bool m_motorListo = false;

    // Reloj de refresco de la posición (el motor lleva la posición real).
    QTimer m_temporizador;

    // El motor de audio: lo crea y lo destruye el controlador.
    std::unique_ptr<NovaAndroidAudioEngine> m_engine;
};
