#include "NovaAudioDeviceManager.h"
#include "NovaLogging.h"

#include <memory>

// Include ARDOUR headers BEFORE private class definition so types are complete
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#pragma push_macro("foreach")
#undef emit
#undef slots
#undef signals
#undef foreach

#include "ardour/audioengine.h"
#include "ardour/audio_backend.h"

#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#pragma pop_macro("foreach")
#endif

// PIMPL implementation - class definition OUTSIDE macro protection
class NovaAudioDeviceManagerPrivate {
public:
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    ARDOUR::AudioEngine *engine = nullptr;
    std::shared_ptr<ARDOUR::AudioBackend> backend;

    QString activeBackend = "None (Dummy)";
    QStringList availableBackends;
    QStringList inputDevices;
    QStringList outputDevices;

    int sampleRate = 44100;
    int bufferSize = 512;
#else
    void *engine = nullptr;

    QString activeBackend = "None (Dummy)";
    QStringList availableBackends;
    QStringList inputDevices;
    QStringList outputDevices;

    int sampleRate = 44100;
    int bufferSize = 512;
#endif
};

NovaAudioDeviceManager::NovaAudioDeviceManager(QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<NovaAudioDeviceManagerPrivate>())
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    d_ptr->availableBackends << "AAudio" << "OpenSL ES" << "None (Dummy)";
    d_ptr->activeBackend = QStringLiteral("AAudio");
#elif defined(Q_OS_WIN)
    d_ptr->availableBackends << "WASAPI" << "ASIO" << "DirectSound" << "None (Dummy)";
    d_ptr->activeBackend = QStringLiteral("WASAPI");
#else
    d_ptr->availableBackends << "ALSA" << "JACK" << "PulseAudio" << "None (Dummy)";
#endif
}

NovaAudioDeviceManager::~NovaAudioDeviceManager() = default;

void NovaAudioDeviceManager::setEngine(void *engine)
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    d_ptr->engine = engine;
#else
    ARDOUR::AudioEngine *ardourEngine = static_cast<ARDOUR::AudioEngine*>(engine);
    d_ptr->engine = ardourEngine;
#endif
    if (d_ptr->engine) {
        refreshDevices();
    }
}

bool NovaAudioDeviceManager::switchBackend(const QString &backendName)
{
    if (!d_ptr->engine) return false;

    qCDebug(novaCore) << "Intentando cambiar backend a:" << backendName;

#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    d_ptr->activeBackend = backendName;
    Q_EMIT activeBackendChanged();
    refreshDevices();
    return true;
#else
    try {
        std::shared_ptr<ARDOUR::AudioBackend> newBackend = d_ptr->engine->set_backend(backendName.toStdString(), "NOVA-Studio", "");
        if (!newBackend) {
            qCWarning(novaCore) << "Imposible instanciar backend:" << backendName << ". Manteniendo actual:" << d_ptr->activeBackend;
            return false;
        }

        d_ptr->backend = newBackend;
        d_ptr->backend->set_sample_rate(static_cast<float>(d_ptr->sampleRate));
        d_ptr->backend->set_buffer_size(d_ptr->bufferSize);

        d_ptr->activeBackend = backendName;
        Q_EMIT activeBackendChanged();

        refreshDevices();
        qCDebug(novaCore) << "Backend cambiado con éxito a:" << d_ptr->activeBackend;
        return true;
    } catch (const std::exception &e) {
        qCCritical(novaCore) << "Excepción al cambiar backend:" << e.what();
        return false;
    }
#endif
}

