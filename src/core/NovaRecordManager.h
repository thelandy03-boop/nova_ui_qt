#pragma once

#include <QObject>
#include <QString>
#include <vector>
#include <memory>

// Forward declarations - NO ARDOUR/PBD includes in public header
class NovaRegionModel;
class NovaTransportController;
class NovaRecordManagerPrivate;

class NovaRecordManager : public QObject
{
    Q_OBJECT

public:
    // 🛡️ Enums para evitar bool traps (ENM-9)
    enum class RecordMode { None, Armed, Recording };
    enum class RecordAction { Stop, PunchOut };

    explicit NovaRecordManager(QObject *parent = nullptr);
    ~NovaRecordManager() override;

    void setSession(void *session);
    void setRegionModel(NovaRegionModel *regionModel);
    void setTransportController(NovaTransportController *transport);

    void toggleRecord();
    bool isRecording() const { return m_isRecording; }

signals:
    void isRecordingChanged();

private slots:
    void onPositionChanged();
    void onTransportStopped();

private:
    void finalizeRecording();

    std::unique_ptr<NovaRecordManagerPrivate> d_ptr;

    NovaRegionModel *m_regionModel = nullptr;
    NovaTransportController *m_transport = nullptr;

    bool m_isRecording = false;
    bool m_liveRegionActive = false;
    double m_recordStartBeat = 0.0;
};