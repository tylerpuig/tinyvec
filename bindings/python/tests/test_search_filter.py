import os
import uuid
import pytest
import numpy as np
import tinyvec
from typing import List, Any, TypedDict, Optional

# Match the dimensions used in the Node.js tests
DIMENSIONS = 128


class ProductSpecs(TypedDict):
    storage: int
    previousOwners: int
    condition: str


class ProductMetadata(TypedDict):
    id: int
    brand: str
    model: str
    year: int
    price: int
    features: List[str]
    inStock: bool
    specs: ProductSpecs
    ratings: List[float]


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


def create_vector(base_value: float) -> np.ndarray:
    """Create a test vector with a specific pattern, matching the Node.js test."""
    vector = np.zeros(DIMENSIONS, dtype=np.float32)
    for i in range(DIMENSIONS):
        vector[i] = base_value + i * 0.01

    return vector


async def insert_test_vectors(client: tinyvec.TinyVecClient) -> int:
    """Insert test vectors with rich metadata, matching the Node.js test data."""
    test_data: List[ProductMetadata] = [
        {
            "id": 0,
            "brand": "Nexus",
            "model": "Galaxy",
            "year": 2020,
            "price": 799,
            "features": ["wireless charging", "water resistant"],
            "inStock": True,
            "specs": {"storage": 128, "previousOwners": 1, "condition": "excellent"},
            "ratings": [4, 5, 4.5],
        },
        {
            "id": 1,
            "brand": "Pear",
            "model": "pPhone",
            "year": 2021,
            "price": 899,
            "features": ["wireless charging", "portrait mode"],
            "inStock": True,
            "specs": {"storage": 256, "previousOwners": 1, "condition": "excellent"},
            "ratings": [4.5, 5, 4.7],
        },
        {
            "id": 2,
            "brand": "Pear",
            "model": "pPhone Mini",
            "year": 2019,
            "price": 650,
            "features": ["wireless charging"],
            "inStock": False,
            "specs": {"storage": 64, "previousOwners": 2, "condition": "good"},
            "ratings": [4, 4.2, 3.8],
        },
        {
            "id": 3,
            "brand": "Nexus",
            "model": "Pixel",
            "year": 2018,
            "price": 550,
            "features": ["water resistant"],
            "inStock": True,
            "specs": {"storage": 32, "previousOwners": 1, "condition": "good"},
            "ratings": [3.5, 4, 3.8],
        },
        {
            "id": 4,
            "brand": "Oceania",
            "model": "Wave Pro",
            "year": 2022,
            "price": 1099,
            "features": ["5G capability", "wireless charging", "portrait mode"],
            "inStock": True,
            "specs": {"storage": 512, "previousOwners": 0, "condition": "excellent"},
            "ratings": [4.8, 5, 4.9],
        },
        {
            "id": 5,
            "brand": "Pinnacle",
            "model": "Summit",
            "year": 2021,
            "price": 999,
            "features": ["5G capability", "wireless charging"],
            "inStock": False,
            "specs": {"storage": 256, "previousOwners": 1, "condition": "excellent"},
            "ratings": [4.6, 4.8, 4.7],
        },
        {
            "id": 6,
            "brand": "Horizon",
            "model": "Edge",
            "year": 2020,
            "price": 850,
            "features": ["portrait mode", "AI assistant"],
            "inStock": True,
            "specs": {"storage": 128, "previousOwners": 1, "condition": "excellent"},
            "ratings": [4.7, 4.9, 4.8],
        },
        {
            "id": 7,
            "brand": "Quantum",
            "model": "Z Series",
            "year": 2019,
            "price": 799,
            "features": ["portrait mode", "AI assistant"],
            "inStock": True,
            "specs": {"storage": 256, "previousOwners": 2, "condition": "good"},
            "ratings": [4.5, 4.6, 4.7],
        },
        {
            "id": 8,
            "brand": "Stellar",
            "model": "X-Class",
            "year": 2018,
            "price": 750,
            "features": ["portrait mode", "AI assistant"],
            "inStock": False,
            "specs": {"storage": 128, "previousOwners": 1, "condition": "good"},
            "ratings": [4.4, 4.5, 4.3],
        },
        {
            "id": 9,
            "brand": "Pear",
            "model": "pPhone Pro",
            "year": 2024,
            "price": 1299,
            "features": ["wireless charging", "AI assistant", "portrait mode"],
            "inStock": True,
            "specs": {"storage": 1024, "previousOwners": 0, "condition": "new"},
            "ratings": [5, 5, 4.9],
        },
    ]

    insertions: List[tinyvec.Insertion] = []
    for entry in test_data:
        metadata_dict = dict(entry)
        # Use the same approach for vector creation as in Node.js
        insertions.append(tinyvec.Insertion(
            # Using DIMENSIONS as baseValue
            vector=create_vector(DIMENSIONS),
            metadata=metadata_dict
        ))

    inserted = await client.insert(insertions)
    return inserted


