// Test unitario de los procesadores de efectos DSP en C++ (ACE EQ, ACE Compressor,
// ACE Delay, ACE Reverb) y de la cadena NovaDspChain.
#include "audio/dsp/NovaDspChain.h"
#include "audio/dsp/NovaBiquadEq.h"
#include "audio/dsp/NovaCompressor.h"
#include "audio/dsp/NovaStereoDelay.h"
#include "audio/dsp/NovaReverb.h"

#include <cmath>
#include <cstdio>
#include <vector>
#include <memory>

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
    std::printf("== Test de Procesadores DSP y Cadena de Efectos ==\n");

    const int kSampleRate = 48000;
    const int kNumFrames = 256;
    const int kChannels = 2;

    // 1. Test NovaDspChain y Bypass
    {
        NovaDspChain chain;
        auto eq = std::make_shared<NovaBiquadEq>();
        chain.addEffect(eq);

        CHECK_NAMED("chain_add_effect", chain.effectCount() == 1,
                    "la cadena debe registrar el efecto añadido");

        std::vector<float> buffer(kNumFrames * kChannels, 0.5f);
        std::vector<float> original = buffer;

        // Con EQ en 0 dB en todas las bandas, la señal pasa prácticamente inalterada
        chain.process(buffer.data(), kNumFrames, kChannels, kSampleRate);
        float diff = 0.0f;
        for (size_t i = 0; i < buffer.size(); ++i) {
            diff += std::abs(buffer[i] - original[i]);
        }
        CHECK_NAMED("eq_flat_transparency", diff < 0.01f,
                    "un ecualizador plano (0 dB) no debe distorsionar la señal");

        // Activar bypass explícito
        eq->setParameter(0, 12.0f); // +12 dB en graves
        eq->setBypassed(true);
        buffer = original;
        chain.process(buffer.data(), kNumFrames, kChannels, kSampleRate);
        diff = 0.0f;
        for (size_t i = 0; i < buffer.size(); ++i) {
            diff += std::abs(buffer[i] - original[i]);
        }
        CHECK_NAMED("effect_bypass_works", diff == 0.0f,
                    "un efecto en bypass debe ser 100% transparente");
    }

    // 2. Test ACE EQ (Boost y Cut)
    {
        NovaBiquadEq eq;
        eq.setParameter(0, 12.0f); // Boost de graves

        // Señal senoidal pura de baja frecuencia (80 Hz)
        std::vector<float> bufferLow(kNumFrames * kChannels);
        for (int i = 0; i < kNumFrames; ++i) {
            float s = 0.2f * std::sin(2.0f * 3.14159f * 80.0f * i / kSampleRate);
            bufferLow[i * 2] = s;
            bufferLow[i * 2 + 1] = s;
        }

        float maxOrig = 0.0f;
        for (float s : bufferLow) maxOrig = std::max(maxOrig, std::abs(s));

        eq.process(bufferLow.data(), kNumFrames, kChannels, kSampleRate);

        float maxBoosted = 0.0f;
        for (float s : bufferLow) maxBoosted = std::max(maxBoosted, std::abs(s));

        CHECK_NAMED("eq_low_shelf_boost", maxBoosted > maxOrig * 1.5f,
                    "el filtro Low Shelf a +12 dB debe incrementar la amplitud de 80 Hz");
    }

    // 3. Test ACE Compressor
    {
        NovaCompressor comp;
        comp.setParameter(0, -20.0f); // Umbral -20 dB
        comp.setParameter(1, 4.0f);   // Ratio 4:1
        comp.setParameter(2, 0.0f);   // 0 dB makeup

        // Señal fuerte de 0 dBFS (amplitud 1.0)
        std::vector<float> buffer(kNumFrames * 4 * kChannels, 1.0f);

        comp.process(buffer.data(), kNumFrames * 4, kChannels, kSampleRate);

        // Al final del bloque, el compresor debe haber atenuado la señal fuerte
        float lastSample = std::abs(buffer.back());
        CHECK_NAMED("compressor_gain_reduction", lastSample < 0.8f,
                    "el compresor debe reducir la ganancia de señales por encima del umbral");
    }

    // 4. Test ACE Delay
    {
        NovaStereoDelay delay;
        delay.setParameter(0, 50.0f);  // 50 ms
        delay.setParameter(1, 40.0f);  // 40% feedback
        delay.setParameter(2, 100.0f); // 100% wet

        // Pulso inicial unitario en frame 0, y silencio el resto
        const int frames = 4800; // 100 ms
        std::vector<float> buffer(frames * kChannels, 0.0f);
        buffer[0] = 1.0f;
        buffer[1] = 1.0f;

        delay.process(buffer.data(), frames, kChannels, kSampleRate);

        // A los 50 ms (frame 2400 aprox), debe aparecer el eco retardado
        int delayTargetFrame = static_cast<int>(0.050f * kSampleRate);
        float echoSample = std::abs(buffer[delayTargetFrame * 2]);
        CHECK_NAMED("delay_echo_reproduction", echoSample > 0.5f,
                    "el delay debe reproducir el eco en el tiempo de retardo especificado");
    }

    // 5. Test ACE Reverb
    {
        NovaReverb reverb;
        reverb.setParameter(0, 70.0f); // 70% room
        reverb.setParameter(1, 50.0f); // 50% wet

        // Pulso unitario seguido de silencio
        const int frames = 4800;
        std::vector<float> buffer(frames * kChannels, 0.0f);
        buffer[0] = 1.0f;
        buffer[1] = 1.0f;

        reverb.process(buffer.data(), frames, kChannels, kSampleRate);

        // La reverberación debe crear una cola con energía no nula a lo largo de los frames siguientes
        float tailEnergy = 0.0f;
        for (int i = 500; i < 2000; ++i) {
            tailEnergy += std::abs(buffer[i * 2]);
        }
        CHECK_NAMED("reverb_spatial_tail", tailEnergy > 0.1f,
                    "el reverb debe generar una cola de reflexión espacial tras el impulso");
    }

    std::printf("\nResultado: %d comprobaciones, %d fallos.\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
