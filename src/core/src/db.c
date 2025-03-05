// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#include <share.h>
#include <io.h>    // for _get_osfhandle
#include <fcntl.h> // for _fileno
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

// Project includes
#include "../include/db.h"
#include "../include/file.h"
#include "../include/minheap.h"
#include "../include/distance.h"
#include "../include/sqlite3.h"
#include "../include/vec_types.h"
#include "../include/query_convert.h"
#include "../include/utils.h"

// Architecture-specific SIMD includes
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#include <xmmintrin.h>
#elif defined(__APPLE__) && defined(__aarch64__)
#include <arm_neon.h>
#endif

// Platform-specific prefetch macros
#if defined(__APPLE__) && defined(__aarch64__)
#define PREFETCH(addr) __builtin_prefetch(addr)
#elif defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
#define PREFETCH(addr) _mm_prefetch((char *)(addr), _MM_HINT_T0)
#else
#define PREFETCH(addr) __builtin_prefetch(addr)
#endif

// Platform-specific aligned memory functions
#ifdef _WIN32
#define aligned_malloc(size, alignment) _aligned_malloc(size, alignment)
#define aligned_free(ptr) _aligned_free(ptr)
#else
#define aligned_malloc(size, alignment) aligned_alloc(alignment, size)
#define aligned_free(ptr) free(ptr)
#endif

ActiveTinyVecConnections *active_tinyvec_connections = NULL;

TinyVecConnection *create_tiny_vec_connection(const char *file_path, const uint32_t dimensions)
{

    TinyVecConnection *current_connection = get_tinyvec_connection(file_path);
    if (current_connection)
    {
        return current_connection;
    }

    // Open vector file
    FILE *vec_file = fopen(file_path, "r+b");
    if (!vec_file)
    {
        printf("Failed to open vector file\n");
        return NULL;
    }

    VecFileHeaderInfo *header_info = get_vec_file_header_info(vec_file, dimensions);
    if (!header_info)
    {
        printf("Failed to get vector file header info\n");
        fclose(vec_file);
        return NULL;
    }

    // Get metadata file paths
    FileMetadataPaths *metadata_paths = get_metadata_file_paths(file_path);
    if (!metadata_paths)
    {
        printf("Failed to get metadata file paths\n");
        fclose(vec_file);
        return NULL;
    }

    // Use 1MB buffer for file I/O
    if (setvbuf(vec_file, NULL, _IOFBF, 1024 * 1024) != 0)
    {
        printf("Failed to set vector file buffer\n");
        free_metadata_file_paths(metadata_paths);
        fclose(vec_file);
        return NULL;
    }

    TinyVecConnection *connection = malloc(sizeof(TinyVecConnection));
    if (!connection)
    {
        free_metadata_file_paths(metadata_paths);
        fclose(vec_file);
        return NULL;
    }

    // Path for SQLite database
    char *sqlite_path = malloc(strlen(file_path) + 9); // +9 for ".metadata.db\0"
    sprintf(sqlite_path, "%s.metadata.db", file_path);
    if (!sqlite_path)
    {
        free(header_info);
        free_metadata_file_paths(metadata_paths);
        fclose(vec_file);
        return NULL;
    }

    sqlite3 *db;
    int rc = sqlite3_open(sqlite_path, &db);
    if (rc != SQLITE_OK)
    {
        printf("Failed to open SQLite database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        free_metadata_file_paths(metadata_paths);
        free(sqlite_path);
        free(header_info);
        fclose(vec_file);
        return NULL;
    }

    // Set WAL journal mode for better performance
    char *err_msg = NULL;
    rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        printf("Failed to set WAL mode: %s\n", err_msg);
        sqlite3_free(err_msg);
        // Continue anyway, this is not fatal
    }

    // Make a proper copy of the file path
    size_t path_len = strlen(file_path) + 1;
    char *path_copy = (char *)malloc(path_len);
    if (!path_copy)
    {
        free(connection);
        fclose(vec_file);
        free_metadata_file_paths(metadata_paths);
        free(header_info);
        return NULL;
    }

    strcpy(path_copy, file_path);
    connection->file_path = path_copy;
    connection->dimensions = header_info->dimensions;
    connection->vec_file = vec_file;
    connection->sqlite_db = db;
    free(sqlite_path);

    add_to_connection_pool(connection);

    return connection;
}

TinyVecConnection *get_tinyvec_connection(const char *file_path)
{

    if (!active_tinyvec_connections)
    {
        return NULL;
    }

    for (int i = 0; i < active_tinyvec_connections->active_connections; i++)
    {

        if (strcmp(active_tinyvec_connections->connections[i]->file_path, file_path) == 0)
        {
            return active_tinyvec_connections->connections[i];
        }
    }

    return NULL;
}