# Now for the tests, matching the Node.js tests one-by-one
@pytest.mark.asyncio
async def test_filter_with_eq_operator(unique_client):
    """Test filtering with $eq operator."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_options = tinyvec.SearchOptions(
        filter={"brand": {"$eq": "Pear"}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(r.metadata.get("brand") == "Pear" for r in results)


@pytest.mark.asyncio
async def test_filter_with_ne_operator(unique_client):
    """Test filtering with $ne (not equal) operator."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_options = tinyvec.SearchOptions(
        filter={"brand": {"$ne": "Pear"}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(r.metadata.get("brand") != "Pear" for r in results)


@pytest.mark.asyncio
async def test_filter_with_gt_operator(unique_client):
    """Test filtering with $gt (greater than) operator with numbers."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_options = tinyvec.SearchOptions(
        filter={"year": {"$gt": 2020}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(r.metadata.get("year") > 2020 for r in results)


@pytest.mark.asyncio
async def test_filter_with_gte_operator(unique_client):
    """Test filtering with $gte (greater than or equal) operator."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_options = tinyvec.SearchOptions(
        filter={"year": {"$gte": 2020}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(r.metadata.get("year") >= 2020 for r in results)


@pytest.mark.asyncio
async def test_filter_with_lt_operator(unique_client):
    """Test filtering with $lt (less than) operator."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_options = tinyvec.SearchOptions(
        filter={"year": {"$lt": 2020}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(r.metadata.get("year") < 2020 for r in results)


@pytest.mark.asyncio
async def test_filter_with_lte_operator(unique_client):
    """Test filtering with $lte (less than or equal) operator."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_options = tinyvec.SearchOptions(
        filter={"year": {"$lte": 2020}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(r.metadata.get("year") <= 2020 for r in results)


@pytest.mark.asyncio
async def test_filter_with_exists_operator(unique_client):
    """Test filtering with $exists operator."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    # Test $exists: false (should find no results since all entries have brand)
    search_options_false = tinyvec.SearchOptions(
        filter={"brand": {"$exists": False}}
    )
    no_brand_results = await unique_client.search(search_vector, 10, search_options_false)
    assert len(no_brand_results) == 0

    # Test $exists: true
    search_options_true = tinyvec.SearchOptions(
        filter={"brand": {"$exists": True}}
    )
    has_brand_results = await unique_client.search(search_vector, 10, search_options_true)
    assert len(has_brand_results) > 0
    assert all("brand" in r.metadata for r in has_brand_results)


@pytest.mark.asyncio
async def test_filter_with_in_operator(unique_client):
    """Test filtering with $in operator (value must be in an array)."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_items = ["Pear", "Nexus"]
    search_options = tinyvec.SearchOptions(
        filter={"brand": {"$in": search_items}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(r.metadata.get("brand") in search_items for r in results)


@pytest.mark.asyncio
async def test_filter_with_nin_operator(unique_client):
    """Test filtering with $nin operator (value must not be in an array)."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_items = ["Pear", "Nexus"]
    search_options = tinyvec.SearchOptions(
        filter={"brand": {"$nin": search_items}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(r.metadata.get("brand") not in search_items for r in results)


@pytest.mark.asyncio
async def test_filter_with_nested_property(unique_client):
    """Test filtering with nested object property."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    # Using the nested specs.storage property
    search_options = tinyvec.SearchOptions(
        filter={"specs": {"storage": {"$lt": 200}}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(r.metadata.get("specs", {}).get(
        "storage") < 200 for r in results)


@pytest.mark.asyncio
async def test_filter_for_values_in_array_field(unique_client):
    """Test filtering for values in array field."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_options = tinyvec.SearchOptions(
        filter={"ratings": {"$in": [4]}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(4 in r.metadata.get("ratings", []) for r in results)


@pytest.mark.asyncio
async def test_filter_with_multiple_conditions(unique_client):
    """Test filtering with multiple conditions combined."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_options = tinyvec.SearchOptions(
        filter={
            "brand": {"$eq": "Pear"},
            "year": {"$gte": 2020},
            "inStock": {"$eq": True}
        }
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(
        r.metadata.get("brand") == "Pear" and
        r.metadata.get("year") >= 2020 and
        r.metadata.get("inStock") is True
        for r in results
    )


@pytest.mark.asyncio
async def test_type_mismatch_should_return_empty(unique_client):
    """Test that no results are returned when filter type doesn't match metadata type."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    # Using string "2020" instead of number 2020
    search_options = tinyvec.SearchOptions(
        filter={"year": {"$eq": "2020"}}
    )

    results = await unique_client.search(search_vector, 10, search_options)

    # Should not match anything as we're comparing string to number
    assert len(results) == 0


@pytest.mark.asyncio
async def test_filter_with_nested_query_structure(unique_client):
    """Test filtering with a nested query structure."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_options = tinyvec.SearchOptions(
        filter={
            "brand": {"$eq": "Pear"},
            "specs": {
                "condition": {"$eq": "excellent"}
            },
            "inStock": {"$eq": True}
        }
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) > 0
    assert all(
        r.metadata.get("brand") == "Pear" and
        r.metadata.get("specs", {}).get("condition") == "excellent" and
        r.metadata.get("inStock") is True
        for r in results
    )


@pytest.mark.asyncio
async def test_exact_match_for_specific_phone(unique_client):
    """Test exact match for Pear pPhone Pro 2024, matching the specific example from Node.js."""
    await insert_test_vectors(unique_client)
    search_vector = create_vector(3)

    search_options = tinyvec.SearchOptions(
        filter={
            "model": {"$eq": "pPhone Pro"},
            "year": {"$eq": 2024},
            "brand": {"$eq": "Pear"}
        }
    )

    results = await unique_client.search(search_vector, 10, search_options)

    assert len(results) == 1
    assert results[0].metadata.get("brand") == "Pear"
    assert results[0].metadata.get("model") == "pPhone Pro"
    assert results[0].metadata.get("year") == 2024


@pytest.mark.asyncio
async def test_filter_with_deeply_nested_object(unique_client):
    """Test filtering for a very deeply nested object."""
    # Create a unique set of deeply nested objects
    insertions: List[tinyvec.Insertion] = []
    for i in range(10):
        item = {
            "item": {
                "inner": {
                    "another": {
                        "value": i
                    }
                }
            }
        }

        insertions.append(tinyvec.Insertion(
            vector=create_vector(DIMENSIONS),
            metadata=item
        ))

    inserted = await unique_client.insert(insertions)
    assert inserted == 10

    search_vector = create_vector(3)

    # Test $gte filter on deeply nested property
    gte_search_options = tinyvec.SearchOptions(
        filter={
            "item": {
                "inner": {
                    "another": {
                        "value": {"$gte": 4}
                    }
                }
            }
        }
    )

    gte_results = await unique_client.search(search_vector, 10, gte_search_options)

    assert len(gte_results) > 0
    assert all(
        r.metadata.get("item", {}).get("inner", {}).get(
            "another", {}).get("value") >= 4
        for r in gte_results
    )

    # Test $lte filter on deeply nested property
    lte_search_options = tinyvec.SearchOptions(
        filter={
            "item": {
                "inner": {
                    "another": {
                        "value": {"$lte": 4}
                    }
                }
            }
        }
    )

    lte_results = await unique_client.search(search_vector, 10, lte_search_options)

    assert len(lte_results) > 0
    assert all(
        r.metadata.get("item", {}).get("inner", {}).get(
            "another", {}).get("value") <= 4
        for r in lte_results
    )
