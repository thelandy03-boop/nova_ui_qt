// Test del calculo de picos: los extremos deben ser exactos.
//
// Contexto: el llamante inicializaba los acumuladores de minimo y maximo a
// 0. El audio es bipolar, asi que 0 no es un extremo valido:
//   - un bin solo-negativo (p.ej. una nota grave sostenida) tenia su MAXIMO
//     clavado en 0 en vez del valor real negativo;
//   - un bin solo-positivo (p.ej. un silencio con ruido de fondo) tenia su
//     MINIMO clavado en 0 en vez del valor real positivo.
// El resultado eran picos con la forma equivocada: la onda se dibujaba
// como un bloque solido contra el eje en vez de la envolvente real.
#include "core/NovaSimdPeaks.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::printf("  FALLO: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);  \
        }                                                                      \
    } while (0)

int main()
{
    std::printf("== Calculo de picos (SIMD) ==\n");

    const int points = 16;
    const int overviewPoints = 4;
    const double detailStep = 64.0;
    const double overviewStep = 256.0;

    // ------------------------------------------------------------------
    // 1. Senal solo negativa: minimo y maximo negativos y exactos
    // ------------------------------------------------------------------
    {
        std::vector<float> samples(1024);
        for (size_t i = 0; i < samples.size(); ++i) {
            samples[i] = -0.5f - (0.25f * static_cast<float>(i % 16) / 16.0f);
        }

        std::vector<float> dMin, dMax, dRms, oMin, oMax, oRms;
        std::vector<int> dCount, oCount;
        initPeakAccumulators(dMin, dMax, dRms, dCount, points);
        initPeakAccumulators(oMin, oMax, oRms, oCount, overviewPoints);

        processPeakBlockSimd(samples.data(), samples.size(),
                             0.0, detailStep, overviewStep,
                             points, overviewPoints,
                             dMin, dMax, dRms, dCount,
                             oMin, oMax, oRms, oCount);

        CHECK(dCount[0] > 0, "deben contarse las muestras del bin");
        CHECK(dMin[0] < -0.5f, "el minimo debe ser el valor mas negativo de la senal");
        CHECK(dMax[0] < 0.0f,
              "el maximo de una senal negativa NO puede ser 0 (fallo del inicializado a 0)");
        CHECK(std::fabs(dMax[0] - (-0.5f)) < 0.001f,
              "el maximo debe ser el valor mas alto (menos negativo) de la senal");
        CHECK(std::fabs(dMin[0] - (-0.734375f)) < 0.001f,
              "el minimo debe ser exacto");
    }

    // ------------------------------------------------------------------
    // 2. Senal solo positiva: minimo y maximo positivos y exactos
    // ------------------------------------------------------------------
    {
        std::vector<float> samples(1024, 0.75f);

        std::vector<float> dMin, dMax, dRms, oMin, oMax, oRms;
        std::vector<int> dCount, oCount;
        initPeakAccumulators(dMin, dMax, dRms, dCount, points);
        initPeakAccumulators(oMin, oMax, oRms, oCount, overviewPoints);

        processPeakBlockSimd(samples.data(), samples.size(),
                             0.0, detailStep, overviewStep,
                             points, overviewPoints,
                             dMin, dMax, dRms, dCount,
                             oMin, oMax, oRms, oCount);

        CHECK(std::fabs(dMax[0] - 0.75f) < 0.001f, "el maximo debe ser 0.75");
        CHECK(std::fabs(dMin[0] - 0.75f) < 0.001f,
              "el minimo de una senal positiva NO puede ser 0 (fallo del inicializado a 0)");
    }

    // ------------------------------------------------------------------
    // 3. Senal bipolar: ambos extremos correctos
    // ------------------------------------------------------------------
    {
        std::vector<float> samples(1024);
        for (size_t i = 0; i < samples.size(); ++i) {
            samples[i] = (i % 2 == 0) ? 0.9f : -0.4f;
        }

        std::vector<float> dMin, dMax, dRms, oMin, oMax, oRms;
        std::vector<int> dCount, oCount;
        initPeakAccumulators(dMin, dMax, dRms, dCount, points);
        initPeakAccumulators(oMin, oMax, oRms, oCount, overviewPoints);

        processPeakBlockSimd(samples.data(), samples.size(),
                             0.0, detailStep, overviewStep,
                             points, overviewPoints,
                             dMin, dMax, dRms, dCount,
                             oMin, oMax, oRms, oCount);

        CHECK(std::fabs(dMax[0] - 0.9f) < 0.001f, "el maximo debe ser 0.9");
        CHECK(std::fabs(dMin[0] + 0.4f) < 0.001f, "el minimo debe ser -0.4");
    }

    // ------------------------------------------------------------------
    // 4. Reproduccion del fallo: con la inicializacion antigua (a 0) los
    //    extremos se clavan, que es exactamente lo que pasaba
    // ------------------------------------------------------------------
    {
        std::vector<float> samples(1024, 0.75f);
        std::vector<float> dMin(points, 0.0f), dMax(points, 0.0f), dRms(points, 0.0f);
        std::vector<float> oMin(overviewPoints, 0.0f), oMax(overviewPoints, 0.0f), oRms(overviewPoints, 0.0f);
        std::vector<int> dCount(points, 0), oCount(overviewPoints, 0);

        processPeakBlockSimd(samples.data(), samples.size(),
                             0.0, detailStep, overviewStep,
                             points, overviewPoints,
                             dMin, dMax, dRms, dCount,
                             oMin, oMax, oRms, oCount);

        CHECK(dMin[0] == 0.0f,
              "reproduccion del fallo: inicializando a 0 el minimo de una senal positiva se clava en 0");
    }

    std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
