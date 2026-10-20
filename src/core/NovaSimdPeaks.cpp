#include "NovaSimdPeaks.h"
#include <cfloat>
#include <cstring>

#ifdef __AVX2__
    #include <immintrin.h>
#elif defined(__ARM_NEON)
    #include <arm_neon.h>
#endif

void initPeakAccumulators(std::vector<float>& min, std::vector<float>& max,
                          std::vector<float>& rms, std::vector<int>& count, int points)
{
    if (points <= 0) {
        return;
    }
    min.assign(static_cast<size_t>(points), FLT_MAX);
    max.assign(static_cast<size_t>(points), -FLT_MAX);
    rms.assign(static_cast<size_t>(points), 0.0f);
    count.assign(static_cast<size_t>(points), 0);
}

void processPeakBlockSimd(const float* samples, size_t numSamples,
                          double startPos, double detailStep, double overviewStep,
                          int detailPoints, int overviewPoints,
                          std::vector<float>& detailMin, std::vector<float>& detailMax,
                          std::vector<float>& detailRms, std::vector<int>& detailCount,
                          std::vector<float>& overviewMin, std::vector<float>& overviewMax,
                          std::vector<float>& overviewRms, std::vector<int>& overviewCount)
{
    size_t i = 0;
    while (i < numSamples) {
        double pos = startPos + i;
        int dIdx = static_cast<int>(pos / detailStep);
        int oIdx = static_cast<int>(pos / overviewStep);
        if (dIdx < 0) dIdx = 0;
        if (dIdx >= detailPoints) dIdx = detailPoints - 1;
        if (oIdx < 0) oIdx = 0;
        if (oIdx >= overviewPoints) oIdx = overviewPoints - 1;

        size_t j = 0;
        while (i + j < numSamples) {
            double posj = startPos + i + j;
            int dIdxj = static_cast<int>(posj / detailStep);
            int oIdxj = static_cast<int>(posj / overviewStep);
            if (dIdxj != dIdx || oIdxj != oIdx) {
                break;
            }
            j++;
        }

        // Process j samples to get min, max, sumSq
        float chunkMin = FLT_MAX;
        float chunkMax = -FLT_MAX;
        float chunkSumSq = 0.0f;

        // Determine vector width
        const size_t vecSize = 
#ifdef __AVX2__
            8;
#elif defined(__ARM_NEON)
            4;
#else
            1;
#endif

        size_t k = 0;
        // Process main SIMD chunks
        while (k + vecSize <= j) {
#ifdef __AVX2__
            __m256 v = _mm256_loadu_ps(&samples[i + k]);
            __m256 vmin = _mm256_set1_ps(chunkMin);
            __m256 vmax = _mm256_set1_ps(chunkMax);
            vmin = _mm256_min_ps(vmin, v);
            vmax = _mm256_max_ps(vmax, v);
            // Horizontal min/max
            __m128 vmin128 = _mm256_castps256_ps128(vmin);
            __m128 vmin128_hi = _mm256_extractf128_ps(vmin, 1);
            vmin128 = _mm_min_ps(vmin128, vmin128_hi);
            vmin128 = _mm_min_ps(vmin128, _mm_movehl_ps(vmin128, vmin128));
            vmin128 = _mm_min_ss(vmin128, _mm_shuffle_ps(vmin128, vmin128, 1));
            chunkMin = _mm_cvtss_f32(vmin128);
            __m128 vmax128 = _mm256_castps256_ps128(vmax);
            __m128 vmax128_hi = _mm256_extractf128_ps(vmax, 1);
            vmax128 = _mm_max_ps(vmax128, vmax128_hi);
            vmax128 = _mm_max_ps(vmax128, _mm_movehl_ps(vmax128, vmax128));
            vmax128 = _mm_max_ss(vmax128, _mm_shuffle_ps(vmax128, vmax128, 1));
            chunkMax = _mm_cvtss_f32(vmax128);
            // Sum of squares
            __m256 vsq = _mm256_mul_ps(v, v);
            __m256 vsum = _mm256_add_ps(_mm256_setzero_ps(), vsq);
            __m128 vsum128 = _mm256_castps256_ps128(vsum);
            __m128 vsum128_hi = _mm256_extractf128_ps(vsum, 1);
            vsum128 = _mm_add_ps(vsum128, vsum128_hi);
            vsum128 = _mm_add_ps(vsum128, _mm_movehl_ps(vsum128, vsum128));
            vsum128 = _mm_add_ss(vsum128, _mm_shuffle_ps(vsum128, vsum128, 1));
            chunkSumSq += _mm_cvtss_f32(vsum128);
#elif defined(__ARM_NEON)
            float32x4_t v = vld1q_f32(&samples[i + k]);
            float32x4_t vmin = vdupq_n_f32(chunkMin);
            float32x4_t vmax = vdupq_n_f32(chunkMax);
            vmin = vminq_f32(vmin, v);
            vmax = vmaxq_f32(vmax, v);
            // Horizontal min
            float32x2_t vmin64 = vpmin_f32(vget_low_f32(vmin), vget_high_f32(vmin));
            chunkMin = vget_lane_f32(vpmin_f32(vmin64, vmin64), 0);
            // Horizontal max
            float32x2_t vmax64 = vpmax_f32(vget_low_f32(vmax), vget_high_f32(vmax));
            chunkMax = vget_lane_f32(vpmax_f32(vmax64, vmax64), 0);
            // Sum of squares
            float32x4_t vsq = vmulq_f32(v, v);
            float32x2_t vsum64 = vadd_f32(vget_low_f32(vsq), vget_high_f32(vsq));
            chunkSumSq += vget_lane_f32(vsum64, 0) + vget_lane_f32(vsum64, 1);
#else
            for (size_t x = 0; x < vecSize; ++x) {
                float s = samples[i + k + x];
                if (s < chunkMin) chunkMin = s;
                if (s > chunkMax) chunkMax = s;
                chunkSumSq += s * s;
            }
#endif
            k += vecSize;
        }
        // Process remainder
        for (size_t x = k; x < j; ++x) {
            float s = samples[i + x];
            if (s < chunkMin) chunkMin = s;
            if (s > chunkMax) chunkMax = s;
            chunkSumSq += s * s;
        }

        // Update detail bin
        if (chunkMin < detailMin[dIdx]) detailMin[dIdx] = chunkMin;
        if (chunkMax > detailMax[dIdx]) detailMax[dIdx] = chunkMax;
        detailRms[dIdx] += chunkSumSq;
        detailCount[dIdx] += static_cast<int>(j);

        // Update overview bin
        if (chunkMin < overviewMin[oIdx]) overviewMin[oIdx] = chunkMin;
        if (chunkMax > overviewMax[oIdx]) overviewMax[oIdx] = chunkMax;
        overviewRms[oIdx] += chunkSumSq;
        overviewCount[oIdx] += static_cast<int>(j);

        i += j;
    }
}