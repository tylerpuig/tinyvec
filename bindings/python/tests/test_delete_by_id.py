import os
import uuid
import pytest
import numpy as np
import tinyvec
from typing import List, Dict, Any, TypedDict
from dataclasses import dataclass

# Match the dimensions used in the Node.js tests
DIMENSIONS = 128
INSERTIONS = 10


class DeleteByIdTestItem(TypedDict):
    text: str
    category: str


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


def generate_random_vector(dimensions: int) -> np.ndarray:
    """Generate a random vector and normalize it."""
    vector = np.random.randn(dimensions).astype(np.float32)
    return vector


async def insert_test_vectors(client: tinyvec.TinyVecClient) -> int:
    """Insert test vectors for delete by ID testing."""
    test_data: List[DeleteByIdTestItem] = []

    for i in range(INSERTIONS):
        test_data.append({
            "text": f"document_{i}",
            "category": "even" if i % 2 == 0 else "odd"
        })

    insertions: List[tinyvec.Insertion] = []
    for item in test_data:
        # Convert TypedDict to regular dict to avoid type issues
        metadata_dict = dict(item)
        insertions.append(tinyvec.Insertion(
            vector=generate_random_vector(DIMENSIONS),
            metadata=metadata_dict
        ))

    inserted = await client.insert(insertions)
    return inserted


@pytest.mark.asyncio
async def test_remove_single_vector_by_id(unique_client):
    """Test removing a single vector by its ID."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Verify initial count
    initial_stats = await unique_client.get_index_stats()
    assert initial_stats.vector_count == INSERTIONS
    assert initial_stats.dimensions == DIMENSIONS

    # Search for a vector with "even" category
    search_options = tinyvec.SearchOptions(
        filter={"category": {"$eq": "even"}}
    )
    search_results = await unique_client.search(
        generate_random_vector(DIMENSIONS),
        1,
        search_options
    )

    assert len(search_results) == 1
    assert search_results[0].metadata.get("category") == "even"

    # # Get the ID of the found vector
    item_id = search_results[0].id

    # # Delete the vector by ID
    delete_result = await unique_client.delete_by_ids([item_id])

    # # Verify deletion result
    assert delete_result.deleted_count == 1
    assert delete_result.success is True

    # # Verify updated count
    stats = await unique_client.get_index_stats()
    assert stats.vector_count == INSERTIONS - 1
    assert stats.dimensions == DIMENSIONS


@pytest.mark.asyncio
async def test_remove_multiple_vectors_by_id(unique_client):
    """Test removing multiple vectors by their IDs."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Verify initial count
    initial_stats = await unique_client.get_index_stats()
    assert initial_stats.vector_count == INSERTIONS
    assert initial_stats.dimensions == DIMENSIONS

    # Search for vectors with "even" category
    search_options = tinyvec.SearchOptions(
        filter={"category": {"$eq": "even"}}
    )
    search_results = await unique_client.search(
        generate_random_vector(DIMENSIONS),
        4,
        search_options
    )

    assert len(search_results) > 0
    assert all(r.metadata.get("category") == "even" for r in search_results)

    # Get the IDs of the found vectors
    item_ids = [r.id for r in search_results]

    # Delete the vectors by IDs
    delete_result = await unique_client.delete_by_ids(item_ids)

    # Verify deletion result
    assert delete_result.deleted_count == len(item_ids)
    assert delete_result.success is True

    # Verify updated count
    stats = await unique_client.get_index_stats()
    assert stats.vector_count == INSERTIONS - len(item_ids)
    assert stats.dimensions == DIMENSIONS


@pytest.mark.asyncio
async def test_properly_handle_empty_array_of_ids(unique_client):
    """Test handling of an empty array of IDs to delete."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Verify initial count
    initial_stats = await unique_client.get_index_stats()
    assert initial_stats.vector_count == INSERTIONS
    assert initial_stats.dimensions == DIMENSIONS

    # Try to delete with an empty array of IDs
    delete_result = await unique_client.delete_by_ids([])

    # Verify deletion result
    assert delete_result.deleted_count == 0
    assert delete_result.success is False

    # Verify count remains unchanged
    stats = await unique_client.get_index_stats()
    assert stats.vector_count == INSERTIONS
    assert stats.dimensions == DIMENSIONS
