import pytest
from pathlib import Path
import shutil
import time

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.dirname(os.path.dirname(__file__))))


def pytest_configure(config):
    config.addinivalue_line(
        "markers", "asyncio: mark test as async"
    )


@pytest.fixture(scope="session", autouse=True)
def base_temp_dir():
    """Create the main temp directory at the start of all tests."""
    temp_dir = Path("temp")
    temp_dir.mkdir(exist_ok=True)
    yield temp_dir
    try:
        shutil.rmtree(temp_dir, ignore_errors=True)
    except Exception:
        shutil.rmtree(temp_dir, ignore_errors=True)


# @pytest.fixture(scope="function")
# def temp_dir(base_temp_dir):
#     """Create a temporary directory for each test inside 'temp'."""
#     test_dir = Path(base_temp_dir) / f"test-{int(time.time() * 1000)}"
#     test_dir.mkdir()
#     yield test_dir
#     try:
#         shutil.rmtree(test_dir, ignore_errors=True)
#     except Exception:
#         shutil.rmtree(test_dir, ignore_errors=True)


@pytest.fixture
def db_path(temp_dir):
    """Get the database path within the temporary directory."""
    # await the temp_dir since it's an async fixture
    test_dir = temp_dir
    return str(test_dir / "test.db")
