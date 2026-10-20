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

    static void *createProject(void *engine, const QString &projectName, const QString &parentDir,
                                bool isScratch = false);
    static void *openProject(void *engine, const QString &sessionPath);

    // Carpeta de trabajo de la app: el proyecto temporal que se abre al
    // arrancar. Vive aparte para no ensuciar la lista de proyectos del
    // usuario ni accumular carpetas "Untitled 2", "Untitled 3"...
    static QString scratchDirectory();

    static QVariantList getRecentProjects();
    static void addProjectToRecent(const QString &name, const QString &path);
    static void removeProjectFromRecent(const QString &path);

    // 🗑️ Carpeta del proyecto actual que puede eliminarse al descartarlo.
    // Devuelve una cadena vacia si el proyecto esta guardado (no se borra)
    // o si no hay carpeta que eliminar.
    static QString discardableProjectPath(void *session);

    // Elimina la carpeta fisica de un proyecto que nunca fue guardado y lo
    // quita de los recientes. No hace nada si la ruta esta vacia.
    static void deleteUnsavedProjectFolder(const QString &path);

private:
    std::unique_ptr<NovaProjectManagerPrivate> d_ptr;
};