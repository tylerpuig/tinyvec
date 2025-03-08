{
  "targets": [
    {
      "target_name": "sqlite3",
      "type": "static_library",
      "sources": [
        "src/core/src/sqlite3.c"
      ],
      "cflags": [
        "-O3"
      ],
      "xcode_settings": {
        "MACOSX_DEPLOYMENT_TARGET": "10.7"
      }
    },
    {
      "target_name": "tinyvec",
      "dependencies": [
        "sqlite3"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": [ "-fno-exceptions" ],
      "conditions": [
        ['OS=="mac" and target_arch=="arm64"', {
          "cflags": [
            "-mfpu=neon",
            "-O3"
          ],
          "cflags_cc": [
            "-mfpu=neon",
            "-O3",
            "-masm=darwin"
          ],
          "xcode_settings": {
            "MACOSX_DEPLOYMENT_TARGET": "10.15"
          }
        }],
        ['OS=="mac" and target_arch=="x64"', {
          "cflags": [
            "-mavx",
            "-mavx2",
            "-O3"
          ],
          "cflags_cc": [
            "-mavx",
            "-mavx2",
            "-O3",
            "-masm=darwin"
          ],
          "xcode_settings": {
            "MACOSX_DEPLOYMENT_TARGET": "10.15"
          }
        }]
      ],
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
        "src/addon/addon.cpp",
        "src/addon/addon_utils.cpp",
        "src/addon/tinyvec.cpp",
        "src/core/src/db.c",
        "src/core/src/cJSON.c",
        "src/core/src/file.c",
        "src/core/src/minheap.c",
        "src/core/src/distance.c",
        "src/core/src/utils.c",
        "src/core/src/query_convert.c"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "src/addon",
        "src/core/include"
      ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1,
          "AdditionalOptions": ["/EHsc", "/std:c++20"]
        }
      },
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7",
        "OTHER_CPLUSPLUSFLAGS": ["-std=c++20"]
      },
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS"
      ]
    }
  ]
}