void NovaAudioDeviceManager::refreshDevices()
{
    if (!d_ptr->engine) return;

    d_ptr->inputDevices.clear();
    d_ptr->outputDevices.clear();

#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    d_ptr->inputDevices << "Micrófono Android (AAudio)" << "Entrada de Audio Móvil";
    d_ptr->outputDevices << "Altavoces Android (AAudio)" << "Salida Auriculares / USB-C";
#elif defined(Q_OS_WIN)
    d_ptr->inputDevices << "Micrófono del Sistema (WASAPI)" << "Entrada DirectSound";
    d_ptr->outputDevices << "Altavoces / Auriculares (WASAPI)" << "Salida DirectSound";
#else
    auto currentBackend = d_ptr->engine->current_backend();
    if (currentBackend) {
        d_ptr->activeBackend = QString::fromStdString(d_ptr->engine->current_backend_name());

        if (currentBackend->use_separate_input_and_output_devices()) {
            auto inputs = currentBackend->enumerate_input_devices();
            for (const auto &dev : inputs) {
                if (dev.available) {
                    d_ptr->inputDevices.append(QString::fromStdString(dev.name));
                }
            }

            auto outputs = currentBackend->enumerate_output_devices();
            for (const auto &dev : outputs) {
                if (dev.available) {
                    d_ptr->outputDevices.append(QString::fromStdString(dev.name));
                }
            }
        } else {
            auto devices = currentBackend->enumerate_devices();
            for (const auto &dev : devices) {
                if (dev.available) {
                    QString devName = QString::fromStdString(dev.name);
                    d_ptr->inputDevices.append(devName);
                    d_ptr->outputDevices.append(devName);
                }
            }
        }
    }

    if (d_ptr->inputDevices.isEmpty()) {
        d_ptr->inputDevices << "Default Hardware Input" << "Micrófono Sistema (ALSA/PipeWire)";
    }
    if (d_ptr->outputDevices.isEmpty()) {
        d_ptr->outputDevices << "Default Hardware Output" << "Altavoces Sistema (ALSA/PipeWire)";
    }
#endif

    Q_EMIT devicesChanged();
    Q_EMIT activeBackendChanged();

    qCDebug(novaCore) << "Dispositivos actualizados. Backend:" << d_ptr->activeBackend
                     << "| Entradas:" << d_ptr->inputDevices.size()
                     << "| Salidas:" << d_ptr->outputDevices.size();
}

bool NovaAudioDeviceManager::setInputDevice(const QString &deviceName)
{
    if (!d_ptr->engine) return false;
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    qCDebug(novaCore) << "Dispositivo de entrada establecido en:" << deviceName;
    return true;
#else
    auto backend = d_ptr->engine->current_backend();
    if (backend) {
        backend->set_input_device_name(deviceName.toStdString());
        qCDebug(novaCore) << "Dispositivo de entrada establecido en:" << deviceName;
        return true;
    }
    return false;
#endif
}

bool NovaAudioDeviceManager::setOutputDevice(const QString &deviceName)
{
    if (!d_ptr->engine) return false;
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    qCDebug(novaCore) << "Dispositivo de salida establecido en:" << deviceName;
    return true;
#else
    auto backend = d_ptr->engine->current_backend();
    if (backend) {
        backend->set_output_device_name(deviceName.toStdString());
        qCDebug(novaCore) << "Dispositivo de salida establecido en:" << deviceName;
        return true;
    }
    return false;
#endif
}

bool NovaAudioDeviceManager::setSampleRate(int rate)
{
    if (rate <= 0 || d_ptr->sampleRate == rate) return false;
    d_ptr->sampleRate = rate;

    if (d_ptr->engine) {
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
        d_ptr->engine->set_sample_rate(d_ptr->sampleRate);
        auto backend = d_ptr->engine->current_backend();
        if (backend) {
            backend->set_sample_rate(static_cast<float>(d_ptr->sampleRate));
        }
#endif
    }

    Q_EMIT audioConfigChanged();
    qCDebug(novaCore) << "Frecuencia de muestreo cambiada a:" << d_ptr->sampleRate << "Hz";
    return true;
}

bool NovaAudioDeviceManager::setBufferSize(int size)
{
    if (size <= 0 || d_ptr->bufferSize == size) return false;
    d_ptr->bufferSize = size;

    if (d_ptr->engine) {
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
        d_ptr->engine->set_buffer_size(d_ptr->bufferSize);
        auto backend = d_ptr->engine->current_backend();
        if (backend) {
            backend->set_buffer_size(d_ptr->bufferSize);
        }
#endif
    }

    Q_EMIT audioConfigChanged();
    qCDebug(novaCore) << "Tamaño de buffer cambiado a:" << d_ptr->bufferSize << "samples";
    return true;
}

QString NovaAudioDeviceManager::activeBackend() const
{
    return d_ptr->activeBackend;
}

QStringList NovaAudioDeviceManager::availableBackends() const
{
    return d_ptr->availableBackends;
}

QStringList NovaAudioDeviceManager::inputDevices() const
{
    return d_ptr->inputDevices;
}

QStringList NovaAudioDeviceManager::outputDevices() const
{
    return d_ptr->outputDevices;
}

int NovaAudioDeviceManager::sampleRate() const
{
    return d_ptr->sampleRate;
}

int NovaAudioDeviceManager::bufferSize() const
{
    return d_ptr->bufferSize;
}