bool add_to_connection_pool(TinyVecConnection *connection)
{
    // Initialize if NULL
    if (!active_tinyvec_connections)
    {
        active_tinyvec_connections = malloc(sizeof(ActiveTinyVecConnections));
        if (!active_tinyvec_connections)
            return false;

        active_tinyvec_connections->connections = NULL;
        active_tinyvec_connections->active_connections = 0;
    }

    // Resize array to accommodate new connection
    TinyVecConnection **temp = realloc(active_tinyvec_connections->connections,
                                       (active_tinyvec_connections->active_connections + 1) * sizeof(TinyVecConnection *));

    if (!temp)
    {
        return false;
    }

    active_tinyvec_connections->connections = temp;

    // Add the new connection
    active_tinyvec_connections->connections[active_tinyvec_connections->active_connections] = connection;
    active_tinyvec_connections->active_connections++;

    return true;
}

IndexFileStats get_index_stats(const char *file_path)
{

    TinyVecConnection *connection = get_tinyvec_connection(file_path);
    if (!connection)
    {
        printf("Failed to get TinyVec connection\n");
        return (IndexFileStats){0, 0};
    }

    VecFileHeaderInfo *header_info = get_vec_file_header_info(connection->vec_file, connection->dimensions);
    if (!header_info)
    {
        printf("Failed to get file header info\n");
        return (IndexFileStats){0, 0};
    }

    IndexFileStats stats = (IndexFileStats){header_info->vector_count, header_info->dimensions};

    free(header_info);
    return stats;
}

DBSearchResult *get_top_k_with_filter(const char *file_path, const float *query_vec,
                                      const int top_k, const char *json_filter)
{
    TinyVecConnection *connection = NULL;
    VecFileHeaderInfo *header_info = NULL;
    float *vec_buffer = NULL;
    MinHeap *min_heap = NULL;
    VecResult *sorted = NULL;
    int *filtered_ids = NULL;
    int filtered_count = 0;

    // Get connection
    connection = get_tinyvec_connection(file_path);
    if (!connection)
    {
        printf("Failed to get connection\n");
        goto cleanup;
    }

    // Get header info, also resets the file position
    header_info = get_vec_file_header_info(connection->vec_file, connection->dimensions);
    if (!header_info)
    {
        printf("Failed to get header info\n");
        goto cleanup;
    }

    if (header_info->dimensions == 0 || header_info->vector_count == 0)
    {
        goto cleanup;
    }

    // Convert JSON filter to SQL
    char *sql_where = json_query_to_sql(json_filter);

    // Get filtered IDs from SQLite
    if (!get_filtered_ids(connection->sqlite_db, sql_where, &filtered_ids, &filtered_count))
    {
        printf("Failed to get filtered IDs\n");
        free(sql_where);
        goto cleanup;
    }

    free(sql_where);

    // If no results match the filter, return empty result
    if (filtered_count == 0)
    {
        printf("No results match the filter\n");
        return NULL;
    }

    // Sort the IDs for binary search later
    qsort(filtered_ids, filtered_count, sizeof(int), compare_ints);

    // Allocate vector buffer
    const int BUFFER_SIZE = calculate_optimal_buffer_size(header_info->dimensions);
    vec_buffer = (float *)aligned_malloc(sizeof(float) * (header_info->dimensions + 1) * BUFFER_SIZE, 32);
    if (!vec_buffer)
    {
        printf("Failed to allocate vector buffer\n");
        goto cleanup;
    }

    float *query_vec_norm = get_normalized_vector(query_vec, header_info->dimensions);

    for (uint64_t i = 0; i < header_info->dimensions; i += 64 / sizeof(float))
    {
        PREFETCH((char *)&query_vec_norm[i]);
    }

    // Create min heap
    min_heap = createMinHeap(top_k);
    if (!min_heap)
    {
        printf("Failed to create min heap\n");
        goto cleanup;
    }

    // Process vectors
    for (uint64_t i = 0; i < header_info->vector_count; i += BUFFER_SIZE)
    {
        int vectors_to_read = fmin(BUFFER_SIZE, header_info->vector_count - i);
        size_t read = fread(vec_buffer, sizeof(float) * (header_info->dimensions + 1), vectors_to_read, connection->vec_file);
        if (read != vectors_to_read)
        {

            continue;
        }

        for (int j = 0; j < vectors_to_read; j++)
        {

            // Get pointer to start of vector+id block
            float *vec_block = vec_buffer + (j * (header_info->dimensions + 1));

            // Extract metadata ID from first float
            int metadata_id = (int)vec_block[0];

            if (binary_search_int(filtered_ids, filtered_count, metadata_id) == -1)
            {
                continue;
            }

            // Actual vector data starts after the metadata ID
            float *current_vec = vec_block + 1;
            PREFETCH((char *)(current_vec + header_info->dimensions));
            float dot = dot_product(query_vec_norm, current_vec, header_info->dimensions);

            if (dot > min_heap->data[0] || min_heap->size < top_k)
            {
                insert(min_heap, dot, metadata_id);
            }
        }
    }

    // Create sorted results
    sorted = createVecResult(min_heap, top_k);
    if (!sorted)
    {
        printf("Failed to create sorted results\n");
        goto cleanup;
    }

    get_metadata_batch(connection->sqlite_db, sorted, min_heap->size);

    int results_found = min_heap->size;

    if (vec_buffer)
        aligned_free(vec_buffer);
    if (header_info)
        free(header_info);
    if (min_heap)
        freeHeap(min_heap);
    if (query_vec_norm)
        free(query_vec_norm);

    DBSearchResult *search_result = malloc(sizeof(DBSearchResult));
    search_result->results = sorted;
    search_result->count = results_found;
    return search_result;

cleanup:
    if (filtered_ids)
        free(filtered_ids);
    if (min_heap && sorted)
    {
        for (int i = 0; i < min_heap->size; i++)
        {
            if (sorted[i].metadata.data)
            {
                free(sorted[i].metadata.data);
            }
        }
        if (sorted)
            free(sorted);
    }

    if (query_vec_norm)
        free(query_vec_norm);
    if (vec_buffer)
        aligned_free(vec_buffer);
    if (header_info)
        free(header_info);
    if (min_heap)
        freeHeap(min_heap);

    return NULL;
}

