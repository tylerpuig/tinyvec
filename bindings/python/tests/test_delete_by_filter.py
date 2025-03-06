from tests.test_search_filter import insert_test_vectors, ProductMetadata
import pytest
import os
import uuid
import numpy as np
from typing import List
import tinyvec

# Constants
DIMENSIONS = 128


def generate_random_vector(dimensions: int) -> np.ndarray:
    """Generate a random vector and normalize it."""
    vector = np.random.randn(dimensions).astype(np.float32)
    return vector


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
async def test_delete_by_filter_with_eq_operator(unique_client):
    """Test deleteByFilter with $eq operator."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Delete Pear phones
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(filter={"brand": {"$eq": "Pear"}})
    )

    # Verify deletion result
    assert delete_result.success is True
    # Should have 3 Pear phones in test data
    assert delete_result.deleted_count == 3

    # Verify deletion by searching for Pear phones
    search_vector = generate_random_vector(DIMENSIONS)
    search_results = await unique_client.search(
        search_vector, 10, tinyvec.SearchOptions(
            filter={"brand": {"$eq": "Pear"}})
    )

    # Should no longer have any Pear phones
    assert len(search_results) == 0


@pytest.mark.asyncio
async def test_delete_by_filter_with_ne_operator(unique_client):
    """Test deleteByFilter with $ne operator."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Get count of non-Pear phones before deletion
    search_vector = generate_random_vector(DIMENSIONS)
    before_results = await unique_client.search(
        search_vector, 10, tinyvec.SearchOptions(
            filter={"brand": {"$ne": "Pear"}})
    )
    non_pear_count = len(before_results)

    # Delete non-Pear phones
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(filter={"brand": {"$ne": "Pear"}})
    )

    # Verify deletion result
    assert delete_result.success is True
    assert delete_result.deleted_count == non_pear_count

    # Verify only Pear phones remain
    after_results = await unique_client.search(search_vector, 10)
    # Should only have the 3 Pear phones left
    assert len(after_results) == 3
    assert all(r.metadata["brand"] == "Pear" for r in after_results)


@pytest.mark.asyncio
async def test_delete_by_filter_with_gt_operator(unique_client):
    """Test deleteByFilter with $gt operator."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Delete phones from after 2020
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(filter={"year": {"$gt": 2020}})
    )

    # Verify deletion
    search_vector = generate_random_vector(DIMENSIONS)

    search_results = await unique_client.search(search_vector, 10, tinyvec.SearchOptions())

    # All remaining phones should be from 2020 or earlier
    assert all(r.metadata["year"] <= 2020 for r in search_results)

    # Should have deleted phones from 2021, 2022, and 2024
    assert delete_result.deleted_count > 0


@pytest.mark.asyncio
async def test_delete_by_filter_with_multiple_conditions(unique_client):
    """Test deleteByFilter with multiple conditions."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Delete expensive Pear phones
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(
            filter={"brand": {"$eq": "Pear"}, "price": {"$gt": 1000}}
        )
    )

    # Should delete just the pPhone Pro (2024)
    assert delete_result.deleted_count == 1

    # Verify remaining Pear phones are all under $1000
    search_vector = generate_random_vector(DIMENSIONS)
    search_results = await unique_client.search(
        search_vector, 10, tinyvec.SearchOptions(
            filter={"brand": {"$eq": "Pear"}})
    )

    assert len(search_results) == 2  # Should have 2 Pear phones left
    assert all(r.metadata["price"] <= 1000 for r in search_results)


@pytest.mark.asyncio
async def test_delete_by_filter_with_nested_object_properties(unique_client):
    """Test deleteByFilter with nested object properties."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Delete phones with high storage
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(filter={"specs": {"storage": {"$gte": 256}}})
    )

    # Verify deletion
    search_vector = generate_random_vector(DIMENSIONS)
    search_results = await unique_client.search(search_vector, 10)

    # All remaining phones should have less than 256GB storage
    assert all(r.metadata["specs"]["storage"]
               < 256 for r in search_results)
    assert delete_result.deleted_count > 0


@pytest.mark.asyncio
async def test_delete_by_filter_with_exists_operator(unique_client):
    """Test deleteByFilter with $exists operator."""
    # Insert one record with missing fields
    normal_vector = generate_random_vector(DIMENSIONS)
    await unique_client.insert([
        tinyvec.Insertion(
            vector=normal_vector,
            metadata={
                "id": 100,
                "brand": "TestBrand",
                # Intentionally missing model
                "year": 2023,
                "price": 900,
                "inStock": True,
                "specs": {"storage": 128, "previousOwners": 0, "condition": "excellent"},
                "ratings": [4.5],
            }
        )
    ])

    # Also insert regular test vectors
    await insert_test_vectors(unique_client)

    # Delete records where model doesn't exist
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(filter={"model": {"$exists": False}})
    )

    # Should have deleted just the one record
    assert delete_result.deleted_count == 1

    # Verify all remaining records have model field
    search_vector = generate_random_vector(DIMENSIONS)
    search_results = await unique_client.search(search_vector, 20)
    assert all("model" in r.metadata for r in search_results)


@pytest.mark.asyncio
async def test_delete_by_filter_with_in_operator(unique_client):
    """Test deleteByFilter with $in operator."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Delete specific brands
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(filter={"brand": {"$in": ["Nexus", "Pear"]}})
    )

    # Should have deleted Nexus and Pear phones
    assert delete_result.deleted_count == 5  # 2 Nexus + 3 Pear

    # Verify deletion
    search_vector = generate_random_vector(DIMENSIONS)
    search_results = await unique_client.search(search_vector, 10)

    # Should not have any Nexus or Pear phones
    assert not any(r.metadata["brand"] in ["Nexus", "Pear"]
                   for r in search_results)


