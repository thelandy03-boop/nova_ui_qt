#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QDirIterator>
#include <QDebug>
#include <QTimer>
#include <QWindow>
#include <QLoggingCategory>
#include <QLockFile>
#include <QStandardPaths>

#include "src/NovaAudioEngine.h"
#include "src/views/NovaWaveformItem.h"
#include "src/views/NovaJsfxHighlighter.h"
#include "src/views/NovaJsfxGfxItem.h"
#include "src/core/NovaUpdateChecker.h"

#if defined(Q_OS_ANDROID)
// El controlador del spike (motor de audio + forma de onda) solo existe en
// Android: en el escritorio la app es la de siempre.
#include "src/core/platform/NovaSpikeController.h"
#endif

int main(int argc, char *argv[])
{
    // 🎨 FORMATO PROFESIONAL DE LOGS EN CONSOLA
    qSetMessagePattern("[%{time hh:mm:ss.zzz}] [%{type}] [%{category}] %{message}");

#if defined(Q_OS_WIN)
    static FILE *s_logFile = fopen("nova_studio_log.txt", "w");
    if (s_logFile) {
        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &msg) {
            if (s_logFile) {
                QByteArray localMsg = msg.toLocal8Bit();
                fprintf(s_logFile, "[%s] %s\n",
                        type == QtDebugMsg ? "DEBUG" : type == QtWarningMsg ? "WARN" : type == QtCriticalMsg ? "CRIT" : "FATAL",
                        localMsg.constData());
                fflush(s_logFile);
            }
        });
    }
#endif

    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    qputenv("QML_DISABLE_DISK_CACHE", "1");

    // 🛡️ Identificadores globales de la aplicación (Silencia warnings de QSettings)
    QCoreApplication::setOrganizationName("NovaStudio");
    QCoreApplication::setOrganizationDomain("novastudio.org");
    QCoreApplication::setApplicationName("NovaStudio");

    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(true);

#if defined(Q_OS_ANDROID)
    // Fase 5 (Touch UX): mejor escalado en pantallas de alta densidad y
    // tamaños táctiles más consistentes en dispositivos Android.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    // 🎨 Registrar tipo nativo de formas de onda
    qmlRegisterType<NovaWaveformItem>("NovaStudio", 1, 0, "NovaWaveformItem");
    // 🎨 Registrar resaltador de sintaxis JSFX / EEL2 en tiempo real
    qmlRegisterType<NovaJsfxHighlighter>("NovaStudio", 1, 0, "JsfxHighlighter");
    // 🎨 Registrar lienzo interactivo de plugins JSFX (@gfx)
    qmlRegisterType<NovaJsfxGfxItem>("NovaStudio", 1, 0, "JsfxGfxView");

#if defined(Q_OS_ANDROID)
    // 🎧 El controlador del spike: motor de audio y forma de onda real.
    qmlRegisterType<NovaSpikeController>("NovaStudio", 1, 0, "SpikeController");
#endif

    // 🔒 Una sola instancia a la vez (solo Linux Desktop con JACK/PipeWire).
#if !defined(Q_OS_WIN) && !defined(Q_OS_ANDROID)
    const QString lockPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("nova_ui_qt.lock"));
    QLockFile instanceLock(lockPath);
    instanceLock.setStaleLockTime(0); // Solo se considera caduca si el proceso murio
    if (!instanceLock.tryLock(0)) {
        qint64 otherPid = 0;
        QString otherHost;
        QString otherApp;
        instanceLock.getLockInfo(&otherPid, &otherHost, &otherApp);
        qCritical() << "❌ Ya hay otra instancia de NOVA Studio en ejecucion (PID" << otherPid << ").";
        qCritical() << "   Ciérrala antes de arrancar otra: el servidor de audio no admite dos apps a la vez.";
        return 2;
    }
