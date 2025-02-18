#ifdef _WIN32
#include <windows.h>
#include <share.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h> // for MAX_PATH
// TODO:
#include <psapi.h>
#else
#include <limits.h> // for PATH_MAX
#define MAX_PATH PATH_MAX
#endif

#include <stdio.h>
#include "../include/vec_types.h"
#include "../include/file.h"
#include <stdbool.h>

MmapInfo *create_mmap(const char *filename)
{
    MmapInfo *info = (MmapInfo *)malloc(sizeof(MmapInfo));
    if (!info)
        return NULL;

    info->map = NULL;
    info->size = 0;

#ifdef _WIN32
    info->file_handle = NULL;
    info->mapping_handle = NULL;

    // Windows implementation
    info->file_handle = CreateFileA(filename,
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);

    if (info->file_handle == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        printf("CreateFileA failed with error: %lu\n", error);
        free(info);
        return NULL;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(info->file_handle, &file_size))
    {
        DWORD error = GetLastError();
        printf("GetFileSizeEx failed with error: %lu\n", error);
        CloseHandle(info->file_handle);
        free(info);
        return NULL;
    }
    info->size = file_size.QuadPart;
    // printf("File size: %llu\n", (unsigned long long)info->size);

    info->mapping_handle = CreateFileMappingA(
        info->file_handle,
        NULL,
        PAGE_READWRITE,
        file_size.HighPart,
        file_size.LowPart,
        NULL);

    if (info->mapping_handle == NULL)
    {
        DWORD error = GetLastError();
        printf("CreateFileMappingA failed with error: %lu\n", error);
        CloseHandle(info->file_handle);
        free(info);
        return NULL;
    }

    info->map = MapViewOfFile(
        info->mapping_handle,
        FILE_MAP_READ,
        0,
        0,
        0);

    if (info->map == NULL)
    {
        DWORD error = GetLastError();
        printf("MapViewOfFile failed with error: %lu\n", error);
        CloseHandle(info->mapping_handle);
        CloseHandle(info->file_handle);
        free(info);
        return NULL;
    }

#else
    // POSIX implementation
    info->fd = open(filename, O_RDONLY);
    if (info->fd == -1)
    {
        free(info);
        return NULL;
    }

    struct stat sb;
    if (fstat(info->fd, &sb) == -1)
    {
        close(info->fd);
        free(info);
        return NULL;
    }
    info->size = sb.st_size;

    info->map = mmap(NULL, info->size, PROT_READ, MAP_PRIVATE, info->fd, 0);
    if (info->map == MAP_FAILED)
    {
        close(info->fd);
        free(info);
        return NULL;
    }
#endif

    return info;
}

MmapInfo *create_mmap_with_retry(const char *filename, int max_retries)
{
    for (int i = 0; i < max_retries; i++)
    {
        MmapInfo *info = create_mmap(filename);
        if (info)
            return info;

#ifdef _WIN32
        Sleep(50); // Wait 50ms between attempts
#else
        usleep(50000);
#endif
    }
    return NULL;
}

void free_mmap(MmapInfo *info)
{
    if (!info)
        return;

#ifdef _WIN32
    if (info->map != NULL)
    {
        if (!UnmapViewOfFile(info->map))
        {
            // Handle error - GetLastError()
        }
        info->map = NULL;
    }
    if (info->mapping_handle != INVALID_HANDLE_VALUE && info->mapping_handle != NULL)
    {
        if (!CloseHandle(info->mapping_handle))
        {
            // Handle error
        }
        info->mapping_handle = NULL;
    }
    if (info->file_handle != INVALID_HANDLE_VALUE && info->file_handle != NULL)
    {
        if (!CloseHandle(info->file_handle))
        {
            // Handle error
        }
        info->file_handle = NULL;
    }
#else
    if (info->map != MAP_FAILED && info->map != NULL)
    {
        if (munmap(info->map, info->size) == -1)
        {
            // Handle error - errno
        }
        info->map = NULL;
    }
    if (info->fd != -1)
    {
        if (close(info->fd) == -1)
        {
            // Handle error
        }
        info->fd = -1;
    }
#endif
    free(info);
}

FileMetadataPaths *get_metadata_file_paths(const char *file_path)
{
    FileMetadataPaths *paths = malloc(sizeof(FileMetadataPaths));
    if (!paths)
        return NULL;

    paths->idx_path = malloc(MAX_PATH);
    paths->md_path = malloc(MAX_PATH);

    if (!paths->idx_path || !paths->md_path)
    {
        free_metadata_file_paths(paths);
        return NULL;
    }

    snprintf(paths->idx_path, MAX_PATH, "%s.idx", file_path);
    snprintf(paths->md_path, MAX_PATH, "%s.meta", file_path);

    return paths;
}

void free_metadata_file_paths(FileMetadataPaths *paths)
{
    if (!paths)
        return;

    free(paths->idx_path);
    free(paths->md_path);
    free(paths);
}