@pytest.mark.asyncio
async def test_delete_by_filter_with_boolean_field(unique_client):
    """Test deleteByFilter with boolean field."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Delete out-of-stock phones
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(filter={"inStock": {"$eq": False}})
    )

    # Verify deletion
    search_vector = generate_random_vector(DIMENSIONS)
    search_results = await unique_client.search(search_vector, 10)

    # All remaining phones should be in stock
    assert all(r.metadata["inStock"] is True for r in search_results)
    assert delete_result.deleted_count > 0


@pytest.mark.asyncio
async def test_delete_by_filter_with_no_matches(unique_client):
    """Test deleteByFilter with no matches."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Try to delete phones with non-existent brand
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(
            filter={"brand": {"$eq": "NonExistentBrand"}})
    )

    # Should report failure with zero deletions
    assert delete_result.success is False
    assert delete_result.deleted_count == 0

    # Verify all records still exist
    search_vector = generate_random_vector(DIMENSIONS)
    search_results = await unique_client.search(search_vector, 10)
    assert len(search_results) == 10  # All 10 test phones should remain


@pytest.mark.asyncio
async def test_delete_by_filter_with_complex_nested_filters(unique_client):
    """Test deleteByFilter with complex nested filters."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Delete high-end phones with specific condition
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(
            filter={
                "price": {"$gte": 900},
                "specs": {
                    "condition": {"$in": ["excellent", "new"]},
                    "storage": {"$gte": 256},
                },
                "inStock": {"$eq": True},
            }
        )
    )

    # Verify deletion
    search_vector = generate_random_vector(DIMENSIONS)
    search_results = await unique_client.search(
        search_vector,
        10,
        tinyvec.SearchOptions(
            filter={
                "price": {"$gte": 900},
                "specs": {
                    "condition": {"$in": ["excellent", "new"]},
                    "storage": {"$gte": 256},
                },
                "inStock": {"$eq": True},
            }
        )
    )

    # Should no longer find any matching phones
    assert len(search_results) == 0
    assert delete_result.deleted_count > 0


@pytest.mark.asyncio
async def test_delete_by_filter_and_reinsert(unique_client):
    """Test deleteByFilter and reinsert."""
    # Insert test vectors
    await insert_test_vectors(unique_client)

    # Delete all Pear phones
    delete_result = await unique_client.delete_by_filter(
        tinyvec.SearchOptions(filter={"brand": {"$eq": "Pear"}})
    )

    assert delete_result.deleted_count == 3

    # Reinsert some Pear phones with different data
    new_phones = [
        tinyvec.Insertion(
            vector=generate_random_vector(DIMENSIONS),
            metadata={
                "id": 101,
                "brand": "Pear",
                "model": "pPhone Ultra",
                "year": 2025,
                "price": 1499,
                "features": ["wireless charging", "satellite", "AI assistant"],
                "inStock": True,
                "specs": {"storage": 2048, "previousOwners": 0, "condition": "new"},
                "ratings": [5, 5, 5],
            }
        ),
        tinyvec.Insertion(
            vector=generate_random_vector(DIMENSIONS),
            metadata={
                "id": 102,
                "brand": "Pear",
                "model": "pPhone Mini Ultra",
                "year": 2025,
                "price": 1299,
                "features": ["wireless charging", "satellite"],
                "inStock": True,
                "specs": {"storage": 1024, "previousOwners": 0, "condition": "new"},
                "ratings": [4.9, 5, 4.8],
            }
        )
    ]

    insert_result = await unique_client.insert(new_phones)
    assert insert_result == 2

    # Search for Pear phones and verify new data
    search_vector = generate_random_vector(DIMENSIONS)
    search_results = await unique_client.search(
        search_vector, 10, tinyvec.SearchOptions(
            filter={"brand": {"$eq": "Pear"}})
    )

    assert len(search_results) == 2
    assert all(r.metadata["year"] == 2025 for r in search_results)
