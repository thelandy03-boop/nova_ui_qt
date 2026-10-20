// Test de la politica de proteccion de carpetas de proyecto.
//
// Contexto: NovaProjectManager::discardUnsavedProject() BORRABA la carpeta
// fisica del proyecto sin comprobar si contenia una sesion guardada, y
// createProject() BORRABA en silencio un proyecto existente con el mismo
// nombre. Este test fija el comportamiento correcto.
//
// No usa framework: asserts simples, sin dependencias mas alla de Qt6Core.
#include "core/NovaProjectPolicy.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtGlobal>
#include <cstdio>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::printf("  FALLO: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);  \
        }                                                                      \
    } while (0)

static bool makeFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    f.write("contenido de prueba");
    f.close();
    return true;
}

int main()
{
    std::printf("== Politica de proteccion de proyectos ==\n");

    // ------------------------------------------------------------------
    // 1. Una carpeta con sesion guardada esta PROTEGIDA
    // ------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString dir = tmp.path() + "/MiBanda";
        QDir().mkpath(dir);
        makeFile(dir + "/MiBanda.ardour");

        CHECK(NovaProjectPolicy::containsSavedSession(dir),
              "una carpeta con .ardour debe detectarse como sesion guardada");
        CHECK(NovaProjectPolicy::isProtectedFolder(dir),
              "una carpeta con sesion guardada debe estar protegida");
        CHECK(!NovaProjectPolicy::mayDeleteFolderOnDiscard(dir),
              "NUNCA se debe borrar una carpeta con sesion guardada al descartar");
        CHECK(!NovaProjectPolicy::canCreateProjectIn(dir),
              "no se debe crear un proyecto encima de una sesion guardada");
    }

    // ------------------------------------------------------------------
    // 2. Una carpeta con la sesion guardada bajo otro nombre tambien cuenta
    // ------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString dir = tmp.path() + "/OtroNombre";
        QDir().mkpath(dir);
        makeFile(dir + "/Snapshot-2026.ardour");

        CHECK(NovaProjectPolicy::containsSavedSession(dir),
              "cualquier .ardour de la carpeta cuenta como sesion guardada");
        CHECK(NovaProjectPolicy::isProtectedFolder(dir),
              "una carpeta con .ardour de otro nombre tambien esta protegida");
    }

    // ------------------------------------------------------------------
    // 3. Una carpeta scratch (restos de un proyecto nunca guardado) NO esta protegida
    // ------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString dir = tmp.path() + "/Untitled";
        QDir().mkpath(dir);
        QDir().mkpath(dir + "/interchange");
        QDir().mkpath(dir + "/peaks");
        QDir().mkpath(dir + "/analysis");

        CHECK(!NovaProjectPolicy::containsSavedSession(dir),
              "una carpeta scratch no tiene sesion guardada");
        CHECK(!NovaProjectPolicy::isProtectedFolder(dir),
              "una carpeta scratch vacia no debe estar protegida");
        CHECK(NovaProjectPolicy::mayDeleteFolderOnDiscard(dir),
              "una carpeta scratch se puede borrar al descartar");
        CHECK(NovaProjectPolicy::canCreateProjectIn(dir),
              "una carpeta scratch se puede reutilizar para un proyecto nuevo");
    }

    // ------------------------------------------------------------------
    // 4. Una carpeta con audio importado tiene datos del usuario: proteger
    // ------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString dir = tmp.path() + "/ConAudio";
        QDir().mkpath(dir + "/interchange");
        makeFile(dir + "/interchange/voz.wav");

        CHECK(NovaProjectPolicy::isProtectedFolder(dir),
              "una carpeta con audio importado debe estar protegida");
        CHECK(!NovaProjectPolicy::mayDeleteFolderOnDiscard(dir),
              "no se debe borrar audio importado al descartar");
    }

    // ------------------------------------------------------------------
    // 5. Carpeta inexistente: se puede crear, no hay nada que borrar
    // ------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString dir = tmp.path() + "/NoExiste";

        CHECK(!NovaProjectPolicy::containsSavedSession(dir),
              "una carpeta inexistente no tiene sesion guardada");
        CHECK(!NovaProjectPolicy::isProtectedFolder(dir),
              "una carpeta inexistente no esta protegida");
        CHECK(NovaProjectPolicy::canCreateProjectIn(dir),
              "se puede crear un proyecto en una carpeta inexistente");
    }

    // ------------------------------------------------------------------
    // 6. El motivo del rechazo es explicable para la UI
    // ------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString dir = tmp.path() + "/Ocupado";
        QDir().mkpath(dir);
        makeFile(dir + "/Ocupado.ardour");

        QString reason;
        CHECK(!NovaProjectPolicy::canCreateProjectIn(dir, &reason),
              "canCreateProjectIn debe rechazar una carpeta con sesion guardada");
        CHECK(!reason.isEmpty(),
              "el rechazo debe incluir un motivo para mostrarlo en la UI");
    }

    // ------------------------------------------------------------------
    // 7. Nombre unico: el proyecto por defecto no puede pisar uno guardado
    // ------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        // "Untitled" ya existe Y esta guardado (el usuario lo guardo antes)
        const QString saved = tmp.path() + "/Untitled";
        QDir().mkpath(saved);
        makeFile(saved + "/Untitled.ardour");

        const QString name = NovaProjectPolicy::uniqueProjectName(tmp.path(), "Untitled");
        CHECK(name != "Untitled",
              "el nombre por defecto debe cambiar si ese nombre ya esta guardado");
        CHECK(NovaProjectPolicy::canCreateProjectIn(tmp.path() + "/" + name),
              "el nombre generado debe ser utilizable");

        // Sin colision: se mantiene el nombre pedido
        const QString libre = NovaProjectPolicy::uniqueProjectName(tmp.path(), "Proyecto Nuevo");
        CHECK(libre == "Proyecto Nuevo",
              "si no hay colision se conserva el nombre pedido");
    }

    std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
