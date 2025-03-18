#include <node_api.h>
#include <cstring>
#include "tinyvec.h"
#include "paginate.hpp"
#include <iostream>
#include "../../../src/core/include/cJSON.h"
#include "addon_utils.h"

namespace paginate_operations
{
    // Execute callback (runs in worker thread)
    void ExecuteGetPaginatedVectors(napi_env env, void *data)
    {
        AsyncPaginationData *pagination_data = static_cast<AsyncPaginationData *>(data);

        try
        {
            PaginationResults *results = get_paginated_vectors(
                pagination_data->file_path,
                pagination_data->offset,
                pagination_data->limit);

            pagination_data->results = results;
        }
        catch (...)
        {
            pagination_data->results = nullptr;
        }
    }

    // Complete callback (runs in main thread after execution completes)
    void CompleteGetPaginatedVectors(napi_env env, napi_status status, void *data)
    {
        AsyncPaginationData *pagination_data = static_cast<AsyncPaginationData *>(data);
        napi_value result;

        if (pagination_data->results == nullptr || pagination_data->results->results == nullptr)
        {

            napi_create_array_with_length(env, 0, &result);
            napi_resolve_deferred(env, pagination_data->deferred, result);
        }
        else if (status != napi_ok)
        {
            // Create error object
            napi_value error, error_message;
            napi_create_string_utf8(env, "Failed to paginate vectors", NAPI_AUTO_LENGTH, &error_message);
            napi_create_error(env, nullptr, error_message, &error);

            // Reject the promise
            napi_reject_deferred(env, pagination_data->deferred, error);
        }
        else
        {
            // Create result array
            napi_create_array_with_length(env, pagination_data->results->count, &result);

            // Fill array with vector objects
            for (int i = 0; i < pagination_data->results->count; i++)
            {
                PaginationItem item = pagination_data->results->results[i];
                napi_value item_obj, vector_array, id_value, metadata_value;

                // Create object for this item
                napi_create_object(env, &item_obj);

                // Set id
                napi_create_int32(env, item.id, &id_value);
                napi_set_named_property(env, item_obj, "id", id_value);

                // Set metadata if available
                if (item.metadata != nullptr)
                {

                    cJSON *parsed_json = cJSON_ParseWithLength((char *)item.metadata, item.md_length);

                    if (parsed_json)
                    {
                        metadata_value = convert_json_to_napi(env, parsed_json);
                        cJSON_Delete(parsed_json); // Clean up after conversion
                        napi_set_named_property(env, item_obj, "metadata", metadata_value);
                    }
                    else
                    {
                        // Fallback to string if JSON parsing fails
                        napi_create_string_utf8(env, item.metadata, NAPI_AUTO_LENGTH, &metadata_value);
                        napi_set_named_property(env, item_obj, "metadata", metadata_value);
                    }
                }

                // Create and set vector array if available
                if (item.vector != nullptr && item.vector_length > 0)
                {
                    napi_create_array_with_length(env, item.vector_length, &vector_array);
                    for (int j = 0; j < item.vector_length; j++)
                    {
                        napi_value vec_value;
                        napi_create_double(env, static_cast<double>(item.vector[j]), &vec_value);
                        napi_set_element(env, vector_array, j, vec_value);
                    }
                    napi_set_named_property(env, item_obj, "vector", vector_array);
                }

                // Add this item to the result array
                napi_set_element(env, result, i, item_obj);
            }

            // Resolve the promise with the result array
            napi_resolve_deferred(env, pagination_data->deferred, result);
        }

        // Clean up
        CleanupAsyncPaginationData(env, pagination_data, true);
    }

    napi_value GetPaginatedVectors(napi_env env, napi_callback_info info)
    {
        napi_status status;
        size_t argc = 2;
        napi_value args[2];
        napi_value promise;
        napi_value resource_name;

        // Get arguments
        status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
        if (status != napi_ok || argc < 2)
        {
            napi_throw_error(env, nullptr, "Expected 2 arguments: file path and options object");
            return nullptr;
        }

        // Check if second argument is an object (options)
        napi_valuetype second_arg_type;
        status = napi_typeof(env, args[1], &second_arg_type);
        if (status != napi_ok || second_arg_type != napi_valuetype::napi_object)
        {
            napi_throw_error(env, nullptr, "Argument must be an options object");
            return nullptr;
        }

        // Create the AsyncPaginationData
        AsyncPaginationData *async_data = new AsyncPaginationData();
        async_data->offset = 0;  // Default values
        async_data->limit = 100; // Default values
        async_data->results = nullptr;

        // Get file path string
        size_t file_path_length;
        status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &file_path_length);
        if (status != napi_ok)
        {
            delete async_data;
            napi_throw_error(env, nullptr, "Failed to get file path length");
            return nullptr;
        }

        async_data->file_path = static_cast<char *>(malloc(file_path_length + 1));
        if (!async_data->file_path)
        {
            delete async_data;
            napi_throw_error(env, nullptr, "Failed to allocate memory for file path");
            return nullptr;
        }

