#include "audio/dsp/jsfx/NovaJsfxPackageManager.h"
#include "audio/dsp/jsfx/NovaJsfxManager.h"
#include "core/NovaLogging.h"
#include "core/platform/NovaPlatformUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QUrl>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <set>


static NovaJsfxPackageManager *s_packageManagerInstance = nullptr;

NovaJsfxPackageManager* NovaJsfxPackageManager::instance()
{
    if (!s_packageManagerInstance) {
        s_packageManagerInstance = new NovaJsfxPackageManager();
    }
    return s_packageManagerInstance;
}

NovaJsfxPackageManager::NovaJsfxPackageManager(QObject *parent)
    : QObject(parent)
{
    s_packageManagerInstance = this;
    loadRepositoriesSettings();
    loadCachedIndex();
    scanInstalledPackages();
}

NovaJsfxPackageManager::~NovaJsfxPackageManager()
{
    if (s_packageManagerInstance == this) {
        s_packageManagerInstance = nullptr;
    }
}

QString NovaJsfxPackageManager::getLocalJsfxRoot() const
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        baseDir = NovaPlatformUtils::getProjectsDirectory();
    }
    QString root = baseDir + QStringLiteral("/JSFX");
    QDir().mkpath(root);
    return QDir::cleanPath(root);
}

QString NovaJsfxPackageManager::localPackagePath(const QString &category, const QString &fileName) const
{
    QString catDir = getLocalJsfxRoot() + QStringLiteral("/") + category;
    QDir().mkpath(catDir);
    return catDir + QStringLiteral("/") + fileName;
}

void NovaJsfxPackageManager::loadRepositoriesSettings()
{
    QSettings settings("NovaStudio", "JSFX");
    // Sin repositorios precargados por defecto
    if (!settings.contains("empty_default_repositories_v1")) {
        settings.setValue("empty_default_repositories_v1", true);
        settings.setValue("repositories", QStringList());
        m_repositories.clear();
        return;
    }
    m_repositories = settings.value("repositories").toStringList();
    bool changed = false;
    for (int i = 0; i < m_repositories.size(); ++i) {
        if (m_repositories[i].contains(QStringLiteral("://.githubusercontent.com")) ||
            m_repositories[i].contains(QStringLiteral(":///.githubusercontent.com"))) {
            m_repositories[i].replace(QStringLiteral("://.githubusercontent.com"), QStringLiteral("://raw.githubusercontent.com"));
            m_repositories[i].replace(QStringLiteral(":///.githubusercontent.com"), QStringLiteral("://raw.githubusercontent.com"));
            changed = true;
        }
    }
    if (changed) {
        saveRepositoriesSettings();
    }
}

void NovaJsfxPackageManager::saveRepositoriesSettings()
{
    QSettings settings("NovaStudio", "JSFX");
    settings.setValue("repositories", m_repositories);
    Q_EMIT repositoriesChanged();
}

QStringList NovaJsfxPackageManager::repositories() const
{
    return m_repositories;
}

void NovaJsfxPackageManager::addRepository(const QString &url)
{
    QString cleanUrl = url.trimmed();
    if (cleanUrl.isEmpty()) return;

    // Normalizar enlaces web estándar de GitHub a enlaces raw
    // Ej: https://github.com/ReaTeam/JSFX/raw/master/index.xml -> https://raw.githubusercontent.com/ReaTeam/JSFX/master/index.xml
    if (cleanUrl.contains(QStringLiteral("github.com/")) && cleanUrl.contains(QStringLiteral("/raw/"))) {
        cleanUrl.replace(QStringLiteral("github.com/"), QStringLiteral("raw.githubusercontent.com/"));
        cleanUrl.replace(QStringLiteral("/raw/"), QStringLiteral("/"));
    }
    cleanUrl.replace(QStringLiteral("://.githubusercontent.com"), QStringLiteral("://raw.githubusercontent.com"));
    cleanUrl.replace(QStringLiteral(":///.githubusercontent.com"), QStringLiteral("://raw.githubusercontent.com"));

    if (m_repositories.contains(cleanUrl)) return;

    m_repositories.append(cleanUrl);
    saveRepositoriesSettings();
    syncRepositories();
}

