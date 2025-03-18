#include <stdlib.h> /* For malloc, free, and bsearch */
#include <string.h> /* For memset, memcpy, strcat, and strlen */
#include "../include/paginate.h"
#include "../include/vec_types.h"
#include "../include/db.h"

const int MAX_IDS_PER_BATCH = 999;

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
        return NULL;
    }

    // Get header info, also resets the file position
    header_info = get_vec_file_header_info(connection->vec_file, connection->dimensions);
    if (!header_info)
    {
        return NULL;
    }

    if (header_info->dimensions == 0 || header_info->vector_count == 0)
    {

        free(header_info);
        return NULL;
    }

    // Simple pagination on all vectors
    int total_count = header_info->vector_count;

    // Ensure skip is within bounds
    if (skip >= total_count)
    {

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

    // Just need to skip ahead by the number of records we want to skip
    if (fseek(connection->vec_file, skip * record_size, SEEK_CUR) != 0)
    {
        free(header_info);
        return NULL;
    }

    // Allocate buffer for the paginated vectors
    vec_buffer = (float *)aligned_malloc(record_size * effective_limit, 16);
    if (!vec_buffer)
    {
        free(header_info);
        return NULL;
    }

    // Read all the paginated vectors at once
    size_t read = fread(vec_buffer, record_size, effective_limit, connection->vec_file);
    if (read != effective_limit)
    {
        aligned_free(vec_buffer);
        free(header_info);
        return NULL;
    }

    // Allocate results array
    results = (PaginationItem *)malloc(sizeof(PaginationItem) * effective_limit);
    if (!results)
    {
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

    for (int i = 0; i < effective_limit; i++)
    {
        float *vec_block = vec_buffer + (i * (header_info->dimensions + 1));
        int metadata_id = (int)vec_block[0];

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

    for (int batch_start = 0; batch_start < effective_limit; batch_start += MAX_IDS_PER_BATCH)
    {
        int batch_size = (batch_start + MAX_IDS_PER_BATCH > effective_limit) ? (effective_limit - batch_start) : MAX_IDS_PER_BATCH;

        if (!get_metadata_batch_paginate(connection->sqlite_db, results, batch_start, batch_size))
        {
            // Handle error - cleanup and return NULL
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
            aligned_free(vec_buffer);
            free(header_info);
            return NULL;
        }
    }

    free(id_list);

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

    pagination_results->results = results;
    pagination_results->count = effective_limit;
    return pagination_results;
}

int compare_id_index_pairs(const void *a, const void *b)
{
    return ((IdIndexPair *)a)->id - ((IdIndexPair *)b)->id;
}

// Function to get and process metadata for a batch of IDs
bool get_metadata_batch_paginate(sqlite3 *db, PaginationItem *results, int batch_start, int batch_size)
{
    char *id_list = NULL;
    char *sql_query = NULL;
    sqlite3_stmt *stmt = NULL;
    IdIndexPair *id_index_pairs = NULL;
    bool success = false;

    // Create ID list string for this batch
    id_list = create_id_list(&results[batch_start], batch_size);
    if (!id_list)
    {
        goto cleanup;
    }

    // Create SQL query
    size_t query_len = snprintf(NULL, 0, "SELECT id, metadata, metadata_length FROM metadata WHERE id IN (%s)", id_list) + 1;
    sql_query = (char *)malloc(query_len);
    if (!sql_query)
    {
        goto cleanup;
    }
    snprintf(sql_query, query_len, "SELECT id, metadata, metadata_length FROM metadata WHERE id IN (%s)", id_list);

    // Prepare statement
    if (sqlite3_prepare_v2(db, sql_query, -1, &stmt, NULL) != SQLITE_OK)
    {
        goto cleanup;
    }

    // Create ID-index pairs for this batch
    id_index_pairs = (IdIndexPair *)malloc(sizeof(IdIndexPair) * batch_size);
    if (!id_index_pairs)
    {
        goto cleanup;
    }

    // Fill and sort the ID-index pairs
    for (int i = 0; i < batch_size; i++)
    {
        id_index_pairs[i].id = results[batch_start + i].id;
        id_index_pairs[i].index = batch_start + i;
    }
    qsort(id_index_pairs, batch_size, sizeof(IdIndexPair), compare_id_index_pairs);

    // Process the metadata results
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *metadata = (const char *)sqlite3_column_text(stmt, 1);
        int metadata_length = sqlite3_column_int(stmt, 2);

        // Binary search for the ID
        IdIndexPair key = {.id = id};
        IdIndexPair *found = bsearch(&key, id_index_pairs, batch_size,
                                     sizeof(IdIndexPair), compare_id_index_pairs);

        if (found)
        {
            int index = found->index;
            if (metadata)
            {
                size_t metadata_len = metadata_length + 1;
                results[index].metadata = (char *)malloc(metadata_len);
                results[index].md_length = metadata_length;
                if (results[index].metadata)
                {
                    memcpy(results[index].metadata, metadata, metadata_length);
                    results[index].metadata[metadata_length] = '\0'; // Ensure null termination
                }
            }
        }
    }

    success = true;

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    if (sql_query)
        free(sql_query);
    if (id_list)
        free(id_list);
    if (id_index_pairs)
        free(id_index_pairs);

    return success;
}

// Helper function to create comma-separated ID list
char *create_id_list(PaginationItem *items, int count)
{
    // Each ID could take up to 10 digits, plus a comma and some extra space
    char *id_list = (char *)malloc(count * 12);
    if (!id_list)
    {
        return NULL;
    }

    id_list[0] = '\0';

    for (int i = 0; i < count; i++)
    {
        char id_str[12];
        snprintf(id_str, sizeof(id_str), "%d,", items[i].id);
        strcat(id_list, id_str);
    }

    // Remove trailing comma
    size_t len = strlen(id_list);
    if (len > 0)
    {
        id_list[len - 1] = '\0';
    }

    return id_list;
}