import tinyvec
import pytest
import numpy as np
from typing import List, Any
import random
import uuid
import os

DIMENSIONS = 128


def safe_get_metadata_value(metadata: Any, key: str, default: Any = None) -> Any:
    """Safely get a value from metadata, regardless of its type."""
    if isinstance(metadata, dict) and key in metadata:
        return metadata[key]
    return default


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
    """Generate a random vector with given dimensions."""
    return np.random.rand(dimensions).astype(np.float32)


def generate_random_vector_list(dimensions: int) -> List[float]:
    """Generate a random vector with given dimensions."""
    return [random.random() for _ in range(dimensions)]


@pytest.mark.asyncio
async def test_update_single_vector_with_both_vector_and_metadata(unique_client):
    """Should update a single vector entry with both vector and metadata."""
    # Insert some test data
    insertions = [
        tinyvec.Insertion(
            vector=generate_random_vector(DIMENSIONS),
            metadata={
                "originalId": 5,
                "name": "Original name",
                "category": "Original category",
                "items": [1, 2, 3]
            }
        )
    ]

    inserted = await unique_client.insert(insertions)
    assert inserted == len(insertions)

    id_to_update = 1
    new_vector = generate_random_vector(DIMENSIONS)
    new_metadata = {
        "originalId": 6,
        "name": "Updated Item",
        "category": "updated",
        "items": [4, 5, 6]
    }

    update_items = [
        tinyvec.UpdateItem(
            id=id_to_update,
            vector=new_vector,
            metadata=new_metadata
        )
    ]

    update_result = await unique_client.update_by_id(update_items)

    assert update_result.success == True
    assert update_result.updated_count == 1

    query_vector = generate_random_vector(DIMENSIONS)
    search_results = await unique_client.search(query_vector, 1)
    item = search_results[0]

    assert item is not None
    assert item.metadata["category"] == new_metadata["category"]
    assert item.metadata["items"] == new_metadata["items"]
    assert item.metadata["name"] == new_metadata["name"]


@pytest.mark.asyncio
async def test_update_multiple_vector_entries(unique_client):
    """Should update multiple vector entries."""
    ids_to_update = [1, 2, 3]

    items_to_insert = [
        tinyvec.Insertion(
            vector=generate_random_vector(DIMENSIONS),
            metadata={
                "originalId": id,
                "name": f"Batch Updated Item {id}",
                "category": "original-category"
            }
        ) for id in ids_to_update
    ]

    inserted = await unique_client.insert(items_to_insert)
    assert inserted == len(ids_to_update)

    update_items = [
        tinyvec.UpdateItem(
            id=ids_to_update[i],
            vector=generate_random_vector(DIMENSIONS),
            metadata={
                "originalId": safe_get_metadata_value(item.metadata, "originalId", i),
                "name": f"Updated Item {safe_get_metadata_value(item.metadata, 'originalId', i)}",
                "category": "updated-category"
            }
        ) for i, item in enumerate(items_to_insert)
    ]

    update_results = await unique_client.update_by_id(update_items)

    assert update_results.success == True
    assert update_results.updated_count == len(ids_to_update)

    # Verify the updates
    search_results = await unique_client.search(generate_random_vector(DIMENSIONS), 3)
    assert len(search_results) == 3

    for id in ids_to_update:
        item = next(
            (r for r in search_results if r.metadata["originalId"] == id), None)
        assert item is not None
        assert item.metadata["category"] == "updated-category"
        assert item.metadata["name"] == f"Updated Item {id}"


@pytest.mark.asyncio
async def test_update_only_vector_without_changing_metadata(unique_client):
    """Should update only vector without changing metadata."""
    id_to_update = 1
    ids = list(range(1, 11))  # 1 to 10

    # Generate a distinctive vector for the update
    original_vector = generate_random_vector(DIMENSIONS)

    insert_items = [
        tinyvec.Insertion(
            vector=original_vector if id == id_to_update else generate_random_vector(
                DIMENSIONS),
            metadata={
                "originalId": id,
                "name": f"Original Item {id}",
                "category": "original-category"
            }
        ) for id in ids
    ]

    insertions = await unique_client.insert(insert_items)
    assert insertions == 10

    # Create a very distinctive vector (all 1s)
    new_vector = np.ones(DIMENSIONS, dtype=np.float32)
    update_items = [
        tinyvec.UpdateItem(
            id=id_to_update,
            vector=new_vector
        )
    ]

    update_results = await unique_client.update_by_id(update_items)
    assert update_results.success == True
    assert update_results.updated_count == 1

    # Search using the updated vector to find our specific item
    search_results = await unique_client.search(new_vector, 1)
    assert len(
        search_results) > 0, "Search returned no results after vector update"

    item = search_results[0]
    assert item is not None, "Expected to find the updated item"
    assert "originalId" in item.metadata, f"Metadata missing or corrupted: {item.metadata}"
    assert item.metadata["originalId"] == id_to_update, f"Found wrong item: {item.metadata}"

    # Now verify the metadata wasn't changed
    assert item.metadata["category"] == "original-category"
    assert item.metadata["name"] == f"Original Item {id_to_update}"


