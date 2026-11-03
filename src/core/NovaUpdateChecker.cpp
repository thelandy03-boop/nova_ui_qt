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
#include <QCryptographicHash>
#include <QSettings>
#include <QTimer>
#include <QVersionNumber>
#include <QCoreApplication>

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
                    QJniObject versionName = pkgInfo.getObjectField(
                        "versionName", "Ljava/lang/String;");
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

    // Directorio de cache y ruta del APK
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);
    m_apkPath = cacheDir + QStringLiteral("/nova_update.apk");
    if (QFile::exists(m_apkPath) && QFileInfo(m_apkPath).size() > 1024 * 1024) {
        m_apkReady = true;
    }

    // Comprobar permiso de instalacion.
    updateInstallPermission();

    // Limpiar APKs antiguos del cache.
    cleanOldApks();
#endif

    // 1. Cargar base de datos de novedades nativamente desde JSON
    loadWhatsNewData();

    // 2. Comprobar con SemVer si hay novedades que mostrar de una actualización
    QSettings settings;
    if (!settings.contains(QStringLiteral("last_seen_version"))) {
        // Primera instalación (Fresh install): registrar versión para no interrumpir al usuario novato,
        // el diálogo se mostrará en su primera actualización real posterior.
        settings.setValue(QStringLiteral("last_seen_version"), m_currentVersion);
        m_showWhatsNew = false;
    } else {
        const QString lastSeenStr = settings.value(QStringLiteral("last_seen_version")).toString();
        const QVersionNumber currentVer = QVersionNumber::fromString(m_currentVersion);
        const QVersionNumber lastSeenVer = QVersionNumber::fromString(lastSeenStr);
        if (!currentVer.isNull() && !lastSeenVer.isNull()) {
            m_showWhatsNew = (currentVer > lastSeenVer);
        } else {
            m_showWhatsNew = (m_currentVersion != lastSeenStr);
        }
    }
}

void NovaUpdateChecker::dismissWhatsNew()
{
    QSettings settings;
    settings.setValue(QStringLiteral("last_seen_version"), m_currentVersion);
    if (m_showWhatsNew) {
        m_showWhatsNew = false;
        Q_EMIT showWhatsNewChanged();
    }
}

void NovaUpdateChecker::triggerWhatsNew()
{
    m_showWhatsNew = true;
    Q_EMIT showWhatsNewChanged();
}

void NovaUpdateChecker::loadWhatsNewData()
{
    const QStringList candidates = {
        QStringLiteral(":/qt/qml/NovaStudio/qml/dialogs/whats_new.json"),
        QStringLiteral("qrc:/qt/qml/NovaStudio/qml/dialogs/whats_new.json"),
        QDir::current().filePath(QStringLiteral("qml/dialogs/whats_new.json")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../qml/dialogs/whats_new.json")),
        QStringLiteral(":/nova_ui_qt/qml/dialogs/whats_new.json"),
        QStringLiteral(":/qml/dialogs/whats_new.json")
    };

    QByteArray jsonData;
    QString usedPath;
    for (const QString &path : candidates) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            jsonData = file.readAll();
            usedPath = path;
            file.close();
            break;
        }
    }

    if (jsonData.isEmpty()) {
        qCWarning(novaCore) << "WhatsNew: No se pudo encontrar whats_new.json en las rutas candidatas.";
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(novaCore) << "WhatsNew: Error de sintaxis en JSON (" << usedPath << "):" << parseError.errorString();
        return;
    }

    QJsonObject root = doc.object();
    m_allReleases = root.value(QStringLiteral("releases")).toObject();

    // Extraer lista de versiones ordenadas semánticamente descendente
    QStringList versions = m_allReleases.keys();
    std::sort(versions.begin(), versions.end(), [](const QString &a, const QString &b) {
        return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
    });
    m_availableVersions = versions;
    Q_EMIT availableVersionsChanged();

    // Seleccionar la versión instalada o fallback a la más reciente disponible
    QString targetVer = m_currentVersion;
    if (!m_allReleases.contains(targetVer) && !m_availableVersions.isEmpty()) {
        targetVer = m_availableVersions.first();
    }
    selectWhatsNewVersion(targetVer);
}

void NovaUpdateChecker::selectWhatsNewVersion(const QString &version)
{
    m_selectedWhatsNewVersion = version;
    QJsonObject releaseObj = m_allReleases.value(version).toObject();
    m_currentWhatsNew = processReleaseData(releaseObj, version);
    Q_EMIT currentWhatsNewChanged();
}

