#include "NovaProjectManager.h"
#include "NovaLogging.h"
#include "NovaProjectPolicy.h"
#include "platform/NovaPlatformUtils.h"
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QSettings>
#include <QDateTime>

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

#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/session_configuration.h"

#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#pragma pop_macro("foreach")
#endif

// PIMPL implementation - class definition OUTSIDE macro protection
class NovaProjectManagerPrivate {
public:
    // Empty for now - all methods are static
};

NovaProjectManager::NovaProjectManager(QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<NovaProjectManagerPrivate>())
{
}

NovaProjectManager::~NovaProjectManager() = default;

QString NovaProjectManager::sanitizePath(const QString &rawPath)
{
    return NovaPlatformUtils::sanitizePath(rawPath);
}

bool NovaProjectManager::saveProject(void *session)
{
    if (!session) return false;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    ARDOUR::Session *ardourSession = static_cast<ARDOUR::Session*>(session);
    try {
        qCDebug(novaCore) << "💾 Guardando estado completo de la sesión...";
        ardourSession->set_dirty();
        int result = ardourSession->save_state("");
        if (result == 0) {
            QString path = sanitizePath(QString::fromStdString(ardourSession->path()));
            QString name = QString::fromStdString(ardourSession->name());
            qCDebug(novaCore) << "💾 [OK] Proyecto guardado exitosamente en:" << path;
            
            addProjectToRecent(name, path);
            return true;
        } else {
            qCCritical(novaCore) << "❌ Error al guardar proyecto. Código de retorno:" << result;
        }
    } catch (const std::exception &e) {
        qCCritical(novaCore) << "Excepción crítica durante el guardado:" << e.what();
    }
#endif
    return false;
}

bool NovaProjectManager::isDirty(void *session)
{
    if (!session) return false;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    ARDOUR::Session *ardourSession = static_cast<ARDOUR::Session*>(session);
    return ardourSession->dirty();
#else
    return false;
#endif
}

void* NovaProjectManager::createProject(void *engine, const QString &projectName, const QString &parentDir,
                                        bool isScratch)
{
    if (!engine || projectName.isEmpty()) return nullptr;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    ARDOUR::AudioEngine *ardourEngine = static_cast<ARDOUR::AudioEngine*>(engine);
    QString targetParentDir = parentDir.isEmpty() ? NovaPlatformUtils::getProjectsDirectory() : sanitizePath(parentDir);
    QString sessionDir = targetParentDir + "/" + projectName;
    qCDebug(novaCore) << "🆕 Generando proyecto nuevo independiente en:" << sessionDir;

    try {
        // 🛡️ Nunca sobrescribir un proyecto que ya tiene datos del usuario
        // (sesion guardada o audio importado): se rechaza en vez de borrar.
        // La carpeta temporal de la app es nuestra y si se puede reutilizar.
        if (!isScratch) {
            QString reason;
            if (!NovaProjectPolicy::canCreateProjectIn(sessionDir, &reason)) {
                qCCritical(novaCore) << "❌ No se puede crear el proyecto:" << reason;
                return nullptr;
            }
        }

        if (QDir(sessionDir).exists()) {
            if (isScratch && NovaProjectPolicy::hasImportedMedia(sessionDir)) {
                // El usuario importo audio en la carpeta temporal: se conserva.
                qCDebug(novaCore) << "ℹ️ La carpeta temporal contiene audio importado, se conserva:" << sessionDir;
            } else {
                // Restos de un proyecto nunca guardado: se limpia y se reutiliza.
                qCDebug(novaCore) << "🧹 Limpiando carpeta residual (sin sesion guardada):" << sessionDir;
                QDir(sessionDir).removeRecursively();
            }
        }
        QDir().mkpath(targetParentDir);

        ARDOUR::BusProfile bus_profile;
        bus_profile.master_out_channels = 2;

        auto session = new ARDOUR::Session(*ardourEngine, sessionDir.toStdString(), projectName.toStdString(), &bus_profile, "", true);

        if (session) {
            ardourEngine->set_session(session);
            session->config.set_session_monitoring(ARDOUR::MonitorDisk);

            qCDebug(novaCore) << "🆕 [OK] Nuevo proyecto instanciado correctamente en memoria.";
            return session;
        }
    } catch (const std::exception &e) {
        qCCritical(novaCore) << "❌ Excepción durante la creación del proyecto:" << e.what();
    }
#endif
    return nullptr;
}