DBSearchResult *get_top_k(const char *file_path, const float *query_vec, const int top_k)
{
    TinyVecConnection *connection = NULL;
    VecFileHeaderInfo *header_info = NULL;
    float *vec_buffer = NULL;
    MinHeap *min_heap = NULL;
    VecResult *sorted = NULL;

    // Get connection
    connection = get_tinyvec_connection(file_path);
    if (!connection)
    {
        printf("Failed to get connection\n");
        goto cleanup;
    }

    // Get header info, also resets the file position
    header_info = get_vec_file_header_info(connection->vec_file, connection->dimensions);
    if (!header_info)
    {
        printf("Failed to get header info\n");
        goto cleanup;
    }

    if (header_info->dimensions == 0 || header_info->vector_count == 0)
    {
        goto cleanup;
    }

    // Allocate vector buffer
    const int BUFFER_SIZE = calculate_optimal_buffer_size(header_info->dimensions);
    vec_buffer = (float *)aligned_malloc(sizeof(float) * (header_info->dimensions + 1) * BUFFER_SIZE, 32);
    if (!vec_buffer)
    {
        printf("Failed to allocate vector buffer\n");
        goto cleanup;
    }

    float *query_vec_norm = get_normalized_vector(query_vec, header_info->dimensions);

    for (uint64_t i = 0; i < header_info->dimensions; i += 64 / sizeof(float))
    {
        PREFETCH((char *)&query_vec_norm[i]);
    }

    // Create min heap
    min_heap = createMinHeap(top_k);
    if (!min_heap)
    {
        printf("Failed to create min heap\n");
        goto cleanup;
    }

    // Process vectors
    int vec_count = 0;
    for (uint64_t i = 0; i < header_info->vector_count; i += BUFFER_SIZE)
    {
        int vectors_to_read = fmin(BUFFER_SIZE, header_info->vector_count - i);
        size_t read = fread(vec_buffer, sizeof(float) * (header_info->dimensions + 1), vectors_to_read, connection->vec_file);
        if (read != vectors_to_read)
        {

            continue;
        }

        for (int j = 0; j < vectors_to_read; j++)
        {

            // Get pointer to start of vector+id block
            float *vec_block = vec_buffer + (j * (header_info->dimensions + 1));

            // Extract metadata ID from first float
            int metadata_id = (int)vec_block[0];

            // Actual vector data starts after the metadata ID
            float *current_vec = vec_block + 1;
            PREFETCH((char *)(current_vec + header_info->dimensions));
            float dot = dot_product(query_vec_norm, current_vec, header_info->dimensions);

            if (dot > min_heap->data[0] || min_heap->size < top_k)
            {
                insert(min_heap, dot, metadata_id);
            }
            vec_count++;
        }
    }

    // Create sorted results
    sorted = createVecResult(min_heap, top_k);
    if (!sorted)
    {
        printf("Failed to create sorted results\n");
        goto cleanup;
    }

    get_metadata_batch(connection->sqlite_db, sorted, min_heap->size);
    int results_found = min_heap->size;

    if (vec_buffer)
        aligned_free(vec_buffer);
    if (header_info)
        free(header_info);
    if (min_heap)
        freeHeap(min_heap);
    free(query_vec_norm);

    DBSearchResult *search_result = malloc(sizeof(DBSearchResult));
    search_result->results = sorted;
    search_result->count = results_found;
    return search_result;

cleanup:
    if (sorted)
    {
        for (int i = 0; i < min_heap->size; i++)
        {
            if (sorted[i].metadata.data)
            {
                free(sorted[i].metadata.data);
            }
        }
        free(sorted);
    }

    if (query_vec_norm)
        free(query_vec_norm);
    if (vec_buffer)
        aligned_free(vec_buffer);
    if (header_info)
        free(header_info);
    if (min_heap)
        freeHeap(min_heap);

    return NULL;
}