void NovaJsfxPackageManager::removeRepository(const QString &url)
{
    if (m_repositories.removeAll(url.trimmed()) > 0) {
        saveRepositoriesSettings();
        if (m_repositories.isEmpty()) {
            m_packages.clear();
            m_cachedCategories.clear();
            m_statusMessage = QStringLiteral("No hay repositorios configurados.");
            Q_EMIT statusMessageChanged(m_statusMessage);
            Q_EMIT packageListChanged();
        } else {
            syncRepositories();
        }
    }
}

void NovaJsfxPackageManager::clearRepositories()
{
    m_repositories.clear();
    saveRepositoriesSettings();
    m_packages.clear();
    m_cachedCategories.clear();
    m_statusMessage = QStringLiteral("No hay repositorios configurados.");
    Q_EMIT statusMessageChanged(m_statusMessage);
    Q_EMIT packageListChanged();
}

void NovaJsfxPackageManager::resetDefaultRepositories()
{
    clearRepositories();
}

QStringList NovaJsfxPackageManager::categories() const
{
    if (!m_cachedCategories.isEmpty()) {
        return m_cachedCategories;
    }

    std::set<QString> cats;
    for (const auto &pair : m_packages) {
        cats.insert(pair.second.category);
    }

    m_cachedCategories.clear();
    m_cachedCategories.append(QStringLiteral("Todas"));
    for (const auto &c : cats) {
        m_cachedCategories.append(c);
    }
    return m_cachedCategories;
}

void NovaJsfxPackageManager::loadCachedIndex()
{
    if (m_repositories.isEmpty()) {
        m_packages.clear();
        m_cachedCategories.clear();
        return;
    }

    QString cacheDir = getLocalJsfxRoot() + QStringLiteral("/cache");
    QDir dir(cacheDir);
    if (!dir.exists()) return;

    QStringList files = dir.entryList(QStringList() << QStringLiteral("*.xml"), QDir::Files);
    for (const auto &file : files) {
        QFile f(cacheDir + QStringLiteral("/") + file);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray data = f.readAll();
            f.close();
            parseIndexXml(data, QStringLiteral("ReaPack"));
        }
    }
    m_cachedCategories.clear();
    Q_EMIT packageListChanged();
}

void NovaJsfxPackageManager::saveCachedIndex(const QByteArray &data, const QString &repoUrl)
{
    QString cacheDir = getLocalJsfxRoot() + QStringLiteral("/cache");
    QDir().mkpath(cacheDir);
    QByteArray hash = QCryptographicHash::hash(repoUrl.toUtf8(), QCryptographicHash::Sha1).toHex();
    QString safeName = QStringLiteral("repo_%1.xml").arg(QString::fromLatin1(hash));
    QFile f(cacheDir + QStringLiteral("/") + safeName);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(data);
        f.close();
    }
}

