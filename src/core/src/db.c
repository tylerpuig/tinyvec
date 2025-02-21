// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#include <share.h>
#include <io.h>    // for _get_osfhandle
#include <fcntl.h> // for _fileno
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

// Project includes
#include "../include/db.h"
#include "../include/file.h"
#include "../include/minheap.h"
#include "../include/distance.h"

// Architecture-specific SIMD includes
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#include <xmmintrin.h>
#elif defined(__APPLE__) && defined(__aarch64__)
#include <arm_neon.h>
#endif

// Platform-specific prefetch macros
#if defined(__APPLE__) && defined(__aarch64__)
#define PREFETCH(addr) __builtin_prefetch(addr)
#elif defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
#define PREFETCH(addr) _mm_prefetch((char *)(addr), _MM_HINT_T0)
#else
#define PREFETCH(addr) __builtin_prefetch(addr)
#endif

// Platform-specific aligned memory functions
#ifdef _WIN32
#define aligned_malloc(size, alignment) _aligned_malloc(size, alignment)
#define aligned_free(ptr) _aligned_free(ptr)
#else
#define aligned_malloc(size, alignment) aligned_alloc(alignment, size)
#define aligned_free(ptr) free(ptr)
#endif

ActiveTinyVecConnections *active_tinyvec_connections = NULL;

TinyVecConnection *create_tiny_vec_connection(const char *file_path, const uint32_t dimensions)
{

    TinyVecConnection *current_connection = get_tinyvec_connection(file_path);
    if (current_connection)
    {
        return current_connection;
    }

    // Open vector file
    FILE *vec_file = fopen(file_path, "r+b");
    if (!vec_file)
    {
        printf("Failed to open vector file\n");
        return NULL;
    }

    VecFileHeaderInfo *header_info = get_vec_file_header_info(vec_file, dimensions);
    if (!header_info)
    {
        printf("Failed to get vector file header info\n");
        fclose(vec_file);
        return NULL;
    }

    // Get metadata file paths
    FileMetadataPaths *metadata_paths = get_metadata_file_paths(file_path);
    if (!metadata_paths)
    {
        printf("Failed to get metadata file paths\n");
        fclose(vec_file);
        return NULL;
    }

    // Use 1MB buffer for file I/O
    if (setvbuf(vec_file, NULL, _IOFBF, 1024 * 1024) != 0)
    {
        printf("Failed to set vector file buffer\n");
        free_metadata_file_paths(metadata_paths);
        fclose(vec_file);
        return NULL;
    }

    TinyVecConnection *connection = malloc(sizeof(TinyVecConnection));
    if (!connection)
    {
        free_metadata_file_paths(metadata_paths);
        fclose(vec_file);
        return NULL;
    }

    // Make a proper copy of the file path
    size_t path_len = strlen(file_path) + 1;
    char *path_copy = (char *)malloc(path_len);
    if (!path_copy)
    {
        free(connection);
        fclose(vec_file);
        free_metadata_file_paths(metadata_paths);
        free(header_info);
        return NULL;
    }
    strcpy(path_copy, file_path);
    connection->file_path = path_copy;
    connection->dimensions = header_info->dimensions;
    connection->vec_file = vec_file;

    add_to_connection_pool(connection);

    return connection;
}

TinyVecConnection *get_tinyvec_connection(const char *file_path)
{

    if (!active_tinyvec_connections)
    {
        return NULL;
    }

    for (int i = 0; i < active_tinyvec_connections->active_connections; i++)
    {

        if (strcmp(active_tinyvec_connections->connections[i]->file_path, file_path) == 0)
        {
            return active_tinyvec_connections->connections[i];
        }
    }

    return NULL;
}

bool add_to_connection_pool(TinyVecConnection *connection)
{
    // Initialize if NULL
    if (!active_tinyvec_connections)
    {
        active_tinyvec_connections = malloc(sizeof(ActiveTinyVecConnections));
        if (!active_tinyvec_connections)
            return false;

        active_tinyvec_connections->connections = NULL;
        active_tinyvec_connections->active_connections = 0;
    }

    // Resize array to accommodate new connection
    TinyVecConnection **temp = realloc(active_tinyvec_connections->connections,
                                       (active_tinyvec_connections->active_connections + 1) * sizeof(TinyVecConnection *));

    if (!temp)
    {
        return false;
    }

    active_tinyvec_connections->connections = temp;

    // Add the new connection
    active_tinyvec_connections->connections[active_tinyvec_connections->active_connections] = connection;
    active_tinyvec_connections->active_connections++;

    return true;
}