void* NovaProjectManager::openProject(void *engine, const QString &sessionPath)
{
    if (!engine) return nullptr;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    ARDOUR::AudioEngine *ardourEngine = static_cast<ARDOUR::AudioEngine*>(engine);
    QString cleanSessionPath = sanitizePath(sessionPath);
    qCDebug(novaCore) << "📂 Intentando cargar proyecto existente en:" << cleanSessionPath;

    try {
        QFileInfo checkFile(cleanSessionPath);
        QString realSessionDir = cleanSessionPath;
        QString sessionName = checkFile.fileName();

        if (checkFile.isFile() && checkFile.suffix() == "ardour") {
            realSessionDir = checkFile.absolutePath();
            sessionName = checkFile.completeBaseName();
        } else if (checkFile.isDir()) {
            realSessionDir = checkFile.absoluteFilePath();
            sessionName = checkFile.fileName();
        } else {
            sessionName = QDir(cleanSessionPath).dirName();
        }

        realSessionDir = sanitizePath(realSessionDir);

        QString expectedArdourFile = realSessionDir + "/" + sessionName + ".ardour";
        if (!QFileInfo::exists(expectedArdourFile)) {
            QDir dir(realSessionDir);
            QStringList ardourFiles = dir.entryList(QStringList() << "*.ardour", QDir::Files);
            if (!ardourFiles.isEmpty()) {
                sessionName = QFileInfo(ardourFiles.first()).completeBaseName();
            }
        }

        std::string pathStd = realSessionDir.toStdString();
        std::string nameStd = sessionName.toStdString();

        ARDOUR::BusProfile bus_profile;
        bus_profile.master_out_channels = 2;

        auto session = new ARDOUR::Session(*ardourEngine, pathStd, nameStd, &bus_profile, "", false);

        if (session) {
            ardourEngine->set_session(session);
            session->config.set_session_monitoring(ARDOUR::MonitorDisk);

            qCDebug(novaCore) << "📂 [OK] Proyecto cargado con éxito:" << realSessionDir << "Snapshot:" << sessionName;
            addProjectToRecent(sessionName, realSessionDir);
            return session;
        }
    } catch (const std::exception &e) {
        qCCritical(novaCore) << "❌ Error al abrir la sesión:" << e.what();
    }
#endif
    return nullptr;
}

QVariantList NovaProjectManager::getRecentProjects()
{
    QSettings settings("NovaStudio", "DAW");
    QVariantList rawList = settings.value("recentProjects").toList();
    QVariantList cleanedList;

    for (const auto &item : rawList) {
        QVariantMap map = item.toMap();
        QString cleanP = sanitizePath(map["path"].toString());
        if (!cleanP.isEmpty() && QDir(cleanP).exists()) {
            map["path"] = cleanP;
            cleanedList.append(map);
        }
    }
    return cleanedList;
}

void NovaProjectManager::addProjectToRecent(const QString &name, const QString &path)
{
    if (name.isEmpty() || name == "TempSession" || name == "DefaultSession" || path.contains("NovaStudio_TempSession")) {
        return;
    }

    QString cleanPath = sanitizePath(path);
    QSettings settings("NovaStudio", "DAW");
    QVariantList list = settings.value("recentProjects").toList();
    QVariantList newList;

    for (int i = 0; i < list.size(); ++i) {
        QVariantMap item = list[i].toMap();
        QString itemPath = sanitizePath(item["path"].toString());
        if (itemPath != cleanPath && !itemPath.isEmpty()) {
            newList.append(item);
        }
    }

    QVariantMap projectMap;
    projectMap["name"] = name;
    projectMap["path"] = cleanPath;
    projectMap["lastModified"] = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm AP");

    newList.prepend(projectMap);

    while (newList.size() > 10) {
        newList.removeLast();
    }

    settings.setValue("recentProjects", newList);
    qCDebug(novaCore) << "📊 Historial de Proyectos Recientes Actualizado:" << name << "->" << cleanPath;
}

void NovaProjectManager::removeProjectFromRecent(const QString &path)
{
    QString cleanTarget = sanitizePath(path);
    QSettings settings("NovaStudio", "DAW");
    QVariantList list = settings.value("recentProjects").toList();
    QVariantList newList;

    for (int i = 0; i < list.size(); ++i) {
        QVariantMap item = list[i].toMap();
        QString itemPath = sanitizePath(item["path"].toString());
        if (itemPath != cleanTarget && !itemPath.isEmpty()) {
            newList.append(item);
        }
    }

    settings.setValue("recentProjects", newList);
    qCDebug(novaCore) << "🗑️ Proyecto eliminado del historial de recientes:" << cleanTarget;
}

QString NovaProjectManager::scratchDirectory()
{
    return NovaPlatformUtils::getProjectsDirectory() + QStringLiteral("/NovaStudio_TempSession");
}

QString NovaProjectManager::discardableProjectPath(void *session)
{
    if (!session) return {};
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_WIN)
    ARDOUR::Session *ardourSession = static_cast<ARDOUR::Session*>(session);
    QString path = sanitizePath(QString::fromStdString(ardourSession->path()));
#else
    QString path = "";
#endif

    if (path.isEmpty()) {
        return {};
    }

    // 🛡️ Un proyecto guardado (o con audio importado) NUNCA se borra: se
    // conserva en disco y sigue apareciendo en los recientes.
    if (!NovaProjectPolicy::mayDeleteFolderOnDiscard(path)) {
        qCDebug(novaCore) << "🛡️ Proyecto con datos guardados: se conserva intacto ->" << path;
        return {};
    }

    return path;
}

void NovaProjectManager::deleteUnsavedProjectFolder(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    removeProjectFromRecent(path);

    if (QDir(path).exists()) {
        qCDebug(novaCore) << "🗑️ Eliminando carpeta de proyecto nunca guardado:" << path;
        QDir(path).removeRecursively();
    }
}