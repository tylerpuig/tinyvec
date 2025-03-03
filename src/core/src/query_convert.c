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

// Forward declaration
void process_query_object(cJSON *obj, StringBuffer *buffer, const char *path_prefix);

// Process $in or $nin operator
void process_in_operator(const char *field_path, cJSON *values, StringBuffer *buffer, bool is_negated)
{
    if (!cJSON_IsArray(values))
    {
        return;
    }

    string_buffer_append(buffer, " AND json_extract(content, '$.");
    string_buffer_append(buffer, field_path);
    string_buffer_append(buffer, "') ");

    if (is_negated)
    {
        string_buffer_append(buffer, "NOT ");
    }

    string_buffer_append(buffer, "IN (");

    int count = cJSON_GetArraySize(values);
    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(values, i);

        if (i > 0)
        {
            string_buffer_append(buffer, ", ");
        }

        append_json_value(buffer, item);
    }

    string_buffer_append(buffer, ")");
}

// Process comparison operators like $eq, $gt, etc.
void process_comparison(const char *field_path, const char *op, cJSON *value, StringBuffer *buffer)
{
    // Start building the condition
    string_buffer_append(buffer, " AND json_extract(content, '$.");
    string_buffer_append(buffer, field_path);
    string_buffer_append(buffer, "') ");

    if (strcmp(op, "$eq") == 0)
    {
        string_buffer_append(buffer, "=");
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
    else if (strcmp(op, "$in") == 0)
    {
        // Don't call a separate function, handle it directly here
        if (cJSON_IsArray(value))
        {
            string_buffer_append(buffer, "IN (");

            int count = cJSON_GetArraySize(value);
            for (int i = 0; i < count; i++)
            {
                cJSON *item = cJSON_GetArrayItem(value, i);

                if (i > 0)
                {
                    string_buffer_append(buffer, ", ");
                }

                append_json_value(buffer, item);
            }

            string_buffer_append(buffer, ")");
        }
    }
    else if (strcmp(op, "$nin") == 0)
    {
        // Same as $in but with NOT
        if (cJSON_IsArray(value))
        {
            string_buffer_append(buffer, "NOT IN (");

            int count = cJSON_GetArraySize(value);
            for (int i = 0; i < count; i++)
            {
                cJSON *item = cJSON_GetArrayItem(value, i);

                if (i > 0)
                {
                    string_buffer_append(buffer, ", ");
                }

                append_json_value(buffer, item);
            }

            string_buffer_append(buffer, ")");
        }
    }
}

// Process field values or nested operators
void process_field(const char *field, cJSON *value, StringBuffer *buffer, const char *path_prefix)
{
    // Handle objects with operators
    if (cJSON_IsObject(value))
    {
        cJSON *child = NULL;
        cJSON_ArrayForEach(child, value)
        {
            const char *op = child->string;
            if (op[0] == '$')
            {
                process_comparison(field, op, child, buffer);
            }
        }
    }
    // Handle direct equality comparison (shorthand)
    else
    {
        string_buffer_append(buffer, " AND json_extract(content, '$.");
        string_buffer_append(buffer, field);
        string_buffer_append(buffer, "') = ");
        append_json_value(buffer, value);
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

        // Process the field
        process_field(key, child, buffer, path_prefix);
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
    process_query_object(root, buffer, "");

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