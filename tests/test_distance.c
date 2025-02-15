#include "greatest.h"
#include "../src/core/include/distance.h"
#include <stdlib.h>
#include <math.h>

typedef struct
{
    float *vec_a;
    float *vec_b;
    int size;
    float expected;
    const char *description;
} DotProductTestCase;

const float ACCEPTABLE_ERROR = 0.01f;

static void create_random_test_vectors(float **a, float **b, int size, float *expected)
{
    *a = (float *)malloc(size * sizeof(float));
    *b = (float *)malloc(size * sizeof(float));
    *expected = 0.0f;

    // Use predictable "random" values for testing
    for (int i = 0; i < size; i++)
    {
        (*a)[i] = (float)(i % 7) + 0.5f; // Values: 0.5, 1.5, 2.5, ..., 6.5, 0.5, ...
        (*b)[i] = (float)(i % 5) + 0.3f; // Values: 0.3, 1.3, 2.3, ..., 4.3, 0.3, ...
        *expected += (*a)[i] * (*b)[i];
    }
}

/* Helper to free test vectors */
static void free_test_vectors(DotProductTestCase *tc)
{
    free(tc->vec_a);
    free(tc->vec_b);
}

/* Test cases array */
static DotProductTestCase create_test_cases[] = {
    {NULL, NULL, 4, 0.0f, "Random 4-element vectors"},
    {NULL, NULL, 8, 0.0f, "Random 8-element vectors"},
    {NULL, NULL, 16, 0.0f, "Random 16-element vectors (AVX block)"},
    {NULL, NULL, 20, 0.0f, "Random 20-element vectors (AVX + remainder)"},
    {NULL, NULL, 256, 0.0f, "Random 256-element vectors (AVX + remainder)"},
};

/* Setup function to initialize test vectors */
static void setup_test_vectors(void)
{
    for (size_t i = 0; i < sizeof(create_test_cases) / sizeof(create_test_cases[0]); i++)
    {
        create_random_test_vectors(
            &create_test_cases[i].vec_a,
            &create_test_cases[i].vec_b,
            create_test_cases[i].size,
            &create_test_cases[i].expected);
    }
}

/* Cleanup function */
static void cleanup_test_vectors(void)
{
    for (size_t i = 0; i < sizeof(create_test_cases) / sizeof(create_test_cases[0]); i++)
    {
        free_test_vectors(&create_test_cases[i]);
    }
}

/* Test the scalar implementation directly */
TEST test_dot_product_scalar(void)
{
    for (size_t i = 0; i < sizeof(create_test_cases) / sizeof(create_test_cases[0]); i++)
    {
        DotProductTestCase tc = create_test_cases[i];
        printf("\nTest case: %s\n", tc.description);
        float result = dot_product_scalar(tc.vec_a, tc.vec_b, tc.size);
        ASSERT_IN_RANGE(tc.expected, result, ACCEPTABLE_ERROR);
    }
    PASS();
}

/* Test edge cases */
TEST test_dot_product_edge_cases(void)
{
    // Test null pointers
    ASSERT_EQ(0.0f, dot_product_scalar(NULL, NULL, 5));

    // Test zero size
    float a[] = {1.0f};
    float b[] = {1.0f};
    ASSERT_EQ(0.0f, dot_product_scalar(a, b, 0));

    // Test negative size
    ASSERT_EQ(0.0f, dot_product_scalar(a, b, -1));

    PASS();
}

/* Test the best implementation (selected at runtime) */
TEST test_dot_product_best_implementation(void)
{
    init_dot_product();

    for (size_t i = 0; i < sizeof(create_test_cases) / sizeof(create_test_cases[0]); i++)
    {
        DotProductTestCase tc = create_test_cases[i];
        printf("\nTest case: %s\n", tc.description);
        float result = dot_product(tc.vec_a, tc.vec_b, tc.size);
        ASSERT_IN_RANGE(tc.expected, result, ACCEPTABLE_ERROR);
    }
    PASS();
}

TEST test_implementations_match(void)
{
    for (size_t i = 0; i < sizeof(create_test_cases) / sizeof(create_test_cases[0]); i++)
    {
        DotProductTestCase tc = create_test_cases[i];

        // Get results from different implementations
        float scalar_result = dot_product_scalar(tc.vec_a, tc.vec_b, tc.size);
        float optimized_result = dot_product(tc.vec_a, tc.vec_b, tc.size);

        // Compare results
        ASSERT_IN_RANGE(scalar_result, optimized_result, ACCEPTABLE_ERROR);
        printf("\nTest case: %s\n", tc.description);
        printf("Scalar: %f, Optimized: %f, Expected: %f\n",
               scalar_result, optimized_result, tc.expected);
    }
    PASS();
}

/* Special test cases for precision */
TEST test_dot_product_precision(void)
{
    float a[] = {1.23456f, 2.34567f, 3.45678f, 4.56789f};
    float b[] = {1.11111f, 2.22222f, 3.33333f, 4.44444f};
    float expected = 38.4086f;

    float result = dot_product_scalar(a, b, 4);
    ASSERT_IN_RANGE(expected, result, ACCEPTABLE_ERROR);

    PASS();
}

SUITE(dot_product_suite)
{
    setup_test_vectors();

    RUN_TEST(test_dot_product_scalar);
    RUN_TEST(test_dot_product_edge_cases);
    RUN_TEST(test_dot_product_best_implementation);
    RUN_TEST(test_dot_product_precision);
    RUN_TEST(test_implementations_match);

    cleanup_test_vectors();
}

/* Main function */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(dot_product_suite);
    GREATEST_MAIN_END();
}