#include <node_api.h>
#include "../../../src/core/include/cJSON.h"
#include "addon_utils.h"
#include "../../../src/core/include/vec_types.h"
#include <iostream>

napi_value convert_json_to_napi(napi_env env, cJSON *json)
{
    if (json == nullptr)
        return nullptr;

    napi_value result;

    switch (json->type)
    {
    case cJSON_False:
        napi_get_boolean(env, false, &result);
        break;
    case cJSON_True:
        napi_get_boolean(env, true, &result);
        break;
    case cJSON_NULL:
        napi_get_null(env, &result);
        break;
    case cJSON_Number:
        napi_create_double(env, json->valuedouble, &result);
        break;
    case cJSON_String:
        napi_create_string_utf8(env, json->valuestring, NAPI_AUTO_LENGTH, &result);
        break;
    case cJSON_Array:
    {
        napi_create_array(env, &result);
        uint32_t index = 0;
        for (cJSON *item = json->child; item; item = item->next)
        {
            napi_value element = convert_json_to_napi(env, item);
            napi_set_element(env, result, index++, element);
        }
        break;
    }
    case cJSON_Object:
    {
        napi_create_object(env, &result);
        for (cJSON *item = json->child; item; item = item->next)
        {
            napi_value value = convert_json_to_napi(env, item);
            napi_set_named_property(env, result, item->string, value);
        }
        break;
    }
    default:
        napi_get_undefined(env, &result);
    }

    return result;
}

napi_ref stringify_ref = nullptr;

AsyncInsertData *prepare_data_for_insertion(napi_env env, napi_callback_info info)
{

    napi_status status;
    AsyncInsertData *asyncData = new AsyncInsertData;

    // Get arguments
    size_t argc = 2;
    napi_value args[2];
    status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok)
        return nullptr;

    // Check argument count
    if (argc < 2)
    {
        napi_throw_error(env, nullptr, "Wrong number of arguments. Expected file path and array of vectors w/ metadata.");
        return nullptr;
    }

    size_t file_path_str_size;
    status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &file_path_str_size);
    if (status != napi_ok)
    {
        // Handle error
        napi_throw_error(env, nullptr, "First argument must be a string.");
        return nullptr;
    }
    char *file_path = new char[file_path_str_size + 1];

    // Actually copy the string
    status = napi_get_value_string_utf8(env, args[0], file_path, file_path_str_size + 1, nullptr);
    if (status != napi_ok)
    {
        delete[] file_path; // Clean up if we fail
        napi_throw_error(env, nullptr, "Failed to read file path.");
        return nullptr;
    }

    // Check if first argument is an array
    bool isArray;
    status = napi_is_array(env, args[1], &isArray);
    if (status != napi_ok || !isArray)
    {
        napi_throw_error(env, nullptr, "First argument must be an array.");
        return nullptr;
    }

    // Get array length
    uint32_t vector_count;
    status = napi_get_array_length(env, args[1], &vector_count);
    if (status != napi_ok)
    {
        napi_throw_error(env, nullptr, "Could not get array length.");
        return nullptr;
    }

    std::vector<float *> vectors;
    // std::vector<size_t> vec_lengths;
    std::vector<char *> metadatas;
    std::vector<size_t> metadata_lengths;

    if (stringify_ref == nullptr)
    {
        napi_value global, json, stringify;
        status = napi_get_global(env, &global);
        if (status != napi_ok)
            return nullptr;

        status = napi_get_named_property(env, global, "JSON", &json);
        if (status != napi_ok)
            return nullptr;

        status = napi_get_named_property(env, json, "stringify", &stringify);
        if (status != napi_ok)
            return nullptr;

        status = napi_create_reference(env, stringify, 1, &stringify_ref);
        if (status != napi_ok)
            return nullptr;
    }

    napi_value global;
    status = napi_get_global(env, &global);
    if (status != napi_ok)
        return nullptr;

    size_t vec_dimensions = 0;

    // For each vector in the array
    for (uint32_t i = 0; i < vector_count; i++)
    {
        // Get vector object
        napi_value vector_obj;
        status = napi_get_element(env, args[1], i, &vector_obj);
        if (status != napi_ok)
            continue;

        // Get vec property (Float32Array)
        napi_value vec_value;
        status = napi_get_named_property(env, vector_obj, "vector", &vec_value);
        if (status != napi_ok)
            continue;

        // Get metadata property
        napi_value metadata_value;
        status = napi_get_named_property(env, vector_obj, "metadata", &metadata_value);
        if (status != napi_ok)
            continue;

        // Get Float32Array data
        bool isTypedArray;
        status = napi_is_typedarray(env, vec_value, &isTypedArray);
        if (!isTypedArray)
            continue;

        void *vec_data;
        size_t vec_length;
        napi_typedarray_type type;
        status = napi_get_typedarray_info(env, vec_value, &type, &vec_length, &vec_data, nullptr, nullptr);
        if (status != napi_ok || type != napi_float32_array)
            continue;

        // Set the dimensions if it's not set yet
        if (vec_dimensions == 0)
        {
            vec_dimensions = vec_length;
        }

        if (vec_length != vec_dimensions)
        {
            continue;
            // napi_throw_error(env, nullptr, "Vectors must have the same dimensions.");
            // return nullptr;
        }

        float *float_data = static_cast<float *>(vec_data);

        napi_value stringify;
        status = napi_get_reference_value(env, stringify_ref, &stringify);
        if (status != napi_ok)
            continue;

        napi_value stringify_args[1] = {metadata_value};
        napi_value metadata_result;
        napi_call_function(env, global, stringify, 1, stringify_args, &metadata_result);

        size_t str_size;
        status = napi_get_value_string_utf8(env, metadata_result, nullptr, 0, &str_size);

        char *metadata_str = new char[str_size + 1];
        status = napi_get_value_string_utf8(env, metadata_result, metadata_str, str_size + 1, nullptr);

        vectors.push_back(float_data);
        metadatas.push_back(metadata_str);
        metadata_lengths.push_back(str_size);
    }

    asyncData->file_path = file_path;
    asyncData->vectors = std::move(vectors);
    asyncData->metadatas = std::move(metadatas);
    asyncData->metadata_lengths = std::move(metadata_lengths);
    asyncData->insert_count = 0;
    asyncData->dimensions = vec_dimensions;

    return asyncData;
}

