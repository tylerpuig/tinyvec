#include "greatest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core/include/db.h"
#include "../src/core/include/file.h"
#include "../src/core/include/minheap.h"
#include "../src/core/include/distance.h"

// Test constants
#define TEST_DIMENSIONS 4
#define TEST_VEC_COUNT 3
#define TEST_DB_PATH "main-file.db"
#define TEST_IDX_PATH "main-file.db.idx"
#define TEST_META_PATH "main-file.db.meta"

// Helper function to create initial DB files
static bool create_test_files(void)
{
    // Create main DB file with header
    FILE *db_file = fopen(TEST_DB_PATH, "wb");
    if (!db_file)
        return false;

    // Write header (vector count and dimensions)
    int initial_count = 0;
    uint32_t dimensions = TEST_DIMENSIONS;
    fwrite(&initial_count, sizeof(int), 1, db_file);
    fwrite(&dimensions, sizeof(uint32_t), 1, db_file);
    fclose(db_file);

    // Create empty index and metadata files
    FILE *idx_file = fopen(TEST_IDX_PATH, "wb");
    FILE *meta_file = fopen(TEST_META_PATH, "wb");

    if (!idx_file || !meta_file)
    {
        if (idx_file)
            fclose(idx_file);
        if (meta_file)
            fclose(meta_file);
        return false;
    }

    fclose(idx_file);
    fclose(meta_file);
    return true;
}

// Helper function to cleanup test files
static void cleanup_test_files(void)
{
    int result;

    result = remove(TEST_DB_PATH);
    printf("Removing %s: %s\n", TEST_DB_PATH, result == 0 ? "success" : "failed");

    result = remove(TEST_IDX_PATH);
    printf("Removing %s: %s\n", TEST_IDX_PATH, result == 0 ? "success" : "failed");

    result = remove(TEST_META_PATH);
    printf("Removing %s: %s\n", TEST_META_PATH, result == 0 ? "success" : "failed");

    result = remove("main-file.db.temp");
    printf("Removing %s: %s\n", "main-file.db.temp", result == 0 ? "success" : "failed");

    result = remove("main-file.db.idx.temp");
    printf("Removing %s: %s\n", "main-file.db.idx.temp", result == 0 ? "success" : "failed");

    result = remove("main-file.db.meta.temp");
    printf("Removing %s: %s\n", "main-file.db.meta.temp", result == 0 ? "success" : "failed");
}

// Helper to generate test vectors
static float **generate_test_vectors(void)
{
    float **vectors = malloc(TEST_VEC_COUNT * sizeof(float *));
    if (!vectors)
        return NULL;

    // Create vectors with known patterns
    float vec1[] = {1.0f, 0.0f, 0.0f, 0.0f};     // Will have dot product 1.0 with query
    float vec2[] = {0.707f, 0.707f, 0.0f, 0.0f}; // Will have dot product ~0.707
    float vec3[] = {0.0f, 1.0f, 0.0f, 0.0f};     // Will have dot product 0.0

    for (int i = 0; i < TEST_VEC_COUNT; i++)
    {
        vectors[i] = malloc(TEST_DIMENSIONS * sizeof(float));
        if (!vectors[i])
        {
            // Cleanup previously allocated memory
            for (int j = 0; j < i; j++)
            {
                free(vectors[j]);
            }
            free(vectors);
            return NULL;
        }
    }

    memcpy(vectors[0], vec1, TEST_DIMENSIONS * sizeof(float));
    memcpy(vectors[1], vec2, TEST_DIMENSIONS * sizeof(float));
    memcpy(vectors[2], vec3, TEST_DIMENSIONS * sizeof(float));

    return vectors;
}

