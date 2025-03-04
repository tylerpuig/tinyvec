#include "cJSON.h"
#ifndef QUERY_CONVERT_H
#define QUERY_CONVERT_H

// String buffer for building SQL queries
typedef struct
{
    char *data;
    size_t length;
    size_t capacity;
} StringBuffer;

StringBuffer *string_buffer_new();
void string_buffer_append(StringBuffer *buffer, const char *str);
void string_buffer_free(StringBuffer *buffer);
void append_json_value(StringBuffer *buffer, cJSON *value);

void process_in_operator(const char *field_path, cJSON *values, StringBuffer *buffer, bool is_negated);
void process_comparison(const char *field_path, const char *op, cJSON *value, StringBuffer *buffer);
void process_field(const char *field_path, cJSON *value, StringBuffer *buffer);
void process_query_object(cJSON *obj, StringBuffer *buffer, const char *path_prefix);
void process_object(cJSON *obj, StringBuffer *buffer, const char *path_prefix);
char *json_query_to_sql(const char *json);

#endif // QUERY_CONVERT_H