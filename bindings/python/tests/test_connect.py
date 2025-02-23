from tinyvec import TinyVecClient, TinyVecConfig, TinyVecInsertion
import pytest
import os
import struct
import numpy as np
from pathlib import Path
from typing import Tuple, Dict, Any, List

pytest_plugins = ['pytest_asyncio']


@pytest.fixture
def client():
    """Create a TinyVec client instance."""
    return TinyVecClient()


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


@pytest.mark.asyncio
async def test_create_new_database_files(client, db_path):
    """Should create new database files when they don't exist."""
    config = TinyVecConfig(dimensions=128)
    client.connect(db_path, config)

    idx_exists, meta_exists = check_files_exist(db_path)
    assert idx_exists is True
    assert meta_exists is True
    assert isinstance(client, TinyVecClient)


@pytest.mark.asyncio
async def test_initialize_header_with_correct_values(client, db_path):
    """Should initialize header with correct values."""
    config = TinyVecConfig(dimensions=128)
    client.connect(db_path, config)

    # Expect to be able to read header even though file is in use
    header = read_initial_header(db_path)
    assert header["vector_count"] == 0
    assert header["dimensions"] == 128


@pytest.mark.asyncio
async def test_initialize_header_with_correct_values_no_dimensions(client, db_path):
    """Should initialize header & index stats with correct values when no dimensions have been specified initially."""
    # config = TinyVecConfig(dimensions=0)
    client.connect(db_path)

    # Expect to be able to read header even though file is in use
    header = read_initial_header(db_path)
    assert header["vector_count"] == 0
    assert header["dimensions"] == 0

    index_stats = await client.get_index_stats()
    assert index_stats.vector_count == 0
    assert index_stats.dimensions == 0

    to_insert: List[TinyVecInsertion] = [
        TinyVecInsertion(vector=np.random.rand(
            128).astype(np.float32), metadata={"id": 1})
    ]

    inserted = await client.insert(to_insert)
    assert inserted == 1

    index_stats = await client.get_index_stats()
    assert index_stats.vector_count == 1
    assert index_stats.dimensions == 128


@pytest.mark.asyncio
async def test_convert_relative_path_to_absolute(client):
    """Should convert relative path to absolute."""
    relative_path = "./test.db"
    absolute_path = os.path.abspath(relative_path)
    config = TinyVecConfig(dimensions=128)

    try:
        client.connect(relative_path, config)
        exists = os.path.exists(absolute_path)
        assert exists is True

        # Attempting to delete files should fail with PermissionError
        with pytest.raises(PermissionError):
            os.unlink(absolute_path)
    finally:
        # Don't try to cleanup - files will be in use
        pass


@pytest.mark.asyncio
async def test_create_empty_metadata_files(client, db_path):
    """Should create empty metadata files."""
    config = TinyVecConfig(dimensions=128)
    client.connect(db_path, config)

    # We expect to be able to check file sizes even though files are in use
    idx_size = os.path.getsize(f"{db_path}.idx")
    meta_size = os.path.getsize(f"{db_path}.meta")

    assert idx_size == 0
    assert meta_size == 0


@pytest.mark.asyncio
async def test_not_overwrite_existing_database(client, db_path):
    """Should not overwrite existing database."""
    # Create first instance
    config = TinyVecConfig(dimensions=128)
    client.connect(db_path, config)

    # Reading and writing the header should work even with file in use
    header = read_initial_header(db_path)
    assert header["vector_count"] == 0
    assert header["dimensions"] == 128

    # Connect again with different dimensions
    new_config = TinyVecConfig(dimensions=256)
    client.connect(db_path, new_config)

    # Header values should remain unchanged
    header = read_initial_header(db_path)
    assert header["dimensions"] == 128