void NovaJsfxPackageManager::parseIndexXml(const QByteArray &xmlData, const QString &repoName)
{
    QXmlStreamReader xml(xmlData);
    QString effectiveRepoName = repoName;
    QString currentCategory;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            QString elemName = xml.name().toString();
            if (elemName == QLatin1String("index")) {
                QString attrName = xml.attributes().value(QLatin1String("name")).toString();
                if (!attrName.isEmpty()) {
                    effectiveRepoName = attrName;
                }
            } else if (elemName == QLatin1String("category")) {
                currentCategory = xml.attributes().value(QLatin1String("name")).toString();
            } else if (elemName == QLatin1String("reapack")) {
                auto attrs = xml.attributes();
                QString type = attrs.value(QLatin1String("type")).toString();
                if (type.isEmpty() || type == QLatin1String("effect")) {
                    JsfxPackageItem item;
                    item.name = attrs.value(QLatin1String("name")).toString();
                    item.description = attrs.value(QLatin1String("desc")).toString();
                    item.category = currentCategory.isEmpty() ? QStringLiteral("Otros") : currentCategory;
                    item.repositoryName = effectiveRepoName;
                    item.id = QStringLiteral("reapack:%1:%2/%3").arg(effectiveRepoName, item.category, item.name);

                    while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name().toString() == QLatin1String("reapack")) && !xml.atEnd()) {
                        xml.readNext();
                        if (xml.tokenType() == QXmlStreamReader::StartElement) {
                            QString subName = xml.name().toString();
                            if (subName == QLatin1String("version")) {
                                item.author = xml.attributes().value(QLatin1String("author")).toString();
                                item.version = xml.attributes().value(QLatin1String("name")).toString();
                                item.extraFiles.clear();
                            } else if (subName == QLatin1String("source")) {
                                QString fileAttr = xml.attributes().value(QLatin1String("file")).toString();
                                QString srcUrl = xml.readElementText().trimmed();
                                if (fileAttr.isEmpty() || fileAttr == item.name) {
                                    item.downloadUrl = srcUrl;
                                } else {
                                    item.extraFiles.push_back({fileAttr, srcUrl});
                                }
                            }
                        }
                    }

                    if (!item.downloadUrl.isEmpty()) {
                        QString targetPath = localPackagePath(item.category, item.name);
                        item.localFilePath = targetPath;
                        item.isInstalled = QFile::exists(targetPath);
                        m_packages[item.id] = item;
                    }
                }
            }
        }
    }
}

void NovaJsfxPackageManager::scanInstalledPackages()
{
    QString jsfxRoot = getLocalJsfxRoot();
    QDir rootDir(jsfxRoot);
    if (!rootDir.exists()) return;

    for (auto &pair : m_packages) {
        JsfxPackageItem &item = pair.second;
        QString path = localPackagePath(item.category, item.name);
        item.localFilePath = path;
        item.isInstalled = QFile::exists(path);
        if (item.isInstalled) {
            NovaJsfxManager::instance().registerUserScript(path);
        }
    }
}

void NovaJsfxPackageManager::syncRepositories()
{
    if (m_isSyncing) return;

    m_isSyncing = true;
    m_syncProgress = 10;
    m_statusMessage = QStringLiteral("Conectando con repositorios ReaPack...");
    Q_EMIT syncingChanged(m_isSyncing);
    Q_EMIT syncProgressChanged(m_syncProgress);
    Q_EMIT statusMessageChanged(m_statusMessage);

    if (m_repositories.isEmpty()) {
        m_packages.clear();
        m_cachedCategories.clear();
        m_isSyncing = false;
        m_statusMessage = QStringLiteral("No hay repositorios configurados.");
        Q_EMIT syncingChanged(m_isSyncing);
        Q_EMIT statusMessageChanged(m_statusMessage);
        Q_EMIT packageListChanged();
        return;
    }

    m_packages.clear();
    m_cachedCategories.clear();

    auto repos = m_repositories;
    auto currentRepoIndex = std::make_shared<int>(0);
    auto errors = std::make_shared<QStringList>();

    auto downloadNext = std::make_shared<std::function<void()>>();
    *downloadNext = [this, repos, currentRepoIndex, errors, downloadNext]() {
        if (*currentRepoIndex >= repos.size()) {
            m_isSyncing = false;
            m_syncProgress = 100;
            if (m_packages.empty() && !errors->isEmpty()) {
                m_statusMessage = QStringLiteral("Error: %1").arg(errors->join(QStringLiteral(" | ")));
            } else {
                m_statusMessage = QStringLiteral("Sincronizado (%1 paquetes disponibles)").arg(m_packages.size());
            }
            m_cachedCategories.clear();
            scanInstalledPackages();
            Q_EMIT syncingChanged(false);
            Q_EMIT syncProgressChanged(100);
            Q_EMIT statusMessageChanged(m_statusMessage);
            Q_EMIT packageListChanged();
            return;
        }

        QString urlStr = repos[*currentRepoIndex];
        (*currentRepoIndex)++;

        m_statusMessage = QStringLiteral("Descargando índice: %1...").arg(QUrl(urlStr).host());
        Q_EMIT statusMessageChanged(m_statusMessage);

        QNetworkRequest req{QUrl(urlStr)};
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NOVA-Studio-ReaPack/1.0"));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply *reply = m_network.get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, urlStr, errors, downloadNext]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                saveCachedIndex(data, urlStr);
                parseIndexXml(data, QStringLiteral("ReaPack"));
                qCDebug(novaCore) << "📦 Índice parseado con éxito:" << urlStr << "Total paquetes:" << m_packages.size();
            } else {
                QString err = QStringLiteral("%1: %2").arg(QUrl(urlStr).host(), reply->errorString());
                qCWarning(novaCore) << "❌ Error al descargar repositorio:" << urlStr << reply->errorString();
                errors->append(err);
            }
            reply->deleteLater();
            (*downloadNext)();
        });
    };

    (*downloadNext)();
}

