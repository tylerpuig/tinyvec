import tinyvec
import pytest
import numpy as np
import uuid
import os

DIMENSIONS = 128


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
async def test_get_paginated_single_entry(unique_client):
    """Test that get_paginated returns a single entry correctly."""
    # Insert a single vector
    insertion = tinyvec.Insertion(
        vector=np.random.rand(128).astype(np.float32),
        metadata={"id": 1, "type": "text",
                  "content": "hello", "paginate": True}
    )

    inserted = await unique_client.insert([insertion])
    assert inserted == 1

    # Get stats to verify insertion
    stats = await unique_client.get_index_stats()
    assert stats.vector_count == 1
    assert stats.dimensions == 128

    # Test pagination
    config = tinyvec.PaginationConfig(skip=0, limit=1)
    paginated_results = await unique_client.get_paginated(config)

    assert len(paginated_results) == 1
    assert paginated_results[0].metadata["id"] == 1
    assert paginated_results[0].metadata["type"] == "text"
    assert paginated_results[0].metadata["content"] == "hello"
    assert paginated_results[0].metadata["paginate"] == True


@pytest.mark.asyncio
async def test_get_paginated_multiple_entries(unique_client):
    """Test that get_paginated returns multiple entries with correct pagination."""
    # Insert 10 vectors
    insertions = []
    for i in range(10):
        insertions.append(tinyvec.Insertion(
            vector=np.random.rand(128).astype(np.float32),
            metadata={"id": i + 1, "type": "text", "content": f"item {i + 1}"}
        ))

    inserted = await unique_client.insert(insertions)
    assert inserted == 10

    # Get first page
    first_page_config = tinyvec.PaginationConfig(skip=0, limit=3)
    first_page = await unique_client.get_paginated(first_page_config)

    assert len(first_page) == 3
    assert first_page[0].metadata["id"] == 1
    assert first_page[1].metadata["id"] == 2
    assert first_page[2].metadata["id"] == 3

    # Get second page
    second_page_config = tinyvec.PaginationConfig(skip=3, limit=3)
    second_page = await unique_client.get_paginated(second_page_config)

    assert len(second_page) == 3
    assert second_page[0].metadata["id"] == 4
    assert second_page[1].metadata["id"] == 5
    assert second_page[2].metadata["id"] == 6

    # Get last page (with fewer items)
    last_page_config = tinyvec.PaginationConfig(skip=9, limit=3)
    last_page = await unique_client.get_paginated(last_page_config)

    assert len(last_page) == 1
    assert last_page[0].metadata["id"] == 10


@pytest.mark.asyncio
async def test_get_paginated_empty_database(unique_client):
    """Test that get_paginated returns an empty array when database is empty."""
    config = tinyvec.PaginationConfig(skip=0, limit=10)
    paginated_results = await unique_client.get_paginated(config)

    assert len(paginated_results) == 0


@pytest.mark.asyncio
async def test_get_paginated_skip_exceeds_count(unique_client):
    """Test that get_paginated returns an empty array when skip exceeds vector count."""
    # Insert 5 vectors
    insertions = []
    for i in range(5):
        insertions.append(tinyvec.Insertion(
            vector=np.random.rand(128).astype(np.float32),
            metadata={"id": i + 1}
        ))

    inserted = await unique_client.insert(insertions)
    assert inserted == 5

    # Skip more than available
    config = tinyvec.PaginationConfig(skip=10, limit=5)
    paginated_results = await unique_client.get_paginated(config)

    assert len(paginated_results) == 0


@pytest.mark.asyncio
async def test_get_paginated_adjust_limit(unique_client):
    """Test that get_paginated adjusts limit to match available vectors."""
    # Insert 3 vectors
    insertions = []
    for i in range(3):
        insertions.append(tinyvec.Insertion(
            vector=np.random.rand(128).astype(np.float32),
            metadata={"id": i + 1}
        ))

    inserted = await unique_client.insert(insertions)
    assert inserted == 3

    # Request more than available
    config = tinyvec.PaginationConfig(skip=0, limit=10)
    paginated_results = await unique_client.get_paginated(config)

    assert len(paginated_results) == 3  # Should adjust to actual count


@pytest.mark.asyncio
async def test_get_paginated_complex_metadata(unique_client):
    """Test that get_paginated handles complex metadata correctly."""
    complex_metadata = {
        "id": 1,
        "type": "document",
        "content": "complex example",
        "nested": {
            "property": "value",
            "array": [1, 2, 3],
        },
        "tags": ["important", "test"],
    }

    insertion = tinyvec.Insertion(
        vector=np.random.rand(128).astype(np.float32),
        metadata=complex_metadata
    )

    await unique_client.insert([insertion])

    config = tinyvec.PaginationConfig(skip=0, limit=1)
    paginated_results = await unique_client.get_paginated(config)

    assert len(paginated_results) == 1
    assert paginated_results[0].metadata == complex_metadata


@pytest.mark.asyncio
async def test_get_paginated_invalid_skip(unique_client):
    """Test that get_paginated throws error with invalid skip parameter."""
    with pytest.raises(Exception) as excinfo:
        config = tinyvec.PaginationConfig(skip=-1, limit=10)
        await unique_client.get_paginated(config)

    assert "Skip must be a positive number" in str(excinfo.value)


@pytest.mark.asyncio
async def test_get_paginated_invalid_limit(unique_client):
    """Test that get_paginated throws error with invalid limit parameter."""
    # Test zero limit
    with pytest.raises(Exception) as excinfo1:
        config = tinyvec.PaginationConfig(skip=0, limit=0)
        await unique_client.get_paginated(config)

    assert "Limit must be a positive number" in str(excinfo1.value)

    # Test negative limit
    with pytest.raises(Exception) as excinfo2:
        config = tinyvec.PaginationConfig(skip=0, limit=-5)
        await unique_client.get_paginated(config)

    assert "Limit must be a positive number" in str(excinfo2.value)


@pytest.mark.asyncio
async def test_get_paginated_large_dataset(unique_client):
    """Test that get_paginated handles a large dataset with proper pagination."""
    total_items = 100
    page_size = 25

    # Create a large batch of insertions
    insertions = []
    for i in range(total_items):
        insertions.append(tinyvec.Insertion(
            vector=np.random.rand(128).astype(np.float32),
            metadata={"id": i + 1, "content": f"Item {i + 1}"}
        ))

    await unique_client.insert(insertions)

    # Verify total count
    stats = await unique_client.get_index_stats()
    assert stats.vector_count == total_items

    # Test pagination logic by fetching all pages
    collected_items = []
    for page in range(0, (total_items + page_size - 1) // page_size):
        config = tinyvec.PaginationConfig(
            skip=page * page_size, limit=page_size)
        page_results = await unique_client.get_paginated(config)

        # Check that last page has correct number of items
        expected_items = min(page_size, total_items - page * page_size)
        assert len(page_results) == expected_items

        collected_items.extend(page_results)

    # Verify we got all items
    assert len(collected_items) == total_items

    # Verify the items are in the correct order
    for i in range(total_items):
        assert collected_items[i].metadata["id"] == i + 1