IndexFileStats get_index_stats(const char *file_path)
{

    TinyVecConnection *connection = get_tinyvec_connection(file_path);
    if (!connection)
        return (IndexFileStats){0, 0};

    VecFileHeaderInfo *header_info = get_vec_file_header_info(connection->vec_file, connection->dimensions);
    if (!header_info)
        return (IndexFileStats){0, 0};

    IndexFileStats stats = (IndexFileStats){header_info->vector_count, connection->dimensions};

    free(header_info);
    return stats;
}

VecResult *get_top_k(const char *file_path, const float *query_vec, const int top_k)
{
    TinyVecConnection *connection = NULL;
    VecFileHeaderInfo *header_info = NULL;
    float *vec_buffer = NULL;
    MinHeap *min_heap = NULL;
    VecResult *sorted = NULL;
    FileMetadataPaths *md_paths = NULL;
    FILE *idx_file = NULL;
    FILE *md_file = NULL;

    // Get connection
    connection = get_tinyvec_connection(file_path);
    if (!connection)
    {
        printf("Failed to get connection\n");
        goto cleanup;
    }

    // Get header info
    header_info = get_vec_file_header_info(connection->vec_file, connection->dimensions);
    if (!header_info)
    {
        printf("Failed to get header info\n");
        goto cleanup;
    }

    // Allocate vector buffer
    const int BUFFER_SIZE = calculate_optimal_buffer_size(header_info->dimensions);
    vec_buffer = (float *)aligned_malloc(sizeof(float) * header_info->dimensions * BUFFER_SIZE, 32);
    if (!vec_buffer)
    {
        printf("Failed to allocate vector buffer\n");
        goto cleanup;
    }

    float *query_vec_norm = get_normalized_vector(query_vec, header_info->dimensions);

    for (uint64_t i = 0; i < header_info->dimensions; i += 64 / sizeof(float))
    {
        PREFETCH((char *)&query_vec_norm[i]);
    }

    // Create min heap
    min_heap = createMinHeap(top_k);
    if (!min_heap)
    {
        printf("Failed to create min heap\n");
        goto cleanup;
    }

    // Process vectors
    int vec_count = 0;
    for (uint64_t i = 0; i < header_info->vector_count; i += BUFFER_SIZE)
    {
        int vectors_to_read = fmin(BUFFER_SIZE, header_info->vector_count - i);
        size_t read = fread(vec_buffer, sizeof(float) * header_info->dimensions, vectors_to_read, connection->vec_file);
        if (read != vectors_to_read)
        {
            printf("Failed to read vectors\n");
            goto cleanup;
        }

        for (int j = 0; j < vectors_to_read; j++)
        {
            float *current_vec = vec_buffer + (j * header_info->dimensions);
            PREFETCH((char *)(current_vec + header_info->dimensions));
            float dot = dot_product(query_vec_norm, current_vec, header_info->dimensions);

            if (dot > min_heap->data[0] || min_heap->size < top_k)
            {
                insert(min_heap, dot, vec_count);
            }
            vec_count++;
        }
    }

    // Create sorted results
    sorted = createVecResult(min_heap, top_k);
    if (!sorted)
    {
        printf("Failed to create sorted results\n");
        goto cleanup;
    }

    // Get metadata paths
    md_paths = get_metadata_file_paths(connection->file_path);
    if (!md_paths)
    {
        printf("Failed to get metadata paths\n");
        goto cleanup;
    }

    // Open index file
    idx_file = open_db_file(md_paths->idx_path);
    if (!idx_file)
    {
        printf("Failed to open index file\n");
        goto cleanup;
    }

    // Open metadata file
    md_file = open_db_file(md_paths->md_path);
    if (!md_file)
    {
        printf("Failed to open metadata file\n");
        goto cleanup;
    }

    // Get file sizes
    if (fseek(idx_file, 0, SEEK_END) != 0)
    {
        printf("Failed to seek index file\n");
        goto cleanup;
    }
    long idx_size = ftell(idx_file);

    if (fseek(md_file, 0, SEEK_END) != 0)
    {
        printf("Failed to seek metadata file\n");
        goto cleanup;
    }
    long md_size = ftell(md_file);

    // Process metadata for each result
    for (int i = 0; i < min_heap->size; i++)
    {
        int offset = sorted[i].index * 12;
        if (fseek(idx_file, 0, SEEK_SET) != 0 || fseek(md_file, 0, SEEK_SET) != 0)
        {
            printf("Failed to reset file positions\n");
            goto cleanup;
        }

        MetadataBytes metadata = get_vec_metadata(offset, idx_file, md_file, idx_size, md_size);
        if (!metadata.data)
        {
            // printf("Failed to get metadata for index %d\n", i);
            continue;
        }
        sorted[i].metadata = metadata;
    }

    if (vec_buffer)
        aligned_free(vec_buffer);
    if (idx_file)
        fclose(idx_file);
    if (md_file)
        fclose(md_file);
    if (header_info)
        free(header_info);
    if (min_heap)
        freeHeap(min_heap);
    if (md_paths)
        free(md_paths);
    free(query_vec_norm);

    return sorted;

cleanup:
    if (sorted)
    {
        for (int i = 0; i < min_heap->size; i++)
        {
            if (sorted[i].metadata.data)
            {
                free(sorted[i].metadata.data);
            }
        }
        free(sorted);
    }

    free(query_vec_norm);
    if (vec_buffer)
        aligned_free(vec_buffer);
    if (idx_file)
        fclose(idx_file);
    if (md_file)
        fclose(md_file);
    if (header_info)
        free(header_info);
    if (min_heap)
        freeHeap(min_heap);
    if (md_paths)
        free(md_paths);

    return NULL;
}

