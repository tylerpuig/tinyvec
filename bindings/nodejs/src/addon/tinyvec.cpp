#include "tinyvec.h"
#include "../../../src/core/include/db.h"
#include "../../../src/core/include/file.h"
#include <iostream>

extern "C" VecResult *vector_query(const char *file_path, const float *query_vec, const int top_k)
{
    return get_top_k(file_path, query_vec, top_k);
}

extern "C" int insert_many_vectors(const char *file_path, float **vectors, char **metadatas, size_t *metadata_lengths,
                                   const size_t vec_count, const uint32_t dimensions)
{
    int inserted_vecs = insert_data(file_path, vectors, metadatas, metadata_lengths, vec_count, dimensions);
    return inserted_vecs;
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

    IndexFileStats stats = get_index_stats(file_path);
    return stats;
}