#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

// Forward declarations - NO ARDOUR/PBD includes in public header
namespace ARDOUR {
    class AudioEngine;
    class AudioBackend;
}
class NovaAudioDeviceManagerPrivate;

class NovaAudioDeviceManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString activeBackend READ activeBackend NOTIFY activeBackendChanged)
    Q_PROPERTY(QStringList availableBackends READ availableBackends NOTIFY availableBackendsChanged)
    Q_PROPERTY(QStringList inputDevices READ inputDevices NOTIFY devicesChanged)
    Q_PROPERTY(QStringList outputDevices READ outputDevices NOTIFY devicesChanged)
    Q_PROPERTY(int sampleRate READ sampleRate WRITE setSampleRate NOTIFY audioConfigChanged)
    Q_PROPERTY(int bufferSize READ bufferSize WRITE setBufferSize NOTIFY audioConfigChanged)

public:
    explicit NovaAudioDeviceManager(QObject *parent = nullptr);
    ~NovaAudioDeviceManager() override;

    void setEngine(void *engine);

    // 🎛️ Métodos expuestos a QML
    Q_INVOKABLE bool switchBackend(const QString &backendName);
    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE bool setInputDevice(const QString &deviceName);
    Q_INVOKABLE bool setOutputDevice(const QString &deviceName);
    Q_INVOKABLE bool setSampleRate(int rate);
    Q_INVOKABLE bool setBufferSize(int size);

    QString activeBackend() const;
    QStringList availableBackends() const;
    QStringList inputDevices() const;
    QStringList outputDevices() const;
    int sampleRate() const;
    int bufferSize() const;

signals:
    void activeBackendChanged();
    void availableBackendsChanged();
    void devicesChanged();
    void audioConfigChanged();

private:
    std::unique_ptr<NovaAudioDeviceManagerPrivate> d_ptr;
};