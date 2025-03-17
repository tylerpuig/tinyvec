#ifndef VEC_TYPES_H
#define VEC_TYPES_H

#include <stdint.h>

typedef struct MetadataBytes
{
    unsigned char *data;
    uint32_t length;
} MetadataBytes;

typedef struct VecResult
{
    float similarity;
    int index;
    MetadataBytes metadata;
} VecResult;

typedef struct DBUpdateItem
{
    int id;
    char *metadata;
    float *vector;
    int vector_length;
} DBUpdateItem;

typedef struct DBSearchResult
{
    VecResult *results;
    int count;
} DBSearchResult;

typedef struct TinyVecConnectionConfig
{
    uint32_t dimensions;
} TinyVecConnectionConfig;

typedef struct ConnectionData
{
    char *file_path;
    uint32_t dimensions;
} ConnectionData;

#endif // VEC_TYPES_H