#include <node_api.h>
#include <vector>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include "tinyvec.h"
#include "../../../src/core/include/cJSON.h"
#include "../../../src/core/include/file.h"
#include "addon_utils.h"
#include "paginate.hpp"

namespace
{

    struct SearchResult
    {
        int index;
        float similarity;
        MetadataBytes metadata;
    };

    struct AsyncSearchData
    {
        float *query_vec;
        int top_k;
        char *file_path;
        char *metadata_filters;
        DBSearchResult *raw_result;
        int returned_results;
        std::vector<SearchResult> results;
        napi_deferred deferred;
        napi_async_work work;
    };

    struct InstanceConnectionPromiseData
    {
        // char *file_path;
        std::string file_path;
        uint32_t dimensions;
        int result;
        napi_deferred deferred;
        napi_async_work work;
    };

    struct GetIndexStatsPromiseData
    {
        char *file_path;
        uint32_t dimensions;
        IndexFileStats result;
        napi_deferred deferred;
        napi_async_work work;
    };

    // static napi_ref stringify_ref = nullptr;

    // Execute callback (runs in worker thread)
    void ExecuteSearch(napi_env env, void *data)
    {
        AsyncSearchData *searchData = static_cast<AsyncSearchData *>(data);

        try
        {
            std::vector<SearchResult> result_vec;

            DBSearchResult *raw_result = nullptr;

            if (searchData->metadata_filters)
            {
                raw_result = vector_query_with_filter(
                    searchData->file_path,
                    searchData->query_vec,
                    searchData->top_k,
                    searchData->metadata_filters);
            }
            else
            {
                raw_result = vector_query(
                    searchData->file_path,
                    searchData->query_vec,
                    searchData->top_k);
            }

            // Handle NULL result
            if (raw_result == nullptr)
            {
                searchData->results = std::vector<SearchResult>();
                return;
            }

            // Save raw result for cleanup
            searchData->raw_result = raw_result;

            // Check for empty results array
            if (!raw_result->results || raw_result->count <= 0)
            {
                searchData->results = std::vector<SearchResult>();
                return;
            }

            // Use the actual count from the search result, which may be less than top_k
            int result_count = (searchData->top_k < raw_result->count) ? searchData->top_k : raw_result->count;

            // Reserve space for the results
            result_vec.reserve(result_count);

            // Copy the results to the result vector
            for (int i = 0; i < result_count; i++)
            {
                result_vec.push_back({raw_result->results[i].index,
                                      raw_result->results[i].similarity,
                                      raw_result->results[i].metadata});
            }

            searchData->results = std::move(result_vec);
        }
        catch (const std::exception &)
        {
            searchData->results.clear();
        }
        catch (...)
        {
            searchData->results.clear();
        }
    }
    // Complete callback (runs in main thread)
    void CompleteSearch(napi_env env, napi_status status, void *data)
    {
        AsyncSearchData *searchData = static_cast<AsyncSearchData *>(data);

        // Create return array
        napi_value returnArray;
        status = napi_create_array(env, &returnArray);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create array", NAPI_AUTO_LENGTH, &error_msg);
            napi_reject_deferred(env, searchData->deferred, error_msg);
            napi_delete_async_work(env, searchData->work);
            delete searchData;
            return;
        }

        napi_value global;
        napi_get_global(env, &global);
        napi_value json, parse;
        napi_get_named_property(env, global, "JSON", &json);
        napi_get_named_property(env, json, "parse", &parse);

        // Convert results to JS objects
        for (size_t i = 0; i < searchData->results.size(); i++)
        {
            napi_value resultObj;
            status = napi_create_object(env, &resultObj);
            if (status != napi_ok)
                break;

            napi_value index, score, metadata;
            status = napi_create_int32(env, searchData->results[i].index, &index);
            if (status != napi_ok)
                break;
            status = napi_create_double(env, searchData->results[i].similarity, &score);
            if (status != napi_ok)
                break;

            cJSON *parsed_json = cJSON_ParseWithLength((char *)searchData->results[i].metadata.data, searchData->results[i].metadata.length);

            if (parsed_json)
            {
                metadata = convert_json_to_napi(env, parsed_json);
                cJSON_Delete(parsed_json); // Clean up after conversion
            }

            status = napi_set_named_property(env, resultObj, "id", index);
            if (status != napi_ok)
                break;
            status = napi_set_named_property(env, resultObj, "similarity", score);
            if (status != napi_ok)
                break;

            status = napi_set_element(env, returnArray, i, resultObj);
            if (status != napi_ok)
                break;

            // Add metadata to result object
            napi_set_named_property(env, resultObj, "metadata", metadata);
        }

