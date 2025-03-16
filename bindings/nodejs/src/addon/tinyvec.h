#ifndef TINYVEC_H
#define TINYVEC_H

#include "../../../src/core/include/vec_types.h"
#include "../../../src/core/include/file.h"
#include "../../../src/core/include/db.h"

#ifdef __cplusplus
extern "C"
{
#endif

    DBSearchResult *vector_query(const char *file_path, const float *query_vec, const int top_k);
    DBSearchResult *vector_query_with_filter(const char *file_path, const float *query_vec, const int top_k, const char *json_filter);
    int delete_vecs_by_ids(const char *file_path, int *ids_to_delete, int delete_count);
    int delete_vecs_by_filter(const char *file_path, const char *json_filter);
    int update_items_by_id(const char *file_path, DBUpsertIem *items, int item_count);

    int
    insert_many_vectors(const char *file_path, float **vectors, char **metadatas, size_t *metadata_lengths,
                        const size_t vec_count, const uint32_t dimensions);

    TinyVecConnection *connect_to_db(const char *file_path, const TinyVecConnectionConfig *config);

    IndexFileStats get_index_file_stats_from_db(const char *file_path);
    bool update_instance_db_file_connection(const char *file_path);

#ifdef __cplusplus
}
#endif

#endif // TINYVEC_H