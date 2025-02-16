{
    "targets": [{
        "target_name": "tinyvec",
        "cflags!": ["-fno-exceptions"],
        # "cflags_cc!": [ "-fno-exceptions" ],
        "cflags": [
            "-mavx",
            "-mavx2",
            "-O3"
        ],
        "cflags_cc": [
            "-mavx",
            "-mavx2",
            "-O3"
        ],
        "sources": [
            "../nodejs/src/addon/addon.cpp",
            "../nodejs/src/addon/addon_utils.cpp",
            "../nodejs/src/addon/tinyvec.cpp",
            "../../src/core/src/db.c",
            "../../src/core/src/cJSON.c",
            "../../src/core/src/file.c",
            "../../src/core/src/minheap.c",
            "../../src/core/src/distance.c"
        ],
        "include_dirs": [
            "<!@(node -p \"require('node-addon-api').include\")",
            "../../src/core/include",
        ],
        "msvs_settings": {
            "VCCLCompilerTool": {
                "ExceptionHandling": 1,
                "AdditionalOptions": ["/EHsc"]
            }
        },
        "defines": [
            "NAPI_DISABLE_CPP_EXCEPTIONS"
        ]
    }]
}