QVariantMap NovaUpdateChecker::processReleaseData(const QJsonObject &releaseObj, const QString &ver)
{
    QVariantMap result;
    result[QStringLiteral("version")] = ver;
    result[QStringLiteral("updateBadge")] = releaseObj.value(QStringLiteral("updateBadge")).toString(QStringLiteral("✨ ACTUALIZACIÓN"));
    result[QStringLiteral("title")] = releaseObj.value(QStringLiteral("title")).toString(QStringLiteral("¿Qué hay de nuevo en Nova?"));
    result[QStringLiteral("description")] = releaseObj.value(QStringLiteral("description")).toString();
    result[QStringLiteral("buttonText")] = releaseObj.value(QStringLiteral("buttonText")).toString(QStringLiteral("¡Empezar a Crear! ➜"));

    QJsonArray rawFeatures = releaseObj.value(QStringLiteral("features")).toArray();
    QVariantList processedList;

    for (const QJsonValue &val : rawFeatures) {
        QJsonObject item = val.toObject();
        QString cat = item.value(QStringLiteral("category")).toString(QStringLiteral("MEJORA")).toUpper();
        QString icon = item.value(QStringLiteral("icon")).toString();
        QString title = item.value(QStringLiteral("title")).toString();
        QString desc = item.value(QStringLiteral("description")).toString();

        QString accentText = QStringLiteral("#60A5FA");
        QString accentBorder = QStringLiteral("#3B82F6");
        QString accentBg = QStringLiteral("#1E3A5F");
        QString accentGlow = QStringLiteral("#143B82F6");

        if (cat == QStringLiteral("NUEVO")) {
            accentText = QStringLiteral("#34D399");
            accentBorder = QStringLiteral("#10B981");
            accentBg = QStringLiteral("#064E3B");
            accentGlow = QStringLiteral("#1410B981");
            if (icon.isEmpty()) icon = QStringLiteral("✦");
        } else if (cat == QStringLiteral("MEJORA")) {
            accentText = QStringLiteral("#60A5FA");
            accentBorder = QStringLiteral("#3B82F6");
            accentBg = QStringLiteral("#1E3A5F");
            accentGlow = QStringLiteral("#143B82F6");
            if (icon.isEmpty()) icon = QStringLiteral("↔");
        } else if (cat == QStringLiteral("CORRECCIÓN") || cat == QStringLiteral("CORRECCION")) {
            accentText = QStringLiteral("#FBBF24");
            accentBorder = QStringLiteral("#F59E0B");
            accentBg = QStringLiteral("#451A03");
            accentGlow = QStringLiteral("#14F59E0B");
            if (icon.isEmpty()) icon = QStringLiteral("♫");
        } else if (cat == QStringLiteral("ESTABLE")) {
            accentText = QStringLiteral("#C4B5FD");
            accentBorder = QStringLiteral("#8B5CF6");
            accentBg = QStringLiteral("#3B185F");
            accentGlow = QStringLiteral("#148B5CF6");
            if (icon.isEmpty()) icon = QStringLiteral("●");
        } else if (cat == QStringLiteral("RENDIMIENTO")) {
            accentText = QStringLiteral("#38BDF8");
            accentBorder = QStringLiteral("#0284C7");
            accentBg = QStringLiteral("#0C4A6E");
            accentGlow = QStringLiteral("#140284C7");
            if (icon.isEmpty()) icon = QStringLiteral("⚡");
        } else {
            if (icon.isEmpty()) icon = QStringLiteral("✦");
        }

        if (item.contains(QStringLiteral("accentText"))) accentText = item.value(QStringLiteral("accentText")).toString();
        if (item.contains(QStringLiteral("accentBorder"))) accentBorder = item.value(QStringLiteral("accentBorder")).toString();
        if (item.contains(QStringLiteral("accentBg"))) accentBg = item.value(QStringLiteral("accentBg")).toString();
        if (item.contains(QStringLiteral("accentGlow"))) accentGlow = item.value(QStringLiteral("accentGlow")).toString();

        QVariantMap card;
        card[QStringLiteral("category")] = cat;
        card[QStringLiteral("badge")] = cat;
        card[QStringLiteral("icon")] = icon;
        card[QStringLiteral("title")] = title;
        card[QStringLiteral("description")] = desc;
        card[QStringLiteral("accentText")] = accentText;
        card[QStringLiteral("accentBorder")] = accentBorder;
        card[QStringLiteral("accentBg")] = accentBg;
        card[QStringLiteral("accentGlow")] = accentGlow;

        processedList.append(card);
    }

    result[QStringLiteral("features")] = processedList;
    result[QStringLiteral("count")] = processedList.size();
    return result;
}

