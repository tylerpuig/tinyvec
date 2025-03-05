#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../include/cJSON.h"
#include "../include/query_convert.h"

// Create a new string buffer
StringBuffer *string_buffer_new()
{
    StringBuffer *buffer = malloc(sizeof(StringBuffer));
    if (buffer)
    {
        buffer->capacity = 256;
        buffer->data = malloc(buffer->capacity);
        if (buffer->data)
        {
            buffer->data[0] = '\0';
            buffer->length = 0;
        }
        else
        {
            free(buffer);
            return NULL;
        }
    }
    return buffer;
}

// Append a string to the buffer
void string_buffer_append(StringBuffer *buffer, const char *str)
{
    size_t str_len = strlen(str);
    size_t new_length = buffer->length + str_len;

    // Resize if needed
    if (new_length >= buffer->capacity)
    {
        size_t new_capacity = buffer->capacity * 2;
        if (new_capacity <= new_length)
        {
            new_capacity = new_length + 1;
        }

        char *new_data = realloc(buffer->data, new_capacity);
        if (new_data)
        {
            buffer->data = new_data;
            buffer->capacity = new_capacity;
        }
        else
        {
            return; // Append failed
        }
    }

    // Append string
    strcpy(buffer->data + buffer->length, str);
    buffer->length = new_length;
}

// Free the string buffer
void string_buffer_free(StringBuffer *buffer)
{
    if (buffer)
    {
        free(buffer->data);
        free(buffer);
    }
}

// Function to handle string values in SQL
void append_sql_string(StringBuffer *buffer, const char *value)
{
    string_buffer_append(buffer, "'");

    // Simple SQL escaping (in production code, use proper escaping)
    for (size_t i = 0; i < strlen(value); i++)
    {
        if (value[i] == '\'')
        {
            string_buffer_append(buffer, "''"); // Escape single quotes
        }
        else
        {
            char tmp[2] = {value[i], '\0'};
            string_buffer_append(buffer, tmp);
        }
    }

    string_buffer_append(buffer, "'");
}

// Append a JSON value to the SQL query
void append_json_value(StringBuffer *buffer, cJSON *value)
{
    if (cJSON_IsString(value))
    {
        append_sql_string(buffer, value->valuestring);
    }
    else if (cJSON_IsNumber(value))
    {
        char num_str[64];
        if (value->valuedouble == (double)value->valueint)
        {
            sprintf(num_str, "%d", value->valueint);
        }
        else
        {
            sprintf(num_str, "%f", value->valuedouble);
        }
        string_buffer_append(buffer, num_str);
    }
    else if (cJSON_IsBool(value))
    {
        string_buffer_append(buffer, cJSON_IsTrue(value) ? "1" : "0");
    }
    else if (cJSON_IsNull(value))
    {
        string_buffer_append(buffer, "NULL");
    }
}

// Process comparison operators like $eq, $gt, etc.
void process_comparison(const char *field_path, const char *op, cJSON *value, StringBuffer *buffer)
{
    // Special handling for array membership operations
    if (strcmp(op, "$in") == 0)
    {
        if (!cJSON_IsArray(value) || cJSON_GetArraySize(value) == 0)
        {
            string_buffer_append(buffer, " AND 0"); // Always false for empty array
            return;
        }

        int array_size = cJSON_GetArraySize(value);

        // Open a group for the OR conditions
        string_buffer_append(buffer, " AND (");

        // For string values, we need json_extract to return the actual string value
        // Check if the first item is a string to determine the approach
        cJSON *first_item = cJSON_GetArrayItem(value, 0);
        int is_string_comparison = cJSON_IsString(first_item);

        // Loop through all array items and combine with OR
        for (int i = 0; i < array_size; i++)
        {
            cJSON *item = cJSON_GetArrayItem(value, i);

            if (is_string_comparison)
            {
                // For strings, use direct comparison with the extracted value
                string_buffer_append(buffer, "json_extract(metadata, '$.");
                string_buffer_append(buffer, field_path);
                string_buffer_append(buffer, "') = ");
                append_json_value(buffer, item);
            }
            else
            {
                // For non-strings, use the json_each approach which works better for arrays
                string_buffer_append(buffer, "EXISTS (SELECT 1 FROM json_each(json_extract(metadata, '$.");
                string_buffer_append(buffer, field_path);
                string_buffer_append(buffer, "')) WHERE value = ");
                append_json_value(buffer, item);
                string_buffer_append(buffer, ")");
            }

            // Add OR between conditions, except for the last item
            if (i < array_size - 1)
            {
                string_buffer_append(buffer, " OR ");
            }
        }

        // Close the group
        string_buffer_append(buffer, ")");
    }
    else if (strcmp(op, "$nin") == 0)
    {
        if (!cJSON_IsArray(value) || cJSON_GetArraySize(value) == 0)
        {
            string_buffer_append(buffer, " AND 1"); // Always true for empty array
            return;
        }

        int array_size = cJSON_GetArraySize(value);

        // Open a group for the AND conditions
        string_buffer_append(buffer, " AND (");

        // For string values, we need json_extract to return the actual string value
        // Check if the first item is a string to determine the approach
        cJSON *first_item = cJSON_GetArrayItem(value, 0);
        int is_string_comparison = cJSON_IsString(first_item);

        // Loop through all array items and combine with AND
        for (int i = 0; i < array_size; i++)
        {
            cJSON *item = cJSON_GetArrayItem(value, i);

            if (is_string_comparison)
            {
                // For strings, use direct comparison with the extracted value
                string_buffer_append(buffer, "json_extract(metadata, '$.");
                string_buffer_append(buffer, field_path);
                string_buffer_append(buffer, "') != ");
                append_json_value(buffer, item);
            }
            else
            {
                // For non-strings, use the json_each approach which works better for arrays
                string_buffer_append(buffer, "NOT EXISTS (SELECT 1 FROM json_each(json_extract(metadata, '$.");
                string_buffer_append(buffer, field_path);
                string_buffer_append(buffer, "')) WHERE value = ");
                append_json_value(buffer, item);
                string_buffer_append(buffer, ")");
            }

            // Add AND between conditions, except for the last item
            if (i < array_size - 1)
            {
                string_buffer_append(buffer, " AND ");
            }
        }

        // Close the group
        string_buffer_append(buffer, ")");
    }
    else
    {
        // Regular field comparison for non-array operators
        string_buffer_append(buffer, " AND json_extract(metadata, '$.");
        string_buffer_append(buffer, field_path);
        string_buffer_append(buffer, "') ");

        if (strcmp(op, "$eq") == 0)
        {
            string_buffer_append(buffer, "= ");
            append_json_value(buffer, value);
        }
        else if (strcmp(op, "$ne") == 0)
        {
            string_buffer_append(buffer, "!=");
            append_json_value(buffer, value);
        }
        else if (strcmp(op, "$gt") == 0)
        {
            string_buffer_append(buffer, ">");
            append_json_value(buffer, value);
        }
        else if (strcmp(op, "$lt") == 0)
        {
            string_buffer_append(buffer, "<");
            append_json_value(buffer, value);
        }
        else if (strcmp(op, "$gte") == 0)
        {
            string_buffer_append(buffer, ">=");
            append_json_value(buffer, value);
        }
        else if (strcmp(op, "$lte") == 0)
        {
            string_buffer_append(buffer, "<=");
            append_json_value(buffer, value);
        }
        else if (strcmp(op, "$exists") == 0)
        {
            if (cJSON_IsTrue(value))
            {
                string_buffer_append(buffer, "IS NOT NULL");
            }
            else
            {
                string_buffer_append(buffer, "IS NULL");
            }
        }
    }
}