int insert_data(const char *file_path, float **vectors, char **metadatas, size_t *metadata_lengths,
                const size_t vec_count, const uint32_t dimensions)
{

    if (dimensions == 0)
        return 0;

    TinyVecConnection *connection = get_tinyvec_connection(file_path);

    char *temp_vec_file_path = malloc(strlen(file_path) + 6); // +6 for ".temp\0"
    sprintf(temp_vec_file_path, "%s.temp", file_path);
    if (!temp_vec_file_path)
    {
        return 0;
    }
    FILE *vec_file = open_db_file(temp_vec_file_path);
    if (!vec_file)
    {
        free(temp_vec_file_path);
        return 0;
    }

    VecFileHeaderInfo *header_info = get_vec_file_header_info(vec_file, dimensions);
    if (!header_info)
    {
        printf("Failed to get vector file header info\n");
        fclose(vec_file);
        free(temp_vec_file_path);
        return 0;
    }

    if (header_info->dimensions != dimensions)
    {
        printf("Dimensions don't match: %d vs %d\n", header_info->dimensions, dimensions);
        fclose(vec_file);
        free(header_info);
        free(temp_vec_file_path);
        return 0;
    }

    FileMetadataPaths *md_paths = get_metadata_file_paths(file_path);
    if (!md_paths)
    {

        fclose(vec_file);
        free_metadata_file_paths(md_paths);
        free(temp_vec_file_path);
        free(header_info);
        return 0;
    }

    char *temp_idx_path = malloc(strlen(md_paths->idx_path) + 6);
    char *temp_meta_path = malloc(strlen(md_paths->md_path) + 6);
    sprintf(temp_idx_path, "%s.temp", md_paths->idx_path);
    sprintf(temp_meta_path, "%s.temp", md_paths->md_path);

    if (!temp_idx_path || !temp_meta_path)
    {
        free(temp_idx_path);
        free(temp_meta_path);
        free_metadata_file_paths(md_paths);
        free(temp_vec_file_path);
        free(header_info);
        return 0;
    }

    FILE *idx_file = fopen(temp_idx_path, "ab");
    FILE *meta_file = fopen(temp_meta_path, "ab");

    if (!idx_file || !meta_file)
    {
        if (vec_file)
        {
            printf("Failed to open vec file\n");
            fclose(vec_file);
        }
        if (idx_file)
        {
            printf("Failed to open idx file\n");
            fclose(idx_file);
        }
        if (meta_file)
        {
            printf("Failed to open meta file\n");
            fclose(meta_file);
        }

        free(header_info);
        free_metadata_file_paths(md_paths);
        free(temp_idx_path);
        free(temp_meta_path);
        free(temp_vec_file_path);
        return 0;
    }

    if (connection)
    {

        fclose(connection->vec_file);
    }

    // After reading dimensions, seek to end for appending
    fseek(vec_file, 0, SEEK_END);

    // Now calculate total vector size using dimensions
    size_t total_vec_size = vec_count * header_info->dimensions * sizeof(float);

    // Calculate total sizes needed
    size_t total_meta_size = 0;
    for (size_t i = 0; i < vec_count; i++)
    {
        total_meta_size += metadata_lengths[i];
    }

    // Allocate buffers
    unsigned char *vec_buffer = malloc(total_vec_size);
    unsigned char *idx_buffer = malloc(vec_count * 12); // 12 bytes per index entry
    unsigned char *meta_buffer = malloc(total_meta_size);

    if (!vec_buffer || !idx_buffer || !meta_buffer)
    {
        free(header_info);
        free(vec_buffer);
        free(idx_buffer);
        free(meta_buffer);
        fclose(vec_file);
        fclose(idx_file);
        fclose(meta_file);
        free(temp_idx_path);
        free(temp_meta_path);
        free(temp_vec_file_path);
        return 0;
    }

    // Get starting metadata offset
    // uint64_t current_meta_offset = ftell(meta_file);
    fseek(meta_file, 0, SEEK_END);
    // uint64_t total_meta_size_offset = ftell(meta_file);
    uint64_t current_meta_offset = ftell(meta_file);

    // Fill buffers
    size_t vec_offset = 0;
    size_t meta_offset = 0;
    size_t idx_offset = 0;

    size_t vec_size = header_info->dimensions * sizeof(float);

    for (size_t i = 0; i < vec_count; i++)
    {
        if (!vectors[i])
        {
            continue;
        }
        normalize_vector(vectors[i], header_info->dimensions);
        // Copy vector
        memcpy(vec_buffer + vec_offset,
               vectors[i],
               vec_size);
        vec_offset += vec_size;

        // Write index entry (offset + length)
        uint64_t meta_pos = current_meta_offset + meta_offset;
        uint32_t meta_len = metadata_lengths[i];

        memcpy(idx_buffer + idx_offset, &meta_pos, sizeof(uint64_t));
        memcpy(idx_buffer + idx_offset + 8, &meta_len, sizeof(uint32_t));
        idx_offset += 12;

        memcpy(meta_buffer + meta_offset,
               metadatas[i],
               metadata_lengths[i]);

        meta_offset += metadata_lengths[i];
    }

    // Single write for each file
    fwrite(vec_buffer, 1, total_vec_size, vec_file);
    fwrite(idx_buffer, 1, vec_count * 12, idx_file);
    fwrite(meta_buffer, 1, total_meta_size, meta_file);

    // Position at start of file
    fseek(vec_file, 0, SEEK_SET);

    // Calculate new total count
    int new_count = header_info->vector_count + vec_count;

    // Write the new vector count
    fwrite(&new_count, sizeof(int), 1, vec_file);

    // Cleanup
    free(header_info);
    free(vec_buffer);
    free(idx_buffer);
    free(meta_buffer);
    fclose(vec_file);
    fclose(idx_file);
    fclose(meta_file);
    free(temp_idx_path);
    free(temp_meta_path);
    free(temp_vec_file_path);

    return (int)vec_count;
}

