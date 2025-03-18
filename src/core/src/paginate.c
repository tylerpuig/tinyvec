#include "../include/paginate.h"
#include "../include/vec_types.h"
#include "../include/db.h"

PaginationResults *get_vectors_with_pagination(const char *file_path, const int skip, const int limit)
{
    TinyVecConnection *connection = NULL;
    VecFileHeaderInfo *header_info = NULL;
    float *vec_buffer = NULL;
    PaginationItem *results = NULL;
    int total_results = 0;

    // Get connection
    connection = get_tinyvec_connection(file_path);
    if (!connection)
    {
        printf("Failed to get connection\n");
        return NULL;
    }

    printf("file_path: %s\n", file_path);
    printf("dimensions: %d\n", connection->dimensions);
    printf("skip: %d\n", skip);
    printf("limit: %d\n", limit);

    // Get header info, also resets the file position
    header_info = get_vec_file_header_info(connection->vec_file, connection->dimensions);
    if (!header_info)
    {
        printf("Failed to get header info\n");
        return NULL;
    }

    if (header_info->dimensions == 0 || header_info->vector_count == 0)
    {
        printf("Dimensions or vector count is zero\n");
        free(header_info);
        return NULL;
    }

    // Simple pagination on all vectors
    int total_count = header_info->vector_count;

    // Ensure skip is within bounds
    if (skip >= total_count)
    {
        printf("Skip is greater than total count\n");
        free(header_info);
        return NULL;
    }

    // Calculate effective limit
    int effective_limit = limit;
    if (skip + effective_limit > total_count)
    {
        effective_limit = total_count - skip;
    }

    // Calculate the record size (one float for ID + dimensions floats for the vector)
    const size_t record_size = (header_info->dimensions + 1) * sizeof(float);
    // printf("Record size: %zu\n", record_size);

    // Just need to skip ahead by the number of records we want to skip
    if (fseek(connection->vec_file, skip * record_size, SEEK_CUR) != 0)
    {
        printf("Failed to seek to skip position\n");
        free(header_info);
        return NULL;
    }

    // Allocate buffer for the paginated vectors
    vec_buffer = (float *)aligned_malloc(record_size * effective_limit, 16);
    if (!vec_buffer)
    {
        printf("Failed to allocate vec_buffer\n");
        free(header_info);
        return NULL;
    }

    // Read all the paginated vectors at once
    size_t read = fread(vec_buffer, record_size, effective_limit, connection->vec_file);
    if (read != effective_limit)
    {
        printf("Failed to read vec_buffer\n");
        aligned_free(vec_buffer);
        free(header_info);
        return NULL;
    }

    // Allocate results array
    results = (PaginationItem *)malloc(sizeof(PaginationItem) * effective_limit);
    if (!results)
    {
        printf("Failed to allocate results\n");
        aligned_free(vec_buffer);
        free(header_info);
        return NULL;
    }

    // Initialize the results array
    memset(results, 0, sizeof(PaginationItem) * effective_limit);

    // Allocate memory for the ID list string
    // Each ID could take up to 10 digits, plus a comma and some extra space
    char *id_list = (char *)malloc(effective_limit * 12);
    if (!id_list)
    {

        printf("Failed to allocate id_list\n");
        // Cleanup allocated vectors
        for (int i = 0; i < effective_limit; i++)
        {
            if (results[i].vector)
            {
                free(results[i].vector);
            }
        }

        free(results);
        aligned_free(vec_buffer);
        free(header_info);
        return NULL;
    }

    id_list[0] = '\0';

    printf("id_list: '%s'\n", id_list);

    for (int i = 0; i < effective_limit; i++)
    {
        float *vec_block = vec_buffer + (i * (header_info->dimensions + 1));
        int metadata_id = (int)vec_block[0];

        printf("ID: %d\n", metadata_id);

        // Store ID
        results[i].id = metadata_id;

        // Copy vector data (excluding the ID)
        results[i].vector_length = header_info->dimensions;
        results[i].vector = (float *)malloc(sizeof(float) * header_info->dimensions);
        if (results[i].vector)
        {
            memcpy(results[i].vector, vec_block + 1, sizeof(float) * header_info->dimensions);
        }

        // Initialize metadata pointer to NULL
        results[i].metadata = NULL;

        // Add ID to list for SQL query
        char id_str[12];
        snprintf(id_str, sizeof(id_str), "%d,", metadata_id);
        strcat(id_list, id_str);
    }

    // Remove trailing comma
    size_t len = strlen(id_list);
    if (len > 0)
    {
        id_list[len - 1] = '\0';
    }

    printf("len: %zu\n", len);

    // Create SQL query to get metadata for all IDs at once
    char *sql_query = NULL;
    size_t query_len = snprintf(NULL, 0, "SELECT id, metadata FROM metadata WHERE id IN (%s)", id_list) + 1;
    sql_query = (char *)malloc(query_len);
    if (sql_query != NULL)
    {
        snprintf(sql_query, query_len, "SELECT id, metadata FROM metadata WHERE id IN (%s)", id_list);
    }
    printf("SQL query: '%s'\n", sql_query);

    // Execute the query
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(connection->sqlite_db, sql_query, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Failed to prepare statement\n");
        free(sql_query);

        // Cleanup allocated vectors
        for (int i = 0; i < effective_limit; i++)
        {
            if (results[i].vector)
            {
                free(results[i].vector);
            }
        }

        free(results);
        aligned_free(vec_buffer);
        free(header_info);
        return NULL;
    }

    IdIndexPair *id_index_pairs = (IdIndexPair *)malloc(sizeof(IdIndexPair) * effective_limit);
    if (!id_index_pairs)
    {
        printf("Failed to allocate id_index_pairs\n");
        free(sql_query);

        // Cleanup allocated vectors
        for (int i = 0; i < effective_limit; i++)
        {
            if (results[i].vector)
            {
                free(results[i].vector);
            }
        }

        free(results);
        aligned_free(vec_buffer);
        free(header_info);
        return NULL;
    }

    // Fill the array with ID-index pairs
    for (int i = 0; i < effective_limit; i++)
    {
        id_index_pairs[i].id = results[i].id;
        id_index_pairs[i].index = i;
    }

    printf("Filled array\n");

    // Sort the array by ID for binary search
    qsort(id_index_pairs, effective_limit, sizeof(IdIndexPair), compare_id_index_pairs);

    printf("Sorted\n");

    // Process the metadata results
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *metadata = (const char *)sqlite3_column_text(stmt, 1);

        // Binary search for the ID
        IdIndexPair key = {.id = id};
        IdIndexPair *found = bsearch(&key, id_index_pairs, effective_limit,
                                     sizeof(IdIndexPair), compare_id_index_pairs);

        if (found)
        {

            printf("Found ID: %d\n", id);

            // printf("Found ID: %d\n", found->id);
            int index = found->index;

            // Copy metadata if present
            if (metadata)
            {
                size_t metadata_len = strlen(metadata) + 1;
                results[index].metadata = (char *)malloc(metadata_len);
                if (results[index].metadata)
                {
                    memcpy(results[index].metadata, metadata, metadata_len);
                }
            }
        }
    }

    sqlite3_finalize(stmt);
    free(id_list);
    free(sql_query);
    free(id_index_pairs);

    // Cleanup the vector buffer
    aligned_free(vec_buffer);
    free(header_info);

    // Create and return the final result structure
    PaginationResults *pagination_results = (PaginationResults *)malloc(sizeof(PaginationResults));
    if (!pagination_results)
    {
        // Cleanup if allocation fails
        for (int i = 0; i < effective_limit; i++)
        {
            if (results[i].vector)
            {
                free(results[i].vector);
            }
            if (results[i].metadata)
            {
                free(results[i].metadata);
            }
        }
        free(results);
        return NULL;
    }

    printf("Returning results\n");
    pagination_results->results = results;
    pagination_results->count = effective_limit;
    return pagination_results;
}

int compare_id_index_pairs(const void *a, const void *b)
{
    return ((IdIndexPair *)a)->id - ((IdIndexPair *)b)->id;
}