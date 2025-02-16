#ifndef TINYVEC_H
#define TINYVEC_H

#include "../../../src/core/include/vec_types.h"
#include "../../../src/core/include/file.h"
#include "../../../src/core/include/db.h"

#ifdef __cplusplus
extern "C"
{
#endif

    VecResult *vector_query(const char *file_path, const float *query_vec, const int top_k);

    int insert_many_vectors(const char *file_path, float **vectors, char **metadatas, size_t *metadata_lengths,
                            const size_t vec_count, const uint32_t dimensions);

    TinyVecConnection *connect_to_db(const char *file_path, const TinyVecConnectionConfig *config);

    IndexFileStats get_index_file_stats_from_db(const char *file_path);

#ifdef __cplusplus
}
#endif

#endif // TINYVEC_H