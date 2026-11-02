#include "NovaPlatformUtils.h"
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QtGlobal>
#include <cstring>
#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif

QString NovaPlatformUtils::getProjectsDirectory()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif

    if (baseDir.isEmpty()) {
        baseDir = QDir::homePath();
    }

    QString projectsDir = baseDir + "/NovaProjects";
    QDir().mkpath(projectsDir);
    return QDir::cleanPath(projectsDir);
}

QString NovaPlatformUtils::copyContentUriToLocal(const QString &contentUri)
{
    if (!contentUri.startsWith(QLatin1String("content://"), Qt::CaseInsensitive)) {
        return contentUri;
    }

    QFile srcFile(contentUri);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        qWarning() << "❌ [NovaPlatformUtils] No se pudo abrir URI content:" << contentUri << srcFile.errorString();
        return contentUri;
    }

    // Directorio de destino seguro dentro del almacenamiento privado de la app
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (baseDir.isEmpty()) {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    if (baseDir.isEmpty()) {
        baseDir = QDir::tempPath();
    }
    QString importsDir = baseDir + QStringLiteral("/imported_media");
    QDir().mkpath(importsDir);

    // Intentar deducir un nombre de archivo
    QString fileName;
    int lastSlash = contentUri.lastIndexOf(QLatin1Char('/'));
    if (lastSlash >= 0 && lastSlash < contentUri.length() - 1) {
        fileName = contentUri.mid(lastSlash + 1);
        fileName = QUrl::fromPercentEncoding(fileName.toUtf8());
    }
    if (fileName.isEmpty() || fileName.contains(QLatin1Char('?')) || fileName.startsWith(QLatin1String("document"))) {
        fileName = QFileInfo(srcFile).fileName();
    }
    if (fileName.isEmpty() || !fileName.contains(QLatin1Char('.'))) {
        // Leemos los primeros 16 bytes para deducir la extensión por cabecera
        QByteArray magic = srcFile.peek(16);
        QString ext = QStringLiteral(".wav");
        if (magic.size() >= 12 && std::memcmp(magic.constData(), "RIFF", 4) == 0) {
            ext = QStringLiteral(".wav");
        } else if (magic.size() >= 3 && (std::memcmp(magic.constData(), "ID3", 3) == 0 ||
                   (static_cast<unsigned char>(magic[0]) == 0xFF && (static_cast<unsigned char>(magic[1]) & 0xE0) == 0xE0))) {
            ext = QStringLiteral(".mp3");
        } else if (magic.size() >= 4 && std::memcmp(magic.constData(), "fLaC", 4) == 0) {
            ext = QStringLiteral(".flac");
        } else if (magic.size() >= 4 && std::memcmp(magic.constData(), "OggS", 4) == 0) {
            ext = QStringLiteral(".ogg");
        } else if (magic.size() >= 8 && std::memcmp(magic.constData() + 4, "ftyp", 4) == 0) {
            ext = QStringLiteral(".m4a");
        }
        fileName = QStringLiteral("audio_%1%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(ext);
    }

    // Sanitizar caracteres ilegales en el nombre de archivo
    fileName.replace(QLatin1Char('/'), QLatin1Char('_'));
    fileName.replace(QLatin1Char('\\'), QLatin1Char('_'));
    fileName.replace(QLatin1Char(':'), QLatin1Char('_'));

    QString destPath = importsDir + QStringLiteral("/") + fileName;

    QFile destFile(destPath);
    if (!destFile.open(QIODevice::WriteOnly)) {
        qWarning() << "❌ [NovaPlatformUtils] No se pudo crear archivo destino:" << destPath << destFile.errorString();
        return contentUri;
    }

    constexpr qint64 kChunkSize = 64 * 1024;
    char buffer[kChunkSize];
    while (!srcFile.atEnd()) {
        qint64 bytesRead = srcFile.read(buffer, kChunkSize);
        if (bytesRead <= 0) break;
        destFile.write(buffer, bytesRead);
    }

    destFile.close();
    srcFile.close();

    qInfo() << "✅ [NovaPlatformUtils] Archivo copiado con éxito de content:// a:" << destPath
            << "Tamaño:" << QFileInfo(destPath).size() << "bytes";

    return QDir::cleanPath(destPath);
}

QString NovaPlatformUtils::sanitizePath(const QString &rawPath)
{
    if (rawPath.isEmpty()) return QString();

    // 1. Primero decodificar percent-encoding (resuelve %20, %2F, %3A, etc.)
    QString path = QUrl::fromPercentEncoding(rawPath.toUtf8());

    // 2. Si es una URI content:// (Android Storage Access Framework)
    if (path.startsWith(QLatin1String("content://"), Qt::CaseInsensitive)) {
        return copyContentUriToLocal(path);
    }

    // 3. Si contiene el esquema "file:", extraer la ruta local absoluta de forma robusta
    if (path.startsWith("file:", Qt::CaseInsensitive)) {
        QUrl url(path);
        if (url.isValid() && url.isLocalFile()) {
            path = url.toLocalFile();
        } else {
            // Limpieza manual de seguridad si el esquema de QUrl falla
            path.remove(0, 5); // Quita "file:"
            
            // Reemplazar diagonales dobles iniciales de esquemas como file:// o file:///
            while (path.startsWith("//")) {
                path.remove(0, 1);
            }
            
            // Garantizar que en Linux/Android empiece con un solo "/"
#if !defined(Q_OS_WIN)
            if (!path.startsWith("/")) {
                path = "/" + path;
            }
#endif
        }
    }

    // 4. Normalizar la ruta final del sistema de archivos
    return QDir::cleanPath(path);
}

bool NovaPlatformUtils::isMobilePlatform()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return true;
#else
    return false;
#endif
}

void NovaPlatformUtils::hapticFeedback()
{
#if defined(Q_OS_ANDROID)
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "com/novastudio/nova/MainActivity",
        "getInstance",
        "()Lcom/novastudio/nova/MainActivity;");
    if (activity.isValid()) {
        activity.callMethod<void>("vibrateTick");
    }
#endif
}