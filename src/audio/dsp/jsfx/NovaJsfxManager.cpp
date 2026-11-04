#include "audio/dsp/jsfx/NovaJsfxManager.h"
#include <QFileInfo>
#include <QFile>
#include <QTextStream>

NovaJsfxManager& NovaJsfxManager::instance()
{
    static NovaJsfxManager s_instance;
    return s_instance;
}

NovaJsfxManager::NovaJsfxManager()
{
    initBuiltinPlugins();
}

void NovaJsfxManager::initBuiltinPlugins()
{
    // 1. Analog Tube Saturator
    {
        JsfxPluginInfo p;
        p.id = QStringLiteral("jsfx:builtin:tube_saturation");
        p.name = QStringLiteral("Analog Tube Saturator (JSFX)");
        p.category = QStringLiteral("SATURACIÓN");
        p.description = QStringLiteral("Emulación analógica de saturación a válvulas triodo con calidez armónica");
        p.scriptContent = QStringLiteral(
            "desc: Analog Tube Saturator (JSFX)\n"
            "slider1:6<0,24,0.1>Drive (dB)\n"
            "slider2:50<0,100,1>Calidez (%)\n"
            "slider3:0<-24,12,0.1>Salida (dB)\n"
            "\n"
            "@init\n"
            "gain = 1;\n"
            "warmth = 0.5;\n"
            "out_gain = 1;\n"
            "\n"
            "@slider\n"
            "gain = 10^(slider1 / 20);\n"
            "warmth = slider2 / 100;\n"
            "out_gain = 10^(slider3 / 20);\n"
            "\n"
            "@sample\n"
            "x0 = spl0 * gain;\n"
            "x1 = spl1 * gain;\n"
            "spl0 = (tanh(x0) + warmth * (x0 * x0 * 0.15)) * out_gain;\n"
            "spl1 = (tanh(x1) + warmth * (x1 * x1 * 0.15)) * out_gain;\n"
        );
        m_plugins[p.id] = p;
    }

    // 2. Stereo Widener Mid/Side
    {
        JsfxPluginInfo p;
        p.id = QStringLiteral("jsfx:builtin:stereo_widener");
        p.name = QStringLiteral("Stereo Widener Mid/Side (JSFX)");
        p.category = QStringLiteral("IMAGEN");
        p.description = QStringLiteral("Expansor y controlador de imagen estéreo mediante procesamiento Mid/Side");
        p.scriptContent = QStringLiteral(
            "desc: Stereo Widener Mid/Side (JSFX)\n"
            "slider1:130<0,200,1>Ancho Estéreo (%)\n"
            "slider2:0<-12,12,0.1>Nivel Mid (dB)\n"
            "\n"
            "@init\n"
            "width = 1.3;\n"
            "mid_gain = 1.0;\n"
            "\n"
            "@slider\n"
            "width = slider1 / 100;\n"
            "mid_gain = 10^(slider2 / 20);\n"
            "\n"
            "@sample\n"
            "mid = (spl0 + spl1) * 0.5 * mid_gain;\n"
            "side = (spl0 - spl1) * 0.5 * width;\n"
            "spl0 = mid + side;\n"
            "spl1 = mid - side;\n"
        );
        m_plugins[p.id] = p;
    }

    // 3. Vocal Air Exciter
    {
        JsfxPluginInfo p;
        p.id = QStringLiteral("jsfx:builtin:vocal_air");
        p.name = QStringLiteral("Vocal Air Exciter (JSFX)");
        p.category = QStringLiteral("EXCITER");
        p.description = QStringLiteral("Excitador de armónicos ultra-altos para dar brillo y presencia cristalina a voces");
        p.scriptContent = QStringLiteral(
            "desc: Vocal Air Exciter (JSFX)\n"
            "slider1:35<0,100,1>Presencia Aérea (%)\n"
            "slider2:10000<6000,16000,100>Corte (Hz)\n"
            "\n"
            "@init\n"
            "hpL = 0;\n"
            "hpR = 0;\n"
            "\n"
            "@slider\n"
            "mix = slider1 / 100;\n"
            "alpha = 0.5;\n"
            "\n"
            "@sample\n"
            "highL = spl0 - hpL;\n"
            "highR = spl1 - hpR;\n"
            "hpL = hpL + 0.3 * highL;\n"
            "hpR = hpR + 0.3 * highR;\n"
            "spl0 = spl0 + tanh(highL * 2.5) * mix * 0.4;\n"
            "spl1 = spl1 + tanh(highR * 2.5) * mix * 0.4;\n"
        );
        m_plugins[p.id] = p;
    }

    // 4. Fast Noise Gate
    {
        JsfxPluginInfo p;
        p.id = QStringLiteral("jsfx:builtin:noise_gate");
        p.name = QStringLiteral("Fast Noise Gate (JSFX)");
        p.category = QStringLiteral("DINÁMICA");
        p.description = QStringLiteral("Puerta de ruido rápida con supresión de siseo y umbral regulable");
        p.scriptContent = QStringLiteral(
            "desc: Fast Noise Gate (JSFX)\n"
            "slider1:-45<-80,0,1>Umbral (dB)\n"
            "slider2:50<5,500,5>Relajación (ms)\n"
            "\n"
            "@init\n"
            "env = 0;\n"
            "\n"
            "@slider\n"
            "thresh = 10^(slider1 / 20);\n"
            "rel = exp(-1 / (slider2 * 0.001 * srate));\n"
            "\n"
            "@sample\n"
            "lvl = max(abs(spl0), abs(spl1));\n"
            "env = (lvl > env) ? lvl : (env * rel);\n"
            "gain = (env > thresh) ? 1.0 : (env / (thresh + 0.00001));\n"
            "spl0 = spl0 * gain;\n"
            "spl1 = spl1 * gain;\n"
        );
        m_plugins[p.id] = p;
    }
}

