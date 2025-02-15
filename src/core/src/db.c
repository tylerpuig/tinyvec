#ifdef _WIN32
#include <windows.h>
#include <share.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include "../include/db.h"
#include "../include/file.h"
#include "../include/minheap.h"

ActiveTinyVecConnections *active_tinyvec_connections = NULL;

TinyVecConnection *create_tiny_vec_connection(const char *file_path, const uint32_t dimensions)
{

    TinyVecConnection *current_connection = get_tinyvec_connection(file_path);
    if (current_connection)
    {
        return current_connection;
    }

    // Open vector file
    FILE *vec_file = open_db_file(file_path);
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
        // free_mmap(idx_mmap);
        // free_mmap(md_mmap);
        free_metadata_file_paths(metadata_paths);
        fclose(vec_file);
        // fclose(idx_file);
        // fclose(md_file);
        return NULL;
    }

    TinyVecConnection *connection = malloc(sizeof(TinyVecConnection));
    if (!connection)
    {
        // free_mmap(idx_mmap);
        // free_mmap(md_mmap);
        free_metadata_file_paths(metadata_paths);
        fclose(vec_file);
        // fclose(idx_file);
        // fclose(md_file);
        return NULL;
    }

    MmapInfo *idx_mmap = create_mmap(metadata_paths->idx_path);
    MmapInfo *md_mmap = create_mmap(metadata_paths->md_path);

    if (idx_mmap)
    {
        connection->idx_mmap = idx_mmap;
    }
    if (md_mmap)
    {
        connection->md_mmap = md_mmap;
    }

    // Make a proper copy of the file path
    size_t path_len = strlen(file_path) + 1;
    char *path_copy = (char *)malloc(path_len);
    if (!path_copy)
    {
        free(connection);
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
        printf("No active connections\n");
        return NULL;
    }

    for (int i = 0; i < active_tinyvec_connections->active_connections; i++)
    {

        // printf("Comparing %s with %s\n", active_tinyvec_connections->connections[i]->file_path, file_path);
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
    if (active_tinyvec_connections == NULL)
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

    TinyVecConnection *connection = get_tinyvec_connection(file_path);
    if (!connection)
    {
        printf("Failed to get connection\n");
        return NULL;
    }
    if (!connection->idx_mmap || !connection->md_mmap)
    {
        printf("Failed to get mmaps for query\n");
        return NULL;
    }

    VecFileHeaderInfo *header_info = get_vec_file_header_info(connection->vec_file, connection->dimensions);
    const int BUFFER_SIZE = calculate_optimal_buffer_size(header_info->dimensions);
    float *vec_buffer = (float *)aligned_malloc(sizeof(float) * header_info->dimensions * BUFFER_SIZE, 32);

    MinHeap *min_heap = createMinHeap(top_k);

    int vec_count = 0;
    for (int i = 0; i < header_info->vector_count; i += BUFFER_SIZE)
    {
        int vectors_to_read = fmin(BUFFER_SIZE, header_info->vector_count - i);

        size_t read = fread(vec_buffer, sizeof(float) * header_info->dimensions, vectors_to_read, connection->vec_file);
        if (read != vectors_to_read)
            continue;

        for (int j = 0; j < vectors_to_read; j++)
        {
            float *current_vec = vec_buffer + (j * header_info->dimensions);
            PREFETCH((char *)(current_vec + header_info->dimensions));
            float dot = dot_product_avx_16_optimized(query_vec, current_vec, header_info->dimensions);

            if (dot > min_heap->data[0] || min_heap->size < top_k)
            {
                insert(min_heap, dot, vec_count);
            }
            vec_count++;
        }
    }

    VecResult *sorted = createVecResult(min_heap, top_k);
    if (!sorted)
    {
        printf("sorted is null\n");
        aligned_free(vec_buffer);
        freeHeap(min_heap);
        free(header_info);
        return NULL;
    }

    for (int i = 0; i < min_heap->size; i++)
    {
        int offset = sorted[i].index * 12;
        sorted[i].metadata = get_vec_metadata(connection->idx_mmap, connection->md_mmap, offset);
    }

    // Clean up
    aligned_free(vec_buffer);
    free(header_info);
    freeHeap(min_heap);

    return sorted;
}

size_t insert_data(const char *file_path, float **vectors, char **metadatas, size_t *metadata_lengths,
                   const size_t vec_count, const uint32_t dimensions)
{

    if (dimensions == 0)
        return 0;

    TinyVecConnection *connection = get_tinyvec_connection(file_path);

    // open vector file
    // read and write binary, file must exist (r+b)
    FILE *vec_file = open_db_file(file_path);
    if (!vec_file)
    {
        return 0;
    }

    VecFileHeaderInfo *header_info = get_vec_file_header_info(vec_file, dimensions);
    if (!header_info)
    {
        printf("Failed to get vector file header info\n");
        fclose(vec_file);
        return 0;
    }

    if (header_info->dimensions != dimensions)
    {
        printf("Dimensions don't match: %d vs %d\n", header_info->dimensions, dimensions);
        fclose(vec_file);
        return 0;
    }

    FileMetadataPaths *md_paths = get_metadata_file_paths(file_path);
    if (!vec_file || !md_paths)
    {
        if (vec_file)
            fclose(vec_file);
        free_metadata_file_paths(md_paths);
        free(header_info);
        return 0;
    }
    FILE *idx_file = fopen(md_paths->idx_path, "ab");
    FILE *meta_file = fopen(md_paths->md_path, "ab");

    if (!vec_file || !idx_file || !meta_file)
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
        return 0;
    }

    printf("vector_count: %lld, dimensions: %d\n", header_info->vector_count, header_info->dimensions, "from write_vectors_batch");

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
        return 0;
    }

    // Get starting metadata offset
    // uint64_t current_meta_offset = ftell(meta_file);
    fseek(meta_file, 0, SEEK_END);
    uint64_t total_meta_size_offset = ftell(meta_file);
    uint64_t current_meta_offset = total_meta_size_offset;

    // Fill buffers
    size_t vec_offset = 0;
    size_t meta_offset = 0;
    size_t idx_offset = 0;

    size_t vec_size = header_info->dimensions * sizeof(float);

    for (size_t i = 0; i < vec_count; i++)
    {
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
    fclose(idx_file);
    fclose(meta_file);

    if (connection)
    {

        if (connection->idx_mmap)
        {
            free_mmap(connection->idx_mmap);
            connection->idx_mmap = NULL;
        }
        if (connection->md_mmap)
        {
            free_mmap(connection->md_mmap);
            connection->md_mmap = NULL;
        }

        // Create new mmaps
        connection->idx_mmap = create_mmap(md_paths->idx_path);
        connection->md_mmap = create_mmap(md_paths->md_path);
        connection->vec_file = vec_file;
    }

    return (int)vec_count;
}