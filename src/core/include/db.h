#ifndef DB_H
#define DB_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _WIN32
#include <windows.h>
#define EXPORT __declspec(dllexport)
#else
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define EXPORT __attribute__((visibility("default")))
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "file.h"
#include "vec_types.h"
#include "sqlite3.h"

    typedef struct TinyVecConnection
    {
        const char *file_path;
        uint32_t dimensions;
        FILE *vec_file;
        FILE *idx_file;
        FILE *md_file;
        MmapInfo *idx_mmap;
        MmapInfo *md_mmap;
        sqlite3 *sqlite_db;
    } TinyVecConnection;

    typedef struct
    {
        TinyVecConnection **connections;
        int active_connections;
    } ActiveTinyVecConnections;

    // Core API functions
    EXPORT TinyVecConnection *create_tiny_vec_connection(const char *file_path, const uint32_t dimensions);
    EXPORT IndexFileStats get_index_stats(const char *file_path);
    EXPORT VecResult *get_top_k(const char *file_path, const float *query_vec, const int top_k);
    EXPORT int insert_data(const char *file_path, float **vectors, char **metadatas, size_t *metadata_lengths,
                           const size_t vec_count, const uint32_t dimensions);
    EXPORT bool update_db_file_connection(const char *file_path);

    // Internal functions
    TinyVecConnection *get_tinyvec_connection(const char *file_path);
    bool add_to_connection_pool(TinyVecConnection *connection);
    size_t calculate_optimal_buffer_size(int dimensions);
    int get_metadata_batch(sqlite3 *db, VecResult *sorted, int count);
#ifdef __cplusplus
}
#endif

#endif // DB_H