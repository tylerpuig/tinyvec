#include "../include/distance.h"
#include <stdbool.h>
#include <string.h>

#if defined(__arm64__) || defined(__aarch64__)
#include <arm_neon.h>
#else
#include <immintrin.h>
#include <xmmintrin.h>
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) && !defined(__APPLE__)
#include <cpuid.h>
#endif

// CPU feature detection
int check_avx_support(void)
{
#if defined(__arm64__) || defined(__aarch64__)
    return 0; // ARM processor, no AVX
#else
#if defined(_MSC_VER)
    uint32_t ecx;
    int cpu_info[4];
    __cpuid(cpu_info, 1);
    ecx = cpu_info[2];
#elif defined(__GNUC__)
    uint32_t eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);
#endif
    return ((ecx & (1 << 27)) && (ecx & (1 << 28)));
#endif
}

// Scalar implementation
float dot_product_scalar(const float *a, const float *b, int size)
{
    if (!a || !b || size <= 0)
        return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < size; i++)
    {
        sum += a[i] * b[i];
    }
    return sum;
}

#ifdef __APPLE__
float dot_product_neon(const float *a, const float *b, int size)
{
    if (!a || !b || size <= 0)
        return 0.0f;
    float32x4_t sum_vec1 = vdupq_n_f32(0);
    float32x4_t sum_vec2 = vdupq_n_f32(0);
    float32x4_t sum_vec3 = vdupq_n_f32(0);
    float32x4_t sum_vec4 = vdupq_n_f32(0);

    int i;
    for (i = 0; i < size - 15; i += 16)
    {
        float32x4_t va1 = vld1q_f32(&a[i]);
        float32x4_t vb1 = vld1q_f32(&b[i]);
        float32x4_t va2 = vld1q_f32(&a[i + 4]);
        float32x4_t vb2 = vld1q_f32(&b[i + 4]);
        float32x4_t va3 = vld1q_f32(&a[i + 8]);
        float32x4_t vb3 = vld1q_f32(&b[i + 8]);
        float32x4_t va4 = vld1q_f32(&a[i + 12]);
        float32x4_t vb4 = vld1q_f32(&b[i + 12]);

        sum_vec1 = vfmaq_f32(sum_vec1, va1, vb1);
        sum_vec2 = vfmaq_f32(sum_vec2, va2, vb2);
        sum_vec3 = vfmaq_f32(sum_vec3, va3, vb3);
        sum_vec4 = vfmaq_f32(sum_vec4, va4, vb4);
    }

    for (; i < size - 3; i += 4)
    {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        sum_vec1 = vfmaq_f32(sum_vec1, va, vb);
    }

    float32x4_t sum = vaddq_f32(sum_vec1, sum_vec2);
    sum = vaddq_f32(sum, sum_vec3);
    sum = vaddq_f32(sum, sum_vec4);

    float32x2_t sum_half = vadd_f32(vget_low_f32(sum), vget_high_f32(sum));
    float final_sum = vaddv_f32(sum_half);

    for (; i < size; i++)
    {
        final_sum += a[i] * b[i];
    }

    return final_sum;
}

float dot_product_neon_wide(const float *a, const float *b, int size)
{
    if (!a || !b || size <= 0)
        return 0.0f;

    // Two pairs of 128-bit registers (effectively like one 256-bit register)
    float32x4_t sum1_low = vdupq_n_f32(0);
    float32x4_t sum1_high = vdupq_n_f32(0);
    float32x4_t sum2_low = vdupq_n_f32(0);
    float32x4_t sum2_high = vdupq_n_f32(0);

    int i;
    for (i = 0; i < size - 15; i += 16)
    {
        // Load 16 elements (like AVX's 256-bit loads)
        float32x4_t va1_low = vld1q_f32(&a[i]);
        float32x4_t va1_high = vld1q_f32(&a[i + 4]);
        float32x4_t va2_low = vld1q_f32(&a[i + 8]);
        float32x4_t va2_high = vld1q_f32(&a[i + 12]);

        float32x4_t vb1_low = vld1q_f32(&b[i]);
        float32x4_t vb1_high = vld1q_f32(&b[i + 4]);
        float32x4_t vb2_low = vld1q_f32(&b[i + 8]);
        float32x4_t vb2_high = vld1q_f32(&b[i + 12]);

        // Multiply and accumulate (similar to AVX's multiply and add)
        sum1_low = vfmaq_f32(sum1_low, va1_low, vb1_low);
        sum1_high = vfmaq_f32(sum1_high, va1_high, vb1_high);
        sum2_low = vfmaq_f32(sum2_low, va2_low, vb2_low);
        sum2_high = vfmaq_f32(sum2_high, va2_high, vb2_high);
    }

    // Handle remaining blocks of 4
    for (; i < size - 3; i += 4)
    {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        sum1_low = vfmaq_f32(sum1_low, va, vb);
    }

    // Combine all partial sums
    float32x4_t sum = vaddq_f32(sum1_low, sum1_high);
    sum = vaddq_f32(sum, sum2_low);
    sum = vaddq_f32(sum, sum2_high);

    // Reduce to final sum
    float32x2_t sum_half = vadd_f32(vget_low_f32(sum), vget_high_f32(sum));
    float final_sum = vaddv_f32(sum_half);

    // Handle remaining elements
    for (; i < size; i++)
    {
        final_sum += a[i] * b[i];
    }

    return final_sum;
}

#else

// AVX implementation for x86
float dot_product_avx_16(const float *a, const float *b, int size)
{
    if (!a || !b || size <= 0)
        return 0.0f;

    __m256 sum1 = _mm256_setzero_ps();
    __m256 sum2 = _mm256_setzero_ps();

    int i;
    for (i = 0; i < size - 15; i += 16)
    {
        __m256 va1 = _mm256_loadu_ps(&a[i]);
        __m256 vb1 = _mm256_loadu_ps(&b[i]);
        __m256 va2 = _mm256_loadu_ps(&a[i + 8]);
        __m256 vb2 = _mm256_loadu_ps(&b[i + 8]);

        sum1 = _mm256_add_ps(sum1, _mm256_mul_ps(va1, vb1));
        sum2 = _mm256_add_ps(sum2, _mm256_mul_ps(va2, vb2));
    }

    __m256 total = _mm256_add_ps(sum1, sum2);

    float result[8];
    _mm256_storeu_ps(result, total);
    float final_sum = 0.0f;
    for (int j = 0; j < 8; j++)
    {
        final_sum += result[j];
    }

    // Handle remaining elements
    for (; i < size; i++)
    {
        final_sum += a[i] * b[i];
    }

    _mm256_zeroupper();
    return final_sum;
}
#endif

// Function pointer type
typedef float (*dot_product_fn)(const float *, const float *, int);

// Global function pointer
static dot_product_fn best_dot_product = NULL;

// Initialize best implementation
void init_dot_product(void)
{
    if (best_dot_product == NULL)
    {
#if defined(__arm64__) || defined(__aarch64__)
        // best_dot_product = dot_product_neon;
        best_dot_product = dot_product_neon_wide;
#else
        if (check_avx_support())
        {
            best_dot_product = dot_product_avx_16;
        }
        else
        {
            best_dot_product = dot_product_scalar;
        }
#endif
    }
}

// Wrapper function
float dot_product(const float *a, float *b, int size)
{
    if (best_dot_product == NULL)
    {
        init_dot_product();
    }
    return best_dot_product(a, b, size);
}