        status = napi_get_value_string_utf8(env, args[0], async_data->file_path, file_path_length + 1, nullptr);
        if (status != napi_ok)
        {
            CleanupAsyncPaginationData(env, async_data, true);
            napi_throw_error(env, nullptr, "Failed to get file path value");
            return nullptr;
        }

        // Extract offset (skip) from options object
        napi_value offset_value;
        status = napi_get_named_property(env, args[1], "skip", &offset_value);
        if (status == napi_ok)
        {
            napi_valuetype offset_type;
            status = napi_typeof(env, offset_value, &offset_type);
            if (status == napi_ok && offset_type == napi_valuetype::napi_number)
            {
                status = napi_get_value_uint32(env, offset_value, &async_data->offset);
                if (status != napi_ok)
                {
                    // Use default value if conversion fails
                    async_data->offset = 0;
                }
            }
        }

        // Extract limit from options object
        napi_value limit_value;
        status = napi_get_named_property(env, args[1], "limit", &limit_value);
        if (status == napi_ok)
        {
            napi_valuetype limit_type;
            status = napi_typeof(env, limit_value, &limit_type);
            if (status == napi_ok && limit_type == napi_valuetype::napi_number)
            {
                status = napi_get_value_uint32(env, limit_value, &async_data->limit);
                if (status != napi_ok)
                {
                    // Use default value if conversion fails
                    async_data->limit = 10;
                }
            }
        }

        // Create promise
        status = napi_create_promise(env, &async_data->deferred, &promise);
        if (status != napi_ok)
        {
            CleanupAsyncPaginationData(env, async_data, true);
            napi_throw_error(env, nullptr, "Failed to create promise");
            return nullptr;
        }

        // Create resource name for async work
        status = napi_create_string_utf8(env, "GetPaginatedVectors", NAPI_AUTO_LENGTH, &resource_name);
        if (status != napi_ok)
        {
            CleanupAsyncPaginationData(env, async_data, true);
            napi_throw_error(env, nullptr, "Failed to create resource name");
            return nullptr;
        }

        // Create async work
        status = napi_create_async_work(
            env,
            nullptr,
            resource_name,
            ExecuteGetPaginatedVectors,
            CompleteGetPaginatedVectors,
            async_data,
            &async_data->work);

        if (status != napi_ok)
        {
            CleanupAsyncPaginationData(env, async_data, true);
            napi_throw_error(env, nullptr, "Failed to create async work");
            return nullptr;
        }

        // Queue async work
        status = napi_queue_async_work(env, async_data->work);
        if (status != napi_ok)
        {
            CleanupAsyncPaginationData(env, async_data, true);
            delete async_data;
            napi_throw_error(env, nullptr, "Failed to queue async work");
            return nullptr;
        }

        return promise;
    }

    void FreePaginationResults(PaginationResults *results)
    {
        if (results == nullptr)
        {
            return;
        }

        // Free each item in the results array
        if (results->results != nullptr)
        {
            for (int i = 0; i < results->count; i++)
            {
                PaginationItem *item = &(results->results[i]);

                // Free metadata if it exists
                if (item->metadata != nullptr)
                {
                    free(item->metadata);
                    item->metadata = nullptr;
                }

                // Free vector if it exists
                if (item->vector != nullptr)
                {
                    free(item->vector);
                    item->vector = nullptr;
                }
            }

            // Free the results array itself
            free(results->results);
            results->results = nullptr;
        }

        // Free the results structure
        free(results);
    }

    /**
     * Cleans up all resources allocated for AsyncPaginationData
     * This should be called when async work is complete (whether successful or not)
     *
     * @param env The N-API environment
     * @param data Pointer to the AsyncPaginationData structure to clean up
     * @param work_status Whether to also delete the async work
     */
    void CleanupAsyncPaginationData(napi_env env, AsyncPaginationData *data, bool delete_work = true)
    {
        if (data == nullptr)
        {
            return;
        }

        // Free the file path if it exists
        if (data->file_path != nullptr)
        {
            free(data->file_path);
            data->file_path = nullptr;
        }

        // Free the pagination results
        if (data->results != nullptr)
        {
            FreePaginationResults(data->results);
            data->results = nullptr;
        }

        // Delete the async work if requested
        if (delete_work && data->work != nullptr)
        {
            napi_delete_async_work(env, data->work);
            data->work = nullptr;
        }

        // Free the data structure itself
        delete data;
    }

    // Module registration
    napi_value RegisterPaginateModule(napi_env env, napi_value exports)
    {
        napi_status status;
        napi_value paginate_fn;

        status = napi_create_function(env, nullptr, 0, GetPaginatedVectors, nullptr, &paginate_fn);
        if (status != napi_ok)
            return nullptr;

        status = napi_set_named_property(env, exports, "getPaginatedVectors", paginate_fn);
        if (status != napi_ok)
            return nullptr;

        return exports;
    }
}