ConnectionData *prepare_data_for_connection(napi_env env, napi_callback_info info)
{
    napi_status status;
    ConnectionData *connection_data = new ConnectionData;

    // Get arguments
    size_t argc = 2;
    napi_value args[2];
    status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok)
        return nullptr;

    // Check for minimum argument count (file path)
    if (argc < 1)
    {
        napi_throw_error(env, nullptr, "Wrong number of arguments. Expected file path.");
        delete connection_data;
        return nullptr;
    }

    // Handle file path argument
    size_t file_path_str_size;
    status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &file_path_str_size);
    if (status != napi_ok)
    {
        napi_throw_error(env, nullptr, "First argument must be a string.");
        delete connection_data;
        return nullptr;
    }

    char *file_path = new char[file_path_str_size + 1];
    status = napi_get_value_string_utf8(env, args[0], file_path, file_path_str_size + 1, nullptr);
    if (status != napi_ok)
    {
        delete[] file_path;
        delete connection_data;
        napi_throw_error(env, nullptr, "Failed to read file path.");
        return nullptr;
    }

    // Default dimensions value
    int32_t config_dimensions_value = 0;

    // Only check second argument if it exists
    if (argc > 1)
    {
        napi_valuetype tinyvec_config;
        status = napi_typeof(env, args[1], &tinyvec_config);
        if (status == napi_ok && tinyvec_config == napi_object)
        {
            napi_value config_dimensions;
            status = napi_create_string_utf8(env, "dimensions", NAPI_AUTO_LENGTH, &config_dimensions);

            if (status == napi_ok)
            {
                napi_value config_dimensions_napi_value;
                status = napi_get_property(env, args[1], config_dimensions, &config_dimensions_napi_value);
                if (status == napi_ok)
                {
                    napi_get_value_int32(env, config_dimensions_napi_value, &config_dimensions_value);
                    // If this fails, we'll just keep the default value of 0
                }
            }
        }
    }

    connection_data->file_path = file_path;
    connection_data->dimensions = config_dimensions_value;

    return connection_data;
}

char *prepare_data_for_index_stats(napi_env env, napi_callback_info info)
{
    napi_status status;

    // Get arguments
    size_t argc = 1; // Maximum number of arguments we'll accept
    napi_value args[1];
    status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok)
        return nullptr;

    // Check for minimum argument count (file path)
    if (argc < 1)
    {
        napi_throw_error(env, nullptr, "Wrong number of arguments. Expected file path.");
        return nullptr;
    }

    // Handle file path argument
    size_t file_path_str_size;
    status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &file_path_str_size);
    if (status != napi_ok)
    {
        napi_throw_error(env, nullptr, "First argument must be a string.");
        return nullptr;
    }

    char *file_path = new char[file_path_str_size + 1];
    status = napi_get_value_string_utf8(env, args[0], file_path, file_path_str_size + 1, nullptr);
    if (status != napi_ok)
    {
        delete[] file_path;
        napi_throw_error(env, nullptr, "Failed to read file path.");
        return nullptr;
    }

    return file_path;
}