@pytest.mark.asyncio
async def test_update_only_metadata_without_changing_vector(unique_client):
    """Should update only metadata without changing vector."""
    id_to_update = 2
    ids = list(range(1, 11))  # 1 to 10

    insert_items = [
        tinyvec.Insertion(
            vector=generate_random_vector(DIMENSIONS),
            metadata={
                "originalId": id,
                "name": f"Original Item {id}",
                "category": "original-category"
            }
        ) for id in ids
    ]

    insertions = await unique_client.insert(insert_items)
    assert insertions == 10

    import datetime
    new_metadata = {
        "originalId": id_to_update,
        "name": "Metadata Only Update",
        "category": "meta-updated",
        "timestamp": datetime.datetime.now().isoformat()
    }

    update_items = [
        tinyvec.UpdateItem(
            id=id_to_update,
            metadata=new_metadata
        )
    ]

    update_results = await unique_client.update_by_id(update_items)
    assert update_results.success == True
    assert update_results.updated_count == 1

    search_results = await unique_client.search(generate_random_vector(DIMENSIONS), 10)
    assert len(search_results) > 0

    item = next(
        (r for r in search_results if r.metadata and r.metadata.get("originalId") == id_to_update), None)

    # Make sure we found the item
    assert item is not None, "Could not find the updated item with the expected originalId"
    assert item.metadata is not None, "Item metadata is None"

    # Check if metadata was updated
    assert item.metadata.get("category") == "meta-updated"
    assert item.metadata.get("name") == "Metadata Only Update"


@pytest.mark.asyncio
async def test_fail_gracefully_when_updating_nonexistent_id(unique_client):
    """Should fail gracefully when trying to update non-existent ID."""
    non_existent_id = 9999

    update_items = [
        tinyvec.UpdateItem(
            id=non_existent_id,
            vector=generate_random_vector(DIMENSIONS),
            metadata={"name": "Doesn't exist"}
        )
    ]

    update_results = await unique_client.update_by_id(update_items)

    # Should indicate no updates
    assert update_results.updated_count == 0
    assert update_results.success == False


@pytest.mark.asyncio
async def test_handle_mixed_success_and_failure_for_batch_updates(unique_client):
    """Should handle mixed success and failure for batch updates."""
    ids = list(range(1, 11))  # 1 to 10

    insert_items = [
        tinyvec.Insertion(
            vector=generate_random_vector(DIMENSIONS),
            metadata={
                "originalId": id,
                "name": f"Original Item {id}",
                "category": "original-category"
            }
        ) for id in ids
    ]

    insertions = await unique_client.insert(insert_items)
    assert insertions == 10

    # Valid IDs that exist and one that doesn't
    valid_ids = [3, 4]
    invalid_id = 9999

    update_items = [
        *[tinyvec.UpdateItem(
            id=id,
            metadata={
                "originalId": id,
                "name": f"Valid Update {id}",
                "category": "original-category"
            }
        ) for id in valid_ids],
        tinyvec.UpdateItem(
            id=invalid_id,
            metadata={"name": "Invalid Update"}
        )
    ]

    update_results = await unique_client.update_by_id(update_items)

    # Operation succeeds but only valid IDs are updated
    assert update_results.success == True
    assert update_results.updated_count == len(valid_ids)

    search_results = await unique_client.search(generate_random_vector(DIMENSIONS), 10)

    # Verify only valid IDs were updated
    for id in valid_ids:
        item = next(
            (r for r in search_results if r.metadata["originalId"] == id), None)
        if item is None:
            continue
        name_value = safe_get_metadata_value(item.metadata, "name", None)
        assert name_value == f"Valid Update {id}"


@pytest.mark.asyncio
async def test_preserve_search_findability_after_vector_update(unique_client):
    """Should preserve search findability after vector update."""
    id_to_update = 5
    ids = list(range(1, 11))  # 1 to 10

    insert_items = [
        tinyvec.Insertion(
            vector=generate_random_vector(DIMENSIONS),
            metadata={
                "originalId": id,
                "name": f"Original Item {id}",
                "category": "original-category"
            }
        ) for id in ids
    ]

    insertions = await unique_client.insert(insert_items)
    assert insertions == 10

    # Distinctive vector (all 1s)
    distinctive_vector = np.ones(DIMENSIONS, dtype=np.float32)

    update_items = [
        tinyvec.UpdateItem(
            id=id_to_update,
            vector=distinctive_vector
        )
    ]

    update_results = await unique_client.update_by_id(update_items)
    assert update_results.success == True
    assert update_results.updated_count == 1

    search_results = await unique_client.search(distinctive_vector, 1)

    # Should return exactly one result that matches our updated ID
    assert len(search_results) == 1

    assert search_results[0].metadata["originalId"] == id_to_update
    # Should be very close to 1
    assert abs(search_results[0].similarity - 1.0) < 1e-5

    # Metadata should be preserved
    assert search_results[0].metadata["category"] == "original-category"
    assert search_results[0].metadata["name"] == f"Original Item {id_to_update}"


@pytest.mark.asyncio
async def test_not_update_any_vectors_with_empty_vector_and_metadata(unique_client):
    """Should not update any vectors with empty vector & metadata."""
    id_to_update = 1

    insert_items = [
        tinyvec.Insertion(
            vector=generate_random_vector(DIMENSIONS),
            metadata={
                "originalId": id_to_update,
                "name": f"Original Item {id_to_update}",
                "category": "original-category"
            }
        )
    ]

    insertions = await unique_client.insert(insert_items)
    assert insertions == 1

    # Assert that creating an UpdateItem with only ID raises the expected error
    with pytest.raises(ValueError, match="Either metadata or vector must be provided"):
        tinyvec.UpdateItem(id=1)
