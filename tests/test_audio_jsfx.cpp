// Test unitario del evaluador, parser y host de scripts JSFX de Cockos REAPER
#include "audio/dsp/jsfx/NovaJsfxVm.h"
#include "audio/dsp/jsfx/NovaJsfxEffect.h"
#include "audio/dsp/jsfx/NovaJsfxManager.h"

#include <cmath>
#include <cstdio>
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
            CHECK_NAMED("jsfx_ace_eq_sliders", aceEq->sliders().size() == 3, "ACE EQ debe exponer 3 bandas de sliders");
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
            std::vector<float> buffer(kNumFrames * kChannels, 0.5f);
            aceDelay->process(buffer.data(), kNumFrames, kChannels, kSampleRate);
            CHECK_NAMED("jsfx_ace_delay_runs", !std::isnan(buffer[0]), "ACE Delay debe procesar audio sin NaN");
        }

        auto aceReverb = mgr.createEffect(QStringLiteral("jsfx:builtin:ace_reverb"));
        CHECK_NAMED("jsfx_create_ace_reverb", aceReverb != nullptr, "debe instanciar ACE Reverb como efecto JSFX real");
        if (aceReverb) {
            std::vector<float> buffer(kNumFrames * kChannels, 0.5f);
            aceReverb->process(buffer.data(), kNumFrames, kChannels, kSampleRate);
            CHECK_NAMED("jsfx_ace_reverb_runs", !std::isnan(buffer[0]), "ACE Reverb debe procesar audio sin NaN");
        }
    }

    std::printf("\nResultado: %d comprobaciones, %d fallos.\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