AsyncDeleteVectorsByIdData *prepare_data_for_deletion_by_id(napi_env env, napi_callback_info info)
{
    napi_status status;
    AsyncDeleteVectorsByIdData *asyncData = new AsyncDeleteVectorsByIdData;
    asyncData->file_path = nullptr;
    asyncData->ids_to_delete = nullptr;
    asyncData->delete_count = 0;

    // Get arguments
    size_t argc = 2;
    napi_value args[2];
    status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok)
    {
        delete asyncData;
        return nullptr;
    }

    // Check argument count
    if (argc < 2)
    {
        napi_throw_error(env, nullptr, "Wrong number of arguments. Expected file path and array of IDs to delete.");
        delete asyncData;
        return nullptr;
    }

    // Get file path
    size_t file_path_str_size;
    status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &file_path_str_size);
    if (status != napi_ok)
    {
        napi_throw_error(env, nullptr, "First argument must be a string (file path).");
        delete asyncData;
        return nullptr;
    }

    asyncData->file_path = new char[file_path_str_size + 1];
    status = napi_get_value_string_utf8(env, args[0], asyncData->file_path, file_path_str_size + 1, nullptr);
    if (status != napi_ok)
    {
        delete[] asyncData->file_path;
        delete asyncData;
        napi_throw_error(env, nullptr, "Failed to read file path.");
        return nullptr;
    }

    // Check if second argument is an array
    bool isArray;
    status = napi_is_array(env, args[1], &isArray);
    if (status != napi_ok || !isArray)
    {
        delete[] asyncData->file_path;
        delete asyncData;
        napi_throw_error(env, nullptr, "Second argument must be an array of IDs to delete.");
        return nullptr;
    }

    // Get array length
    uint32_t array_length;
    status = napi_get_array_length(env, args[1], &array_length);
    if (status != napi_ok)
    {
        delete[] asyncData->file_path;
        delete asyncData;
        napi_throw_error(env, nullptr, "Could not get array length.");
        return nullptr;
    }

    if (array_length == 0)
    {
        delete[] asyncData->file_path;
        delete asyncData;
        napi_throw_error(env, nullptr, "The array of IDs to delete cannot be empty.");
        return nullptr;
    }

    // Allocate memory for the IDs
    asyncData->ids_to_delete = new int[array_length];
    asyncData->delete_count = array_length;

    // Extract each ID from the array
    for (uint32_t i = 0; i < array_length; i++)
    {
        napi_value element;
        status = napi_get_element(env, args[1], i, &element);
        if (status != napi_ok)
        {
            delete[] asyncData->file_path;
            delete[] asyncData->ids_to_delete;
            delete asyncData;
            napi_throw_error(env, nullptr, "Failed to get array element.");
            return nullptr;
        }

        // Convert to integer
        int id_value;
        status = napi_get_value_int32(env, element, &id_value);
        if (status != napi_ok)
        {
            delete[] asyncData->file_path;
            delete[] asyncData->ids_to_delete;
            delete asyncData;
            napi_throw_error(env, nullptr, "Array elements must be integers.");
            return nullptr;
        }

        asyncData->ids_to_delete[i] = id_value;
    }

    return asyncData;
}

AsyncDeleteVectorsByFilterData *prepare_data_for_deletion_by_filter(napi_env env, napi_callback_info info)
{
    napi_status status;
    AsyncDeleteVectorsByFilterData *asyncData = new AsyncDeleteVectorsByFilterData;
    asyncData->file_path = nullptr;
    asyncData->json_filter = nullptr;

    // Get arguments
    size_t argc = 2;
    napi_value args[2];
    status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok)
    {
        delete asyncData;
        return nullptr;
    }

    // Check argument count
    if (argc < 2)
    {
        napi_throw_error(env, nullptr, "Wrong number of arguments. Expected file path and array of IDs to delete.");
        delete asyncData;
        return nullptr;
    }

    // Get file path
    size_t file_path_str_size;
    status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &file_path_str_size);
    if (status != napi_ok)
    {
        napi_throw_error(env, nullptr, "First argument must be a string (file path).");
        delete asyncData;
        return nullptr;
    }

    asyncData->file_path = new char[file_path_str_size + 1];
    status = napi_get_value_string_utf8(env, args[0], asyncData->file_path, file_path_str_size + 1, nullptr);
    if (status != napi_ok)
    {
        delete[] asyncData->file_path;
        delete asyncData;
        napi_throw_error(env, nullptr, "Failed to read file path.");
        return nullptr;
    }

    // Get JSON filter
    size_t json_filter_str_size;
    status = napi_get_value_string_utf8(env, args[1], nullptr, 0, &json_filter_str_size);
    if (status != napi_ok)
    {
        delete[] asyncData->file_path;
        delete asyncData;
        napi_throw_error(env, nullptr, "Second argument must be a string (JSON filter).");
        return nullptr;
    }

    asyncData->json_filter = new char[json_filter_str_size + 1];
    status = napi_get_value_string_utf8(env, args[1], asyncData->json_filter, json_filter_str_size + 1, nullptr);
    if (status != napi_ok)
    {
        delete[] asyncData->file_path;
        delete[] asyncData->json_filter;
        delete asyncData;
        napi_throw_error(env, nullptr, "Failed to read JSON filter.");
        return nullptr;
    }

    return asyncData;
}

