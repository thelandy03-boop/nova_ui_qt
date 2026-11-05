// Test unitario del evaluador, parser y host de scripts JSFX de Cockos REAPER
#include "audio/dsp/jsfx/NovaJsfxVm.h"
#include "audio/dsp/jsfx/NovaJsfxEffect.h"
#include "audio/dsp/jsfx/NovaJsfxManager.h"

#include <QFile>
#include <QRegularExpression>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK_NAMED(nombre, cond, msg)                                          \
    do {                                                                        \
        ++g_checks;                                                             \
        if (cond) {                                                             \
            std::printf("  [ok] %s\n", (nombre));                               \
        } else {                                                                \
            ++g_failures;                                                       \
            std::printf("  [FALLO] %s: %s  (%s:%d)\n", (nombre), (msg),         \
                        __FILE__, __LINE__);                                    \
        }                                                                       \
    } while (0)

struct PlantillaJsfx {
    QString nombre;
    QString codigo;
};

// JsfxTemplates.qml guarda cada script como concatenación de literales QML.
// Se deshacen los escapes (\n, \", \\) para quedarse con el texto real.
static QString desescaparQml(const QString &texto)
{
    QString salida;
    salida.reserve(texto.size());
    for (int i = 0; i < texto.size(); ++i) {
        const QChar c = texto.at(i);
        if (c != QLatin1Char('\\') || i + 1 >= texto.size()) {
            salida += c;
            continue;
        }
        const QChar siguiente = texto.at(++i);
        if (siguiente == QLatin1Char('n')) salida += QLatin1Char('\n');
        else if (siguiente == QLatin1Char('r')) salida += QLatin1Char('\r');
        else if (siguiente == QLatin1Char('t')) salida += QLatin1Char('\t');
        else salida += siguiente;
    }
    return salida;
}

static std::vector<PlantillaJsfx> leerPlantillasJsfx()
{
    std::vector<PlantillaJsfx> plantillas;

    QFile archivo(QStringLiteral(
        NOVA_SOURCE_DIR "/qml/skins/logic/dock/components/JsfxTemplates.qml"));
    if (!archivo.open(QIODevice::ReadOnly)) return plantillas;

    const QString qml = QString::fromUtf8(archivo.readAll());
    QRegularExpression propiedades(
        QStringLiteral("readonly\\s+property\\s+string\\s+(\\w+)\\s*:"));
    QRegularExpression literales(QStringLiteral("\"((?:[^\"\\\\]|\\\\.)*)\""));

    std::vector<QRegularExpressionMatch> matches;
    auto it = propiedades.globalMatch(qml);
    while (it.hasNext()) matches.push_back(it.next());

    for (size_t i = 0; i < matches.size(); ++i) {
        const int inicio = matches[i].capturedEnd();
        const int fin = (i + 1 < matches.size()) ? matches[i + 1].capturedStart()
                                                 : qml.size();
        const QString bloque = qml.mid(inicio, fin - inicio);

        PlantillaJsfx plantilla;
        plantilla.nombre = matches[i].captured(1);
        auto literal = literales.globalMatch(bloque);
        while (literal.hasNext()) {
            plantilla.codigo += desescaparQml(literal.next().captured(1));
        }
        if (!plantilla.codigo.isEmpty()) plantillas.push_back(std::move(plantilla));
    }

    return plantillas;
}

