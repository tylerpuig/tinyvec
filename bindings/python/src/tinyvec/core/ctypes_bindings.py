import ctypes
import numpy as np
import platform
from pathlib import Path


def get_lib_path() -> Path:
    """Get path to the native library based on platform."""
    # Get the directory containing this Python file
    this_dir = Path(__file__).parent.absolute()

    # Platform specific library name
    if platform.system() == "Windows":
        lib_name = "tinyveclib.dll"
    elif platform.system() == "Darwin":
        lib_name = "tinyveclib.dylib"
    else:  # Linux
        lib_name = "tinyveclib.so"

    # Expected paths - try current directory first, then fall back to build/lib directories
    possible_paths = [
        # Same directory as this file (installed package location)
        this_dir / lib_name,

        # Development paths
        this_dir.parent.parent.parent.parent / "build" / lib_name,
        this_dir.parent.parent.parent.parent / "lib" / lib_name,

        # Source directory paths
        this_dir.parent.parent / "src" / "tinyvec" / "core" / lib_name,
        this_dir.parent.parent / "tinyvec" / "core" / lib_name,
    ]

    for path in possible_paths:
        if path.exists():
            return path

    raise RuntimeError(
        f"Could not find native library {lib_name} in any of: {possible_paths}"
    )


class TinyVecConnection(ctypes.Structure):
    _fields_ = [
        ("file_path", ctypes.c_char_p),
        ("dimensions", ctypes.c_uint32)
    ]


class MetadataBytes(ctypes.Structure):
    _fields_ = [
        ("data", ctypes.POINTER(ctypes.c_ubyte)),
        ("length", ctypes.c_uint32)
    ]


class VecResult(ctypes.Structure):
    _fields_ = [
        ("similarity", ctypes.c_float),
        ("index", ctypes.c_int),
        ("metadata", MetadataBytes)
    ]


class TinyVecIndexStats(ctypes.Structure):
    _fields_ = [
        ("vector_count", ctypes.c_uint64),
        ("dimensions", ctypes.c_uint32),
    ]


class DBSearchResult(ctypes.Structure):
    _fields_ = [
        ("results", ctypes.POINTER(VecResult)),
        ("count", ctypes.c_int)
    ]


# Load the library
lib = ctypes.CDLL(str(get_lib_path()))

# Define function signatures
lib.create_tiny_vec_connection.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
lib.create_tiny_vec_connection.restype = ctypes.POINTER(TinyVecConnection)

lib.get_top_k.argtypes = [
    ctypes.c_char_p,
    np.ctypeslib.ndpointer(dtype=np.float32),
    ctypes.c_int
]
lib.get_top_k.restype = ctypes.POINTER(DBSearchResult)

lib.get_top_k_with_filter.argtypes = [
    ctypes.c_char_p,
    np.ctypeslib.ndpointer(dtype=np.float32),
    ctypes.c_int,
    ctypes.c_char_p
]
lib.get_top_k_with_filter.restype = ctypes.POINTER(DBSearchResult)

lib.get_index_stats.argtypes = [ctypes.c_char_p]
lib.get_index_stats.restype = TinyVecIndexStats

lib.insert_data.argtypes = [
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(ctypes.c_size_t),
    ctypes.c_size_t,
    ctypes.c_uint32
]
lib.insert_data.restype = ctypes.c_size_t

lib.update_db_file_connection.argtypes = [ctypes.c_char_p]
lib.update_db_file_connection.restype = ctypes.c_bool

lib.delete_data_by_ids.argtypes = [
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_int),
    ctypes.c_int
]
lib.delete_data_by_ids.restype = ctypes.c_int

lib.delete_data_by_filter.argtypes = [
    ctypes.c_char_p,
    ctypes.c_char_p
]
lib.delete_data_by_filter.restype = ctypes.c_int