AsyncUpdateVectorsByIdData *prepare_data_for_update_by_id(napi_env env, napi_callback_info info)
{
    napi_status status;
    AsyncUpdateVectorsByIdData *async_data = new AsyncUpdateVectorsByIdData;
    async_data->file_path = nullptr;
    async_data->update_count = 0;
    async_data->update_items = nullptr;

    // Get arguments
    size_t argc = 2;
    napi_value args[2];
    status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok)
    {
        delete async_data;
        return nullptr;
    }

    // Check argument count
    if (argc < 2)
    {
        napi_throw_error(env, nullptr, "Wrong number of arguments. Expected file path and array of items to update.");
        delete async_data;
        return nullptr;
    }

    // Get file path
    size_t file_path_str_size;
    status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &file_path_str_size);
    if (status != napi_ok)
    {
        napi_throw_error(env, nullptr, "First argument must be a string (file path).");
        delete async_data;
        return nullptr;
    }

    async_data->file_path = new char[file_path_str_size + 1];
    status = napi_get_value_string_utf8(env, args[0], async_data->file_path, file_path_str_size + 1, nullptr);
    if (status != napi_ok)
    {
        delete[] async_data->file_path;
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to read file path.");
        return nullptr;
    }

    // Extract items array
    uint32_t items_length;
    napi_get_array_length(env, args[1], &items_length);

    // Allocate memory for update items
    DBUpsertIem *update_items = new DBUpsertIem[items_length];

    // Process each item
    for (uint32_t i = 0; i < items_length; i++)
    {
        napi_value item;
        napi_get_element(env, args[1], i, &item);

        // Get id
        napi_value id_value;
        napi_get_named_property(env, item, "id", &id_value);
        napi_get_value_int32(env, id_value, &update_items[i].id);

        // Get metadata (string)
        napi_value metadata_value;
        napi_get_named_property(env, item, "metadata", &metadata_value);
        size_t metadata_len;
        napi_get_value_string_utf8(env, metadata_value, nullptr, 0, &metadata_len);
        update_items[i].metadata = new char[metadata_len + 1];
        napi_get_value_string_utf8(env, metadata_value, update_items[i].metadata, metadata_len + 1, nullptr);

        // Get vector (Float32Array)
        napi_value vector_value;
        napi_get_named_property(env, item, "vector", &vector_value);

        // Get vector length and data
        bool is_typedarray;
        napi_is_typedarray(env, vector_value, &is_typedarray);
        if (is_typedarray)
        {
            napi_typedarray_type type;
            size_t vector_length;
            void *data;
            napi_value arraybuffer;
            size_t byte_offset;
            napi_get_typedarray_info(env, vector_value, &type, &vector_length, &data, &arraybuffer, &byte_offset);

            if (type == napi_float32_array)
            {
                // Allocate and copy vector data
                update_items[i].vector = new float[vector_length];
                update_items[i].vector_length = vector_length;
                memcpy(update_items[i].vector, static_cast<float *>(data), vector_length * sizeof(float));
            }
        }
    }

    async_data->update_items = update_items;
    async_data->update_count = items_length;

    return async_data;
}

void cleanup_async_update_data(AsyncUpdateVectorsByIdData *async_data)
{
    if (!async_data)
        return;

    // Free file path
    if (async_data->file_path)
    {
        delete[] async_data->file_path;
        async_data->file_path = nullptr;
    }

    // Free update items
    if (async_data->update_items)
    {
        for (int i = 0; i < async_data->update_count; i++)
        {
            if (async_data->update_items[i].metadata)
            {
                delete[] async_data->update_items[i].metadata;
            }
            if (async_data->update_items[i].vector)
            {
                delete[] async_data->update_items[i].vector;
            }
        }
        delete[] async_data->update_items;
        async_data->update_items = nullptr;
    }

    async_data->update_count = 0;
    async_data->actually_updated_count = 0;
    async_data->success = false;

    // Note: we don't delete the async_data itself here
    // or handle the napi_async_work/deferred
    // as that depends on the context of the cleanup
}