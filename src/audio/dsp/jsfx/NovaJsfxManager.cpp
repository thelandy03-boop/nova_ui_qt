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
        p.description = QStringLiteral("Ecualizador paramétrico oficial de 4 bandas (Frequency, Gain y Bandwidth)");
        p.scriptContent = QStringLiteral(
            "desc: ACE EQ (Parametric 4-Band)\n"
            "slider1:300<20,2000,1>Frequency 1 (Hz)\n"
            "slider2:0<-18,18,0.1>Gain 1 (dB)\n"
            "slider3:1.0<0.1,5,0.05>Bandwidth 1 (oct)\n"
            "slider4:1000<100,10000,1>Frequency 2 (Hz)\n"
            "slider5:0<-18,18,0.1>Gain 2 (dB)\n"
            "slider6:1.0<0.1,5,0.05>Bandwidth 2 (oct)\n"
            "slider7:3000<500,15000,1>Frequency 3 (Hz)\n"
            "slider8:0<-18,18,0.1>Gain 3 (dB)\n"
            "slider9:1.0<0.1,5,0.05>Bandwidth 3 (oct)\n"
            "slider10:8000<1000,20000,1>Frequency 4 (Hz)\n"
            "slider11:0<-18,18,0.1>Gain 4 (dB)\n"
            "slider12:1.0<0.1,5,0.05>Bandwidth 4 (oct)\n"
            "\n"
            "@init\n"
            "pi = 3.141592653589793;\n"
            "x1_1L = 0; x2_1L = 0; y1_1L = 0; y2_1L = 0;\n"
            "x1_1R = 0; x2_1R = 0; y1_1R = 0; y2_1R = 0;\n"
            "x1_2L = 0; x2_2L = 0; y1_2L = 0; y2_2L = 0;\n"
            "x1_2R = 0; x2_2R = 0; y1_2R = 0; y2_2R = 0;\n"
            "x1_3L = 0; x2_3L = 0; y1_3L = 0; y2_3L = 0;\n"
            "x1_3R = 0; x2_3R = 0; y1_3R = 0; y2_3R = 0;\n"
            "x1_4L = 0; x2_4L = 0; y1_4L = 0; y2_4L = 0;\n"
            "x1_4R = 0; x2_4R = 0; y1_4R = 0; y2_4R = 0;\n"
            "\n"
            "@slider\n"
            "// Banda 1\n"
            "A1 = 10^(slider2 / 40); w1 = 2 * pi * slider1 / srate;\n"
            "cos1 = cos(w1); sin1 = sin(w1); alpha1 = sin1 * slider3 * 0.5;\n"
            "b0_1 = 1 + alpha1 * A1; b1_1 = -2 * cos1; b2_1 = 1 - alpha1 * A1;\n"
            "a0_1 = 1 + alpha1 / A1; a1_1 = -2 * cos1; a2_1 = 1 - alpha1 / A1;\n"
            "\n"
            "// Banda 2\n"
            "A2 = 10^(slider5 / 40); w2 = 2 * pi * slider4 / srate;\n"
            "cos2 = cos(w2); sin2 = sin(w2); alpha2 = sin2 * slider6 * 0.5;\n"
            "b0_2 = 1 + alpha2 * A2; b1_2 = -2 * cos2; b2_2 = 1 - alpha2 * A2;\n"
            "a0_2 = 1 + alpha2 / A2; a1_2 = -2 * cos2; a2_2 = 1 - alpha2 / A2;\n"
            "\n"
            "// Banda 3\n"
            "A3 = 10^(slider8 / 40); w3 = 2 * pi * slider7 / srate;\n"
            "cos3 = cos(w3); sin3 = sin(w3); alpha3 = sin3 * slider9 * 0.5;\n"
            "b0_3 = 1 + alpha3 * A3; b1_3 = -2 * cos3; b2_3 = 1 - alpha3 * A3;\n"
            "a0_3 = 1 + alpha3 / A3; a1_3 = -2 * cos3; a2_3 = 1 - alpha3 / A3;\n"
            "\n"
            "// Banda 4\n"
            "A4 = 10^(slider11 / 40); w4 = 2 * pi * slider10 / srate;\n"
            "cos4 = cos(w4); sin4 = sin(w4); alpha4 = sin4 * slider12 * 0.5;\n"
            "b0_4 = 1 + alpha4 * A4; b1_4 = -2 * cos4; b2_4 = 1 - alpha4 * A4;\n"
            "a0_4 = 1 + alpha4 / A4; a1_4 = -2 * cos4; a2_4 = 1 - alpha4 / A4;\n"
            "\n"
            "@sample\n"
            "// Canal Izquierdo en cascada\n"
            "s1L = (b0_1 * spl0 + b1_1 * x1_1L + b2_1 * x2_1L - a1_1 * y1_1L - a2_1 * y2_1L) / a0_1;\n"
            "x2_1L = x1_1L; x1_1L = spl0; y2_1L = y1_1L; y1_1L = s1L;\n"
            "s2L = (b0_2 * s1L + b1_2 * x1_2L + b2_2 * x2_2L - a1_2 * y1_2L - a2_2 * y2_2L) / a0_2;\n"
            "x2_2L = x1_2L; x1_2L = s1L; y2_2L = y1_2L; y1_2L = s2L;\n"
            "s3L = (b0_3 * s2L + b1_3 * x1_3L + b2_3 * x2_3L - a1_3 * y1_3L - a2_3 * y2_3L) / a0_3;\n"
            "x2_3L = x1_3L; x1_3L = s2L; y2_3L = y1_3L; y1_3L = s3L;\n"
            "spl0 = (b0_4 * s3L + b1_4 * x1_4L + b2_4 * x2_4L - a1_4 * y1_4L - a2_4 * y2_4L) / a0_4;\n"
            "x2_4L = x1_4L; x1_4L = s3L; y2_4L = y1_4L; y1_4L = spl0;\n"
            "\n"
            "// Canal Derecho en cascada\n"
            "s1R = (b0_1 * spl1 + b1_1 * x1_1R + b2_1 * x2_1R - a1_1 * y1_1R - a2_1 * y2_1R) / a0_1;\n"
            "x2_1R = x1_1R; x1_1R = spl1; y2_1R = y1_1R; y1_1R = s1R;\n"
            "s2R = (b0_2 * s1R + b1_2 * x1_2R + b2_2 * x2_2R - a1_2 * y1_2R - a2_2 * y2_2R) / a0_2;\n"
            "x2_2R = x1_2R; x1_2R = s1R; y2_2R = y1_2R; y1_2R = s2R;\n"
            "s3R = (b0_3 * s2R + b1_3 * x1_3R + b2_3 * x2_3R - a1_3 * y1_3R - a2_3 * y2_3R) / a0_3;\n"
            "x2_3R = x1_3R; x1_3R = s2R; y2_3R = y1_3R; y1_3R = s3R;\n"
            "spl1 = (b0_4 * s3R + b1_4 * x1_4R + b2_4 * x2_4R - a1_4 * y1_4R - a2_4 * y2_4R) / a0_4;\n"
            "x2_4R = x1_4R; x1_4R = s3R; y2_4R = y1_4R; y1_4R = spl1;\n"
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