size_t calculate_optimal_buffer_size(int dimensions)
{
    // Target memory size we want to work with (let's say ~4MB)
    const size_t TARGET_BUFFER_MEMORY = 4 * 1024 * 1024; // 4MB

    // Calculate how many vectors would fit in our target memory
    size_t bytes_per_vector = dimensions * sizeof(float);
    size_t optimal_vectors = TARGET_BUFFER_MEMORY / bytes_per_vector;

    // Round to nearest power of 2 (optional, but can be more efficient)
    // or we could round to nearest multiple of 512 or 1024

    // Add some bounds checking
    if (optimal_vectors < 512)
        optimal_vectors = 512; // Minimum size
    if (optimal_vectors > 8192)
        optimal_vectors = 8192; // Maximum size

    return optimal_vectors;
}

bool update_db_file_connection(const char *file_path)
{
    if (!file_path)
    {
        printf("Null file path provided\n");
        return false;
    }

    TinyVecConnection *connection = get_tinyvec_connection(file_path);
    if (!connection)
    {
        printf("Failed to get connection for path: %s\n", file_path);
        return false;
    }

    // Close any existing open files
    if (connection->vec_file)
        fclose(connection->vec_file);

    connection->vec_file = NULL;

    FILE *vec_file = open_db_file(connection->file_path);
    if (!vec_file)
    {
        printf("Failed to open vector file: %s\n", connection->file_path);
        return false;
    }

    connection->vec_file = vec_file;

    return true;
}