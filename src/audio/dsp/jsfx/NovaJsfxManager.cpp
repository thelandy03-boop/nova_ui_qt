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

    // 5. ACE EQ (Parametric 3-Band Biquad)
    {
        JsfxPluginInfo p;
        p.id = QStringLiteral("jsfx:builtin:ace_eq");
        p.name = QStringLiteral("ACE EQ");
        p.category = QStringLiteral("EQ");
        p.description = QStringLiteral("Ecualizador paramétrico oficial de 3 bandas (Graves 80Hz, Medios 1kHz, Agudos 10kHz)");
        p.scriptContent = QStringLiteral(
            "desc: ACE EQ (Parametric 3-Band)\n"
            "slider1:0<-12,12,0.1>Graves 80Hz (dB)\n"
            "slider2:0<-12,12,0.1>Medios 1kHz (dB)\n"
            "slider3:0<-12,12,0.1>Agudos 10kHz (dB)\n"
            "\n"
            "@init\n"
            "pi = 3.141592653589793;\n"
            "x1_lL = 0; x2_lL = 0; y1_lL = 0; y2_lL = 0;\n"
            "x1_lR = 0; x2_lR = 0; y1_lR = 0; y2_lR = 0;\n"
            "x1_mL = 0; x2_mL = 0; y1_mL = 0; y2_mL = 0;\n"
            "x1_mR = 0; x2_mR = 0; y1_mR = 0; y2_mR = 0;\n"
            "x1_hL = 0; x2_hL = 0; y1_hL = 0; y2_hL = 0;\n"
            "x1_hR = 0; x2_hR = 0; y1_hR = 0; y2_hR = 0;\n"
            "\n"
            "@slider\n"
            "A_l = 10^(slider1 / 40);\n"
            "w0_l = 2 * pi * 80 / srate;\n"
            "cos_l = cos(w0_l); sin_l = sin(w0_l);\n"
            "alpha_l = sin_l / (2 * 0.7071);\n"
            "b0_l = A_l * ((A_l + 1) - (A_l - 1) * cos_l + 2 * sqrt(A_l) * alpha_l);\n"
            "b1_l = 2 * A_l * ((A_l - 1) - (A_l + 1) * cos_l);\n"
            "b2_l = A_l * ((A_l + 1) - (A_l - 1) * cos_l - 2 * sqrt(A_l) * alpha_l);\n"
            "a0_l = (A_l + 1) + (A_l - 1) * cos_l + 2 * sqrt(A_l) * alpha_l;\n"
            "a1_l = -2 * ((A_l - 1) + (A_l + 1) * cos_l);\n"
            "a2_l = (A_l + 1) + (A_l - 1) * cos_l - 2 * sqrt(A_l) * alpha_l;\n"
            "\n"
            "A_m = 10^(slider2 / 40);\n"
            "w0_m = 2 * pi * 1000 / srate;\n"
            "cos_m = cos(w0_m); sin_m = sin(w0_m);\n"
            "alpha_m = sin_m / (2 * 1.0);\n"
            "b0_m = 1 + alpha_m * A_m;\n"
            "b1_m = -2 * cos_m;\n"
            "b2_m = 1 - alpha_m * A_m;\n"
            "a0_m = 1 + alpha_m / A_m;\n"
            "a1_m = -2 * cos_m;\n"
            "a2_m = 1 - alpha_m / A_m;\n"
            "\n"
            "A_h = 10^(slider3 / 40);\n"
            "w0_h = 2 * pi * 10000 / srate;\n"
            "cos_h = cos(w0_h); sin_h = sin(w0_h);\n"
            "alpha_h = sin_h / (2 * 0.7071);\n"
            "b0_h = A_h * ((A_h + 1) + (A_h - 1) * cos_h + 2 * sqrt(A_h) * alpha_h);\n"
            "b1_h = -2 * A_h * ((A_h - 1) + (A_h + 1) * cos_h);\n"
            "b2_h = A_h * ((A_h + 1) - (A_h - 1) * cos_h - 2 * sqrt(A_h) * alpha_h);\n"
            "a0_h = (A_h + 1) - (A_h - 1) * cos_h + 2 * sqrt(A_h) * alpha_h;\n"
            "a1_h = 2 * ((A_h - 1) - (A_h + 1) * cos_h);\n"
            "a2_h = (A_h + 1) - (A_h - 1) * cos_h - 2 * sqrt(A_h) * alpha_h;\n"
            "\n"
            "@sample\n"
            "outL = (b0_l * spl0 + b1_l * x1_lL + b2_l * x2_lL - a1_l * y1_lL - a2_l * y2_lL) / a0_l;\n"
            "x2_lL = x1_lL; x1_lL = spl0; y2_lL = y1_lL; y1_lL = outL;\n"
            "outL2 = (b0_m * outL + b1_m * x1_mL + b2_m * x2_mL - a1_m * y1_mL - a2_m * y2_mL) / a0_m;\n"
            "x2_mL = x1_mL; x1_mL = outL; y2_mL = y1_mL; y1_mL = outL2;\n"
            "spl0 = (b0_h * outL2 + b1_h * x1_hL + b2_h * x2_hL - a1_h * y1_hL - a2_h * y2_hL) / a0_h;\n"
            "x2_hL = x1_hL; x1_hL = outL2; y2_hL = y1_hL; y1_hL = spl0;\n"
            "\n"
            "outR = (b0_l * spl1 + b1_l * x1_lR + b2_l * x2_lR - a1_l * y1_lR - a2_l * y2_lR) / a0_l;\n"
            "x2_lR = x1_lR; x1_lR = spl1; y2_lR = y1_lR; y1_lR = outR;\n"
            "outR2 = (b0_m * outR + b1_m * x1_mR + b2_m * x2_mR - a1_m * y1_mR - a2_m * y2_mR) / a0_m;\n"
            "x2_mR = x1_mR; x1_mR = outR; y2_mR = y1_mR; y1_mR = outR2;\n"
            "spl1 = (b0_h * outR2 + b1_h * x1_hR + b2_h * x2_hR - a1_h * y1_hR - a2_h * y2_hR) / a0_h;\n"
            "x2_hR = x1_hR; x1_hR = outR2; y2_hR = y1_hR; y1_hR = spl1;\n"
        );
        m_plugins[p.id] = p;
        m_plugins[QStringLiteral("urn:ardour:a-eq")] = p;
    }

    // 6. ACE Compressor (Studio Dynamics)
    {
        JsfxPluginInfo p;
        p.id = QStringLiteral("jsfx:builtin:ace_compressor");
        p.name = QStringLiteral("ACE Compressor");
        p.category = QStringLiteral("DINÁMICA");
        p.description = QStringLiteral("Compresor analógico transparente con control de umbral, ratio y ganancia");
        p.scriptContent = QStringLiteral(
            "desc: ACE Compressor (Studio Dynamics)\n"
            "slider1:-12<-40,0,0.5>Umbral (dB)\n"
            "slider2:3<1,20,0.5>Ratio (:1)\n"
            "slider3:2<0,20,0.5>Ganancia (dB)\n"
            "\n"
            "@init\n"
            "env = 0;\n"
            "gain_red = 1;\n"
            "\n"
            "@slider\n"
            "thresh_db = slider1;\n"
            "ratio = slider2;\n"
            "makeup = 10^(slider3 / 20);\n"
            "att = exp(-1 / (0.010 * srate));\n"
            "rel = exp(-1 / (0.100 * srate));\n"
            "\n"
            "@sample\n"
            "max_s = max(abs(spl0), abs(spl1));\n"
            "env = (max_s > env) ? (att * env + (1 - att) * max_s) : (rel * env + (1 - rel) * max_s);\n"
            "env_db = 20 * log10(max(env, 0.00001));\n"
            "excess = env_db - thresh_db;\n"
            "gr_db = (excess > 0 && ratio > 1) ? (excess * (1 - 1 / ratio)) : 0;\n"
            "target_gain = 10^(-gr_db / 20);\n"
            "gain_red = 0.9 * gain_red + 0.1 * target_gain;\n"
            "final_gain = gain_red * makeup;\n"
            "spl0 = spl0 * final_gain;\n"
            "spl1 = spl1 * final_gain;\n"
        );
        m_plugins[p.id] = p;
        m_plugins[QStringLiteral("urn:ardour:a-comp")] = p;
    }

    // 7. ACE Delay (Stereo Tap)
    {
        JsfxPluginInfo p;
        p.id = QStringLiteral("jsfx:builtin:ace_delay");
        p.name = QStringLiteral("ACE Delay");
        p.category = QStringLiteral("DELAY");
        p.description = QStringLiteral("Efecto de retardo digital estéreo con tiempo, feedback y mezcla dry/wet");
        p.scriptContent = QStringLiteral(
            "desc: ACE Delay (Stereo Tap)\n"
            "slider1:250<10,1000,10>Tiempo (ms)\n"
            "slider2:30<0,95,1>Feedback (%)\n"
            "slider3:25<0,100,1>Mezcla (%)\n"
            "\n"
            "@init\n"
            "buf_pos = 0;\n"
            "max_samples = 96000;\n"
            "\n"
            "@slider\n"
            "delay_samples = floor(slider1 * 0.001 * srate);\n"
            "delay_samples = min(max_samples - 1, max(10, delay_samples));\n"
            "fb = slider2 / 100;\n"
            "wet = slider3 / 100;\n"
            "dry = 1 - wet;\n"
            "\n"
            "@sample\n"
            "read_pos = (buf_pos - delay_samples + max_samples) % max_samples;\n"
            "delayed_L = read_pos[0];\n"
            "delayed_R = (read_pos + 100000)[0];\n"
            "buf_pos[0] = spl0 + delayed_L * fb;\n"
            "(buf_pos + 100000)[0] = spl1 + delayed_R * fb;\n"
            "spl0 = spl0 * dry + delayed_L * wet;\n"
            "spl1 = spl1 * dry + delayed_R * wet;\n"
            "buf_pos = (buf_pos + 1) % max_samples;\n"
        );
        m_plugins[p.id] = p;
        m_plugins[QStringLiteral("urn:ardour:a-delay")] = p;
    }

    // 8. ACE Reverb (Studio Room)
    {
        JsfxPluginInfo p;
        p.id = QStringLiteral("jsfx:builtin:ace_reverb");
        p.name = QStringLiteral("ACE Reverb");
        p.category = QStringLiteral("ESPACIO");
        p.description = QStringLiteral("Procesador de reverberación espacial de sala y estudio con simulación Schroeder");
        p.scriptContent = QStringLiteral(
            "desc: ACE Reverb (Studio Room)\n"
            "slider1:45<0,100,1>Tamaño Sala (%)\n"
            "slider2:20<0,100,1>Mezcla Wet (%)\n"
            "\n"
            "@init\n"
            "pos1 = 0; pos2 = 0; pos3 = 0; pos4 = 0;\n"
            "pos5 = 0; pos6 = 0;\n"
            "del1 = 1116; del2 = 1188; del3 = 1277; del4 = 1356;\n"
            "ap1 = 556; ap2 = 441;\n"
            "\n"
            "@slider\n"
            "room = 0.7 + (slider1 / 100) * 0.28;\n"
            "wet = slider2 / 100;\n"
            "dry = 1 - wet;\n"
            "\n"
            "@sample\n"
            "in_mono = (spl0 + spl1) * 0.015;\n"
            "out1 = pos1[0];\n"
            "pos1[0] = in_mono + out1 * room;\n"
            "pos1 = (pos1 + 1) % del1;\n"
            "out2 = (pos2 + 2000)[0];\n"
            "(pos2 + 2000)[0] = in_mono + out2 * room;\n"
            "pos2 = (pos2 + 1) % del2;\n"
            "out3 = (pos3 + 4000)[0];\n"
            "(pos3 + 4000)[0] = in_mono + out3 * room;\n"
            "pos3 = (pos3 + 1) % del3;\n"
            "out4 = (pos4 + 6000)[0];\n"
            "(pos4 + 6000)[0] = in_mono + out4 * room;\n"
            "pos4 = (pos4 + 1) % del4;\n"
            "comb_sum = out1 + out2 + out3 + out4;\n"
            "buf_ap1 = (pos5 + 8000)[0];\n"
            "ap_out1 = -comb_sum + buf_ap1;\n"
            "(pos5 + 8000)[0] = comb_sum + buf_ap1 * 0.5;\n"
            "pos5 = (pos5 + 1) % ap1;\n"
            "buf_ap2 = (pos6 + 10000)[0];\n"
            "ap_out2 = -ap_out1 + buf_ap2;\n"
            "(pos6 + 10000)[0] = ap_out1 + buf_ap2 * 0.5;\n"
            "pos6 = (pos6 + 1) % ap2;\n"
            "rev_sig = ap_out2 * 2.0;\n"
            "spl0 = spl0 * dry + rev_sig * wet;\n"
            "spl1 = spl1 * dry + rev_sig * wet;\n"
        );
        m_plugins[p.id] = p;
        m_plugins[QStringLiteral("urn:ardour:a-reverb")] = p;
    }
}

