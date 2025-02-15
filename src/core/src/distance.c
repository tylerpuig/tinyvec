#include "../include/distance.h"
#include <stdbool.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) && !defined(__APPLE__)
#include <cpuid.h>
#endif

// CPU feature detection
int check_avx_support(void)
{
#ifdef __APPLE__
    return 0; // Not needed for Mac implementation
#else
    uint32_t eax, ebx, ecx, edx;

#if defined(_MSC_VER)
    int cpu_info[4];
    __cpuid(cpu_info, 1);
    ecx = cpu_info[2];
#elif defined(__GNUC__)
    __cpuid(1, eax, ebx, ecx, edx);
#else
#error "Unsupported compiler"
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
// Mac vDSP implementation
float dot_product_vdsp(const float *a, const float *b, int size)
{
    if (!a || !b || size <= 0)
        return 0.0f;

    float result;
    vDSP_dotpr(a, 1, b, 1, &result, size);
    return result;
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
#ifdef __APPLE__
        best_dot_product = dot_product_vdsp;
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