#endif

    // Instancia de audio Nova / Ardour Bridge
    NovaAudioEngine audioEngine;

    // Comprobador de actualizaciones obligatorias
    NovaUpdateChecker updateChecker;
    if (!audioEngine.initEngine()) {
        qCritical() << "❌ No se pudo inicializar el motor de audio.";
        qCritical() << "   Revisa que PipeWire/JACK este en marcha (pw-jack) y que no haya otra app usando el audio.";
        return 3;
    }

    QQmlApplicationEngine engine;

    // Registrar audioEngine globalmente en QML
    engine.rootContext()->setContextProperty("AudioEngine", &audioEngine);
    engine.rootContext()->setContextProperty("UpdateChecker", &updateChecker);

    QStringList candidates;
    candidates << QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../qml")
               << QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../../qml")
               << QDir::cleanPath(QDir::currentPath() + "/../qml")
               << QDir::cleanPath(QDir::currentPath() + "/qml");

    QString qmlDir;
    QString mainQml;
    for (const QString &c : candidates) {
        const QString m = c + "/Main.qml";
        if (QFileInfo::exists(m)) {
            qmlDir = c;
            mainQml = m;
            break;
        }
    }

    if (!mainQml.isEmpty()) {
        qDebug() << "HOT-RELOAD activo desde:" << qmlDir;

        engine.addImportPath(qmlDir);
        engine.addImportPath(qmlDir + "/skins");
        engine.addImportPath(qmlDir + "/skins/draft");
        engine.addImportPath(qmlDir + "/skins/bandlab");
        engine.addImportPath(qmlDir + "/skins/reaper");

        const QUrl url = QUrl::fromLocalFile(mainQml);

        auto *watcher = new QFileSystemWatcher(&app);
        auto *reloadTimer = new QTimer(&app);
        reloadTimer->setSingleShot(true);
        reloadTimer->setInterval(200);

        auto addAllPaths = [watcher, qmlDir]() {
            const auto files = watcher->files();
            if (!files.isEmpty()) watcher->removePaths(files);
            const auto dirs = watcher->directories();
            if (!dirs.isEmpty()) watcher->removePaths(dirs);

            watcher->addPath(qmlDir);
            QDirIterator it(qmlDir,
                            QStringList() << "*.qml" << "qmldir" << "*.svg",
                            QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                watcher->addPath(path);
                watcher->addPath(QFileInfo(path).absolutePath());
            }
        };

        addAllPaths();

        auto doReload = [&engine, url, addAllPaths, &audioEngine, &updateChecker]() {
            qDebug() << "HOT-RELOAD ejecutando...";

            const auto roots = engine.rootObjects();
            for (QObject *obj : roots) {
                if (auto *w = qobject_cast<QWindow *>(obj))
                    w->hide();
                obj->deleteLater();
            }

            engine.clearComponentCache();
            engine.rootContext()->setContextProperty("AudioEngine", &audioEngine);
    engine.rootContext()->setContextProperty("UpdateChecker", &updateChecker);
            addAllPaths();

            QTimer::singleShot(0, &engine, [&engine, url]() {
                engine.load(url);
                if (engine.rootObjects().isEmpty())
                    qCritical() << "HOT-RELOAD: fallo al recargar Main.qml";
                else
                    qDebug() << "HOT-RELOAD OK";
            });
        };

        QObject::connect(reloadTimer, &QTimer::timeout, &app, doReload);

        auto scheduleReload = [reloadTimer](const QString &path) {
            qDebug() << "QML cambio:" << path;
            reloadTimer->start();
        };

        QObject::connect(watcher, &QFileSystemWatcher::fileChanged, &app, scheduleReload);
        QObject::connect(watcher, &QFileSystemWatcher::directoryChanged, &app, scheduleReload);

        engine.load(url);
        if (engine.rootObjects().isEmpty()) {
            qCritical() << "No se pudo cargar" << mainQml;
            return -1;
        }
    } else {
        qDebug() << "Modo QRC (sin hot-reload de fuentes)";
        engine.addImportPath(QStringLiteral("qrc:/qt/qml/NovaStudio"));
#if defined(Q_OS_ANDROID)
        // Port completo: en Android se carga la misma UI de escritorio
        // (Main.qml) que en Linux/Windows. El motor de audio se apoya en
        // los stubs de Android (NovaAndroidStubs / NovaAndroidAudioEngine);
        // la funcionalidad completa de pistas/sesiones se habilita por fases.
        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/NovaStudio/qml/Main.qml")));
#else
        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/NovaStudio/qml/Main.qml")));
#endif
        if (engine.rootObjects().isEmpty())
            return -1;
    }

    // Comprobar actualizaciones obligatorias al arrancar
    updateChecker.check();

    return app.exec();
}