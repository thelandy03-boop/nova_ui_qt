#include "NovaProjectPolicy.h"

#include <QDir>
#include <QFileInfo>

namespace NovaProjectPolicy {

bool containsSavedSession(const QString &sessionDir)
{
    if (sessionDir.isEmpty()) {
        return false;
    }

    QDir dir(sessionDir);
    if (!dir.exists()) {
        return false;
    }

    // Cualquier .ardour de la carpeta cuenta: puede no coincidir con el
    // nombre de la carpeta (snapshots, "Proyecto 2.ardour", etc).
    const QStringList sessions = dir.entryList(QStringList() << "*.ardour", QDir::Files);
    return !sessions.isEmpty();
}

bool hasImportedMedia(const QString &sessionDir)
{
    if (sessionDir.isEmpty()) {
        return false;
    }

    // Subcarpetas donde Ardour guarda el audio importado y sus picos.
    const QStringList mediaDirs = {QStringLiteral("interchange"),
                                   QStringLiteral("peaks"),
                                   QStringLiteral("externals")};

    for (const QString &sub : mediaDirs) {
        const QDir dir(sessionDir + QLatin1Char('/') + sub);
        if (!dir.exists()) {
            continue;
        }

        const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            if (entry.isDir()) {
                // Una subcarpeta con contenido tambien cuenta como datos.
                const QDir inner(entry.absoluteFilePath());
                if (!inner.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
                    return true;
                }
            } else {
                return true;
            }
        }
    }

    return false;
}

bool isProtectedFolder(const QString &sessionDir)
{
    return containsSavedSession(sessionDir) || hasImportedMedia(sessionDir);
}

bool mayDeleteFolderOnDiscard(const QString &sessionDir)
{
    if (sessionDir.isEmpty()) {
        return false;
    }
    if (!QDir(sessionDir).exists()) {
        return false;
    }
    return !isProtectedFolder(sessionDir);
}

bool canCreateProjectIn(const QString &sessionDir, QString *reason)
{
    if (sessionDir.isEmpty()) {
        if (reason) {
            *reason = QStringLiteral("La ruta del proyecto esta vacia.");
        }
        return false;
    }

    if (!QDir(sessionDir).exists()) {
        return true; // Se creara desde cero.
    }

    if (isProtectedFolder(sessionDir)) {
        if (reason) {
            *reason = containsSavedSession(sessionDir)
                ? QStringLiteral("Ya existe un proyecto guardado en \"%1\". Elige otro nombre.")
                      .arg(sessionDir)
                : QStringLiteral("\"%1\" ya contiene audio importado. Elige otro nombre.")
                      .arg(sessionDir);
        }
        return false;
    }

    // Restos de un proyecto nunca guardado: se puede limpiar y reutilizar.
    return true;
}

QString uniqueProjectName(const QString &baseDir, const QString &desiredName)
{
    if (desiredName.isEmpty()) {
        return QStringLiteral("Untitled");
    }

    if (canCreateProjectIn(baseDir + QLatin1Char('/') + desiredName)) {
        return desiredName;
    }

    for (int i = 2; i < 1000; ++i) {
        const QString candidate = QStringLiteral("%1 %2").arg(desiredName).arg(i);
        if (canCreateProjectIn(baseDir + QLatin1Char('/') + candidate)) {
            return candidate;
        }
    }

    // No hay nombre libre: createProject lo rechazara con un motivo claro.
    return desiredName;
}

} // namespace NovaProjectPolicy