// Helper to generate test metadata
static char **generate_test_metadata(size_t *lengths)
{
    char **metadata = malloc(TEST_VEC_COUNT * sizeof(char *));
    if (!metadata)
        return NULL;

    const char *test_meta[] = {
        "{\"id\": 1}",
        "{\"id\": 2}",
        "{\"id\": 3}"};

    for (int i = 0; i < TEST_VEC_COUNT; i++)
    {
        lengths[i] = strlen(test_meta[i]);
        metadata[i] = strdup(test_meta[i]);
        if (!metadata[i])
        {
            // Cleanup
            for (int j = 0; j < i; j++)
            {
                free(metadata[j]);
            }
            free(metadata);
            return NULL;
        }
    }

    return metadata;
}

// Helper to free test data
static void free_test_data(float **vectors, char **metadata)
{
    if (vectors)
    {
        for (int i = 0; i < TEST_VEC_COUNT; i++)
        {
            free(vectors[i]);
        }
        free(vectors);
    }

    if (metadata)
    {
        for (int i = 0; i < TEST_VEC_COUNT; i++)
        {
            free(metadata[i]);
        }
        free(metadata);
    }
}

// Test DB file creation and connection
TEST test_db_creation_and_connection(void)
{

    TinyVecConnection *conn = create_tiny_vec_connection(TEST_DB_PATH, TEST_DIMENSIONS);
    ASSERT(conn != NULL);
    ASSERT(conn->dimensions == TEST_DIMENSIONS);
    ASSERT(conn->vec_file != NULL);

    int fclose_result = fclose(conn->vec_file);
    printf("fclose_result: %d\n", fclose_result);

    PASS();
}

// Test vector insertion
TEST test_vector_insertion(void)
{

    TinyVecConnection *conn = create_tiny_vec_connection(TEST_DB_PATH, TEST_DIMENSIONS);
    ASSERT(conn != NULL);

    float **vectors = generate_test_vectors();
    ASSERT(vectors != NULL);

    size_t metadata_lengths[TEST_VEC_COUNT];
    char **metadata = generate_test_metadata(metadata_lengths);
    ASSERT(metadata != NULL);

    int inserted = insert_data(TEST_DB_PATH, vectors, metadata, metadata_lengths,
                               TEST_VEC_COUNT, TEST_DIMENSIONS);
    ASSERT_EQ(inserted, TEST_VEC_COUNT);

    free_test_data(vectors, metadata);
    PASS();
}

// Test vector search
TEST test_vector_search(void)
{

    TinyVecConnection *conn = create_tiny_vec_connection(TEST_DB_PATH, TEST_DIMENSIONS);
    ASSERT(conn != NULL);

    float **vectors = generate_test_vectors();
    ASSERT(vectors != NULL);

    size_t metadata_lengths[TEST_VEC_COUNT];
    char **metadata = generate_test_metadata(metadata_lengths);
    ASSERT(metadata != NULL);

    ASSERT_EQ(insert_data(TEST_DB_PATH, vectors, metadata, metadata_lengths,
                          TEST_VEC_COUNT, TEST_DIMENSIONS),
              TEST_VEC_COUNT);

    update_db_file_connection(TEST_DB_PATH);

    // Create query vector [1,0,0,0] - should match exactly with first vector
    float query_vec[TEST_DIMENSIONS] = {1.0f, 0.0f, 0.0f, 0.0f};

    // Get top 2 results
    VecResult *results = get_top_k(TEST_DB_PATH, query_vec, 2);
    ASSERT(results != NULL);

    // Clean up
    for (int i = 0; i < 2; i++)
    {
        if (results[i].metadata.data)
        {
            free(results[i].metadata.data);
        }
    }
    free(results);
    free_test_data(vectors, metadata);
    // cleanup_test_files();
    PASS();
}

SUITE(db_suite)
{
    RUN_TEST(test_db_creation_and_connection);
    RUN_TEST(test_vector_insertion);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{

    GREATEST_MAIN_BEGIN();
    create_test_files();
    RUN_SUITE(db_suite);
    cleanup_test_files();
    GREATEST_MAIN_END();

    return 0;
}
