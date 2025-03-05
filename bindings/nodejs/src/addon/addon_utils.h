#ifndef ADDONUTILS_H
#define ADDONUTILS_H

#include <node_api.h>
#include <vector>
#include "../../../src/core/include/cJSON.h"
#include "../../../src/core/include/vec_types.h"

struct AsyncInsertData
{
    char *file_path;
    std::vector<float *> vectors;
    std::vector<char *> metadatas;
    std::vector<size_t> metadata_lengths;
    napi_deferred deferred;
    napi_async_work work;
    int insert_count;
    size_t dimensions;
};

struct AsyncDeleteVectorsByIdData
{
    char *file_path;
    int *ids_to_delete;
    int delete_count;
    int actually_deleted_count;
    napi_deferred deferred;
    napi_async_work work;
};

struct AsyncDeleteVectorsByFilterData
{
    char *file_path;
    char *json_filter;
    int actually_deleted_count;
    napi_deferred deferred;
    napi_async_work work;
};

napi_value convert_json_to_napi(napi_env env, cJSON *json);
AsyncInsertData *prepare_data_for_insertion(napi_env env, napi_callback_info info);
ConnectionData *prepare_data_for_connection(napi_env env, napi_callback_info info);
char *prepare_data_for_index_stats(napi_env env, napi_callback_info info);
AsyncDeleteVectorsByIdData *prepare_data_for_deletion_by_id(napi_env env, napi_callback_info info);
AsyncDeleteVectorsByFilterData *prepare_data_for_deletion_by_filter(napi_env env, napi_callback_info info);

#endif // ADDONUTILS_H