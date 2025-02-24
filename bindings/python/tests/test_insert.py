from tinyvec import TinyVecClient, TinyVecConfig, TinyVecInsertion
import pytest
import numpy as np
from typing import List, Dict, Any, cast
from dataclasses import dataclass
import random

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

    inserted = await client.insert(insertions)
    assert inserted == 1

    stats = await client.get_index_stats()
    assert stats.vector_count == 1


@pytest.mark.asyncio
async def test_insert_list_update_dimensions(db_path):
    """Should update file dimensions if inserting a list and dimensions have not been set."""

    new_client = TinyVecClient()
    new_client.connect(db_path)

    index_stats = await new_client.get_index_stats()
    assert index_stats.dimensions == 0
    assert index_stats.vector_count == 0

    insertions = [
        TinyVecInsertion(
            vector=generate_random_vector_list(128),
            metadata={"id": 1}
        )
    ]

    inserted = await new_client.insert(insertions)
    assert inserted == 1

    stats = await new_client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128

    search_results = await new_client.search(generate_random_vector_list(128), 1)
    assert len(search_results) == 1


@pytest.mark.asyncio
async def test_allow_null_metadata(db_path):
    """Should allow inserting vectors with null metadata."""

    new_client = TinyVecClient()
    new_client.connect(db_path)

    index_stats = await new_client.get_index_stats()
    assert index_stats.dimensions == 0
    assert index_stats.vector_count == 0

    insertion = TinyVecInsertion(
        vector=generate_random_vector_list(128),
        metadata=None
    )

    inserted = await new_client.insert([insertion])
    assert inserted == 1

    stats = await new_client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128

    search_results = await new_client.search(generate_random_vector_list(128), 1)
    assert len(search_results) == 1
    assert search_results[0].metadata == None


@pytest.mark.asyncio
async def test_allow_nested_metadata(db_path):
    """Should allow inserting vectors with nested metadata."""

    new_client = TinyVecClient()
    new_client.connect(db_path)

    index_stats = await new_client.get_index_stats()
    assert index_stats.dimensions == 0
    assert index_stats.vector_count == 0

    insertion = TinyVecInsertion(
        vector=generate_random_vector_list(128),
        metadata={"id": 1, "nested": {"key": "value"}}
    )

    inserted = await new_client.insert([insertion])
    assert inserted == 1

    stats = await new_client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128

    search_results = await new_client.search(generate_random_vector_list(128), 1)
    assert len(search_results) == 1
    metadata = search_results[0].metadata
    assert metadata != None
    nested = cast(Dict[str, Any], metadata.get("nested", {}))
    assert nested.get("key") == "value"


@pytest.mark.asyncio
async def test_allow_nested_metadata_lists(db_path):
    """Should allow inserting vectors with nested metadata lists."""

    new_client = TinyVecClient()
    new_client.connect(db_path)

    index_stats = await new_client.get_index_stats()
    assert index_stats.dimensions == 0
    assert index_stats.vector_count == 0

    insertion = TinyVecInsertion(
        vector=generate_random_vector_list(128),
        metadata={"id": 1, "nested": {"list": [1, 2, 3]}}
    )

    inserted = await new_client.insert([insertion])
    assert inserted == 1

    stats = await new_client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128

    search_results = await new_client.search(generate_random_vector_list(128), 1)
    assert len(search_results) == 1
    metadata = search_results[0].metadata
    assert metadata != None

    nested = cast(Dict[str, Any], metadata.get("nested", []))
    assert nested.get("list") == [1, 2, 3]
