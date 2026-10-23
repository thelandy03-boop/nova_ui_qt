#include "NovaUpdateChecker.h"
#include "core/NovaLogging.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDesktopServices>
#include <QDebug>
#include <QSslCertificate>
#include <QSslConfiguration>

namespace {
// Token que identifica el ABI del binario en el nombre del asset del release.
QString currentAbiToken()
{
#if defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("arm64");
#elif defined(Q_PROCESSOR_X86_64)
    return QStringLiteral("x86_64");
#elif defined(Q_PROCESSOR_X86)
    return QStringLiteral("x86");
#elif defined(Q_PROCESSOR_ARM)
    return QStringLiteral("armeabi");
#else
    return QString();
#endif
}
} // namespace

NovaUpdateChecker::NovaUpdateChecker(QObject *parent)
    : QObject(parent)
{
}

void NovaUpdateChecker::check()
{
#if !defined(Q_OS_ANDROID)
    // Los releases publican APKs: en escritorio no hay actualizacion que
    // instalar, asi que no se consulta ni se bloquea la app.
    return;
#endif
    if (m_checking) return;
    m_checking = true;
    Q_EMIT checkingChanged();

    // Consulta la última release del repositorio en GitHub.
    QUrl url(QStringLiteral("https://api.github.com/repos/thelandy03-boop/nova_ui_qt/releases/latest"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NOVA-Studio-UpdateChecker"));
    req.setRawHeader("Accept", "application/vnd.github+json");

    // Android no trae un almacen de CAs usable por OpenSSL: se usa el bundle
    // Mozilla embebido en el binario.
    const QList<QSslCertificate> caCerts =
        QSslCertificate::fromPath(QStringLiteral(":/certs/cacert.pem"));
    if (!caCerts.isEmpty()) {
        QSslConfiguration ssl = req.sslConfiguration();
        ssl.setCaCertificates(caCerts);
        req.setSslConfiguration(ssl);
    } else {
        qCWarning(novaCore) << "UpdateChecker: no se pudo cargar el bundle de CAs";
    }

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
}

void NovaUpdateChecker::onReplyFinished(QNetworkReply *reply)
{
    m_checking = false;
    Q_EMIT checkingChanged();

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(novaCore) << "UpdateChecker: error de red:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString tag = obj.value(QStringLiteral("tag_name")).toString();
    // Quitar prefijo 'v' si lo tiene.
    if (tag.startsWith(QLatin1Char('v'))) tag = tag.mid(1);

    // Buscar un asset .apk para descarga directa, prefiriendo el ABI actual
    // (arm64-v8a en telefonos, x86_64 en Waydroid/emulador).
    QString apkUrl;
    const QString abi = currentAbiToken();
    QJsonArray assets = obj.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &a : assets) {
        QJsonObject ao = a.toObject();
        QString name = ao.value(QStringLiteral("name")).toString();
        if (!name.endsWith(QStringLiteral(".apk"), Qt::CaseInsensitive)) continue;
        if (!abi.isEmpty() && name.contains(abi, Qt::CaseInsensitive)) {
            apkUrl = ao.value(QStringLiteral("browser_download_url")).toString();
            break;
        }
    }
    // Sin asset para este ABI: abrir la pagina de la release, nunca ofrecer
    // un APK de otra arquitectura.
    if (apkUrl.isEmpty()) {
        apkUrl = obj.value(QStringLiteral("html_url")).toString();
    }

    m_latestVersion = tag;
    m_downloadUrl = apkUrl;
    Q_EMIT latestVersionChanged();
    Q_EMIT downloadUrlChanged();

#if defined(Q_OS_ANDROID)
    // La actualizacion obligatoria solo aplica a Android: los releases
    // publican APKs, no instaladores de escritorio.
    if (!tag.isEmpty() && isNewer(tag, m_currentVersion)) {
        m_updateRequired = true;
        Q_EMIT updateRequiredChanged();
        qCInfo(novaCore) << "UpdateChecker: nueva version disponible:" << tag
                         << "(instalada:" << m_currentVersion << ")";
    }
#endif
}

bool NovaUpdateChecker::isNewer(const QString &latest, const QString &current) const
{
    // Comparacion simple por componentes numericos (x.y.z).
    QStringList l = latest.split(QLatin1Char('.'));
    QStringList c = current.split(QLatin1Char('.'));
    int n = qMax(l.size(), c.size());
    for (int i = 0; i < n; ++i) {
        int lv = (i < l.size()) ? l[i].toInt() : 0;
        int cv = (i < c.size()) ? c[i].toInt() : 0;
        if (lv != cv) return lv > cv;
    }
    return false;
}

void NovaUpdateChecker::openDownloadPage()
{
    if (!m_downloadUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_downloadUrl));
    }
}
