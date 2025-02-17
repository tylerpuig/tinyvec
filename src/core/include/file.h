#ifndef FILE_H
#define FILE_H

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include "cJSON.h"
#include <stdbool.h>
#include "vec_types.h"

typedef struct
{
#ifdef _WIN32
    void *map;
    size_t size;
    HANDLE file_handle;
    HANDLE mapping_handle;
#else
    void *map;
    size_t size;
    int fd;
#endif
} MmapInfo;

typedef struct FileMetadataPaths
{
    char *idx_path;
    char *md_path;
} FileMetadataPaths;

typedef struct VecFileHeaderInfo
{
    uint32_t dimensions;
    uint64_t vector_count;
} VecFileHeaderInfo;

typedef struct IndexFileStats
{
    uint64_t vector_count;
    uint32_t dimensions;
} IndexFileStats;

MmapInfo *create_mmap(const char *filename);
MmapInfo *create_mmap_with_retry(const char *filename, int max_retries);
void free_mmap(MmapInfo *info);
VecFileHeaderInfo *get_vec_file_header_info(FILE *vec_file, const uint32_t dimensions);
FileMetadataPaths *get_metadata_file_paths(const char *file_path);
void free_metadata_file_paths(FileMetadataPaths *paths);
MetadataBytes get_vec_metadata(const MmapInfo *idx_map, const MmapInfo *md_map, const int idx_offset);
FILE *open_db_file(const char *file_path);
bool file_exists(const char *filename);
bool create_file(const char *filename);
#endif // FILE_H