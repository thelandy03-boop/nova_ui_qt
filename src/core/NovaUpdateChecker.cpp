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
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#include <QJniEnvironment>
#endif

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
#if defined(Q_OS_ANDROID)
    // Leer la version instalada directamente del APK (PackageManager) para
    // que nunca desincronice con el release publicado. Si falla, se usa el
    // valor por defecto compilado.
    try {
        QJniObject activity = QJniObject::callStaticObjectMethod(
            "org/qtproject/qt/android/QtNative",
            "activity",
            "()Landroid/app/Activity;");
        if (activity.isValid()) {
            QJniObject pm = activity.callObjectMethod(
                "getPackageManager",
                "()Landroid/content/pm/PackageManager;");
            if (pm.isValid()) {
                QJniObject pkgName = QJniObject::fromString(QStringLiteral("com.novastudio.nova"));
                QJniObject pkgInfo = pm.callObjectMethod(
                    "getPackageInfo",
                    "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;",
                    pkgName.object<jstring>(), 0);
                if (pkgInfo.isValid()) {
                    QJniObject versionName = pkgInfo.getObjectField<jstring>(
                        "versionName");
                    QString v = versionName.toString();
                    if (!v.isEmpty()) {
                        m_currentVersion = v;
                        qCInfo(novaCore) << "UpdateChecker: version instalada leida del APK:" << m_currentVersion;
                    }
                }
            }
        }
    } catch (...) {
        qCWarning(novaCore) << "UpdateChecker: no se pudo leer la version del APK, usando valor compilado";
    }
#endif
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

    // Consulta la ultima release del repositorio en GitHub.
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

void NovaUpdateChecker::downloadAndInstall()
{
#if !defined(Q_OS_ANDROID)
    // En escritorio no hay instalacion integrada: abrir la pagina.
    openDownloadPage();
    return;
#endif
    if (m_downloading || m_downloadUrl.isEmpty()) return;

    m_downloading = true;
    m_downloadProgress = 0;
    m_downloadError.clear();
    Q_EMIT downloadingChanged();
    Q_EMIT downloadProgressChanged();
    Q_EMIT downloadErrorChanged();

    // Directorio de cache de la app (escribible, persistente).
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);
    m_apkPath = cacheDir + QStringLiteral("/nova_update.apk");

    // Eliminar archivo anterior si existe.
    QFile::remove(m_apkPath);

    QUrl url(m_downloadUrl);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NOVA-Studio-UpdateChecker"));
    // Seguir redirecciones (GitHub redirige a S3).
    // QNetworkRequest::RedirectPolicy (Qt 5.9+) o FollowRedirectsAttribute (Qt 5.6+)
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
#endif

    m_downloadReply = m_nam.get(req);
    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &NovaUpdateChecker::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished,
            this, &NovaUpdateChecker::onDownloadFinished);
}

void NovaUpdateChecker::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        m_downloadProgress = static_cast<int>((received * 100) / total);
    } else {
        m_downloadProgress = 0;
    }
    Q_EMIT downloadProgressChanged();
}

void NovaUpdateChecker::onDownloadFinished()
{
    m_downloading = false;
    Q_EMIT downloadingChanged();

    if (!m_downloadReply) return;

    if (m_downloadReply->error() != QNetworkReply::NoError) {
        m_downloadError = m_downloadReply->errorString();
        Q_EMIT downloadErrorChanged();
        qCWarning(novaCore) << "UpdateChecker: error descargando APK:" << m_downloadError;
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }

    // Guardar el APK en disco.
    QFile file(m_apkPath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_downloadError = QStringLiteral("No se pudo escribir el APK en disco");
        Q_EMIT downloadErrorChanged();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }
    file.write(m_downloadReply->readAll());
    file.close();
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;

    qCInfo(novaCore) << "UpdateChecker: APK descargado en" << m_apkPath;

    // Lanzar el instalador de Android via JNI.
#if defined(Q_OS_ANDROID)
    QJniObject jPath = QJniObject::fromString(m_apkPath);
    QJniObject::callStaticMethod<void>(
        "com/novastudio/nova/MainActivity",
        "installApk",
        "(Ljava/lang/String;)V",
        jPath.object<jstring>());
#endif
}