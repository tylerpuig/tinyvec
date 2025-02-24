# This file makes the tinyvec.core directory a Python package
"""
Core C implementation for tinyvec
"""

import os
import sys
import ctypes
from pathlib import Path

# Load the shared library


def _load_library():
    # Determine the shared library name based on platform
    if sys.platform.startswith('win'):
        lib_name = 'tinyveclib.dll'
    elif sys.platform.startswith('darwin'):
        lib_name = 'tinyveclib.dylib'
    else:
        lib_name = 'tinyveclib.so'

    # Path to the shared library
    lib_path = Path(__file__).parent / lib_name

    if not lib_path.exists():
        raise ImportError(f"Could not find the shared library at {lib_path}")

    # Load the shared library
    try:
        return ctypes.CDLL(str(lib_path))
    except Exception as e:
        raise ImportError(f"Failed to load the shared library: {e}")


try:
    lib = _load_library()
except ImportError as e:
    # Handle the case where the shared library is not available
    lib = None
    print(f"Warning: {e}")
