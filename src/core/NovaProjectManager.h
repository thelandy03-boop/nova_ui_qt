#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

// Forward declarations - NO ARDOUR/PBD includes in public header
namespace ARDOUR {
    class Session;
    class AudioEngine;
}
class NovaProjectManagerPrivate;

class NovaProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit NovaProjectManager(QObject *parent = nullptr);
    ~NovaProjectManager() override;

    static QString sanitizePath(const QString &rawPath);
    static bool saveProject(void *session);
    static bool isDirty(void *session);

    static void *createProject(void *engine, const QString &projectName, const QString &parentDir);
    static void *openProject(void *engine, const QString &sessionPath);

    static QVariantList getRecentProjects();
    static void addProjectToRecent(const QString &name, const QString &path);
    static void removeProjectFromRecent(const QString &path);

    // 🗑️ Limpia carpetas de proyectos que nunca fueron guardados si el usuario descarta
    static void discardUnsavedProject(void *session);

private:
    std::unique_ptr<NovaProjectManagerPrivate> d_ptr;
};