QVariantList NovaJsfxPackageManager::searchPackages(const QString &filterText,
                                                     const QString &category,
                                                     bool onlyInstalled) const
{
    QVariantList list;
    QString query = filterText.trimmed().toLower();
    bool hasQuery = !query.isEmpty();
    bool hasCat = (!category.isEmpty() && category != QStringLiteral("Todas"));

    for (const auto &pair : m_packages) {
        const auto &item = pair.second;

        if (onlyInstalled && !item.isInstalled) {
            continue;
        }

        if (hasCat && item.category.compare(category, Qt::CaseInsensitive) != 0) {
            continue;
        }

        if (hasQuery) {
            bool matches = item.name.toLower().contains(query) ||
                           item.description.toLower().contains(query) ||
                           item.author.toLower().contains(query) ||
                           item.category.toLower().contains(query);
            if (!matches) continue;
        }

        QVariantMap map;
        map[QStringLiteral("id")] = item.id;
        map[QStringLiteral("name")] = item.name;
        map[QStringLiteral("desc")] = item.description;
        map[QStringLiteral("category")] = item.category;
        map[QStringLiteral("author")] = item.author.isEmpty() ? QStringLiteral("ReaTeam") : item.author;
        map[QStringLiteral("version")] = item.version.isEmpty() ? QStringLiteral("1.0") : item.version;
        map[QStringLiteral("isInstalled")] = item.isInstalled;
        map[QStringLiteral("downloadUrl")] = item.downloadUrl;
        list.append(map);
    }

    return list;
}

bool NovaJsfxPackageManager::isPackageInstalled(const QString &packageId) const
{
    auto it = m_packages.find(packageId);
    if (it != m_packages.end()) {
        return it->second.isInstalled;
    }
    return false;
}

QString NovaJsfxPackageManager::getPackageCode(const QString &packageId) const
{
    auto it = m_packages.find(packageId);
    if (it == m_packages.end()) return QString();

    QString path = it->second.localFilePath;
    if (path.isEmpty() || !QFile::exists(path)) return QString();

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(file.readAll());
    }
    return QString();
}