VecFileHeaderInfo *get_vec_file_header_info(FILE *vec_file, const uint32_t dimensions)
{
    // Always seek to beginning of file first
    fseek(vec_file, 0, SEEK_SET);
    // Save starting position
    long start_pos = ftell(vec_file);

    uint32_t total_vectors = 0;
    if (fread(&total_vectors, sizeof(uint32_t), 1, vec_file) != 1)
    {
        // Seek back to start before writing
        fseek(vec_file, start_pos, SEEK_SET);
        if (fwrite(&total_vectors, sizeof(uint32_t), 1, vec_file) != 1)
        {
            printf("Failed to write total vectors to vector file\n");
            return NULL;
        }
    }

    uint32_t read_dimensions = 0;
    size_t read_dimensions_result = fread(&read_dimensions, sizeof(uint32_t), 1, vec_file);
    if (read_dimensions_result != 1)
    {
        // Seek to dimensions position
        fseek(vec_file, start_pos + sizeof(uint32_t), SEEK_SET);
        read_dimensions = dimensions; // Use the provided dimensions
        if (fwrite(&read_dimensions, sizeof(uint32_t), 1, vec_file) != 1)
        {
            printf("Failed to write dimensions to vector file\n");
            return NULL;
        }
    }
    else if (read_dimensions != dimensions && dimensions != 0)
    {

        fseek(vec_file, start_pos + sizeof(uint32_t), SEEK_SET);
        fwrite(&dimensions, sizeof(uint32_t), 1, vec_file);
    }

    VecFileHeaderInfo *header_info = malloc(sizeof(VecFileHeaderInfo));
    if (!header_info)
        return NULL;

    if (dimensions == 0)
    {
        header_info->dimensions = read_dimensions;
    }
    else
    {
        header_info->dimensions = dimensions;
    }
    header_info->vector_count = total_vectors;

    // Reset file position to end of header
    // Skip past the total vectors and dimensions bytes
    fseek(vec_file, start_pos + 2 * sizeof(uint32_t), SEEK_SET);

    return header_info;
}

bool file_exists(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    bool is_exist = false;
    if (fp != NULL)
    {
        is_exist = true;
        fclose(fp); // close the file
    }
    return is_exist;
}

MetadataBytes get_vec_metadata(const int idx_offset, FILE *idx_file, FILE *md_file, long idx_size, long md_size)
{
    MetadataBytes result = {NULL, 0};

    // First validate idx_offset is within bounds
    if (idx_offset + 12 > idx_size)
    {
        // Need 12 bytes (8 for offset + 4 for length)
        printf("Index offset out of bounds: %d > %ld\n", idx_offset + 12, idx_size);
        return result;
    }

    // Seek to the index offset
    if (fseek(idx_file, idx_offset, SEEK_SET) != 0)
    {
        printf("Failed to seek to index offset\n");
        return result;
    }

    // Read offset and length
    uint64_t offset;
    uint32_t length;

    if (fread(&offset, sizeof(offset), 1, idx_file) != 1)
    {
        printf("Failed to read metadata offset\n");
        return result;
    }

    if (fread(&length, sizeof(length), 1, idx_file) != 1)
    {
        printf("Failed to read metadata length\n");
        return result;
    }

    // Validate offset and length
    if (offset >= md_size || offset + length > md_size)
    {
        printf("Invalid metadata offset/length - would exceed file size\n");
        return result;
    }

    // Validate length isn't unreasonable
    if (length > 1024 * 1024)
    {
        // Cap at 1MB
        printf("Suspiciously large metadata length: %u\n", length);
        return result;
    }

    // Allocate buffer for metadata
    result.data = (unsigned char *)malloc(length);
    if (!result.data)
    {
        printf("Failed to allocate %u bytes for metadata\n", length);
        return result;
    }

    // Seek to metadata offset
    if (fseek(md_file, offset, SEEK_SET) != 0)
    {
        printf("Failed to seek to metadata offset\n");
        free(result.data);
        result.data = NULL;
        return result;
    }

    // Read metadata
    if (fread(result.data, 1, length, md_file) != length)
    {
        printf("Failed to read metadata bytes\n");
        free(result.data);
        result.data = NULL;
        return result;
    }

    result.length = length;
    return result;
}

int reset_file_positions(FILE *idx_file, FILE *md_file, long idx_pos, long md_pos)
{
    if (fseek(idx_file, idx_pos, SEEK_SET) != 0)
    {
        printf("Failed to reset index file position\n");
        return 0;
    }

    if (fseek(md_file, md_pos, SEEK_SET) != 0)
    {
        printf("Failed to reset metadata file position\n");
        return 0;
    }

    return 1;
}

bool create_file(const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
        return false;
    fclose(fp);
    return true;
}

FILE *open_db_file(const char *file_path)
{
    FILE *db_file;

#ifdef _WIN32
    // Windows-specific sharing mode
    // Try to open existing file first
    if (!file_exists(file_path))
    {

        if (!create_file(file_path))
        {
            printf("Failed to create file: %s\n", file_path);
            return NULL;
        }
    }
    db_file = _fsopen(file_path, "r+b", _SH_DENYNO);
#else
    // Unix-like systems (Linux, macOS, etc.)
    db_file = fopen(file_path, "r+b");
    if (!db_file)
    {
        FILE *new_db_file = fopen(file_path, "wb");
        if (new_db_file)
        {
            fclose(new_db_file);
            db_file = fopen(file_path, "r+b");
            if (!db_file)
            {
                printf("Failed to reopen vector file\n");
                return NULL;
            }
        }
        else
        {
            printf("Failed to create vector file\n");
            return NULL;
        }
    }
#endif

    return db_file;
}
