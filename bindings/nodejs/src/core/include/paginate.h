#ifndef PAGINATE_H
#define PAGINATE_H

#include "../include/vec_types.h"

typedef struct IdIndexPair
{
    int id;
    int index;
} IdIndexPair;

#ifdef __cplusplus
extern "C"
{
#endif

    PaginationResults *get_vectors_with_pagination(const char *file_path, const int skip, const int limit);
    int compare_id_index_pairs(const void *a, const void *b);

#ifdef __cplusplus
}
#endif

#endif // PAGINATE_H