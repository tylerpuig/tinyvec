#ifndef DISTANCE_H
#define DISTANCE_H

#include <stdint.h>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#include <immintrin.h>
#endif

// CPU feature detection
int check_avx_support(void);

// Scalar implementation
float dot_product_scalar(const float *a, const float *b, int size);

// AVX implementation for x86
#ifndef __APPLE__
float dot_product_avx_16(const float *a, const float *b, int size);
#else
// vDSP implementation for Mac
float dot_product_vdsp(const float *a, const float *b, int size);
#endif

// Initialize the optimal implementation
void init_dot_product(void);

// Main interface - automatically uses best available implementation
float dot_product(const float *a, float *b, int size);

#endif // DISTANCE_H