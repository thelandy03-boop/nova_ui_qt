#pragma once

#include "audio/dsp/jsfx/NovaJsfxEffect.h"
#include <QString>
#include <QVariantList>
#include <memory>
#include <map>

/**
 * Gestor de plugins JSFX de REAPER en NOVA Studio.
 * Administra los scripts JSFX integrados y los cargados por el usuario.
 */
class NovaJsfxManager {
public:
    static NovaJsfxManager& instance();

    struct JsfxPluginInfo {
        QString id;
        QString name;
        QString category;
        QString description;
        QString scriptContent;
        QString filePath; // si fue cargado de archivo
    };

    // Lista de plugins disponibles (para mostrar en la UI de Add Plugin)
    QList<JsfxPluginInfo> availableJsfxPlugins() const;

    // Crea una instancia de NovaJsfxEffect a partir de un id o ruta
    std::shared_ptr<NovaJsfxEffect> createEffect(const QString& idOrPath);

    // Obtiene el contenido del script por ID o por nombre del plugin
    QString getScriptContent(const QString& idOrName) const;

    // Registra un nuevo script JSFX personalizado cargado por el usuario
    bool registerUserScript(const QString& filePath, QString* outError = nullptr);
    bool registerUserScript(const QString& filePath, const QString& category, QString* outError = nullptr);
    bool registerUserScriptContent(const QString& id, const QString& content, QString* outError = nullptr);
    bool registerUserScriptContent(const QString& id, const QString& content, const QString& category, QString* outError = nullptr);

private:
    NovaJsfxManager();
    void initBuiltinPlugins();

    std::map<QString, JsfxPluginInfo> m_plugins;
};
