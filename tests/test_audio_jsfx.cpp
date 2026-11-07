// Test unitario del evaluador, parser y host de scripts JSFX de Cockos REAPER
#include "audio/dsp/jsfx/NovaJsfxVm.h"
#include "audio/dsp/jsfx/NovaJsfxEffect.h"
#include "audio/dsp/jsfx/NovaJsfxManager.h"

#include <QFile>
#include <QRegularExpression>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

// Los blobs de @serialize guardan float32 little-endian, igual que REAPER.
static float leerFloatLe(const QByteArray &datos, int desplazamiento)
{
    if (desplazamiento < 0 || desplazamiento + 4 > datos.size()) return 0.0f;

    const std::uint32_t bits =
        static_cast<std::uint32_t>(static_cast<unsigned char>(datos[desplazamiento]))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(datos[desplazamiento + 1])) << 8)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(datos[desplazamiento + 2])) << 16)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(datos[desplazamiento + 3])) << 24);

    float valor = 0.0f;
    std::memcpy(&valor, &bits, sizeof(valor));
    return valor;
}

static void escribirFloatLe(QByteArray &datos, int desplazamiento, float valor)
{
    if (desplazamiento < 0 || desplazamiento + 4 > datos.size()) return;

    std::uint32_t bits = 0;
    std::memcpy(&bits, &valor, sizeof(bits));
    datos[desplazamiento] = static_cast<char>(bits & 0xffu);
    datos[desplazamiento + 1] = static_cast<char>((bits >> 8) & 0xffu);
    datos[desplazamiento + 2] = static_cast<char>((bits >> 16) & 0xffu);
    datos[desplazamiento + 3] = static_cast<char>((bits >> 24) & 0xffu);
}

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

    // 10. @gfx se ignora y @serialize se compila sin ejecutarse en audio.
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
        CHECK_NAMED("wdl_gfx_ignorado_serialize_compila", ok,
                    (err.isEmpty() ? QStringLiteral("@gfx debe ignorarse y @serialize compilar") : err.toUtf8().constData()));
        CHECK_NAMED("wdl_serialize_detectado", effect.hasSerialize(),
                    "el efecto debe saber que el script tiene @serialize");

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

    // 14. El transporte real de la sesion llega a tempo, play_position,
    //     beat_position y play_state, tanto en @slider como en @sample.
    {
        const QString script = QStringLiteral(
            "desc: Transporte\n"
            "slider1:0<0,1,0.01>Ganancia\n"
            "@slider\n"
            "transporte = tempo + play_position + beat_position + play_state;\n"
            "@sample\n"
            "spl0 = transporte;\n"
            "spl1 = transporte;\n"
        );

        NovaJsfxEffect effect;
        // 90 BPM, 2 s de reproduccion: beat_position = 3, play_state = 1.
        effect.setTransport(2.0, 90.0, true);

        QString err;
        const bool ok = effect.loadScript(script, &err);
        CHECK_NAMED("transporte_compila", ok,
                    (err.isEmpty() ? QStringLiteral("el script debe compilar")
                                   : err.toUtf8().constData()));

        std::vector<float> buffer(4 * kChannels, 0.0f);
        effect.process(buffer.data(), 4, kChannels, 48000);
        CHECK_NAMED("transporte_tempo_posicion_estado",
                    std::abs(buffer[0] - 96.0f) < 1e-4f,
                    "90 BPM + 2 s + 3 beats + reproduciendo debe dar 96");

        // Cambiar el transporte y forzar @slider: 120 BPM, 4 s, parado.
        effect.setTransport(4.0, 120.0, false);
        effect.setParameter(0, 0.5f);
        buffer.assign(4 * kChannels, 0.0f);
        effect.process(buffer.data(), 4, kChannels, 48000);
        CHECK_NAMED("transporte_actualiza_slider",
                    std::abs(buffer[0] - 132.0f) < 1e-4f,
                    "120 BPM + 4 s + 8 beats + parado debe dar 132");

        // Sin sesion se conserva el comportamiento historico: 120 BPM fijos.
        NovaJsfxEffect sinTransporte;
        sinTransporte.loadScript(script, &err);
        buffer.assign(4 * kChannels, 0.0f);
        sinTransporte.process(buffer.data(), 4, kChannels, 48000);
        CHECK_NAMED("transporte_sin_sesion_120",
                    std::abs(buffer[0] - 120.0f) < 1e-4f,
                    "sin transporte el tempo por defecto debe seguir siendo 120");
    }

    // 15. @serialize guarda y restaura variables y RAM con file_var/file_mem.
    //     Los valores viajan como float32 little-endian, como en REAPER.
    {
        const QString script = QStringLiteral(
            "desc: Estado interno\n"
            "@init\n"
            "estado = 7;\n"
            "ram = 0;\n"
            "ram[0] = 11;\n"
            "ram[1] = 22;\n"
            "@serialize\n"
            "leyendo = file_avail(0) >= 0;\n"
            "file_var(0, estado);\n"
            "file_mem(0, ram, 2);\n"
            "@sample\n"
            "spl0 = estado + ram[0] + ram[1] + leyendo;\n"
            "spl1 = spl0;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        const bool ok = effect.loadScript(script, &err);
        CHECK_NAMED("serialize_compila", ok,
                    (err.isEmpty() ? QStringLiteral("el script con @serialize debe compilar")
                                   : err.toUtf8().constData()));

        const QByteArray estado = effect.saveState();
        CHECK_NAMED("serialize_guarda_blob", estado.size() == 12,
                    "debe guardar 3 valores de 32 bits (estado + 2 de RAM)");
        CHECK_NAMED("serialize_guarda_variable",
                    std::abs(leerFloatLe(estado, 0) - 7.0f) < 1e-6f,
                    "file_var debe guardar el valor de la variable");
        CHECK_NAMED("serialize_guarda_memoria",
                    std::abs(leerFloatLe(estado, 4) - 11.0f) < 1e-6f
                        && std::abs(leerFloatLe(estado, 8) - 22.0f) < 1e-6f,
                    "file_mem debe guardar los dos valores de RAM");
        CHECK_NAMED("serialize_guardado_repetible", effect.saveState() == estado,
                    "dos guardados seguidos deben producir el mismo blob");

        QByteArray restaurado = estado;
        escribirFloatLe(restaurado, 0, 99.0f);
        escribirFloatLe(restaurado, 4, 33.0f);
        escribirFloatLe(restaurado, 8, 44.0f);
        CHECK_NAMED("serialize_carga_blob", effect.loadState(restaurado),
                    "loadState debe aceptar un estado valido");

        std::vector<float> buffer(2 * kChannels, 0.0f);
        effect.process(buffer.data(), 2, kChannels, kSampleRate);
        CHECK_NAMED("serialize_restaura_estado",
                    std::abs(buffer[0] - 177.0f) < 1e-4f,
                    "debe leer 99 + 33 + 44 + 1 (modo lectura)");
    }

    // 16. file_rewind(0) vuelve al principio del estado en modo lectura.
    {
        const QString script = QStringLiteral(
            "desc: Rebobinar\n"
            "@serialize\n"
            "file_var(0, a);\n"
            "file_rewind(0);\n"
            "file_var(0, b);\n"
            "@sample\n"
            "spl0 = a + b;\n"
            "spl1 = spl0;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        CHECK_NAMED("serialize_rewind_compila", effect.loadScript(script, &err), err.toUtf8().constData());

        QByteArray estado(4, '\0');
        escribirFloatLe(estado, 0, 5.0f);
        CHECK_NAMED("serialize_rewind_carga", effect.loadState(estado),
                    "el estado de 4 bytes debe cargarse");

        std::vector<float> buffer(2 * kChannels, 0.0f);
        effect.process(buffer.data(), 2, kChannels, kSampleRate);
        CHECK_NAMED("serialize_rewind_relee",
                    std::abs(buffer[0] - 10.0f) < 1e-4f,
                    "tras file_rewind debe volver a leer el primer valor");
    }

    // 17. Los sliders no entran en el blob: se guardan aparte.
    {
        const QString script = QStringLiteral(
            "desc: Sliders aparte\n"
            "slider1:2<0,10,0.1>Ganancia\n"
            "@init\n"
            "interno = 42;\n"
            "@serialize\n"
            "file_var(0, interno);\n"
            "@sample\n"
            "spl0 = slider1 + interno;\n"
            "spl1 = spl0;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        CHECK_NAMED("serialize_slider_compila", effect.loadScript(script, &err), err.toUtf8().constData());

        const QByteArray estado = effect.saveState();
        CHECK_NAMED("serialize_no_guarda_sliders", estado.size() == 4,
                    "el blob solo debe contener el estado interno");
        CHECK_NAMED("serialize_slider_intacto",
                    std::abs(effect.getParameter(0) - 2.0f) < 1e-6f,
                    "guardar el estado no debe tocar el slider");
    }

    // 18. Un script sin @serialize no produce estado y rechaza cargas.
    {
        const QString script = QStringLiteral(
            "desc: Sin estado\n"
            "slider1:1<0,10,0.1>Ganancia\n"
            "@sample\n"
            "spl0 = spl0 * slider1;\n"
            "spl1 = spl1 * slider1;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        CHECK_NAMED("serialize_ausente_compila", effect.loadScript(script, &err), err.toUtf8().constData());
        CHECK_NAMED("serialize_ausente_vacio", !effect.hasSerialize() && effect.saveState().isEmpty(),
                    "sin @serialize no debe haber blob");

        QByteArray estado(4, '\0');
        escribirFloatLe(estado, 0, 1.0f);
        CHECK_NAMED("serialize_ausente_rechaza_carga", !effect.loadState(estado),
                    "sin @serialize loadState debe devolver false");
    }

    // 19. Un @serialize no vacio impide que @init borre la RAM; sin el,
    //     el reset historico sigue vaciandola.
    {
        const QString conSerialize = QStringLiteral(
            "desc: Conserva RAM\n"
            "@init\n"
            "ram = 0;\n"
            "ram[0] = ram[0] + 1;\n"
            "@serialize\n"
            "file_var(0, marcador);\n"
            "@sample\n"
            "spl0 = ram[0];\n"
            "spl1 = spl0;\n"
        );
        const QString sinSerialize = QStringLiteral(
            "desc: Borra RAM\n"
            "@init\n"
            "ram = 0;\n"
            "ram[0] = ram[0] + 1;\n"
            "@sample\n"
            "spl0 = ram[0];\n"
            "spl1 = spl0;\n"
        );

        auto valorTrasProcesar = [](NovaJsfxEffect &effect) {
            std::vector<float> buffer(2 * kChannels, 0.0f);
            effect.process(buffer.data(), 2, kChannels, kSampleRate);
            return buffer[0];
        };

        QString err;
        NovaJsfxEffect conEstado;
        CHECK_NAMED("serialize_ram_compila", conEstado.loadScript(conSerialize, &err), err.toUtf8().constData());
        const float primeroCon = valorTrasProcesar(conEstado);
        conEstado.reset();
        const float segundoCon = valorTrasProcesar(conEstado);
        CHECK_NAMED("serialize_conserva_ram",
                    std::abs(segundoCon - (primeroCon + 1.0f)) < 1e-6f,
                    "@init debe sumar sin borrar la RAM cuando hay @serialize");

        NovaJsfxEffect sinEstado;
        CHECK_NAMED("serialize_control_compila", sinEstado.loadScript(sinSerialize, &err), err.toUtf8().constData());
        const float primeroSin = valorTrasProcesar(sinEstado);
        sinEstado.reset();
        const float segundoSin = valorTrasProcesar(sinEstado);
        CHECK_NAMED("serialize_sin_estado_borra_ram",
                    std::abs(primeroSin - 1.0f) < 1e-6f
                        && std::abs(segundoSin - 1.0f) < 1e-6f,
                    "sin @serialize el reset debe vaciar la RAM y dejarla en 1");
    }

    // 20. Soporte de capa grafica (@gfx): compilacion, dimensiones, renderizado de pixeles.
    {
        const QString scriptGfx = QStringLiteral(
            "desc: Test Grafico\n"
            "@init\n"
            "color = 1.0;\n"
            "@gfx 320 240\n"
            "gfx_r = color;\n"
            "gfx_g = 0.5;\n"
            "gfx_b = 0.25;\n"
            "gfx_rect(0, 0, 10, 10, 1);\n"
            "@sample\n"
            "spl0 = spl0;\n"
        );

        NovaJsfxEffect effect;
        QString err;
        CHECK_NAMED("gfx_compila", effect.loadScript(scriptGfx, &err), err.toUtf8().constData());
        CHECK_NAMED("gfx_detectado", effect.hasGfx(), "@gfx debe ser detectado");

        int ancho = 0, alto = 0;
        bool tieneDim = effect.getGfxDimensions(ancho, alto);
        CHECK_NAMED("gfx_dimensiones", tieneDim && ancho == 320 && alto == 240,
                    "debe devolver las dimensiones 320x240");

        // Preparar buffer de pixeles (BGRA 320x240)
        std::vector<uint8_t> pixeles(320 * 240 * 4, 0);
        effect.setupGfx(pixeles.data(), 320, 240, 320 * 4, 1.0);
        bool renderOk = effect.renderGfx();
        CHECK_NAMED("gfx_renderiza", renderOk, "debe renderizar el frame grafico");

        // Al dibujar un rectangulo en 0,0,10,10, los primeros pixeles no deben ser negros
        uint32_t primerPixel = *reinterpret_cast<const uint32_t*>(pixeles.data());
        CHECK_NAMED("gfx_pixeles_dibujados", primerPixel != 0,
                    "el rectangulo dibujado debe modificar el buffer de pixeles");
    }

    // 21. Soporte de eventos MIDI (midirecv, midisend y encadenamiento)
    {
        // 21.1 midirecv: sintetizador que recibe notas MIDI y produce audio
        const QString scriptSinte = QStringLiteral(
            "desc: MIDI Synth Receiver\n"
            "in_pin: left input\n"
            "in_pin: right input\n"
            "out_pin: left output\n"
            "out_pin: right output\n"
            "@init\n"
            "freq = 0.0;\n"
            "gain = 0.0;\n"
            "phase = 0.0;\n"
            "@block\n"
            "while (midirecv(offset, msg1, msg2, msg3)) (\n"
            "  status = msg1 & 240;\n"
            "  (status == 144 && msg3 > 0) ? (\n"
            "    freq = 440.0 * 2.0 ^ ((msg2 - 69.0) / 12.0);\n"
            "    gain = msg3 / 127.0;\n"
            "  );\n"
            "  (status == 128 || (status == 144 && msg3 == 0)) ? (\n"
            "    gain = 0.0;\n"
            "  );\n"
            ");\n"
            "@sample\n"
            "gain > 0.0 ? (\n"
            "  s = sin(phase) * gain;\n"
            "  phase += 2.0 * 3.141592653589793 * freq / srate;\n"
            "  spl0 = s;\n"
            "  spl1 = s;\n"
            ") : (\n"
            "  spl0 = 0.0;\n"
            "  spl1 = 0.0;\n"
            ");\n"
        );

        NovaJsfxEffect sinte;
        QString err;
        CHECK_NAMED("midi_sinte_compila", sinte.loadScript(scriptSinte, &err), err.toUtf8().constData());

        // Antes de recibir nota, con buffer de silencio, la salida debe ser silencio
        std::vector<float> audioBuf(128, 0.0f);
        sinte.process(audioBuf.data(), 64, 2, 48000);
        float picoInicial = 0.0f;
        for (float s : audioBuf) picoInicial = std::max(picoInicial, std::abs(s));
        CHECK_NAMED("midi_silencio_inicial", picoInicial < 1e-6f, "sin nota debe salir silencio");

        // Enviar Note On (A4, 440Hz, vel 100)
        sinte.sendMidiNoteOn(0, 69, 100, 0);
        std::fill(audioBuf.begin(), audioBuf.end(), 0.0f);
        sinte.process(audioBuf.data(), 64, 2, 48000);

        float picoSonido = 0.0f;
        for (float s : audioBuf) picoSonido = std::max(picoSonido, std::abs(s));
        CHECK_NAMED("midi_nota_on_produce_audio", picoSonido > 0.1f,
                    "al recibir Note On el script debe sintetizar senoidal");

        // Enviar Note Off
        sinte.sendMidiNoteOff(0, 69, 0, 0);
        std::fill(audioBuf.begin(), audioBuf.end(), 0.0f);
        sinte.process(audioBuf.data(), 64, 2, 48000);
        float picoApagado = 0.0f;
        for (float s : audioBuf) picoApagado = std::max(picoApagado, std::abs(s));
        CHECK_NAMED("midi_nota_off_silencia", picoApagado < 1e-6f,
                    "al recibir Note Off debe volver al silencio");

        // 21.2 midisend: procesador que transpone notas MIDI (+12 semitonos)
        const QString scriptTransp = QStringLiteral(
            "desc: MIDI Transposer\n"
            "@block\n"
            "while (midirecv(offset, msg1, msg2, msg3)) (\n"
            "  status = msg1 & 240;\n"
            "  (status == 144 || status == 128) ? (\n"
            "    midisend(offset, msg1, msg2 + 12, msg3);\n"
            "  ) : (\n"
            "    midisend(offset, msg1, msg2, msg3);\n"
            "  );\n"
            ");\n"
        );

        NovaJsfxEffect transposer;
        CHECK_NAMED("midi_transp_compila", transposer.loadScript(scriptTransp, &err), err.toUtf8().constData());

        // Inyectamos Note On (canal 0, nota 60 = C4, vel 90) en frame offset 10
        std::vector<NovaMidiEvent> midiIn = { NovaMidiEvent::noteOn(0, 60, 90, 10) };
        std::vector<NovaMidiEvent> midiOut;
        std::vector<float> dummyBuf(64, 0.0f);
        transposer.processWithMidi(dummyBuf.data(), 32, 2, 48000, midiIn, midiOut);

        CHECK_NAMED("midi_salida_generada", midiOut.size() == 1, "el script debio emitir un evento con midisend");
        if (midiOut.size() == 1) {
            const auto &ev = midiOut[0];
            CHECK_NAMED("midi_offset_preservado", ev.offset == 10, "offset temporal preservado");
            CHECK_NAMED("midi_status_note_on", ev.data.size() >= 3 && ev.data[0] == 0x90, "debe ser Note On");
            CHECK_NAMED("midi_nota_transpuesta", ev.data.size() >= 3 && ev.data[1] == 72,
                        "la nota 60 transpuesta +12 debe ser 72");
            CHECK_NAMED("midi_velocidad_preservada", ev.data.size() >= 3 && ev.data[2] == 90,
                        "la velocidad original de 90 debe preservarse");
        }
    }

    std::printf("\nResultado: %d comprobaciones, %d fallos.\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
