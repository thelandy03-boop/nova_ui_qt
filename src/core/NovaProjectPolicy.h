#pragma once

#include <QString>

// Politica de proteccion de carpetas de proyecto.
//
// Regla central: una carpeta que contiene datos del usuario (una sesion
// guardada o audio importado) NUNCA se borra ni se sobrescribe de forma
// automatica. Estas funciones son puras y sin estado para poder testearlas
// sin necesitar una sesion de Ardour.
namespace NovaProjectPolicy {

// True si la carpeta contiene una sesion de Ardour ya guardada (un *.ardour).
bool containsSavedSession(const QString &sessionDir);

// True si la carpeta contiene audio importado por el usuario.
bool hasImportedMedia(const QString &sessionDir);

// True si la carpeta tiene datos que no se deben tocar de forma automatica.
bool isProtectedFolder(const QString &sessionDir);

// True si se puede eliminar la carpeta fisica al descartar el proyecto.
bool mayDeleteFolderOnDiscard(const QString &sessionDir);

// True si se puede crear un proyecto nuevo en esa carpeta. Si no se puede,
// 'reason' recibe un texto listo para mostrar en la UI.
bool canCreateProjectIn(const QString &sessionDir, QString *reason = nullptr);

// Devuelve un nombre de proyecto utilizable dentro de baseDir. Si el nombre
// pedido esta ocupado por un proyecto guardado, devuelve "Nombre 2", "Nombre 3"...
QString uniqueProjectName(const QString &baseDir, const QString &desiredName);

} // namespace NovaProjectPolicy
