#pragma once
#include <node_api.h>
#include "../../../src/core/include/vec_types.h"

namespace paginate_operations
{

    struct AsyncPaginationData
    {
        char *file_path;
        uint32_t offset;
        uint32_t limit;
        PaginationResults *results;
        napi_deferred deferred;
        napi_async_work work;
    };

    void ExecuteGetPaginatedVectors(napi_env env, void *data);
    void CompleteGetPaginatedVectors(napi_env env, napi_status status, void *data);
    napi_value GetPaginatedVectors(napi_env env, napi_callback_info info);
    napi_value RegisterPaginateModule(napi_env env, napi_value exports);
}