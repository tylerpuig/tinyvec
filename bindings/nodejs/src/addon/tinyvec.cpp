#include "tinyvec.h"
#include "../../../src/core/include/db.h"
#include "../../../src/core/include/file.h"
#include "../../../src/core/include/paginate.h"
#include <iostream>

extern "C" DBSearchResult *vector_query(const char *file_path, const float *query_vec, const int top_k)
{
    return get_top_k(file_path, query_vec, top_k);
}

extern "C" DBSearchResult *vector_query_with_filter(const char *file_path, const float *query_vec, const int top_k, const char *json_filter)
{
    return get_top_k_with_filter(file_path, query_vec, top_k, json_filter);
}

extern "C" int delete_vecs_by_ids(const char *file_path, int *ids_to_delete, int delete_count)
{
    return delete_data_by_ids(file_path, ids_to_delete, delete_count);
}

extern "C" int delete_vecs_by_filter(const char *file_path, const char *json_filter)
{
    return delete_data_by_filter(file_path, json_filter);
}

extern "C" int insert_many_vectors(const char *file_path, float **vectors, char **metadatas, size_t *metadata_lengths,
                                   const size_t vec_count, const uint32_t dimensions)
{
    return insert_data(file_path, vectors, metadatas, metadata_lengths, vec_count, dimensions);
}

extern "C" TinyVecConnection *connect_to_db(const char *file_path, const TinyVecConnectionConfig *config)
{

    TinyVecConnection *connection = create_tiny_vec_connection(file_path, config->dimensions);

    if (!connection)
    {
        std::cout << "Failed to create connection" << std::endl;
        return NULL;
    }

    return connection;
}

extern "C" IndexFileStats get_index_file_stats_from_db(const char *file_path)
{

    return get_index_stats(file_path);
}

extern "C" bool update_instance_db_file_connection(const char *file_path)
{
    return update_db_file_connection(file_path);
}

extern "C" int update_items_by_id(const char *file_path, DBUpdateItem *items, int item_count)
{
    return batch_update_items_by_id(file_path, items, item_count);
}
extern "C" PaginationResults *get_paginated_vectors(const char *file_path, const int skip, const int limit)
{
    return get_vectors_with_pagination(file_path, skip, limit);
}