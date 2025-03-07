import tinyvec
import pytest
import os
import uuid
import struct
import numpy as np
from typing import Tuple, Dict, Any, List

pytest_plugins = ['pytest_asyncio']
DIMENSIONS = 128


def check_files_exist(base_path: str) -> Tuple[bool, bool]:
    """Check if index and metadata files exist."""
    idx_exists = os.path.exists(f"{base_path}.idx")
    meta_exists = os.path.exists(f"{base_path}.meta")
    return idx_exists, meta_exists


def read_initial_header(file_path: str) -> Dict[str, int]:
    """Read the initial header from the database file."""
    with open(file_path, "rb") as f:
        buffer = f.read(8)
        return {
            "vector_count": struct.unpack("<i", buffer[:4])[0],
            "dimensions": struct.unpack("<i", buffer[4:8])[0]
        }


@pytest.fixture(scope="function")
def unique_client():
    """Create a client with a unique database path for each test."""
    # Create a unique temp directory path with a random UUID
    unique_id = str(uuid.uuid4())
    temp_dir = f"temp/test-{unique_id}"

    # Ensure the directory exists
    os.makedirs(temp_dir, exist_ok=True)

    # Create and connect the client
    db_path = os.path.join(temp_dir, "test.db")
    client = tinyvec.TinyVecClient()
    client.connect(db_path, tinyvec.ClientConfig(dimensions=DIMENSIONS))

    # Yield the client for the test to use
    yield client


@pytest.mark.asyncio
async def test_create_new_database_files(unique_client):
    """Should create new database files when they don't exist."""
    assert isinstance(unique_client, tinyvec.TinyVecClient)


@pytest.mark.asyncio
async def test_initialize_header_with_correct_values(unique_client):
    """Should initialize header with correct values."""
    db_path = unique_client.file_path
    # Expect to be able to read header even though file is in use
    header = read_initial_header(db_path)
    assert header["vector_count"] == 0
    assert header["dimensions"] == DIMENSIONS


@pytest.mark.asyncio
async def test_initialize_header_with_correct_values_no_dimensions(unique_client):
    """Should initialize header & index stats with correct values when no dimensions have been specified initially."""

    db_path = unique_client.file_path

    # Expect to be able to read header even though file is in use
    header = read_initial_header(db_path)
    assert header["vector_count"] == 0
    assert header["dimensions"] == DIMENSIONS

    index_stats = await unique_client.get_index_stats()
    assert index_stats.vector_count == 0
    assert index_stats.dimensions == DIMENSIONS

    to_insert: List[tinyvec.Insertion] = [
        tinyvec.Insertion(vector=np.random.rand(
            128).astype(np.float32), metadata={"id": 1})
    ]

    inserted = await unique_client.insert(to_insert)
    assert inserted == 1

    index_stats = await unique_client.get_index_stats()
    assert index_stats.vector_count == 1
    assert index_stats.dimensions == 128


@pytest.mark.asyncio
async def test_not_overwrite_existing_database(unique_client):
    """Should not overwrite existing database."""
    file_path = unique_client.file_path

    new_client = tinyvec.TinyVecClient()
    new_client.connect(file_path)

   # Read existing file header
    header = read_initial_header(file_path)
    assert header["vector_count"] == 0
    assert header["dimensions"] == 128
