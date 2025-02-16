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
        status = napi_get_named_property(env, vector_obj, "vec", &vec_value);
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
    size_t argc = 2; // Maximum number of arguments we'll accept
    napi_value args[2];
    status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok)
        return nullptr;

    // Check for minimum argument count (file path)
    if (argc < 1)
    {
        napi_throw_error(env, nullptr, "Wrong number of arguments. Expected file path.");
        delete connection_data; // Don't forget to clean up
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
                    // Note: If this fails, we'll just keep the default value of 0
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