QList<NovaJsfxManager::JsfxPluginInfo> NovaJsfxManager::availableJsfxPlugins() const
{
    QList<JsfxPluginInfo> list;
    // Devolvemos sólo las entradas canónicas principales (evitamos duplicar los alias urn:ardour:*)
    for (const auto& pair : m_plugins) {
        if (!pair.first.startsWith(QStringLiteral("urn:ardour:"))) {
            list.append(pair.second);
        }
    }
    return list;
}

QString NovaJsfxManager::getScriptContent(const QString& idOrName) const
{
    auto it = m_plugins.find(idOrName);
    if (it != m_plugins.end()) {
        return it->second.scriptContent;
    }

    // Búsqueda aproximada por coincidencia de nombre o id canónico
    QString lower = idOrName.toLower();
    for (const auto& pair : m_plugins) {
        if (pair.first.toLower() == lower || pair.second.name.toLower() == lower) {
            return pair.second.scriptContent;
        }
    }

    // Mapeo por palabras clave de efectos estándar
    if (lower.contains(QStringLiteral("eq"))) return getScriptContent(QStringLiteral("jsfx:builtin:ace_eq"));
    if (lower.contains(QStringLiteral("comp"))) return getScriptContent(QStringLiteral("jsfx:builtin:ace_compressor"));
    if (lower.contains(QStringLiteral("delay"))) return getScriptContent(QStringLiteral("jsfx:builtin:ace_delay"));
    if (lower.contains(QStringLiteral("reverb"))) return getScriptContent(QStringLiteral("jsfx:builtin:ace_reverb"));
    if (lower.contains(QStringLiteral("tube")) || lower.contains(QStringLiteral("saturat"))) return getScriptContent(QStringLiteral("jsfx:builtin:tube_saturation"));
    if (lower.contains(QStringLiteral("widen"))) return getScriptContent(QStringLiteral("jsfx:builtin:stereo_widener"));
    if (lower.contains(QStringLiteral("air")) || lower.contains(QStringLiteral("excit"))) return getScriptContent(QStringLiteral("jsfx:builtin:vocal_air"));
    if (lower.contains(QStringLiteral("gate"))) return getScriptContent(QStringLiteral("jsfx:builtin:noise_gate"));

    return QString();
}

std::shared_ptr<NovaJsfxEffect> NovaJsfxManager::createEffect(const QString& idOrPath)
{
    QString lookupId = idOrPath;
    if (idOrPath.contains(QStringLiteral("a-eq"))) lookupId = QStringLiteral("jsfx:builtin:ace_eq");
    else if (idOrPath.contains(QStringLiteral("a-comp"))) lookupId = QStringLiteral("jsfx:builtin:ace_compressor");
    else if (idOrPath.contains(QStringLiteral("a-delay"))) lookupId = QStringLiteral("jsfx:builtin:ace_delay");
    else if (idOrPath.contains(QStringLiteral("a-reverb"))) lookupId = QStringLiteral("jsfx:builtin:ace_reverb");

    auto it = m_plugins.find(lookupId);
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