void NovaJsfxPackageManager::installPackage(const QString &packageId)
{
    auto it = m_packages.find(packageId);
    if (it == m_packages.end()) {
        Q_EMIT packageInstallFinished(packageId, false, QStringLiteral("Paquete no encontrado en el índice"));
        return;
    }

    JsfxPackageItem &item = it->second;
    QString targetPath = localPackagePath(item.category, item.name);
    QString rawUrl = item.downloadUrl;
    auto extraFiles = item.extraFiles;
    QDir targetDir = QFileInfo(targetPath).dir();

    // Normalizar a raw si es enlace web de GitHub
    if (rawUrl.contains(QStringLiteral("github.com/")) && rawUrl.contains(QStringLiteral("/raw/"))) {
        rawUrl.replace(QStringLiteral("github.com/"), QStringLiteral("raw.githubusercontent.com/"));
        rawUrl.replace(QStringLiteral("/raw/"), QStringLiteral("/"));
    }
    rawUrl.replace(QStringLiteral("://.githubusercontent.com"), QStringLiteral("://raw.githubusercontent.com"));
    rawUrl.replace(QStringLiteral(":///.githubusercontent.com"), QStringLiteral("://raw.githubusercontent.com"));

    qCDebug(novaCore) << "⬇️ Descargando paquete JSFX:" << item.name << "desde:" << rawUrl;

    QNetworkRequest req{QUrl(rawUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NOVA-Studio-ReaPack/1.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, packageId, targetPath, extraFiles, targetDir]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray scriptBytes = reply->readAll();
            QFile file(targetPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                file.write(scriptBytes);
                file.close();

                // Descargar dependencias auxiliares si las hay (ej: ReEQ firhalfband.jsfx-inc, svf_filter.jsfx-inc)
                for (const auto &extra : extraFiles) {
                    QString extraPath = targetDir.filePath(extra.relativePath);
                    QDir().mkpath(QFileInfo(extraPath).dir().absolutePath());
                    QString extraUrl = extra.url;
                    if (extraUrl.contains(QStringLiteral("github.com/")) && extraUrl.contains(QStringLiteral("/raw/"))) {
                        extraUrl.replace(QStringLiteral("github.com/"), QStringLiteral("raw.githubusercontent.com/"));
                        extraUrl.replace(QStringLiteral("/raw/"), QStringLiteral("/"));
                    }
                    extraUrl.replace(QStringLiteral("://.githubusercontent.com"), QStringLiteral("://raw.githubusercontent.com"));
                    QNetworkRequest extraReq{QUrl(extraUrl)};
                    extraReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NOVA-Studio-ReaPack/1.0"));
                    extraReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
                    QNetworkReply *extraReply = m_network.get(extraReq);
                    connect(extraReply, &QNetworkReply::finished, this, [extraReply, extraPath]() {
                        if (extraReply->error() == QNetworkReply::NoError) {
                            QFile ef(extraPath);
                            if (ef.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                                ef.write(extraReply->readAll());
                                ef.close();
                            }
                        }
                        extraReply->deleteLater();
                    });
                }

                auto it = m_packages.find(packageId);
                if (it != m_packages.end()) {
                    it->second.isInstalled = true;
                    it->second.localFilePath = targetPath;
                }

                // Registrar en el motor JSFX de NOVA
                QString regError;
                bool ok = NovaJsfxManager::instance().registerUserScript(targetPath, &regError);
                if (ok) {
                    qCDebug(novaCore) << "✅ Paquete JSFX instalado y registrado con éxito:" << targetPath;
                    Q_EMIT packageInstallFinished(packageId, true, QString());
                } else {
                    qCWarning(novaCore) << "⚠️ Paquete descargado pero dio advertencia al registrar:" << regError;
                    Q_EMIT packageInstallFinished(packageId, true, QString());
                }
                Q_EMIT packageListChanged();
            } else {
                Q_EMIT packageInstallFinished(packageId, false, QStringLiteral("Error al guardar archivo en disco"));
            }
        } else {
            qCCritical(novaCore) << "❌ Error de red al descargar paquete:" << reply->errorString();
            Q_EMIT packageInstallFinished(packageId, false, reply->errorString());
        }
        reply->deleteLater();
    });
}

void NovaJsfxPackageManager::uninstallPackage(const QString &packageId)
{
    auto it = m_packages.find(packageId);
    if (it == m_packages.end()) return;

    JsfxPackageItem &item = it->second;
    QString targetPath = item.localFilePath.isEmpty() ? localPackagePath(item.category, item.name) : item.localFilePath;

    if (QFile::exists(targetPath)) {
        QFile::remove(targetPath);
        qCDebug(novaCore) << "🗑️ Paquete JSFX desinstalado:" << targetPath;
    }

    item.isInstalled = false;
    Q_EMIT packageListChanged();
}