// Process field values or nested operators
void process_field(const char *field_path, cJSON *value, StringBuffer *buffer)
{
    // Handle direct equality comparison (shorthand)
    if (!cJSON_IsObject(value) || cJSON_GetArraySize(value) == 0)
    {
        string_buffer_append(buffer, " AND json_extract(metadata, '$.");
        string_buffer_append(buffer, field_path);
        string_buffer_append(buffer, "') = ");
        append_json_value(buffer, value);
        return;
    }

    // Check if this is an operator object (keys starting with $)
    int has_operators = 0;
    cJSON *child = NULL;
    cJSON_ArrayForEach(child, value)
    {
        const char *key = child->string;
        if (key && key[0] == '$')
        {
            has_operators = 1;
            process_comparison(field_path, key, child, buffer);
        }
    }

    // If no operators found, it's a nested object
    if (!has_operators)
    {
        process_object(value, buffer, field_path);
    }
}

void process_object(cJSON *obj, StringBuffer *buffer, const char *path_prefix)
{
    cJSON *child = NULL;
    cJSON_ArrayForEach(child, obj)
    {
        const char *key = child->string;
        if (!key)
            continue;

        // Build the full field path
        char field_path[256] = {0};
        if (path_prefix && *path_prefix)
        {
            snprintf(field_path, sizeof(field_path), "%s.%s", path_prefix, key);
        }
        else
        {
            strncpy(field_path, key, sizeof(field_path) - 1);
        }

        // Process this field
        process_field(field_path, child, buffer);
    }
}

// Process the MongoDB-style query object
void process_query_object(cJSON *obj, StringBuffer *buffer, const char *path_prefix)
{
    cJSON *child = NULL;
    cJSON_ArrayForEach(child, obj)
    {
        const char *key = child->string;

        // Skip if no key (shouldn't happen)
        if (!key)
            continue;

        // Build the full field path if there's a prefix
        char field_path[256] = {0};
        if (path_prefix && *path_prefix)
        {
            snprintf(field_path, sizeof(field_path), "%s.%s", path_prefix, key);
            // Process the field with the combined path
            process_field(field_path, child, buffer);
        }
        else
        {
            // Process the field with just the key as the path
            process_field(key, child, buffer);
        }
    }
}

// Main function to convert json query to SQLite
char *json_query_to_sql(const char *json)
{
    // Parse the JSON
    cJSON *root = cJSON_Parse(json);
    if (!root)
    {
        return strdup("1=1"); // Default to true condition if parsing fails
    }

    // Create a string buffer for the WHERE clause
    StringBuffer *buffer = string_buffer_new();
    if (!buffer)
    {
        cJSON_Delete(root);
        return strdup("1=1");
    }

    // Start with a true condition
    string_buffer_append(buffer, "1=1");

    // Process the query
    process_object(root, buffer, "");

    // Get the result and clean up
    char *result = strdup(buffer->data);
    string_buffer_free(buffer);
    cJSON_Delete(root);

    return result;
}
// Example usage
//     const char *query = "{\"name\":{\"$eq\":\"John\"},\"age\":{\"$gt\":25},\"tags\":{\"$in\":[\"admin\",\"user\"]}}";

//     char *sql_where = json_query_to_sql(query);
//     printf("SQL WHERE: %s\n", sql_where);
//     free(sql_where);