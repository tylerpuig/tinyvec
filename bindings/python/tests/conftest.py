# import pytest
# import os
# import shutil
# import tempfile
# import time
from pathlib import Path


def pytest_configure(config):
    config.addinivalue_line(
        "markers", "asyncio: mark test as async"
    )


# def pytest_sessionfinish(session, exitstatus):
#     """Clean up any remaining test files after all tests are done."""
#     temp_dir = tempfile.gettempdir()

#     # Wait a bit to ensure all file handles are released
#     time.sleep(1)

#     # Find and remove all tinyvec test directories
#     for item in Path(temp_dir).glob("tinyvec-test-*"):
#         try:
#             if item.is_dir():
#                 shutil.rmtree(item, ignore_errors=True)
#         except Exception as e:
#             print(f"Warning: Failed to remove test directory {item}: {e}")

#     # Also clean up any test.db files in current directory
#     try:
#         for ext in ["", ".idx", ".meta"]:
#             test_file = Path("./test.db" + ext)
#             if test_file.exists():
#                 test_file.unlink()
#     except Exception as e:
#         print(
#             f"Warning: Failed to remove test files in current directory: {e}")
