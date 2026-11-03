#ifndef NOVAPLATFORMUTILS_H
#define NOVAPLATFORMUTILS_H

#include <QString>

class NovaPlatformUtils
{
public:
    // 📁 Devuelve la ruta de carpetas de proyectos según el sistema operativo (Desktop vs Android)
    static QString getProjectsDirectory();

    // 🧹 Sanitizador de rutas unificado (maneja file://, file:/, content:// y URL encoding)
    static QString sanitizePath(const QString &rawPath);

    // 📦 Si la ruta es una URI content:// (Android SAF), copia el contenido a la caché interna y devuelve la ruta local real.
    static QString copyContentUriToLocal(const QString &contentUri);

    // 📱 Detecta si el binario actual está ejecutándose en Android o móvil
    static bool isMobilePlatform();

    // 📳 Dispara vibración táctil corta del sistema en dispositivos móviles
    static void hapticFeedback();

    // 🧹 Limpieza preventiva de archivos temporales antiguos en la caché de medios importados
    static void cleanOldImportedMedia();
};

#endif // NOVAPLATFORMUTILS_H