bool get_filtered_ids(sqlite3 *db, const char *where_clause, int **ids_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    char *sql = NULL;
    int *ids = NULL;
    int count = 0;
    int capacity = 1024; // Initial capacity
    bool success = false;

    // Allocate initial array
    ids = malloc(capacity * sizeof(int));
    if (!ids)
        return false;

    // Build query
    sql = sqlite3_mprintf("SELECT id FROM metadata WHERE %s", where_clause);
    if (!sql)
        goto cleanup;

    // Prepare statement
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }

    // Execute query and collect IDs
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // Resize array if needed
        if (count >= capacity)
        {
            capacity *= 2;
            int *new_ids = realloc(ids, capacity * sizeof(int));
            if (!new_ids)
                goto cleanup;
            ids = new_ids;
        }

        ids[count++] = sqlite3_column_int(stmt, 0);
    }

    // Set output parameters
    *ids_out = ids;
    *count_out = count;
    success = true;

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    if (sql)
        sqlite3_free(sql);
    if (!success && ids)
        free(ids);

    return success;
}

// Retrieves metadata for an array of IDs from the SQLite database
int get_metadata_batch(sqlite3 *db, VecResult *sorted, int count)
{
    if (!db || !sorted || count <= 0)
    {
        return -1;
    }

    // Build the SQL query with placeholders for the IN clause
    char *sql_base = "SELECT id, metadata, metadata_length FROM metadata WHERE id IN (";
    char *placeholders = NULL;
    placeholders = malloc((count * 2) + 1); // ", ?" for each item except the first, plus null terminator

    if (!placeholders)
    {
        return -1;
    }

    // Build the placeholders string "?,?,?..."
    memset(placeholders, 0, (count * 2) + 1);
    for (int i = 0; i < count; i++)
    {
        if (i == 0)
        {
            strcat(placeholders, "?");
        }
        else
        {
            strcat(placeholders, ",?");
        }
    }

    // Combine the base SQL with the placeholders and closing parenthesis
    char *full_sql = malloc(strlen(sql_base) + strlen(placeholders) + 2); // +2 for ")" and null terminator
    if (!full_sql)
    {
        free(placeholders);
        return -1;
    }

    sprintf(full_sql, "%s%s)", sql_base, placeholders);
    free(placeholders);

    // Prepare the statement
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, full_sql, -1, &stmt, NULL);
    free(full_sql);

    if (rc != SQLITE_OK)
    {
        printf("Failed to prepare metadata query: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Bind all the IDs to the SQL statement
    for (int i = 0; i < count; i++)
    {
        rc = sqlite3_bind_int(stmt, i + 1, sorted[i].index);
        if (rc != SQLITE_OK)
        {
            printf("Failed to bind parameter %d: %s\n", i + 1, sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    // Execute the query and process results
    int successful_retrievals = 0;

    // Create a lookup map for faster result matching
    int *id_map = malloc(count * sizeof(int));
    if (!id_map)
    {
        sqlite3_finalize(stmt);
        return -1;
    }

    // Initialize map with -1 (not found)
    for (int i = 0; i < count; i++)
    {
        id_map[i] = -1;
    }

    // Fill the map with index positions
    for (int i = 0; i < count; i++)
    {
        id_map[i] = i;
    }

    // Process each row from the database
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *text = sqlite3_column_text(stmt, 1);
        int length = sqlite3_column_int(stmt, 2);

        // Find the corresponding result in our array
        int result_idx = -1;
        for (int i = 0; i < count; i++)
        {
            if (sorted[i].index == id)
            {
                result_idx = i;
                break;
            }
        }

        if (result_idx >= 0)
        {
            // Allocate memory for the metadata text
            unsigned char *data_copy = malloc(length + 1); // +1 for null terminator
            if (!data_copy)
            {
                continue; // Skip this result if malloc fails
            }

            // Copy the text data
            memcpy(data_copy, text, length);
            data_copy[length] = '\0'; // Ensure null termination

            // Update the result with the metadata
            sorted[result_idx].metadata.data = data_copy;
            sorted[result_idx].metadata.length = length;

            successful_retrievals++;
        }
    }

    // Cleanup
    sqlite3_finalize(stmt);
    free(id_map);

    // If we didn't retrieve all items, set empty JSON for the rest
    for (int i = 0; i < count; i++)
    {
        if (!sorted[i].metadata.data)
        {
            sorted[i].metadata.data = strdup("{}");
            sorted[i].metadata.length = 2;
        }
    }

    return successful_retrievals;
}

int insert_data(const char *file_path, float **vectors, char **metadatas, size_t *metadata_lengths,
                const size_t vec_count, const uint32_t dimensions)
{

    TinyVecConnection *connection = get_tinyvec_connection(file_path);
    if (!connection || !connection->sqlite_db)
    {
        return 0;
    }

    char *temp_vec_file_path = malloc(strlen(file_path) + 6); // +6 for ".temp\0"
    sprintf(temp_vec_file_path, "%s.temp", file_path);
    if (!temp_vec_file_path)
    {
        return 0;
    }
    FILE *vec_file = open_db_file(temp_vec_file_path);
    if (!vec_file)
    {
        free(temp_vec_file_path);
        return 0;
    }

    VecFileHeaderInfo *header_info = get_vec_file_header_info(vec_file, dimensions);
    if (!header_info)
    {
        printf("Failed to get vector file header info\n");
        fclose(vec_file);
        free(temp_vec_file_path);
        return 0;
    }

    // Create metadata table if it doesn't exist
    char *create_table_sql = "CREATE TABLE IF NOT EXISTS metadata ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                             "metadata TEXT,"
                             "metadata_length INTEGER"
                             ");";

    char *err_msg = NULL;
    int rc = sqlite3_exec(connection->sqlite_db, create_table_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(connection->sqlite_db);
        // free(sqlite_path);
        free(header_info);
        fclose(vec_file);
        free(temp_vec_file_path);
        return 0;
    }

    // Create index on the id column if it doesn't exist
    char *create_index_sql = "CREATE INDEX IF NOT EXISTS idx_metadata_id ON metadata(id);";
    rc = sqlite3_exec(connection->sqlite_db, create_index_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        printf("Index creation error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(connection->sqlite_db);
        // free(sqlite_path);
        free(header_info);
        fclose(vec_file);
        free(temp_vec_file_path);
        return 0;
    }

    // Begin transaction for better performance
    rc = sqlite3_exec(connection->sqlite_db, "BEGIN TRANSACTION;", 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        printf("Transaction start error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(connection->sqlite_db);
        // free(sqlite_path);
        free(header_info);
        fclose(vec_file);
        free(temp_vec_file_path);
        return 0;
    }

    bool reset_dimensions = false;
    if (header_info->dimensions == 0 && header_info->dimensions != dimensions)
    {
        reset_dimensions = true;
    }

    FileMetadataPaths *md_paths = get_metadata_file_paths(file_path);
    if (!md_paths)
    {

        fclose(vec_file);
        free_metadata_file_paths(md_paths);
        free(temp_vec_file_path);
        free(header_info);
        return 0;
    }

    char *temp_idx_path = malloc(strlen(md_paths->idx_path) + 6);
    char *temp_meta_path = malloc(strlen(md_paths->md_path) + 6);
    sprintf(temp_idx_path, "%s.temp", md_paths->idx_path);
    sprintf(temp_meta_path, "%s.temp", md_paths->md_path);

    if (!temp_idx_path || !temp_meta_path)
    {
        free(temp_idx_path);
        free(temp_meta_path);
        free_metadata_file_paths(md_paths);
        free(temp_vec_file_path);
        free(header_info);
        return 0;
    }

    FILE *idx_file = fopen(temp_idx_path, "ab");
    FILE *meta_file = fopen(temp_meta_path, "ab");

    if (!idx_file || !meta_file)
    {
        if (vec_file)
        {
            printf("Failed to open vec file\n");
            fclose(vec_file);
        }
        if (idx_file)
        {
            printf("Failed to open idx file\n");
            fclose(idx_file);
        }
        if (meta_file)
        {
            printf("Failed to open meta file\n");
            fclose(meta_file);
        }

        free(header_info);
        free_metadata_file_paths(md_paths);
        free(temp_idx_path);
        free(temp_meta_path);
        free(temp_vec_file_path);
        return 0;
    }

    if (connection)
    {

        fclose(connection->vec_file);
    }

    // After reading dimensions, seek to end for appending
    fseek(vec_file, 0, SEEK_END);

    size_t vec_size_with_id = (dimensions + 1) * sizeof(float);
    // Now calculate total vector size using dimensions
    size_t total_vec_size = vec_count * vec_size_with_id;

    // Calculate total sizes needed
    size_t total_meta_size = 0;
    for (size_t i = 0; i < vec_count; i++)
    {
        total_meta_size += metadata_lengths[i];
    }

    // Allocate buffers
    unsigned char *vec_buffer = malloc(total_vec_size);

    if (!vec_buffer)
    {
        free(header_info);
        free(vec_buffer);
        sqlite3_exec(connection->sqlite_db, "ROLLBACK;", 0, 0, NULL);
        sqlite3_close(connection->sqlite_db);

        fclose(vec_file);
        fclose(idx_file);
        fclose(meta_file);
        free(temp_idx_path);
        free(temp_meta_path);
        free(temp_vec_file_path);
        return 0;
    }

    // Get starting metadata offset
    // uint64_t current_meta_offset = ftell(meta_file);
    fseek(meta_file, 0, SEEK_END);
    // uint64_t total_meta_size_offset = ftell(meta_file);
    uint64_t current_meta_offset = ftell(meta_file);

    // Fill buffers
    size_t vec_offset = 0;
    size_t meta_offset = 0;
    size_t idx_offset = 0;

    size_t vec_size = header_info->dimensions * sizeof(float);

    sqlite3_stmt *stmt;
    const char *insert_sql = "INSERT INTO metadata (metadata, metadata_length) VALUES (?, ?);";
    rc = sqlite3_prepare_v2(connection->sqlite_db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(connection->sqlite_db));
        free(vec_buffer);
        sqlite3_exec(connection->sqlite_db, "ROLLBACK;", 0, 0, NULL);
        sqlite3_close(connection->sqlite_db);
        // free(sqlite_path);
        free(header_info);
        fclose(vec_file);
        free(temp_vec_file_path);
        return 0;
    }

    for (size_t i = 0; i < vec_count; i++)
    {
        if (!vectors[i])
        {
            continue;
        }

        // insert metadata to table
        sqlite3_bind_text(stmt, 1, metadatas[i], metadata_lengths[i], SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, metadata_lengths[i]); // Bind the length as an integer

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            printf("Failed to insert metadata: %s\n", sqlite3_errmsg(connection->sqlite_db));
            // Continue anyway
        }

        // Get the rowid of the inserted metadata
        sqlite3_int64 metadata_id = sqlite3_last_insert_rowid(connection->sqlite_db);

        // Store metadata ID as a float in the first position
        float md_id_float = (float)metadata_id;
        memcpy(vec_buffer + vec_offset, &md_id_float, sizeof(float));
        vec_offset += sizeof(float);

        // Normalize vector
        normalize_vector(vectors[i], header_info->dimensions);
        // Copy vector into buffer
        memcpy(vec_buffer + vec_offset,
               vectors[i],
               vec_size);
        vec_offset += vec_size;

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    // Finalize statement
    sqlite3_finalize(stmt);

    // Commit transaction
    rc = sqlite3_exec(connection->sqlite_db, "END TRANSACTION;", 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        printf("Transaction commit error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }

    // Single write for each file
    fwrite(vec_buffer, 1, total_vec_size, vec_file);

    // Position at start of file
    fseek(vec_file, 0, SEEK_SET);

    // Calculate new total count
    int new_count = header_info->vector_count + vec_count;

    // Write the new vector count
    fwrite(&new_count, sizeof(int), 1, vec_file);

    if (reset_dimensions)
    {
        fwrite(&dimensions, sizeof(uint32_t), 1, vec_file);
    }

    // Cleanup
    free(header_info);
    free(vec_buffer);
    fclose(vec_file);
    fclose(idx_file);
    fclose(meta_file);
    free(temp_idx_path);
    free(temp_meta_path);
    free(temp_vec_file_path);

    return (int)vec_count;
}

size_t calculate_optimal_buffer_size(int dimensions)
{
    // Target memory size we want to work with (let's say ~4MB)
    const size_t TARGET_BUFFER_MEMORY = 4 * 1024 * 1024; // 4MB

    // Calculate how many vectors would fit in our target memory
    // Account for the metadata ID float (+1)
    size_t bytes_per_vector = (dimensions + 1) * sizeof(float);
    size_t optimal_vectors = TARGET_BUFFER_MEMORY / bytes_per_vector;

    // Add some bounds checking
    if (optimal_vectors < 512)
        optimal_vectors = 512; // Minimum size
    if (optimal_vectors > 8192)
        optimal_vectors = 8192; // Maximum size

    return optimal_vectors;
}

bool update_db_file_connection(const char *file_path)
{
    if (!file_path)
    {
        printf("Null file path provided\n");
        return false;
    }

    TinyVecConnection *connection = get_tinyvec_connection(file_path);
    if (!connection)
    {
        printf("Failed to get connection for path: %s\n", file_path);
        return false;
    }

    // Close any existing open files
    if (connection->vec_file)
        fclose(connection->vec_file);

    connection->vec_file = NULL;

    FILE *vec_file = open_db_file(connection->file_path);
    if (!vec_file)
    {
        printf("Failed to open vector file: %s\n", connection->file_path);
        return false;
    }

    connection->vec_file = vec_file;

    return true;
}

int delete_data_by_ids(const char *file_path, int *ids_to_delete, int delete_count)
{
    if (!file_path || !ids_to_delete || delete_count <= 0)
    {
        return 0;
    }

    // Sort the IDs for faster lookup using binary search
    qsort(ids_to_delete, delete_count, sizeof(int), compare_ints);

    TinyVecConnection *connection = get_tinyvec_connection(file_path);
    if (!connection || !connection->sqlite_db || !connection->vec_file)
    {
        return 0;
    }

    // Create temporary file paths
    char *temp_vec_file_path = malloc(strlen(file_path) + 6); // +6 for ".temp\0"
    if (!temp_vec_file_path)
    {
        return 0;
    }
    sprintf(temp_vec_file_path, "%s.temp", file_path);

    // Open temporary vector file for writing
    FILE *temp_vec_file = fopen(temp_vec_file_path, "wb");
    if (!temp_vec_file)
    {
        free(temp_vec_file_path);
        return 0;
    }

    // Get header info from the original file
    // Note: We need to rewind the file pointer first
    rewind(connection->vec_file);
    VecFileHeaderInfo *header_info = get_vec_file_header_info(connection->vec_file, 0);
    if (!header_info)
    {
        fclose(temp_vec_file);
        free(temp_vec_file_path);
        return 0;
    }

    int original_vector_count = header_info->vector_count;

    // Calculate new vector count (original count minus deleted count)
    int new_vector_count = original_vector_count - delete_count;

    // Write the new header to the temp file
    fwrite(&new_vector_count, sizeof(int), 1, temp_vec_file);
    fwrite(&header_info->dimensions, sizeof(uint32_t), 1, temp_vec_file);

    // Define buffer size (in number of vectors) and allocate buffer
    const int BUFFER_SIZE = 1024; // Adjust as needed for memory constraints
    const size_t vec_block_size = sizeof(float) * (header_info->dimensions + 1);
    const size_t buffer_size_bytes = vec_block_size * BUFFER_SIZE;

    float *read_buffer = malloc(buffer_size_bytes);
    float *write_buffer = malloc(buffer_size_bytes);
    if (!read_buffer || !write_buffer)
    {
        if (read_buffer)
            free(read_buffer);
        if (write_buffer)
            free(write_buffer);
        free(header_info);
        fclose(temp_vec_file);
        free(temp_vec_file_path);
        return 0;
    }

    // Start after the header in the original file
    // Position file pointer at the beginning of vector data
    // (header_info already moved the file pointer past the header)

    // Track how many vectors we've preserved
    int preserved_count = 0;
    int write_buffer_count = 0; // Count of vectors in the write buffer

    // Process the file in chunks
    for (uint64_t i = 0; i < header_info->vector_count; i += BUFFER_SIZE)
    {
        // Calculate how many vectors to read in this chunk
        int vectors_to_read = fmin(BUFFER_SIZE, header_info->vector_count - i);

        // Read a chunk of vectors
        size_t read = fread(read_buffer, vec_block_size, vectors_to_read, connection->vec_file);
        if (read != vectors_to_read)
        {
            printf("Failed to read expected number of vectors: expected %d, got %zu\n",
                   vectors_to_read, read);
            break; // Or handle the error as appropriate
        }

        // Process each vector in the read buffer
        for (int j = 0; j < vectors_to_read; j++)
        {
            // Get pointer to start of vector+id block
            float *vec_block = read_buffer + (j * (header_info->dimensions + 1));

            // Extract metadata ID from first float
            int metadata_id = (int)vec_block[0];

            // Skip this vector if its ID is in the delete list
            if (binary_search_int(ids_to_delete, delete_count, metadata_id) != -1)
            {
                continue; // Skip this vector
            }

            // Keep this vector: copy it to the write buffer
            memcpy(write_buffer + (write_buffer_count * (header_info->dimensions + 1)),
                   vec_block, vec_block_size);
            write_buffer_count++;
            preserved_count++;

            // If write buffer is full, write it to the temp file
            if (write_buffer_count >= BUFFER_SIZE)
            {
                fwrite(write_buffer, vec_block_size, write_buffer_count, temp_vec_file);
                write_buffer_count = 0; // Reset buffer counter
            }
        }
    }

    // Write any remaining vectors in the write buffer
    if (write_buffer_count > 0)
    {
        fwrite(write_buffer, vec_block_size, write_buffer_count, temp_vec_file);
    }

    // Verify we preserved the expected number of vectors
    if (preserved_count != new_vector_count)
    {
        printf("Warning: Expected to preserve %d vectors, but actually preserved %d\n",
               new_vector_count, preserved_count);
        // Update the header with the actual count
        rewind(temp_vec_file);
        fwrite(&preserved_count, sizeof(int), 1, temp_vec_file);
    }

    // Clean up resources
    free(read_buffer);
    free(write_buffer);

    int vectors_actually_removed = original_vector_count - preserved_count;

    fclose(connection->vec_file);

    // Delete the metadata from the SQLite database in batches
    const int BATCH_SIZE = 500;
    char *err_msg = NULL;
    int rc = SQLITE_OK;

    // Begin transaction for better performance
    rc = sqlite3_exec(connection->sqlite_db, "BEGIN TRANSACTION;", 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        printf("Failed to begin transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        goto cleanup;
    }

    // Process deletions in batches
    for (int batch_start = 0; batch_start < delete_count; batch_start += BATCH_SIZE)
    {
        int batch_end = batch_start + BATCH_SIZE;
        if (batch_end > delete_count)
            batch_end = delete_count;
        int batch_size = batch_end - batch_start;

        // Create the SQL for this batch
        char *sql = sqlite3_mprintf("DELETE FROM metadata WHERE id IN (");
        for (int i = batch_start; i < batch_end; i++)
        {
            char *temp = sqlite3_mprintf("%s%d%s", sql, ids_to_delete[i],
                                         (i < batch_end - 1) ? "," : ")");
            sqlite3_free(sql);
            sql = temp;
        }

        // Execute the deletion for this batch
        rc = sqlite3_exec(connection->sqlite_db, sql, 0, 0, &err_msg);
        if (rc != SQLITE_OK)
        {
            printf("SQL error when deleting metadata batch %d-%d: %s\n",
                   batch_start, batch_end - 1, err_msg);
            sqlite3_free(err_msg);
            // Continue with other batches even if this one failed
        }

        sqlite3_free(sql);
    }

    // Commit the transaction
    rc = sqlite3_exec(connection->sqlite_db, "COMMIT;", 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        printf("Failed to commit transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        // Try to rollback
        sqlite3_exec(connection->sqlite_db, "ROLLBACK;", 0, 0, NULL);
    }

cleanup:
    // Clean up remaining resources
    free(header_info);
    fclose(temp_vec_file);
    free(temp_vec_file_path);

    return vectors_actually_removed;
}

int delete_data_by_filter(const char *file_path, const char *json_filter)
{

    TinyVecConnection *connection = get_tinyvec_connection(file_path);
    if (!connection || !connection->sqlite_db)
    {
        return 0;
    }

    int *filtered_ids = NULL;
    int filtered_count = 0;

    char *sql_where = json_query_to_sql(json_filter);

    if (!get_filtered_ids(connection->sqlite_db, sql_where, &filtered_ids, &filtered_count))
    {
        printf("Failed to get filtered IDs\n");
        free(sql_where);
        return 0;
    }

    free(sql_where);

    int delete_count = delete_data_by_ids(connection->file_path, filtered_ids, filtered_count);

    free(filtered_ids);
    return delete_count;
}