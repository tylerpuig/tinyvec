import tinyvec
import pytest
import numpy as np
from typing import List, Dict, Any, cast
from dataclasses import dataclass
import random
import uuid
import os

DIMENSIONS = 128

pytest_plugins = ['pytest_asyncio']


def generate_random_vector(dimensions: int) -> np.ndarray:
    """Generate a random vector with given dimensions."""
    return np.random.rand(dimensions).astype(np.float32)


def generate_random_vector_list(dimensions: int) -> List[float]:
    """Generate a random vector with given dimensions."""
    return [random.random() for _ in range(dimensions)]


@pytest.fixture
def client(db_path):
    """Create and connect a TinyVec client instance."""
    client = tinyvec.TinyVecClient()
    client.connect(db_path, tinyvec.ClientConfig(dimensions=128))
    return client


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
async def test_insert_single_vector(unique_client):
    """Should insert a single vector successfully."""
    insertion = tinyvec.Insertion(
        vector=generate_random_vector(128),
        metadata={"id": 1}
    )

    inserted = await unique_client.insert([insertion])
    assert inserted == 1

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128


@pytest.mark.asyncio
async def test_insert_multiple_vectors(unique_client):
    """Should insert multiple vectors successfully."""
    INSERTION_COUNT = 10
    insertions = [
        tinyvec.Insertion(
            vector=generate_random_vector(128),
            metadata={"id": i}
        )
        for i in range(INSERTION_COUNT)
    ]

    inserted = await unique_client.insert(insertions)
    assert inserted == INSERTION_COUNT

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == INSERTION_COUNT
    assert stats.dimensions == 128


@pytest.mark.asyncio
async def test_insert_with_varying_metadata(unique_client):
    """Should handle batch inserts with varying metadata."""
    insertions = [
        tinyvec.Insertion(
            vector=generate_random_vector(128),
            metadata={"id": 1, "type": "text", "content": "hello"}
        ),
        tinyvec.Insertion(
            vector=generate_random_vector(128),
            metadata={"id": 2, "type": "image", "size": "large"}
        ),
        tinyvec.Insertion(
            vector=generate_random_vector(128),
            metadata={"id": 3, "type": "audio", "duration": 120}
        )
    ]

    inserted = await unique_client.insert(insertions)
    assert inserted == 3

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 3


@pytest.mark.asyncio
async def test_insert_empty_array(unique_client):
    """Should handle empty insertion array."""
    inserted = await unique_client.insert([])
    assert inserted == 0

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 0


@pytest.mark.asyncio
async def test_multiple_insert_operations(unique_client):
    """Should maintain consistency after multiple insert operations."""
    # First batch
    first_batch = [
        tinyvec.Insertion(
            vector=generate_random_vector(128),
            metadata={"id": i, "batch": 1}
        )
        for i in range(5)
    ]

    # Second batch
    second_batch = [
        tinyvec.Insertion(
            vector=generate_random_vector(128),
            metadata={"id": i + 5, "batch": 2}
        )
        for i in range(3)
    ]

    await unique_client.insert(first_batch)
    await unique_client.insert(second_batch)

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 8  # 5 + 3
    assert stats.dimensions == 128


@pytest.mark.asyncio
async def test_insert_invalid_dimensions(unique_client):
    """Should handle vectors with wrong dimensions."""
    insertions = [
        tinyvec.Insertion(
            vector=generate_random_vector(64),  # Wrong dimensions
            metadata={"id": 1}
        )
    ]

    # Should return 0 for invalid insertions
    inserted = await unique_client.insert(insertions)
    assert inserted == 0

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 0  # Nothing should be inserted


@pytest.mark.asyncio
async def test_insert_list(unique_client):
    """Should handle a non numpy array list."""
    insertions = [
        tinyvec.Insertion(
            vector=generate_random_vector_list(128),
            metadata={"id": 1}
        )
    ]

    inserted = await unique_client.insert(insertions)
    assert inserted == 1

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 1


@pytest.mark.asyncio
async def test_insert_list_update_dimensions(unique_client):
    """Should update file dimensions if inserting a list and dimensions have not been set."""

    index_stats = await unique_client.get_index_stats()
    assert index_stats.dimensions == DIMENSIONS
    assert index_stats.vector_count == 0

    insertions = [
        tinyvec.Insertion(
            vector=generate_random_vector_list(128),
            metadata={"id": 1}
        )
    ]

    inserted = await unique_client.insert(insertions)
    assert inserted == 1

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128

    search_results = await unique_client.search(generate_random_vector_list(128), 1)
    assert len(search_results) == 1


@pytest.mark.asyncio
async def test_allow_null_metadata(unique_client):
    """Should allow inserting vectors with null metadata."""

    index_stats = await unique_client.get_index_stats()
    assert index_stats.dimensions == DIMENSIONS
    assert index_stats.vector_count == 0

    insertion = tinyvec.Insertion(
        vector=generate_random_vector_list(128),
        metadata=None
    )

    inserted = await unique_client.insert([insertion])
    assert inserted == 1

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128

    search_results = await unique_client.search(generate_random_vector_list(128), 1)
    assert len(search_results) == 1
    assert search_results[0].metadata == None


@pytest.mark.asyncio
async def test_allow_nested_metadata(unique_client):
    """Should allow inserting vectors with nested metadata."""

    index_stats = await unique_client.get_index_stats()
    assert index_stats.dimensions == DIMENSIONS
    assert index_stats.vector_count == 0

    insertion = tinyvec.Insertion(
        vector=generate_random_vector_list(128),
        metadata={"id": 1, "nested": {"key": "value"}}
    )

    inserted = await unique_client.insert([insertion])
    assert inserted == 1

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128

    search_results = await unique_client.search(generate_random_vector_list(128), 1)
    assert len(search_results) == 1
    metadata = search_results[0].metadata
    assert metadata != None
    nested = cast(Dict[str, Any], metadata.get("nested", {}))
    assert nested.get("key") == "value"


@pytest.mark.asyncio
async def test_allow_nested_metadata_lists(unique_client):
    """Should allow inserting vectors with nested metadata lists."""

    index_stats = await unique_client.get_index_stats()
    assert index_stats.dimensions == DIMENSIONS
    assert index_stats.vector_count == 0

    insertion = tinyvec.Insertion(
        vector=generate_random_vector_list(128),
        metadata={"id": 1, "nested": {"list": [1, 2, 3]}}
    )

    inserted = await unique_client.insert([insertion])
    assert inserted == 1

    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128

    search_results = await unique_client.search(generate_random_vector_list(128), 1)
    assert len(search_results) == 1
    metadata = search_results[0].metadata
    assert metadata != None

    nested = cast(Dict[str, Any], metadata.get("nested", []))
    assert nested.get("list") == [1, 2, 3]