void NovaUpdateChecker::reloadWhatsNew()
{
    loadWhatsNewData();
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
    QString fallbackApkUrl;
    QString fallbackSha256;
    qint64 fallbackSize = 0;
    qint64 expectedSize = 0;
    const QString abi = currentAbiToken();
    QJsonArray assets = obj.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &a : assets) {
        QJsonObject ao = a.toObject();
        QString name = ao.value(QStringLiteral("name")).toString();
        if (!name.endsWith(QStringLiteral(".apk"), Qt::CaseInsensitive)) continue;
        if (!abi.isEmpty() && name.contains(abi, Qt::CaseInsensitive)) {
            apkUrl = ao.value(QStringLiteral("browser_download_url")).toString();
            expectedSize = ao.value(QStringLiteral("size")).toInteger();
            // Extraer SHA256 del asset si esta disponible (API de GitHub).
            QString digest = ao.value(QStringLiteral("digest")).toString();
            if (digest.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive)) {
                m_expectedSha256 = digest.mid(7).toLower();
            }
            break;
        }
        // Permitir NovaStudio.apk sin sufijo de arquitectura como paquete estándar de teléfono
        if (fallbackApkUrl.isEmpty() && name.compare(QStringLiteral("NovaStudio.apk"), Qt::CaseInsensitive) == 0) {
            fallbackApkUrl = ao.value(QStringLiteral("browser_download_url")).toString();
            fallbackSize = ao.value(QStringLiteral("size")).toInteger();
            QString digest = ao.value(QStringLiteral("digest")).toString();
            if (digest.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive)) {
                fallbackSha256 = digest.mid(7).toLower();
            }
        }
    }
    // Si no hubo asset específico del ABI y estamos en teléfono (arm64-v8a o por defecto), usar NovaStudio.apk
    if (apkUrl.isEmpty() && !fallbackApkUrl.isEmpty() && (abi.isEmpty() || abi == QStringLiteral("arm64-v8a"))) {
        apkUrl = fallbackApkUrl;
        expectedSize = fallbackSize;
        if (!fallbackSha256.isEmpty()) {
            m_expectedSha256 = fallbackSha256;
        }
    }
    m_expectedFileSize = expectedSize;
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
    if (m_downloading) return;

    if (m_apkPath.isEmpty()) {
        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(cacheDir);
        m_apkPath = cacheDir + QStringLiteral("/nova_update.apk");
    }

    // Si el APK ya se descargó previamente y es válido, intentar instalar directamente
    if (m_apkReady && QFile::exists(m_apkPath) && QFileInfo(m_apkPath).size() > 1024 * 1024) {
        if (m_expectedSha256.isEmpty() || verifySha256(m_apkPath, m_expectedSha256)) {
            qCInfo(novaCore) << "UpdateChecker: APK ya disponible en cache, lanzando instalador directamente";
            installApk();
            return;
        }
    }

    if (m_downloadUrl.isEmpty()) return;

    m_apkReady = false;
    Q_EMIT apkReadyChanged();

    m_downloading = true;
    m_downloadProgress = 0;
    m_downloadError.clear();
    Q_EMIT downloadingChanged();
    Q_EMIT downloadProgressChanged();
    Q_EMIT downloadErrorChanged();

    // Eliminar archivo anterior si existe.
    QFile::remove(m_apkPath);

    QUrl url(m_downloadUrl);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NOVA-Studio-UpdateChecker"));
    // Timeout explicito de 300 segundos (5 minutos) para permitir descargas completas de archivos de ~40MB
    req.setTransferTimeout(300000);
    // Seguir redirecciones (GitHub redirige a S3).
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

    qint64 downloadedSize = QFileInfo(m_apkPath).size();
    qCInfo(novaCore) << "UpdateChecker: APK descargado en" << m_apkPath << "Tamaño:" << downloadedSize << "bytes";

    // Validar integridad mínima: un APK de NovaStudio completo pesa ~39MB.
    // Si pesa menos de 5MB o difiere del tamaño esperado de GitHub, está truncado/corrupto.
    if (downloadedSize < 5 * 1024 * 1024 || (m_expectedFileSize > 0 && downloadedSize != m_expectedFileSize)) {
        m_downloadError = QStringLiteral("La descarga se interrumpió o el archivo quedó incompleto. Pulsa en Descargar para reintentar.");
        Q_EMIT downloadErrorChanged();
        QFile::remove(m_apkPath);
        qCWarning(novaCore) << "UpdateChecker: APK incompleto (" << downloadedSize
                            << "bytes vs esperado:" << m_expectedFileSize << "), archivo eliminado.";
        return;
    }

    // Verificar SHA256 si tenemos hash esperado.
    if (!m_expectedSha256.isEmpty()) {
        if (!verifySha256(m_apkPath, m_expectedSha256)) {
            m_downloadError = QStringLiteral("El APK descargado no coincide con el hash esperado (SHA256). Descarga cancelada.");
            Q_EMIT downloadErrorChanged();
            QFile::remove(m_apkPath);
            qCWarning(novaCore) << "UpdateChecker: SHA256 no coincide, APK eliminado";
            return;
        }
        qCInfo(novaCore) << "UpdateChecker: SHA256 verificado correctamente";
    }

    m_apkReady = true;
    Q_EMIT apkReadyChanged();

    // Proceder con la instalación
    installApk();
}

