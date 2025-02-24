#ifndef DISTANCE_H
#define DISTANCE_H

#include <stdint.h>

// CPU feature detection
int check_avx_support(void);

// Scalar implementation
float dot_product_scalar(const float *a, const float *b, int size);

void normalize_vector(float *arr, uint32_t length);

#ifdef __APPLE__
// NEON implementations for Mac/ARM
float dot_product_neon(const float *a, const float *b, int size);
float dot_product_neon_wide(const float *a, const float *b, int size);
#else
// AVX implementation for x86
float dot_product_avx_16(const float *a, const float *b, int size);
#endif

// Initialize the optimal implementation
void init_dot_product(void);

// Main interface - automatically uses best available implementation
float dot_product(const float *a, const float *b, int size);
float *get_normalized_vector(const float *vec, uint32_t length);

#endif // DISTANCE_H