QList<NovaJsfxManager::JsfxPluginInfo> NovaJsfxManager::availableJsfxPlugins() const
{
    QList<JsfxPluginInfo> list;
    for (const auto& pair : m_plugins) {
        list.append(pair.second);
    }
    return list;
}

std::shared_ptr<NovaJsfxEffect> NovaJsfxManager::createEffect(const QString& idOrPath)
{
    auto it = m_plugins.find(idOrPath);
    if (it != m_plugins.end()) {
        auto effect = std::make_shared<NovaJsfxEffect>(it->second.scriptContent, it->second.name);
        return effect;
    }

    // Si es una ruta de archivo local
    if (QFile::exists(idOrPath)) {
        auto effect = std::make_shared<NovaJsfxEffect>();
        if (effect->loadScriptFile(idOrPath)) {
            return effect;
        }
    }

    return nullptr;
}

bool NovaJsfxManager::registerUserScript(const QString& filePath, QString* outError)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (outError) *outError = QStringLiteral("No se pudo leer el archivo: %1").arg(filePath);
        return false;
    }
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    QString baseName = QFileInfo(filePath).baseName();
    QString id = QStringLiteral("jsfx:user:%1").arg(baseName);
    return registerUserScriptContent(id, content, outError);
}

bool NovaJsfxManager::registerUserScriptContent(const QString& id, const QString& content, QString* outError)
{
    auto tempEffect = std::make_shared<NovaJsfxEffect>();
    if (!tempEffect->loadScript(content, outError)) {
        return false;
    }

    JsfxPluginInfo p;
    p.id = id;
    p.name = tempEffect->name().isEmpty() ? QFileInfo(id).baseName() : tempEffect->name();
    p.category = QStringLiteral("JSFX");
    p.description = QStringLiteral("Script JSFX de REAPER cargado por el usuario");
    p.scriptContent = content;

    m_plugins[id] = p;
    return true;
}
