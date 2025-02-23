{
    "targets": [{
        "target_name": "tinyvec",
        "cflags!": ["-fno-exceptions"],
        # "cflags_cc!": [ "-fno-exceptions" ],
        "conditions": [
            ['OS=="mac"', {
                "cflags": [
                    "-mfpu=neon",
                    "-O3",
                    "-mtune=native"
                ],
                "cflags_cc": [
                    "-mfpu=neon",
                    "-O3",
                    "-mtune=native",
                    "-masm=darwin"
                ],
                "include_dirs": [
                    "../../src/core/include",
                ],
                "xcode_settings": {
                    "MACOSX_DEPLOYMENT_TARGET": "10.15"
                }
            }]
        ],
        "cflags": [
            "-mavx",
            "-mavx2",
            "-O3",
        ],
        "cflags_cc": [
            "-mavx",
            "-mavx2",
            "-O3",
        ],
        "sources": [
            "src/addon/addon.cpp",
            "src/addon/addon_utils.cpp",
            "src/addon/tinyvec.cpp",
            "src/core/src/db.c",
            "src/core/src/cJSON.c",
            "src/core/src/file.c",
            "src/core/src/minheap.c",
            "src/core/src/distance.c"
        ],
        "include_dirs": [
            "<!@(node -p \"require('node-addon-api').include\")",
            "src/addon",
            "src/core/include",
        ],
        "msvs_settings": {
            "VCCLCompilerTool": {
                "ExceptionHandling": 1,
                "AdditionalOptions": ["/EHsc"]
            }
        },
        "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "CLANG_CXX_LIBRARY": "libc++",
            "MACOSX_DEPLOYMENT_TARGET": "10.7"
        },
        "defines": [
            "NAPI_DISABLE_CPP_EXCEPTIONS"
        ]
    }]
}