void NovaUpdateChecker::installApk()
{
#if !defined(Q_OS_ANDROID)
    openDownloadPage();
    return;
#endif

    if (m_apkPath.isEmpty() || !QFile::exists(m_apkPath)) {
        m_apkReady = false;
        Q_EMIT apkReadyChanged();
        downloadAndInstall();
        return;
    }

    // Comprobar permiso de instalacion antes de lanzar el intent.
    updateInstallPermission();
    if (!m_installPermissionGranted) {
        // Abrir Ajustes para que el usuario conceda el permiso.
        requestInstallPermission();
        m_downloadError = QStringLiteral("Concede el permiso para instalar aplicaciones en Ajustes de Android y pulsa Instalar.");
        Q_EMIT downloadErrorChanged();
        return;
    }

    m_downloadError.clear();
    Q_EMIT downloadErrorChanged();

    // Lanzar el instalador de Android via JNI.
#if defined(Q_OS_ANDROID)
    qCInfo(novaCore) << "UpdateChecker: invocando MainActivity.installApk:" << m_apkPath;
    QJniObject jPath = QJniObject::fromString(m_apkPath);
    QJniObject::callStaticMethod<void>(
        "com/novastudio/nova/MainActivity",
        "installApk",
        "(Ljava/lang/String;)V",
        jPath.object<jstring>());
#endif
}

void NovaUpdateChecker::checkInstallPermission()
{
#if defined(Q_OS_ANDROID)
    updateInstallPermission();
    if (m_installPermissionGranted && !m_downloadError.isEmpty()) {
        m_downloadError.clear();
        Q_EMIT downloadErrorChanged();
    }
#endif
}

void NovaUpdateChecker::requestInstallPermission()
{
#if defined(Q_OS_ANDROID)
    QJniObject::callStaticMethod<void>(
        "com/novastudio/nova/MainActivity",
        "openInstallPermissionSettings",
        "()V");
#endif
}

void NovaUpdateChecker::updateInstallPermission()
{
#if defined(Q_OS_ANDROID)
    jboolean granted = QJniObject::callStaticMethod<jboolean>(
        "com/novastudio/nova/MainActivity",
        "canInstallApks",
        "()Z");
    bool newVal = (granted == JNI_TRUE);
    if (newVal != m_installPermissionGranted) {
        m_installPermissionGranted = newVal;
        Q_EMIT installPermissionGrantedChanged();
    }
#endif
}

bool NovaUpdateChecker::verifySha256(const QString &filePath, const QString &expectedHash)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return false;
    file.close();
    QString actual = QString::fromLatin1(hash.result().toHex());
    return actual.compare(expectedHash, Qt::CaseInsensitive) == 0;
}

void NovaUpdateChecker::cleanOldApks()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir dir(cacheDir);
    if (!dir.exists()) return;
    // Eliminar APKs antiguos (nova_update*.apk) excepto el actual.
    QStringList filters;
    filters << "nova_update*.apk";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &fi : files) {
        if (fi.absoluteFilePath() != m_apkPath) {
            QFile::remove(fi.absoluteFilePath());
        }
    }
}