int main()
{
    std::printf("== Test de Máquina Virtual y Host de Scripts JSFX (REAPER) ==\n");

    const int kSampleRate = 48000;
    const int kNumFrames = 256;
    const int kChannels = 2;

    // 1. Test básico de NovaJsfxVm: Expresiones aritméticas y funciones
    {
        NovaJsfxVm vm;
        NovaJsfxVm::Program prog;
        std::string err;
        bool ok = vm.compile("x = 10 + 5 * 2; y = sin(0); z = 2 ^ 3; w = tanh(1.5);", prog, &err);
        CHECK_NAMED("vm_compile_basic", ok, "la compilación de expresiones aritméticas debe ser exitosa");

        vm.execute(prog);
        CHECK_NAMED("vm_precedence_arithmetic", std::abs(vm.getVariableByName("x") - 20.0) < 1e-6,
                    "10 + 5 * 2 debe ser igual a 20 (precedencia de multiplicación)");
        CHECK_NAMED("vm_math_func_sin", std::abs(vm.getVariableByName("y") - 0.0) < 1e-6,
                    "sin(0) debe ser 0.0");
        CHECK_NAMED("vm_power_operator", std::abs(vm.getVariableByName("z") - 8.0) < 1e-6,
                    "2 ^ 3 debe ser igual a 8.0");
        CHECK_NAMED("vm_tanh_func", vm.getVariableByName("w") > 0.9 && vm.getVariableByName("w") < 1.0,
                    "tanh(1.5) debe evaluar correctamente");
    }

    // 2. Test de Memoria RAM Virtual de JSFX
    {
        NovaJsfxVm vm;
        NovaJsfxVm::Program prog;
        bool ok = vm.compile("ptr = 100; ptr[0] = 42.5; val = ptr[0];", prog);
        CHECK_NAMED("vm_compile_mem", ok, "debe compilar acceso a memoria indexada");

        vm.execute(prog);
        CHECK_NAMED("vm_ram_read_write", std::abs(vm.getVariableByName("val") - 42.5) < 1e-6,
                    "la memoria RAM indexada ptr[0] debe persistir y leerse correctamente");
    }

    // 2b. Memoria con base entre paréntesis: (expr)[idx] para leer y escribir.
    // Es la forma que usan los delays y reverbs reales de JSFX.
    {
        NovaJsfxVm vm;
        NovaJsfxVm::Program prog;
        bool ok = vm.compile(
            "base = 1000; offset = 37;"
            "(base + offset)[5] = 12.25;"
            "leido = (base + offset)[5];"
            "otro = (base + offset + 1)[5];",
            prog);
        CHECK_NAMED("vm_compile_mem_parentesis", ok,
                    "debe compilar (expresión)[índice] para leer y escribir memoria");

        vm.execute(prog);
        CHECK_NAMED("vm_mem_parentesis_lectura",
                    std::abs(vm.getVariableByName("leido") - 12.25) < 1e-6,
                    "(base + offset)[5] debe leer el valor escrito en la misma dirección");
        CHECK_NAMED("vm_mem_parentesis_direccion",
                    std::abs(vm.getVariableByName("otro")) < 1e-9,
                    "(base + offset + 1)[5] debe apuntar a una dirección distinta");
    }

    // 3. Test de Script JSFX completo con Sliders y @sample
    {
        QString script = QStringLiteral(
            "desc: Simple JSFX Gain Controller\n"
            "slider1:6<-24,24,0.1>Ganancia (dB)\n"
            "slider2:50<0,100,1>Mix (%)\n"
            "\n"
            "@slider\n"
            "gain = 10^(slider1 / 20);\n"
            "\n"
            "@sample\n"
            "spl0 = spl0 * gain;\n"
            "spl1 = spl1 * gain;\n"
        );

        NovaJsfxEffect effect(script);
        CHECK_NAMED("jsfx_name_parsed", effect.name() == QStringLiteral("Simple JSFX Gain Controller"),
                    "el nombre del script debe extraerse de desc:");
        CHECK_NAMED("jsfx_sliders_count", effect.sliders().size() == 2,
                    "debe detectar exactamente 2 sliders");
        CHECK_NAMED("jsfx_slider1_meta", effect.sliders()[0].name == QStringLiteral("Ganancia (dB)")
                                         && effect.sliders()[0].defaultValue == 6.0f,
                    "metadatos del slider1 deben coincidir con la declaración");

        // Probar procesamiento de audio con ganancia de +6 dB
        std::vector<float> buffer(kNumFrames * kChannels, 0.1f);
        effect.process(buffer.data(), kNumFrames, kChannels, kSampleRate);

        // +6 dB es aproximadamente un factor de 2.0 (0.1 * 2.0 = ~0.2)
        CHECK_NAMED("jsfx_audio_processing_gain", buffer[0] > 0.19f && buffer[0] < 0.21f,
                    "las muestras deben ser multiplicadas por el factor calculado en @sample");

        // Cambiar parámetro slider1 a -6 dB (factor ~0.5)
        effect.setParameter(0, -6.0f);
        buffer.assign(kNumFrames * kChannels, 0.2f);
        effect.process(buffer.data(), kNumFrames, kChannels, kSampleRate);
        CHECK_NAMED("jsfx_slider_parameter_change", buffer[0] > 0.09f && buffer[0] < 0.11f,
                    "cambiar slider1 a -6 dB debe atenuar la señal a la mitad");
    }

    // 3b. Delay realista con memoria entre paréntesis: debe compilar y sonar.
    {
        QString script = QStringLiteral(
            "desc: Eco con Memoria\n"
            "slider1:10<10,100,1>Tiempo (ms)\n"
            "slider2:50<0,100,1>Mezcla (%)\n"
            "\n"
            "@init\n"
            "buf = 0;\n"
            "max_samples = 96000;\n"
            "\n"
            "@slider\n"
            "d = floor(slider1 * 0.001 * srate);\n"
            "d = min(max_samples - 1, max(10, d));\n"
            "wet = slider2 / 100;\n"
            "\n"
            "@sample\n"
            "read_pos = (buf - d + max_samples) % max_samples;\n"
            "delayed = (read_pos + 100000)[0];\n"
            "(buf + 100000)[0] = spl0 + delayed * 0.5;\n"
            "spl0 = spl0 + delayed * wet;\n"
            "spl1 = spl0;\n"
            "buf = (buf + 1) % max_samples;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        bool ok = effect.loadScript(script, &err);
        CHECK_NAMED("jsfx_delay_parentesis_compila", ok,
                    (err.isEmpty() ? QStringLiteral("loadScript() debe aceptar (expr)[idx]") : err).toUtf8().constData());

        std::vector<float> buffer(1100 * kChannels, 0.0f);
        buffer[0] = 1.0f;
        buffer[1] = 1.0f;
        effect.process(buffer.data(), 1100, kChannels, kSampleRate);

        CHECK_NAMED("jsfx_delay_primer_eco",
                    buffer[480 * kChannels] > 0.45f && buffer[480 * kChannels] < 0.55f,
                    "el primer eco debe aparecer a 480 muestras (10 ms a 48 kHz)");
        CHECK_NAMED("jsfx_delay_segundo_eco",
                    buffer[960 * kChannels] > 0.22f && buffer[960 * kChannels] < 0.28f,
                    "el feedback debe producir un segundo eco atenuado");
    }

    // 4. Test de Plugins JSFX integrados de NovaJsfxManager
    {
        auto& mgr = NovaJsfxManager::instance();
        auto plugins = mgr.availableJsfxPlugins();
        CHECK_NAMED("jsfx_builtin_plugins_count", plugins.size() >= 8,
                    "debe tener los 8 plugins de estudio reales integrados");

        auto saturator = mgr.createEffect(QStringLiteral("jsfx:builtin:tube_saturation"));
        CHECK_NAMED("jsfx_create_tube_saturator", saturator != nullptr,
                    "debe instanciar el Analog Tube Saturator");

        if (saturator) {
            std::vector<float> buffer(kNumFrames * kChannels, 0.5f);
            saturator->process(buffer.data(), kNumFrames, kChannels, kSampleRate);
            CHECK_NAMED("jsfx_tube_saturator_runs", buffer[0] > 0.0f,
                        "el saturador de tubos debe procesar audio sin colapsar");
        }

        auto aceEq = mgr.createEffect(QStringLiteral("jsfx:builtin:ace_eq"));
        CHECK_NAMED("jsfx_create_ace_eq", aceEq != nullptr, "debe instanciar ACE EQ como efecto JSFX real");
        if (aceEq) {
            CHECK_NAMED("jsfx_ace_eq_sliders", aceEq->sliders().size() == 12, "ACE EQ debe exponer 4 bandas paramétricas completas (12 sliders)");
            std::vector<float> buffer(kNumFrames * kChannels, 0.5f);
            aceEq->process(buffer.data(), kNumFrames, kChannels, kSampleRate);
            CHECK_NAMED("jsfx_ace_eq_runs", !std::isnan(buffer[0]), "ACE EQ debe procesar audio sin NaN");
        }

        auto aceComp = mgr.createEffect(QStringLiteral("jsfx:builtin:ace_compressor"));
        CHECK_NAMED("jsfx_create_ace_comp", aceComp != nullptr, "debe instanciar ACE Compressor como efecto JSFX real");
        if (aceComp) {
            std::vector<float> buffer(kNumFrames * kChannels, 0.8f);
            aceComp->process(buffer.data(), kNumFrames, kChannels, kSampleRate);
            CHECK_NAMED("jsfx_ace_comp_runs", !std::isnan(buffer[0]), "ACE Compressor debe procesar audio sin NaN");
        }

        auto aceDelay = mgr.createEffect(QStringLiteral("jsfx:builtin:ace_delay"));
        CHECK_NAMED("jsfx_create_ace_delay", aceDelay != nullptr, "debe instanciar ACE Delay como efecto JSFX real");
        if (aceDelay) {
            QString err;
            bool compila = aceDelay->loadScript(
                mgr.getScriptContent(QStringLiteral("jsfx:builtin:ace_delay")), &err);
            CHECK_NAMED("jsfx_ace_delay_compila", compila,
                        (err.isEmpty() ? QStringLiteral("ACE Delay debe compilar") : err).toUtf8().constData());

            aceDelay->setParameter(0, 10.0f); // 10 ms de retardo
            std::vector<float> buffer(1100 * kChannels, 0.0f);
            buffer[0] = 1.0f;
            buffer[1] = 1.0f;
            aceDelay->process(buffer.data(), 1100, kChannels, kSampleRate);
            CHECK_NAMED("jsfx_ace_delay_eco",
                        buffer[480 * kChannels] > 0.2f && buffer[480 * kChannels] < 0.3f,
                        "ACE Delay debe producir el eco a 10 ms con mezcla del 25%");
        }

        auto aceReverb = mgr.createEffect(QStringLiteral("jsfx:builtin:ace_reverb"));
        CHECK_NAMED("jsfx_create_ace_reverb", aceReverb != nullptr, "debe instanciar ACE Reverb como efecto JSFX real");
        if (aceReverb) {
            QString err;
            bool compila = aceReverb->loadScript(
                mgr.getScriptContent(QStringLiteral("jsfx:builtin:ace_reverb")), &err);
            CHECK_NAMED("jsfx_ace_reverb_compila", compila,
                        (err.isEmpty() ? QStringLiteral("ACE Reverb debe compilar") : err).toUtf8().constData());

            std::vector<float> buffer(kNumFrames * kChannels, 0.5f);
            aceReverb->process(buffer.data(), kNumFrames, kChannels, kSampleRate);
            CHECK_NAMED("jsfx_ace_reverb_runs", !std::isnan(buffer[0]), "ACE Reverb debe procesar audio sin NaN");
        }
    }

    // 5. El motor WDL ejecuta el lenguaje real: while, loop, funciones y this.
    {
        QString script = QStringLiteral(
            "desc: Pruebas de Lenguaje EEL2\n"
            "slider1:4<1,8,1>Iteraciones\n"
            "\n"
            "@init\n"
            "function doble(v) ( v * 2; );\n"
            "function acumula(v) ( this.s += v; this.s; );\n"
            "this.s = 0;\n"
            "\n"
            "@sample\n"
            "suma = 0; i = 0;\n"
            "while (i < slider1) ( suma += i; i += 1; );\n"
            "loop(2, suma += 10);\n"
            "d = doble(suma);\n"
            "a = acumula(1);\n"
            "b = acumula(2);\n"
            "spl0 = spl0 + a + b + (d == 52 ? 0 : 1000);\n"
            "spl1 = spl0;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        bool ok = effect.loadScript(script, &err);
        CHECK_NAMED("wdl_lenguaje_compila", ok,
                    (err.isEmpty() ? QStringLiteral("while/loop/funciones deben compilar") : err).toUtf8().constData());

        std::vector<float> buffer(4 * kChannels, 0.0f);
        effect.process(buffer.data(), 4, kChannels, kSampleRate);
        CHECK_NAMED("wdl_while_loop_funciones_this",
                    std::abs(buffer[0] - 4.0f) < 1e-4f,
                    "while=6, loop=26, doble=52 y this acumulado 1+3 deben dar 4");
    }

    // 6. Funciones de compatibilidad que WDL no trae de serie.
    {
        QString script = QStringLiteral(
            "desc: Funciones de Compatibilidad\n"
            "@sample\n"
            "spl0 = tanh(0) + log2(2) + exp2(0) + round(1.6) + trunc(1.9)"
            " + clamp(5, 0, 2) + lerp(0, 10, 0.5);\n"
            "spl1 = spl0;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        bool ok = effect.loadScript(script, &err);
        CHECK_NAMED("wdl_funciones_extra_compilan", ok,
                    (err.isEmpty() ? QStringLiteral("tanh/log2/exp2/round/trunc/clamp/lerp deben existir") : err).toUtf8().constData());

        std::vector<float> buffer(2 * kChannels, 0.0f);
        effect.process(buffer.data(), 2, kChannels, kSampleRate);
        CHECK_NAMED("wdl_funciones_extra_valores",
                    std::abs(buffer[0] - 12.0f) < 1e-4f,
                    "0+1+1+2+1+2+5 debe dar 12");
    }

    // 7. La cabecera se puede leer sin compilar el cuerpo (editor en vivo).
    {
        const QString roto = QStringLiteral(
            "desc: Metadatos sin compilar\n"
            "slider1:3<0,10,1>Ganancia\n"
            "slider2:-6<-60,12,0.5>Nivel (dB)\n"
            "@sample\n"
            "esto no es EEL2 valido !!!\n");

        NovaJsfxEffect::Metadata meta = NovaJsfxEffect::parseMetadata(roto);
        CHECK_NAMED("jsfx_metadata_sin_compilar", meta.name == QStringLiteral("Metadatos sin compilar")
                                                     && meta.sliders.size() == 2,
                    "desc y sliders deben leerse aunque el cuerpo no compile");
        CHECK_NAMED("jsfx_metadata_unidad", meta.sliders.size() == 2
                                                && meta.sliders[1].unit == QStringLiteral("dB"),
                    "la unidad debe deducirse del nombre del slider");

        NovaJsfxEffect effect;
        QString err;
        CHECK_NAMED("wdl_error_sintaxis", !effect.loadScript(roto, &err) && !err.isEmpty(),
                    "un cuerpo invalido debe devolver error de compilacion");
    }

    // 8. Un recompile fallido no puede dejar el efecto anterior a medias.
    {
        const QString bueno = QStringLiteral(
            "desc: Gain Estable\n"
            "slider1:1<0,2,0.1>Ganancia\n"
            "@sample\n"
            "spl0 = spl0 * slider1;\n"
            "spl1 = spl1 * slider1;\n");

        NovaJsfxEffect effect;
        QString err;
        bool ok = effect.loadScript(bueno, &err);
        CHECK_NAMED("wdl_recompila_base", ok,
                    (err.isEmpty() ? QStringLiteral("el script base debe compilar") : err).toUtf8().constData());

        std::vector<float> buffer(8 * kChannels, 0.25f);
        effect.process(buffer.data(), 8, kChannels, kSampleRate);
        const float antes = buffer[0];

        QString errMalo;
        bool malo = effect.recompile(QStringLiteral("desc: Roto\n@sample\nspl0 = spl0 * ;\n"), &errMalo);
        CHECK_NAMED("wdl_recompila_falla", !malo && !errMalo.isEmpty(),
                    "un recompile invalido debe devolver error");

        buffer.assign(8 * kChannels, 0.25f);
        effect.process(buffer.data(), 8, kChannels, kSampleRate);
        CHECK_NAMED("wdl_recompila_conserva_motor", std::abs(buffer[0] - antes) < 1e-6f,
                    "el motor anterior debe seguir procesando tras un recompile fallido");
    }

    // 9. @block recibe samplesblock y se ejecuta antes de @sample.
    {
        QString script = QStringLiteral(
            "desc: Bloque\n"
            "@block\n"
            "bloque_gain = (samplesblock == 64) ? 0.5 : 0;\n"
            "@sample\n"
            "spl0 = spl0 * bloque_gain;\n"
            "spl1 = spl1 * bloque_gain;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        bool ok = effect.loadScript(script, &err);
        CHECK_NAMED("wdl_block_compila", ok,
                    (err.isEmpty() ? QStringLiteral("@block debe compilar") : err).toUtf8().constData());

        std::vector<float> buffer(64 * kChannels, 1.0f);
        effect.process(buffer.data(), 64, kChannels, kSampleRate);
        CHECK_NAMED("wdl_block_samplesblock",
                    std::abs(buffer[0] - 0.5f) < 1e-6f,
                    "@block debe ver samplesblock=64 y aplicar 0.5");
    }

    // 10. @gfx y @serialize se siguen ignorando, como en la fachada anterior.
    {
        QString script = QStringLiteral(
            "desc: Con Gfx y Serialize\n"
            "@init\n"
            "x = 1;\n"
            "@gfx 100 100\n"
            "gfx_setpixel(0, 0, 1, 0, 0);\n"
            "@serialize\n"
            "file_var(0, x);\n"
            "@sample\n"
            "spl0 = spl0 * 0.5;\n"
            "spl1 = spl1 * 0.5;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        bool ok = effect.loadScript(script, &err);
        CHECK_NAMED("wdl_ignora_gfx_serialize", ok,
                    (err.isEmpty() ? QStringLiteral("@gfx y @serialize deben ignorarse") : err.toUtf8().constData()));

        std::vector<float> buffer(2 * kChannels, 1.0f);
        effect.process(buffer.data(), 2, kChannels, kSampleRate);
        CHECK_NAMED("wdl_audio_tras_gfx", std::abs(buffer[0] - 0.5f) < 1e-6f,
                    "@sample debe procesar aunque existan @gfx y @serialize");
    }

    // 11. El FFT de WDL queda registrado (el script compila y ejecuta).
    {
        QString script = QStringLiteral(
            "desc: FFT disponible\n"
            "@init\n"
            "buf = 0;\n"
            "memset(buf, 0, 64);\n"
            "buf[0] = 1;\n"
            "fft(buf, 16);\n"
            "ifft(buf, 16);\n"
            "valor = buf[0];\n"
            "@sample\n"
            "spl0 = spl0 + 0 * valor;\n"
            "spl1 = spl1 + 0 * valor;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        bool ok = effect.loadScript(script, &err);
        CHECK_NAMED("wdl_fft_registrado", ok,
                    (err.isEmpty() ? QStringLiteral("fft/ifft deben estar registradas") : err.toUtf8().constData()));

        std::vector<float> buffer(2 * kChannels, 0.5f);
        effect.process(buffer.data(), 2, kChannels, kSampleRate);
        CHECK_NAMED("wdl_fft_no_rompe_audio", std::isfinite(buffer[0])
                                                  && std::abs(buffer[0] - 0.5f) < 1e-6f,
                    "tras usar fft/ifft el audio debe seguir siendo finito y sin cambios");
    }

    // 12. srate se actualiza cuando el dispositivo cambia de tasa.
    {
        QString script = QStringLiteral(
            "desc: Tasa de Muestreo\n"
            "@slider\n"
            "gain = srate / 48000;\n"
            "@sample\n"
            "spl0 = spl0 * gain;\n"
            "spl1 = spl1 * gain;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        bool ok = effect.loadScript(script, &err);
        CHECK_NAMED("wdl_srate_compila", ok,
                    (err.isEmpty() ? QStringLiteral("srate debe compilar") : err.toUtf8().constData()));

        std::vector<float> buffer(4 * kChannels, 0.25f);
        effect.process(buffer.data(), 4, kChannels, 48000);
        CHECK_NAMED("wdl_srate_48k", std::abs(buffer[0] - 0.25f) < 1e-6f,
                    "a 48 kHz la ganancia debe ser 1");

        buffer.assign(4 * kChannels, 0.25f);
        effect.process(buffer.data(), 4, kChannels, 96000);
        CHECK_NAMED("wdl_srate_96k", std::abs(buffer[0] - 0.5f) < 1e-6f,
                    "a 96 kHz @slider debe recalcular gain=2");
    }

    // 13. Las plantillas del editor JSFX compilan con el motor WDL.
    {
        const auto plantillas = leerPlantillasJsfx();
        CHECK_NAMED("jsfx_plantillas_qml_encontradas", plantillas.size() == 8,
                    "deben encontrarse las 8 plantillas del editor JSFX");

        for (const PlantillaJsfx &plantilla : plantillas) {
            NovaJsfxEffect effect;
            QString err;
            const bool ok = effect.loadScript(plantilla.codigo, &err);

            const std::string nombre =
                "jsfx_plantilla_" + plantilla.nombre.toStdString();
            const QByteArray errUtf8 = err.toUtf8();
            const char *mensaje = err.isEmpty()
                                      ? "la plantilla debe compilar con WDL"
                                      : errUtf8.constData();
            CHECK_NAMED(nombre.c_str(), ok, mensaje);
        }
    }

    std::printf("\nResultado: %d comprobaciones, %d fallos.\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
