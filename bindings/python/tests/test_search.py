import pytest
import numpy as np
import tempfile
import os
from typing import List
from tinyvec import TinyVecClient, TinyVecConfig, TinyVecInsertion

pytest_plugins = ['pytest_asyncio']

DIMENSIONS = 128


def normalize_vector(vector: np.ndarray) -> np.ndarray:
    """Normalize a vector to unit length."""
    norm = np.sqrt(np.sum(vector * vector))
    epsilon = 1e-12
    return vector / (norm + epsilon)


def create_vector(base_value: float) -> np.ndarray:
    """Create a vector with a specific pattern."""
    vector = np.array(
        [base_value + i * 0.01 for i in range(DIMENSIONS)], dtype=np.float32)
    return normalize_vector(vector)


def create_vector_list(base_value: float) -> List[float]:
    """Create a vector with a specific pattern."""
    vector = [base_value + i * 0.01 for i in range(DIMENSIONS)]
    return vector


@pytest.fixture(scope="function")
def temp_dir():
    """Create a temporary directory for each test."""
    temp_dir = tempfile.mkdtemp(prefix="tinyvec-test-")
    yield temp_dir


@pytest.fixture
def db_path(temp_dir):
    """Get the database path within the temporary directory."""
    return os.path.join(temp_dir, "test.db")


async def insert_test_vectors(client: TinyVecClient) -> List[TinyVecInsertion]:
    """Helper to prepare test data."""
    insertions = [
        TinyVecInsertion(
            vector=create_vector(i),
            metadata={
                "id": i,
                "text": f"document_{i}",
                "category": "even" if i % 2 == 0 else "odd"
            }
        )
        for i in range(10)
    ]

    await client.insert(insertions)
    return insertions


@pytest.fixture
def client(db_path):
    """Create and connect a TinyVec client instance."""
    client = TinyVecClient()
    client.connect(db_path, TinyVecConfig(dimensions=128))
    return client


@pytest.mark.asyncio
async def test_find_exact_matches(client):
    """Should find exact matches."""
    insertions = await insert_test_vectors(client)

    # Search using the exact same vector as document_5
    search_vector = create_vector(5)
    results = await client.search(search_vector, 1)

    assert len(results) == 1
    assert results[0].metadata["id"] == 5
    assert results[0].metadata["text"] == "document_5"


@pytest.mark.asyncio
async def test_return_correct_number_with_top_k(client):
    """Should return correct number of results with topK."""
    await insert_test_vectors(client)

    search_vector = create_vector(3)
    top_k = 5
    results = await client.search(search_vector, top_k)

    assert len(results) == top_k


@pytest.mark.asyncio
async def test_return_results_in_order(client):
    """Should return results in order of similarity."""
    await insert_test_vectors(client)

    search_vector = create_vector(2)
    results = await client.search(search_vector, 3)

    # The closest vectors should be 2, then 1 or 3
    assert results[0].metadata["id"] == 2

    # Check scores are in descending order (higher is more similar)
    scores = [r.similarity for r in results]
    assert scores == sorted(scores, reverse=True)


@pytest.mark.asyncio
async def test_handle_no_similar_results(client):
    """Should handle search with no similar results."""
    await insert_test_vectors(client)

    # Create a very different vector
    search_vector = np.full(DIMENSIONS, 999, dtype=np.float32)
    results = await client.search(search_vector, 5)

    # Should still return results, but with lower similarity scores
    assert len(results) == 5


@pytest.mark.asyncio
async def test_throw_error_different_dimensions(client):
    """Should throw an error for a query vector with different dimensions."""
    await insert_test_vectors(client)

    # Create a vector with wrong dimensions
    search_vector = np.full(64, 0.04, dtype=np.float32)

    with pytest.raises(RuntimeError):
        await client.search(search_vector, 5)


@pytest.mark.asyncio
async def test_preserve_metadata_types(client):
    """Should preserve metadata types in search results."""
    insertion = TinyVecInsertion(
        vector=create_vector(0),
        metadata={
            "id": 1,
            "numeric": 42,
            "string": "test",
            "boolean": True,
            "nested": {"key": "value"},
            "array": [1, 2, 3]
        }
    )

    await client.insert([insertion])

    results = await client.search(create_vector(0), 1)

    assert results[0].metadata == insertion.metadata
    assert isinstance(results[0].metadata["numeric"], (int, float))
    assert isinstance(results[0].metadata["string"], str)
    assert isinstance(results[0].metadata["boolean"], bool)
    assert isinstance(results[0].metadata["array"], list)


@pytest.mark.asyncio
async def test_multiple_searches_different_top_k(client):
    """Should handle multiple searches with different topK."""
    await insert_test_vectors(client)
    search_vector = create_vector(5)

    results1 = await client.search(search_vector, 3)
    results2 = await client.search(search_vector, 7)

    assert len(results1) == 3
    assert len(results2) == 7

    # First 3 results should be identical in both searches
    assert [r.metadata["id"] for r in results1] == [
        r.metadata["id"] for r in results2[:3]]


@pytest.mark.asyncio
async def test_list_search(client):
    """Should handle a non numpy array list."""
    await insert_test_vectors(client)
    search_vector = create_vector_list(5)

    results1 = await client.search(search_vector, 3)
    results2 = await client.search(search_vector, 7)

    assert len(results1) == 3
    assert len(results2) == 7

    # First 3 results should be identical in both searches
    assert [r.metadata["id"] for r in results1] == [
        r.metadata["id"] for r in results2[:3]]