        // If there was an error, reject the promise
        if (status != napi_ok)
        {
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create result object", NAPI_AUTO_LENGTH, &error_msg);
            status = napi_reject_deferred(env, searchData->deferred, error_msg);
        }
        else
        {
            // Resolve promise
            status = napi_resolve_deferred(env, searchData->deferred, returnArray);
        }

        status = napi_delete_async_work(env, searchData->work);

        // Clean up
        // if (searchData->raw_result)
        // {
        //     // Free metadata for each result
        //     if (searchData->raw_result->results)
        //     {
        //         for (int i = 0; i < searchData->raw_result->count; i++)
        //         {
        //             if (searchData->raw_result->results[i].metadata.data)
        //             {
        //                 free(searchData->raw_result->results[i].metadata.data);
        //             }
        //         }
        //         free(searchData->raw_result->results);
        //     }
        //     free(searchData->raw_result);
        // }
        if (searchData->metadata_filters)
        {
            delete[] searchData->metadata_filters;
        }
        if (searchData->file_path)
        {
            delete[] searchData->file_path;
        }
        delete searchData;
    }

    napi_value Search(napi_env env, napi_callback_info info)
    {
        napi_status status;

        // Get arguments
        size_t argc = 4;
        // Array of args
        napi_value args[4];
        status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
        if (status != napi_ok)
            return nullptr;

        // Check if we received the required arguments
        if (argc < 3)
        {
            napi_throw_error(env, nullptr, "Wrong number of arguments. Expected an array of numbers and a top-k value.");
            return nullptr;
        }

        // Verify that first argument is an array
        bool isTypedArray;
        status = napi_is_typedarray(env, args[0], &isTypedArray);
        if (status != napi_ok || !isTypedArray)
        {
            napi_throw_error(env, nullptr, "First argument must be a Float 32 array.");
            return nullptr;
        }

        napi_typedarray_type type;
        size_t length;
        void *data;
        status = napi_get_typedarray_info(env, args[0], &type, &length, &data, nullptr, nullptr);
        if (type != napi_float32_array)
        {
            napi_throw_error(env, nullptr, "Argument must be a Float32Array");
            return nullptr;
        }

        int top_k;
        status = napi_get_value_int32(env, args[1], &top_k);
        if (status != napi_ok)
        {
            // Handle error
            napi_throw_error(env, nullptr, "Second argument must be a number.");
            return nullptr;
        }

        // Ensure top_k is positive
        if (top_k <= 0)
        {
            napi_throw_error(env, nullptr, "Top_k must be positive.");
            return nullptr;
        }

        size_t file_path_str_size;
        status = napi_get_value_string_utf8(env, args[2], nullptr, 0, &file_path_str_size);
        if (status != napi_ok)
        {
            // Handle error
            napi_throw_error(env, nullptr, "Third argument must be a string.");
            return nullptr;
        }

        char *file_path = new char[file_path_str_size + 1];
        status = napi_get_value_string_utf8(env, args[2], file_path, file_path_str_size + 1, nullptr);
        if (status != napi_ok)
        {
            napi_throw_error(env, nullptr, "Failed to read file path.");
            return nullptr;
        }

        AsyncSearchData *asyncData = new AsyncSearchData;
        float *float_data = static_cast<float *>(data);
        asyncData->query_vec = float_data;
        asyncData->top_k = top_k;
        asyncData->file_path = file_path;
        asyncData->metadata_filters = NULL;

        if (argc > 3)
        {

            napi_valuetype valuetype;
            status = napi_typeof(env, args[3], &valuetype);

            if (status == napi_ok && valuetype == napi_object)
            {
                // Check if the object has a "filter" property
                bool hasFilterProperty;
                status = napi_has_named_property(env, args[3], "filter", &hasFilterProperty);

                if (status == napi_ok && hasFilterProperty)
                {
                    // Get the filter property value
                    napi_value filterValue;
                    status = napi_get_named_property(env, args[3], "filter", &filterValue);

                    if (status == napi_ok)
                    {
                        // Get the string size
                        size_t filter_str_size;
                        status = napi_get_value_string_utf8(env, filterValue, nullptr, 0, &filter_str_size);

                        if (status == napi_ok)
                        {
                            // Allocate memory for the filter string
                            char *metadata_filters = new char[filter_str_size + 1];
                            status = napi_get_value_string_utf8(env, filterValue, metadata_filters, filter_str_size + 1, nullptr);

                            if (status == napi_ok)
                            {
                                asyncData->metadata_filters = metadata_filters;
                            }
                            else
                            {
                                delete[] metadata_filters;
                            }
                        }
                    }
                }
            }
        }

        // Create promise
        napi_value promise;
        status = napi_create_promise(env, &asyncData->deferred, &promise);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create promise", NAPI_AUTO_LENGTH, &error_msg);
            napi_reject_deferred(env, asyncData->deferred, error_msg);
            delete asyncData;
            return nullptr;
        }

        // Create async work
        napi_value resource_name;
        status = napi_create_string_utf8(env, "Search", NAPI_AUTO_LENGTH, &resource_name);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create resource name", NAPI_AUTO_LENGTH, &error_msg);
            napi_reject_deferred(env, asyncData->deferred, error_msg);
            delete asyncData;
            return nullptr;
        }

        status = napi_create_async_work(
            env,
            nullptr,
            resource_name,
            ExecuteSearch,
            CompleteSearch,
            asyncData,
            &asyncData->work);

        // Queue the work
        status = napi_queue_async_work(env, asyncData->work);

        // Return the promise
        return promise;
    }

    void ExecuteInsertVectorsAsync(napi_env env, void *data)
    {
        AsyncInsertData *asyncData = static_cast<AsyncInsertData *>(data);
        int inserted_vecs = 0;
        try
        {
            inserted_vecs = insert_many_vectors(asyncData->file_path, asyncData->vectors.data(),
                                                asyncData->metadatas.data(), asyncData->metadata_lengths.data(),
                                                asyncData->vectors.size(), asyncData->dimensions);

            delete[] asyncData->file_path;
            for (size_t i = 0; i < asyncData->vectors.size(); i++)
            {
                delete[] asyncData->metadatas[i];
            }
            asyncData->insert_count = inserted_vecs;
            // return inserted_vecs_val;
        }
        catch (...)
        {
            // Handle errors in complete callback
            asyncData->insert_count = inserted_vecs;
        }
    }

    void CompleteInsertVectorsAsync(napi_env env, napi_status status, void *data)
    {
        AsyncInsertData *insertData = static_cast<AsyncInsertData *>(data);

        // Create napi return value
        napi_value inserted_vecs_val;
        status = napi_create_int32(env, insertData->insert_count, &inserted_vecs_val);
        // status = napi_create_bigint_int64(env, insertData->insert_count, &inserted_vecs_val);

        // Verify the value
        int32_t verify_val;
        status = napi_get_value_int32(env, inserted_vecs_val, &verify_val);

        napi_value final_result;
        status = napi_create_int32(env, verify_val, &final_result);

        // If there was an error, reject the promise
        if (status != napi_ok)
        {
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to insert vectors.", NAPI_AUTO_LENGTH, &error_msg);
            status = napi_reject_deferred(env, insertData->deferred, error_msg);
        }
        else
        {
            // Resolve promise
            status = napi_resolve_deferred(env, insertData->deferred, final_result);
        }

        // Clean up
        status = napi_delete_async_work(env, insertData->work);
        delete insertData;
    }

    napi_value InsertVectorsAsync(napi_env env, napi_callback_info info)
    {

        AsyncInsertData *asyncData = prepare_data_for_insertion(env, info);

        // Create promise
        napi_value promise;
        napi_status status;
        status = napi_create_promise(env, &asyncData->deferred, &promise);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create promise", NAPI_AUTO_LENGTH, &error_msg);
            napi_reject_deferred(env, asyncData->deferred, error_msg);
            delete asyncData;
            return nullptr;
        }

        // Create async work
        napi_value resource_name;
        status = napi_create_string_utf8(env, "InsertVectorsAsync", NAPI_AUTO_LENGTH, &resource_name);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create resource name", NAPI_AUTO_LENGTH, &error_msg);
            napi_reject_deferred(env, asyncData->deferred, error_msg);
            delete asyncData;
            return nullptr;
        }

        status = napi_create_async_work(
            env,
            nullptr,
            resource_name,
            ExecuteInsertVectorsAsync,
            CompleteInsertVectorsAsync,
            asyncData,
            &asyncData->work);

        // Queue the work
        status = napi_queue_async_work(env, asyncData->work);

        // Return the promise
        return promise;
    };

    void ExecuteConnection(napi_env env, void *data)
    {
        InstanceConnectionPromiseData *promise_data = static_cast<InstanceConnectionPromiseData *>(data);

        TinyVecConnectionConfig tinyvec_config;
        tinyvec_config.dimensions = promise_data->dimensions;

        std::string file_path_copy = promise_data->file_path;

        try
        {
            TinyVecConnection *connection_config = connect_to_db(file_path_copy.c_str(), &tinyvec_config);
            if (!connection_config)
            {
                promise_data->result = 0;
                return;
            }
            promise_data->dimensions = connection_config->dimensions;
            promise_data->result = 1;
        }
        catch (...)
        {
            // Handle errors in complete callback
            promise_data->result = 0;
        }
    };

    void CompleteConnection(napi_env env, napi_status status, void *data)
    {
        InstanceConnectionPromiseData *promise_data = static_cast<InstanceConnectionPromiseData *>(data);

        // Create the return object
        napi_value return_object;
        status = napi_create_object(env, &return_object);

        // Set the path property
        napi_value napi_config_file_path;
        status = napi_create_string_utf8(env, promise_data->file_path.c_str(), NAPI_AUTO_LENGTH, &napi_config_file_path);
        status = napi_set_named_property(env, return_object, "filePath", napi_config_file_path);

        // delete[] promise_data->file_path;

        // If there was an error, reject the promise
        if (status != napi_ok || promise_data->result != 1)
        {
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to connect.", NAPI_AUTO_LENGTH, &error_msg);
            status = napi_reject_deferred(env, promise_data->deferred, error_msg);
        }
        else
        {
            // Resolve promise with the object containing filePath
            status = napi_resolve_deferred(env, promise_data->deferred, return_object);
        }

        // Clean up
        status = napi_delete_async_work(env, promise_data->work);
        delete promise_data;
    };

    void ExecuteGetIndexStats(napi_env env, void *data)
    {
        GetIndexStatsPromiseData *promise_data = static_cast<GetIndexStatsPromiseData *>(data);
        std::string file_path_copy = promise_data->file_path;

        try
        {
            IndexFileStats stats = get_index_file_stats_from_db(file_path_copy.c_str());
            promise_data->result = stats;
        }
        catch (...)
        {
            // Handle errors in complete callback
            promise_data->result = IndexFileStats{0, 0};
        }
    };

    void CompleteGetIndexStats(napi_env env, napi_status status, void *data)
    {
        GetIndexStatsPromiseData *promise_data = static_cast<GetIndexStatsPromiseData *>(data);

        // Set the path property
        napi_value napi_config_file_path;
        status = napi_create_string_utf8(env, promise_data->file_path, NAPI_AUTO_LENGTH, &napi_config_file_path);

        // Create the return object
        napi_value return_object;
        status = napi_create_object(env, &return_object);

        uint32_t dimensions = promise_data->result.dimensions;
        uint32_t vector_count = promise_data->result.vector_count;

        // Create and set dimensions
        napi_value napi_dimensions;
        status = napi_create_uint32(env, dimensions, &napi_dimensions);

        status = napi_set_named_property(env, return_object, "dimensions", napi_dimensions);

        // Create and set vector count
        napi_value napi_vector_count;
        status = napi_create_uint32(env, vector_count, &napi_vector_count);

        status = napi_set_named_property(env, return_object, "vectors", napi_vector_count);

        delete[] promise_data->file_path;

        // If there was an error, reject the promise
        if (status != napi_ok)
        {
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to get index stats.", NAPI_AUTO_LENGTH, &error_msg);
            status = napi_reject_deferred(env, promise_data->deferred, error_msg);
        }
        else
        {
            // Resolve promise with the object containing filePath
            status = napi_resolve_deferred(env, promise_data->deferred, return_object);
        }

        // Clean up
        status = napi_delete_async_work(env, promise_data->work);
        delete promise_data;
    };

    napi_value Connect(napi_env env, napi_callback_info info)
    {

        ConnectionData *connection_data = prepare_data_for_connection(env, info);

        InstanceConnectionPromiseData *promise_data = new InstanceConnectionPromiseData;
        promise_data->file_path = std::move(connection_data->file_path);
        promise_data->dimensions = connection_data->dimensions;

        delete connection_data;

        // Create promise
        napi_value promise;
        napi_status status;
        status = napi_create_promise(env, &promise_data->deferred, &promise);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create promise", NAPI_AUTO_LENGTH, &error_msg);
            napi_reject_deferred(env, promise_data->deferred, error_msg);
            delete promise_data;
            return nullptr;
        }

        // Create async work
        napi_value resource_name;
        status = napi_create_string_utf8(env, "Connect", NAPI_AUTO_LENGTH, &resource_name);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create resource name", NAPI_AUTO_LENGTH, &error_msg);
            napi_reject_deferred(env, promise_data->deferred, error_msg);
            delete promise_data;
            return nullptr;
        }

        status = napi_create_async_work(
            env,
            nullptr,
            resource_name,
            ExecuteConnection,
            CompleteConnection,
            promise_data,
            &promise_data->work);

        // Queue the work
        status = napi_queue_async_work(env, promise_data->work);

        // Return the promise
        return promise;
    };
    napi_value ConnectSync(napi_env env, napi_callback_info info)
    {
        ConnectionData *connection_data = prepare_data_for_connection(env, info);
        if (!connection_data)
        {
            return nullptr;
        }

        TinyVecConnectionConfig tinyvec_config;
        tinyvec_config.dimensions = connection_data->dimensions;
        napi_status status;

        try
        {
            TinyVecConnection *connection_config = connect_to_db(connection_data->file_path, &tinyvec_config);
            if (!connection_config)
            {
                napi_throw_error(env, nullptr, "Failed to connect to database");
                delete connection_data;
                return nullptr;
            }

            // Create the return object
            napi_value return_object;
            status = napi_create_object(env, &return_object);
            if (status != napi_ok)
            {
                napi_throw_error(env, nullptr, "Failed to create return object");
                delete connection_data;
                return nullptr;
            }

            napi_value napi_config_file_path;
            status = napi_create_string_utf8(env, connection_config->file_path, NAPI_AUTO_LENGTH, &napi_config_file_path);
            if (status != napi_ok)
            {
                napi_throw_error(env, nullptr, "Failed to create file path string");
                delete connection_data;
                return nullptr;
            }

            status = napi_set_named_property(env, return_object, "filePath", napi_config_file_path);
            if (status != napi_ok)
            {
                napi_throw_error(env, nullptr, "Failed to set file path property");
                delete connection_data;
                return nullptr;
            }

            // Clean up and return
            delete connection_data;
            return return_object;
        }
        catch (const std::exception &e)
        {
            napi_throw_error(env, nullptr, e.what());
            delete connection_data;
            return nullptr;
        }
        catch (...)
        {
            napi_throw_error(env, nullptr, "Unknown error occurred during connection");
            delete connection_data;
            return nullptr;
        }
    }
    napi_value GetIndexStats(napi_env env, napi_callback_info info)
    {

        char *file_path = prepare_data_for_index_stats(env, info);
        // Create promise
        napi_value promise;
        napi_status status;

        GetIndexStatsPromiseData *promise_data = new GetIndexStatsPromiseData;
        promise_data->file_path = std::move(file_path);

        status = napi_create_promise(env, &promise_data->deferred, &promise);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create promise", NAPI_AUTO_LENGTH, &error_msg);
            napi_reject_deferred(env, promise_data->deferred, error_msg);
            delete promise_data;
            return nullptr;
        }

        // Create async work
        napi_value resource_name;
        status = napi_create_string_utf8(env, "IndexStats", NAPI_AUTO_LENGTH, &resource_name);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create resource name", NAPI_AUTO_LENGTH, &error_msg);
            napi_reject_deferred(env, promise_data->deferred, error_msg);
            delete promise_data;
            return nullptr;
        }

        status = napi_create_async_work(
            env,
            nullptr,
            resource_name,
            ExecuteGetIndexStats,
            CompleteGetIndexStats,
            promise_data,
            &promise_data->work);

        // Queue the work
        status = napi_queue_async_work(env, promise_data->work);

        // Return the promise
        return promise;
    };

    napi_value UpdateDatabaseFileConnection(napi_env env, napi_callback_info info)
    {
        // Get args from JavaScript
        size_t argc = 1;
        napi_value args[1];
        napi_status status;
        napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

        // Check args
        if (argc < 1)
        {
            napi_throw_error(env, nullptr, "Wrong number of arguments");
            return nullptr;
        }

        size_t file_path_str_size;
        // First get the string size needed
        status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &file_path_str_size);
        if (status != napi_ok)
        {
            napi_throw_error(env, nullptr, "Failed to get string length");
            return nullptr;
        }
        char *file_path = new char[file_path_str_size + 1];

        // Now actually get the string
        status = napi_get_value_string_utf8(env, args[0], file_path, file_path_str_size + 1, &file_path_str_size);
        if (status != napi_ok)
        {
            delete[] file_path; // Clean up if we fail
            napi_throw_error(env, nullptr, "Failed to get string value");
            return nullptr;
        }

        napi_value result_value;
        try
        {
            bool update_result = update_instance_db_file_connection(file_path);

            status = napi_get_boolean(env, update_result, &result_value);
            if (status != napi_ok)
            {
                delete[] file_path;
                napi_throw_error(env, nullptr, "Failed to create return value");
                return nullptr;
            }

            delete[] file_path;
            return result_value;
        }
        catch (...)
        {
            delete[] file_path;
            napi_throw_error(env, nullptr, "Failed to update mmaps");
            return nullptr;
        }
    }

    void ExecuteDeleteVectorsById(napi_env env, void *data)
    {
        AsyncDeleteVectorsByIdData *asyncData = static_cast<AsyncDeleteVectorsByIdData *>(data);

        int actually_deleted = 0;
        actually_deleted = delete_vecs_by_ids(
            asyncData->file_path,
            asyncData->ids_to_delete,
            asyncData->delete_count);

        // asyncData->work = actually_deleted <= 0 ? nullptr : reinterpret_cast<napi_async_work>(1);
        asyncData->actually_deleted_count = actually_deleted;
        asyncData->success = (actually_deleted > 0);
    }

    void CompleteDeleteVectorsById(napi_env env, napi_status status, void *data)
    {
        AsyncDeleteVectorsByIdData *asyncData = static_cast<AsyncDeleteVectorsByIdData *>(data);

        // Check if the operation was successful
        bool success = asyncData->work != nullptr;

        // Create result object
        napi_value result_obj;
        napi_create_object(env, &result_obj);

        // Add count property
        napi_value count_value;
        napi_create_int32(env, asyncData->actually_deleted_count, &count_value);
        napi_set_named_property(env, result_obj, "deletedCount", count_value);

        // Add success property
        napi_value success_value;
        napi_get_boolean(env, true, &success_value);
        napi_set_named_property(env, result_obj, "success", success_value);

        // Resolve the promise
        napi_resolve_deferred(env, asyncData->deferred, result_obj);

        // Clean up
        napi_delete_async_work(env, asyncData->work);
        delete[] asyncData->file_path;
        delete[] asyncData->ids_to_delete;
        delete asyncData;
    }

    napi_value DeleteVectorsById(napi_env env, napi_callback_info info)
    {
        napi_status status;

        // Prepare the data for deletion
        AsyncDeleteVectorsByIdData *asyncData = prepare_data_for_deletion_by_id(env, info);
        if (!asyncData)
        {
            return nullptr;
        }

        // Create promise
        napi_value promise;
        status = napi_create_promise(env, &asyncData->deferred, &promise);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create promise", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            delete[] asyncData->file_path;
            delete[] asyncData->ids_to_delete;
            delete asyncData;
            return nullptr;
        }

        // Create async work name
        napi_value resource_name;
        status = napi_create_string_utf8(env, "DeleteVectorsById", NAPI_AUTO_LENGTH, &resource_name);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create resource name", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            delete[] asyncData->file_path;
            delete[] asyncData->ids_to_delete;
            delete asyncData;
            return nullptr;
        }

        // Create async work
        status = napi_create_async_work(
            env,
            nullptr,
            resource_name,
            ExecuteDeleteVectorsById,
            CompleteDeleteVectorsById,
            asyncData,
            &asyncData->work);

        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create async work", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            delete[] asyncData->file_path;
            delete[] asyncData->ids_to_delete;
            delete asyncData;
            return nullptr;
        }

        // Queue the work
        status = napi_queue_async_work(env, asyncData->work);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to queue async work", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            napi_delete_async_work(env, asyncData->work);
            delete[] asyncData->file_path;
            delete[] asyncData->ids_to_delete;
            delete asyncData;
            return nullptr;
        }

        // Return the promise
        return promise;
    }

    void ExecuteDeleteVectorsByFilter(napi_env env, void *data)
    {
        AsyncDeleteVectorsByFilterData *asyncData = static_cast<AsyncDeleteVectorsByFilterData *>(data);

        int actually_deleted = 0;
        actually_deleted = delete_vecs_by_filter(
            asyncData->file_path, asyncData->json_filter);

        asyncData->actually_deleted_count = actually_deleted;
        asyncData->success = (actually_deleted > 0);
    }

    void CompleteDeleteVectorsByFilter(napi_env env, napi_status status, void *data)
    {
        AsyncDeleteVectorsByFilterData *asyncData = static_cast<AsyncDeleteVectorsByFilterData *>(data);

        // Check if the operation was successful
        bool success = asyncData->success;

        // Create result object
        napi_value result_obj;
        napi_create_object(env, &result_obj);

        // Add count property
        napi_value count_value;
        napi_create_int32(env, asyncData->actually_deleted_count, &count_value);
        napi_set_named_property(env, result_obj, "deletedCount", count_value);

        // Add success property
        napi_value success_value;
        napi_get_boolean(env, success, &success_value);
        napi_set_named_property(env, result_obj, "success", success_value);

        // Resolve the promise
        napi_resolve_deferred(env, asyncData->deferred, result_obj);

        // Clean up
        napi_delete_async_work(env, asyncData->work);
        delete[] asyncData->file_path;
        delete[] asyncData->json_filter;
        delete asyncData;
    }

    napi_value DeleteVectorsByFilter(napi_env env, napi_callback_info info)
    {
        napi_status status;

        // Prepare the data for deletion
        AsyncDeleteVectorsByFilterData *asyncData = prepare_data_for_deletion_by_filter(env, info);
        if (!asyncData)
        {
            return nullptr;
        }

        // Create promise
        napi_value promise;
        status = napi_create_promise(env, &asyncData->deferred, &promise);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create promise", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            delete[] asyncData->file_path;
            delete[] asyncData->json_filter;
            delete asyncData;
            return nullptr;
        }

        // Create async work name
        napi_value resource_name;
        status = napi_create_string_utf8(env, "DeleteVectorsByFilter", NAPI_AUTO_LENGTH, &resource_name);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create resource name", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            delete[] asyncData->file_path;
            delete[] asyncData->json_filter;
            delete asyncData;
            return nullptr;
        }

        // Create async work
        status = napi_create_async_work(
            env,
            nullptr,
            resource_name,
            ExecuteDeleteVectorsByFilter,
            CompleteDeleteVectorsByFilter,
            asyncData,
            &asyncData->work);

        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create async work", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            delete[] asyncData->file_path;
            delete[] asyncData->json_filter;
            delete asyncData;
            return nullptr;
        }

        // Queue the work
        status = napi_queue_async_work(env, asyncData->work);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to queue async work", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            napi_delete_async_work(env, asyncData->work);
            delete[] asyncData->file_path;
            delete[] asyncData->json_filter;
            delete asyncData;
            return nullptr;
        }

        // Return the promise
        return promise;
    }

    void ExecuteUpdateVectorsById(napi_env env, void *data)
    {
        AsyncUpdateVectorsByIdData *async_data = static_cast<AsyncUpdateVectorsByIdData *>(data);

        int actually_updated = update_items_by_id(
            async_data->file_path,
            async_data->update_items,
            async_data->update_count);

        async_data->actually_updated_count = actually_updated;
        async_data->success = (actually_updated > 0);
    }

    void CompleteUpdateVectorsById(napi_env env, napi_status status, void *data)
    {
        AsyncUpdateVectorsByIdData *async_data = static_cast<AsyncUpdateVectorsByIdData *>(data);

        // Check if the operation was successful
        bool success = async_data->success;

        // Create result object
        napi_value result_obj;
        napi_create_object(env, &result_obj);

        // Add count property
        napi_value count_value;
        napi_create_int32(env, async_data->actually_updated_count, &count_value);
        napi_set_named_property(env, result_obj, "updatedCount", count_value);

        // Add success property
        napi_value success_value;
        napi_get_boolean(env, success, &success_value);
        napi_set_named_property(env, result_obj, "success", success_value);

        // Resolve the promise
        napi_resolve_deferred(env, async_data->deferred, result_obj);

        // Clean up
        napi_delete_async_work(env, async_data->work);
        cleanup_async_update_data(async_data);
        delete async_data;
    }

    napi_value UpdateVectorsById(napi_env env, napi_callback_info info)
    {
        napi_status status;

        // Prepare the data for deletion
        AsyncUpdateVectorsByIdData *async_data = prepare_data_for_update_by_id(env, info);
        if (!async_data)
        {
            return nullptr;
        }

        // Create promise
        napi_value promise;
        status = napi_create_promise(env, &async_data->deferred, &promise);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create promise", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            cleanup_async_update_data(async_data);
            delete async_data;
            return nullptr;
        }

        // Create async work name
        napi_value resource_name;
        status = napi_create_string_utf8(env, "UpdateVectorsById", NAPI_AUTO_LENGTH, &resource_name);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create resource name", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            cleanup_async_update_data(async_data);
            delete async_data;
            return nullptr;
        }

        // Create async work
        status = napi_create_async_work(
            env,
            nullptr,
            resource_name,
            ExecuteUpdateVectorsById,
            CompleteUpdateVectorsById,
            async_data,
            &async_data->work);

        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to create async work", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            cleanup_async_update_data(async_data);
            delete async_data;
            return nullptr;
        }

        // Queue the work
        status = napi_queue_async_work(env, async_data->work);
        if (status != napi_ok)
        {
            // Handle error
            napi_value error_msg;
            napi_create_string_utf8(env, "Failed to queue async work", NAPI_AUTO_LENGTH, &error_msg);
            napi_throw(env, error_msg);
            napi_delete_async_work(env, async_data->work);
            cleanup_async_update_data(async_data);
            delete async_data;
            return nullptr;
        }

        // Return the promise
        return promise;
    }

    napi_value
    Init(napi_env env, napi_value exports)
    {
        napi_status status;
        napi_value searchFn, insertFnAsync, connectFn, getIndexStatsFn, updateDbFileConnectionFn, deleteVectorsByIdFn, deleteVectorsByFilterFn, updateVectorsByIdFn;

        // Create search function
        status = napi_create_function(env, nullptr, 0, Search, nullptr, &searchFn);
        if (status != napi_ok)
            return nullptr;

        status = napi_create_function(env, nullptr, 0, InsertVectorsAsync, nullptr, &insertFnAsync);
        if (status != napi_ok)
            return nullptr;

        status = napi_create_function(env, nullptr, 0, ConnectSync, nullptr, &connectFn);
        if (status != napi_ok)
            return nullptr;

        status = napi_create_function(env, nullptr, 0, GetIndexStats, nullptr, &getIndexStatsFn);
        if (status != napi_ok)
            return nullptr;

        status = napi_create_function(env, nullptr, 0, UpdateDatabaseFileConnection, nullptr, &updateDbFileConnectionFn);
        if (status != napi_ok)
            return nullptr;

        status = napi_create_function(env, nullptr, 0, DeleteVectorsById, nullptr, &deleteVectorsByIdFn);
        if (status != napi_ok)
            return nullptr;
        status = napi_create_function(env, nullptr, 0, DeleteVectorsByFilter, nullptr, &deleteVectorsByFilterFn);
        if (status != napi_ok)
            return nullptr;
        status = napi_create_function(env, nullptr, 0, UpdateVectorsById, nullptr, &updateVectorsByIdFn);
        if (status != napi_ok)
            return nullptr;

        // Add both functions to exports
        status = napi_set_named_property(env, exports, "search", searchFn);
        if (status != napi_ok)
            return nullptr;

        status = napi_set_named_property(env, exports, "insertVectors", insertFnAsync);
        if (status != napi_ok)
            return nullptr;

        status = napi_set_named_property(env, exports, "connect", connectFn);
        if (status != napi_ok)
            return nullptr;

        status = napi_set_named_property(env, exports, "getIndexStats", getIndexStatsFn);
        if (status != napi_ok)
            return nullptr;

        status = napi_set_named_property(env, exports, "updateDbFileConnection", updateDbFileConnectionFn);
        if (status != napi_ok)
            return nullptr;

        status = napi_set_named_property(env, exports, "deleteByIds", deleteVectorsByIdFn);
        if (status != napi_ok)
            return nullptr;

        status = napi_set_named_property(env, exports, "deleteByFilter", deleteVectorsByFilterFn);
        if (status != napi_ok)
            return nullptr;

        status = napi_set_named_property(env, exports, "upsertById", updateVectorsByIdFn);
        if (status != napi_ok)
            return nullptr;

        paginate_operations::RegisterPaginateModule(env, exports);

        return exports;
    }

    NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)

}