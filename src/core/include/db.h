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

    typedef struct TinyVecConnection
    {
        const char *file_path;
        uint32_t dimensions;
        FILE *vec_file;
        FILE *idx_file;
        FILE *md_file;
        MmapInfo *idx_mmap;
        MmapInfo *md_mmap;
    } TinyVecConnection;

    typedef struct
    {
        TinyVecConnection **connections;
        int active_connections;
    } ActiveTinyVecConnections;

    // Core API functions
    EXPORT TinyVecConnection *create_tiny_vec_connection(const char *file_path, const uint32_t dimensions);
    EXPORT IndexFileStats get_index_stats(const char *file_path);

    // Internal functions
    TinyVecConnection *get_tinyvec_connection(const char *file_path);
    bool add_to_connection_pool(TinyVecConnection *connection);
#ifdef __cplusplus
}
#endif

#endif // DB_H