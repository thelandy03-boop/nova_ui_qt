#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <vector>
#include <map>
#include <memory>

/**
 * Elemento de paquete JSFX parseado de un repositorio ReaPack (index.xml).
 */
struct JsfxPackageItem {
    QString id;
    QString name;
    QString category;
    QString description;
    QString author;
    QString version;
    QString downloadUrl;
    QString repositoryName;
    bool isInstalled = false;
    QString localFilePath;

    struct SourceFile {
        QString relativePath;
        QString url;
    };
    std::vector<SourceFile> extraFiles;
};

/**
 * Gestor de paquetes JSFX de la comunidad (estilo ReaPack) para NOVA Studio.
 * Descarga y parsea index.xml de repositorios remotos (como ReaTeam),
 * gestiona la descarga, instalación y desinstalación de plugins JSFX,
 * y los registra en tiempo real en NovaJsfxManager para su uso en pistas.
 */
class NovaJsfxPackageManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isSyncing READ isSyncing NOTIFY syncingChanged)
    Q_PROPERTY(int syncProgress READ syncProgress NOTIFY syncProgressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int totalPackagesCount READ totalPackagesCount NOTIFY packageListChanged)
    Q_PROPERTY(QStringList categories READ categories NOTIFY packageListChanged)
    Q_PROPERTY(QStringList repositories READ repositories NOTIFY repositoriesChanged)

public:
    static NovaJsfxPackageManager* instance();
    explicit NovaJsfxPackageManager(QObject *parent = nullptr);
    ~NovaJsfxPackageManager() override;

    bool isSyncing() const { return m_isSyncing; }
    int syncProgress() const { return m_syncProgress; }
    QString statusMessage() const { return m_statusMessage; }
    int totalPackagesCount() const { return static_cast<int>(m_packages.size()); }
    QStringList categories() const;
    QStringList repositories() const;

    Q_INVOKABLE void syncRepositories();
    Q_INVOKABLE void addRepository(const QString &url);
    Q_INVOKABLE void removeRepository(const QString &url);
    Q_INVOKABLE void clearRepositories();
    Q_INVOKABLE void resetDefaultRepositories();

    Q_INVOKABLE QVariantList searchPackages(const QString &filterText = QString(),
                                            const QString &category = QString(),
                                            bool onlyInstalled = false) const;
    Q_INVOKABLE void installPackage(const QString &packageId);
    Q_INVOKABLE void uninstallPackage(const QString &packageId);
    Q_INVOKABLE bool isPackageInstalled(const QString &packageId) const;
    Q_INVOKABLE QString getPackageCode(const QString &packageId) const;

signals:
    void syncingChanged(bool isSyncing);
    void syncProgressChanged(int progress);
    void statusMessageChanged(const QString &message);
    void packageListChanged();
    void repositoriesChanged();
    void packageInstallFinished(const QString &packageId, bool success, const QString &error);

private:
    void loadRepositoriesSettings();
    void saveRepositoriesSettings();
    void loadCachedIndex();
    void saveCachedIndex(const QByteArray &data, const QString &repoUrl);
    void parseIndexXml(const QByteArray &xmlData, const QString &repoName);
    void scanInstalledPackages();
    QString getLocalJsfxRoot() const;
    QString localPackagePath(const QString &category, const QString &fileName) const;

    bool m_isSyncing = false;
    int m_syncProgress = 0;
    QString m_statusMessage;
    QStringList m_repositories;
    QNetworkAccessManager m_network;
    std::map<QString, JsfxPackageItem> m_packages;
    mutable QStringList m_cachedCategories;
};
