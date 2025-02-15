#include "greatest.h"
#include <stdio.h>
#include <stdlib.h>
#include "../src/core/include/vec_types.h"
#include "../src/core/include/file.h"

// Test utilities
static const char *TEST_VEC_FILE = "test_vectors.vec";
static const char *TEST_FILE_FOR_MMAP = "test_mmap.txt";

void setup_test_file(const char *filename, const char *content)
{
    FILE *f = fopen(filename, "wb");
    if (f != NULL)
    {
        fwrite(content, strlen(content), 1, f);
        fclose(f);
    }
}

void setup_vec_file(const char *filename, uint32_t total_vectors, uint32_t dimensions)
{
    FILE *f = fopen(filename, "wb");
    if (f != NULL)
    {
        fwrite(&total_vectors, sizeof(uint32_t), 1, f);
        fwrite(&dimensions, sizeof(uint32_t), 1, f);
        fclose(f);
    }
}

void cleanup_test_file(const char *filename)
{
    remove(filename);
}

// Test for create_mmap and free_mmap
TEST test_mmap_creation_and_cleanup(void)
{
    const char *test_content = "Hello, World!";
    setup_test_file(TEST_FILE_FOR_MMAP, test_content);

    MmapInfo *info = create_mmap(TEST_FILE_FOR_MMAP);
    ASSERT(info != NULL);
    ASSERT(info->map != NULL);
    ASSERT(info->size == strlen(test_content));

    // Verify content
    ASSERT(memcmp(info->map, test_content, strlen(test_content)) == 0);

    free_mmap(info);
    cleanup_test_file(TEST_FILE_FOR_MMAP);
    PASS();
}

// Test for metadata file paths
TEST test_metadata_file_paths(void)
{
    const char *base_path = "test_file";
    FileMetadataPaths *paths = get_metadata_file_paths(base_path);

    ASSERT(paths != NULL);
    ASSERT(paths->idx_path != NULL);
    ASSERT(paths->md_path != NULL);

    ASSERT_STR_EQ(paths->idx_path, "test_file.idx");
    ASSERT_STR_EQ(paths->md_path, "test_file.meta");

    free_metadata_file_paths(paths);
    PASS();
}

// Test for vec file header info
TEST test_vec_file_header_new_file(void)
{
    uint32_t test_dimensions = 128;
    uint32_t test_total_vectors = 1000;

    setup_vec_file(TEST_VEC_FILE, test_total_vectors, test_dimensions);

    FILE *vec_file = fopen(TEST_VEC_FILE, "rb+");
    ASSERT(vec_file != NULL);

    VecFileHeaderInfo *header = get_vec_file_header_info(vec_file, 0); // Use 0 to read existing dimensions
    ASSERT(header != NULL);
    ASSERT_EQ(header->dimensions, test_dimensions);
    ASSERT_EQ(header->vector_count, test_total_vectors);

    free(header);
    fclose(vec_file);
    cleanup_test_file(TEST_VEC_FILE);
    PASS();
}

TEST test_vec_file_header_update_dimensions(void)
{
    uint32_t initial_dimensions = 128;
    uint32_t new_dimensions = 256;
    uint32_t test_total_vectors = 1000;

    setup_vec_file(TEST_VEC_FILE, test_total_vectors, initial_dimensions);

    FILE *vec_file = fopen(TEST_VEC_FILE, "rb+");
    ASSERT(vec_file != NULL);

    // Update dimensions
    VecFileHeaderInfo *header = get_vec_file_header_info(vec_file, new_dimensions);
    ASSERT(header != NULL);
    ASSERT_EQ(header->dimensions, new_dimensions);
    ASSERT_EQ(header->vector_count, test_total_vectors);

    free(header);

    // Verify changes persisted
    rewind(vec_file);
    VecFileHeaderInfo *updated_header = get_vec_file_header_info(vec_file, 0);
    ASSERT(updated_header != NULL);
    ASSERT_EQ(updated_header->dimensions, new_dimensions);
    ASSERT_EQ(updated_header->vector_count, test_total_vectors);

    free(updated_header);
    fclose(vec_file);
    cleanup_test_file(TEST_VEC_FILE);
    PASS();
}

/* Add all test suites here */
SUITE(file_handling_suite)
{
    RUN_TEST(test_mmap_creation_and_cleanup);
    RUN_TEST(test_metadata_file_paths);
    RUN_TEST(test_vec_file_header_new_file);
    RUN_TEST(test_vec_file_header_update_dimensions);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(file_handling_suite);
    GREATEST_MAIN_END();
    return 0;
}