from tinyvec import TinyVecClient, TinyVecConfig, TinyVecInsertion
import pytest
import os
import tempfile
import numpy as np
from typing import Dict, Any, List
from dataclasses import dataclass
import random

pytest_plugins = ['pytest_asyncio']


def generate_random_vector(dimensions: int) -> np.ndarray:
    """Generate a random vector with given dimensions."""
    return np.random.rand(dimensions).astype(np.float32)


def generate_random_vector_list(dimensions: int) -> List[float]:
    """Generate a random vector with given dimensions."""
    return [random.random() for _ in range(dimensions)]


@pytest.fixture(scope="function")
def temp_dir():
    """Create a temporary directory for each test."""
    temp_dir = tempfile.mkdtemp(prefix="tinyvec-test-")
    yield temp_dir


@pytest.fixture
def db_path(temp_dir):
    """Get the database path within the temporary directory."""
    return os.path.join(temp_dir, "test.db")


@pytest.fixture
def client(db_path):
    """Create and connect a TinyVec client instance."""
    client = TinyVecClient()
    client.connect(db_path, TinyVecConfig(dimensions=128))
    return client


@pytest.mark.asyncio
async def test_insert_single_vector(client):
    """Should insert a single vector successfully."""
    insertion = TinyVecInsertion(
        vector=generate_random_vector(128),
        metadata={"id": 1}
    )

    inserted = await client.insert([insertion])
    assert inserted == 1

    stats = await client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128


@pytest.mark.asyncio
async def test_insert_multiple_vectors(client):
    """Should insert multiple vectors successfully."""
    INSERTION_COUNT = 10
    insertions = [
        TinyVecInsertion(
            vector=generate_random_vector(128),
            metadata={"id": i}
        )
        for i in range(INSERTION_COUNT)
    ]

    inserted = await client.insert(insertions)
    assert inserted == INSERTION_COUNT

    stats = await client.get_index_stats()
    assert stats.vector_count == INSERTION_COUNT
    assert stats.dimensions == 128


@pytest.mark.asyncio
async def test_insert_with_varying_metadata(client):
    """Should handle batch inserts with varying metadata."""
    insertions = [
        TinyVecInsertion(
            vector=generate_random_vector(128),
            metadata={"id": 1, "type": "text", "content": "hello"}
        ),
        TinyVecInsertion(
            vector=generate_random_vector(128),
            metadata={"id": 2, "type": "image", "size": "large"}
        ),
        TinyVecInsertion(
            vector=generate_random_vector(128),
            metadata={"id": 3, "type": "audio", "duration": 120}
        )
    ]

    inserted = await client.insert(insertions)
    assert inserted == 3

    stats = await client.get_index_stats()
    assert stats.vector_count == 3


@pytest.mark.asyncio
async def test_insert_empty_array(client):
    """Should handle empty insertion array."""
    inserted = await client.insert([])
    assert inserted == 0

    stats = await client.get_index_stats()
    assert stats.vector_count == 0


@pytest.mark.asyncio
async def test_multiple_insert_operations(client):
    """Should maintain consistency after multiple insert operations."""
    # First batch
    first_batch = [
        TinyVecInsertion(
            vector=generate_random_vector(128),
            metadata={"id": i, "batch": 1}
        )
        for i in range(5)
    ]

    # Second batch
    second_batch = [
        TinyVecInsertion(
            vector=generate_random_vector(128),
            metadata={"id": i + 5, "batch": 2}
        )
        for i in range(3)
    ]

    await client.insert(first_batch)
    await client.insert(second_batch)

    stats = await client.get_index_stats()
    assert stats.vector_count == 8  # 5 + 3
    assert stats.dimensions == 128


@pytest.mark.asyncio
async def test_insert_invalid_dimensions(client):
    """Should handle vectors with wrong dimensions."""
    insertions = [
        TinyVecInsertion(
            vector=generate_random_vector(64),  # Wrong dimensions
            metadata={"id": 1}
        )
    ]

    # Should return 0 for invalid insertions
    inserted = await client.insert(insertions)
    assert inserted == 0

    stats = await client.get_index_stats()
    assert stats.vector_count == 0  # Nothing should be inserted


@pytest.mark.asyncio
async def test_insert_list(client):
    """Should handle a non numpy array list."""
    insertions = [
        TinyVecInsertion(
            vector=generate_random_vector_list(128),
            metadata={"id": 1}
        )
    ]

    # Should return 0 for invalid insertions
    inserted = await client.insert(insertions)
    assert inserted == 1

    stats = await client.get_index_stats()
    assert stats.vector_count == 1
