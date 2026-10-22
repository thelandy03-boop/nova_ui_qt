#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * Comprobador de actualizaciones obligatorias.
 *
 * Consulta la última release de GitHub y, si es más nueva que la versión
 * instalada, marca `updateRequired = true`. La UI debe bloquear el uso de la
 * app hasta que el usuario descargue e instale la nueva versión.
 */
class NovaUpdateChecker : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool updateRequired READ updateRequired NOTIFY updateRequiredChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY downloadUrlChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)

public:
    explicit NovaUpdateChecker(QObject *parent = nullptr);

    bool updateRequired() const { return m_updateRequired; }
    QString latestVersion() const { return m_latestVersion; }
    QString downloadUrl() const { return m_downloadUrl; }
    QString currentVersion() const { return m_currentVersion; }
    bool checking() const { return m_checking; }

    /** Lanza la comprobación (asíncrona). */
    Q_INVOKABLE void check();

    /** Abre la URL de descarga en el navegador/sistema. */
    Q_INVOKABLE void openDownloadPage();

signals:
    void updateRequiredChanged();
    void latestVersionChanged();
    void downloadUrlChanged();
    void checkingChanged();

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    bool isNewer(const QString &latest, const QString &current) const;

    QNetworkAccessManager m_nam;
    bool m_updateRequired = false;
    QString m_latestVersion;
    QString m_downloadUrl;
    QString m_currentVersion = QStringLiteral("1.0.1");
    bool m_checking = false;
};
