#pragma once

#include <QObject>
#include <QString>
#include <memory>

// Forward declaration - NO includes de ARDOUR/PBD en header público
class NovaSessionManagerPrivate;

class NovaSessionManager : public QObject
{
    Q_OBJECT

public:
    explicit NovaSessionManager(QObject *parent = nullptr);
    ~NovaSessionManager() override;

    bool initSession();
    void closeCurrentSession();
    void shutdownAudioEngine();

    void setSession(void *session) { m_session = session; }
    void *session() const { return m_session; }
    void *engine() const { return m_engine; }

signals:
    void sessionInitialized(void *session);

private:
    std::unique_ptr<NovaSessionManagerPrivate> d_ptr;
    void *m_session = nullptr;
    void *m_engine = nullptr;
};