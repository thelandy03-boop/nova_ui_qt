#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * Comprobador de actualizaciones obligatorias con descarga e instalacion
 * integrada (Android). Consulta la ultima release de GitHub y, si es mas
 * nueva que la version instalada, marca `updateRequired = true`. La UI debe
 * bloquear el uso de la app hasta que el usuario descargue e instale la
 * nueva version. La descarga se hace dentro de la app y la instalacion se
 * lanza mediante un Intent (ACTION_VIEW) con FileProvider.
 */
class NovaUpdateChecker : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool updateRequired READ updateRequired NOTIFY updateRequiredChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY downloadUrlChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString downloadError READ downloadError NOTIFY downloadErrorChanged)

public:
    explicit NovaUpdateChecker(QObject *parent = nullptr);

    bool updateRequired() const { return m_updateRequired; }
    QString latestVersion() const { return m_latestVersion; }
    QString downloadUrl() const { return m_downloadUrl; }
    QString currentVersion() const { return m_currentVersion; }
    bool checking() const { return m_checking; }
    bool downloading() const { return m_downloading; }
    int downloadProgress() const { return m_downloadProgress; }
    QString downloadError() const { return m_downloadError; }

    /** Lanza la comprobacion (asincrona). */
    Q_INVOKABLE void check();

    /** Abre la URL de descarga en el navegador/sistema (fallback). */
    Q_INVOKABLE void openDownloadPage();

    /** Descarga el APK e inicia la instalacion integrada (Android). */
    Q_INVOKABLE void downloadAndInstall();

signals:
    void updateRequiredChanged();
    void latestVersionChanged();
    void downloadUrlChanged();
    void checkingChanged();
    void downloadingChanged();
    void downloadProgressChanged();
    void downloadErrorChanged();

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();

private:
    bool isNewer(const QString &latest, const QString &current) const;
    void startDownload(const QString &url);

    QNetworkAccessManager m_nam;
    bool m_updateRequired = false;
    QString m_latestVersion;
    QString m_downloadUrl;
    QString m_currentVersion = QStringLiteral("1.0.7");
    bool m_checking = false;
    bool m_downloading = false;
    int m_downloadProgress = 0;
    QString m_downloadError;
    QNetworkReply *m_downloadReply = nullptr;
    QString m_apkPath;
};