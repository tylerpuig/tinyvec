# TinyVecDB Python API Documentation

This document provides a comprehensive overview of the TinyVecDB Python API.

## Table of Contents

- [Installation](#installation)
- [Core Concepts](#core-concepts)
- [Basic Usage](#basic-usage)
- [API Reference](#api-reference)
  - [TinyVecClient](#tinyvecclient)

## Installation

```bash
pip install tinyvecdb
```

## Core Concepts

TinyVecDB is an embedded vector database that emphasizes speed, low memory usage, and simplicity. The core of TinyVecDB is written in C, and this library provides a Python binding to that engine. The key concepts are:

- **Embeddings**: Fixed-dimension float vectors (e.g., 512 dimensions)
- **Metadata**: JSON-serializable data associated with each vector
- **Similarity Search**: Finding the nearest neighbors to a query vector using cosine similarity
- **Filtering**: Query vectors based on metadata attributes

## Basic Usage

```python
import asyncio
import numpy as np
import tinyvec

async def example():
    # Connect to database (will create the file if it doesn't exist)
    client = tinyvec.TinyVecClient()
    config = tinyvec.ClientConfig(dimensions=512)
    client.connect("./vectors.db", config)

    # Create sample vectors
    insertions = []
    for i in range(50):
        # Using NumPy (more efficient)
        vec = np.random.rand(512).astype(np.float32)
        vec = vec / np.linalg.norm(vec)  # Normalize the vector

        # Or using standard Python lists
        # vec = [random.random() for _ in range(512)]

        insertions.append(tinyvec.Insertion(
            vector=vec,
            metadata={"name": f"item-{i}", "category": "example"}
        ))

    # Insert vectors
    inserted = await client.insert(insertions)
    print("Inserted:", inserted)

    # Search for similar vectors (without filtering)
    query_vec = np.random.rand(512).astype(np.float32)
    results = await client.search(query_vec, 5)
    # Example results:
    # [SearchResult(similarity=0.801587700843811, id=8, metadata={'category': 'example', 'name': 'item-8'}),
    #  SearchResult(similarity=0.7834401726722717, id=16, metadata={'category': 'example', 'name': 'item-16'}),
    #  SearchResult(similarity=0.7815409898757935, id=5, metadata={'category': 'example', 'name': 'item-5'})]

    # Search with filtering
    search_options = tinyvec.SearchOptions(
        filter={"category": {"$eq": "example"}}
    )
    filtered_results = await client.search(query_vec, 5, search_options)

    # Delete items by ID
    delete_result = await client.delete_by_ids([1, 2, 3])
    print(f"Deleted {delete_result.deleted_count} vectors. Success: {delete_result.success}")

    # Delete by metadata filter
    filter_result = await client.delete_by_filter(search_options)
    print(f"Deleted {filter_result.deleted_count} vectors by filter. Success: {filter_result.success}")

    # Get database statistics
    stats = await client.get_index_stats()
    print(f"Database has {stats.vector_count} vectors with {stats.dimensions} dimensions")

if __name__ == "__main__":
    asyncio.run(example())
```

## API Reference

### TinyVecClient

The main class you'll interact with is `TinyVecClient`. It provides all methods for managing the vector database.

#### Constructor and Connection

##### `TinyVecClient()`

Creates a new TinyVecDB client instance.

**Example:**

```python
client = tinyvec.TinyVecClient()
```

##### `connect(path, config)`

Connects to a TinyVecDB database.

**Parameters:**

- `path`: `str` - Path to the database file
- `config`: `ClientConfig` - Configuration options

**Example:**

```python
config = tinyvec.ClientConfig(dimensions=512)
client.connect("./vectors.db", config)
```

#### Instance Methods

##### `async insert(vectors)`

Inserts vectors with metadata into the database. Each metadata item must be a JSON-serializable object.

**Parameters:**

- `vectors`: `List[Insertion]` - List of vectors to insert

**Returns:**

- `int` - The number of vectors successfully inserted

**Example:**

```python
vector = np.zeros(512, dtype=np.float32) + 0.1
count = await client.insert([
  tinyvec.Insertion(
    vector=vector,
    metadata={"document_id": "doc1", "title": "Example Document", "category": "reference"}
  )
])
# Example: count = 1
```

##### `async search(query_vector, top_k, search_options=None)`

Searches for the most similar vectors to the query vector.

**Parameters:**

- `query_vector`: `Union[List[float], np.ndarray]`  
  A query vector to search for, which can be any of the following types:

  - Python list of numbers
  - NumPy array (any numeric dtype)

  Internally, it will be converted to a float32 array for similarity calculations.

- `top_k`: `int` - Number of results to return
- `search_options`: `SearchOptions` - Optional. Contains filter criteria for the search.

**Returns:**

- `List[SearchResult]` - List of search results

**Example:**

```python
# Search without filtering
results = await client.search(query_vector, 10)
# Example results:
# [SearchResult(similarity=0.801587700843811, id=8, metadata={'id': 8}),
#  SearchResult(similarity=0.7834401726722717, id=16, metadata={'id': 16}),
#  SearchResult(similarity=0.7815409898757935, id=5, metadata={'id': 5})]

# Search with filtering
search_options = tinyvec.SearchOptions(
    filter={"year": {"$eq": 2024}}
)
filtered_results = await client.search(query_vector, 10, search_options)
```

##### `async delete_by_ids(ids)`

Deletes vectors by their IDs.

**Parameters:**

- `ids`: `List[int]` - List of vector IDs to delete

**Returns:**

- `DeletionResult` - Object containing deletion count and success status

**Example:**

```python
result = await client.delete_by_ids([1, 2, 3])
print(f"Deleted {result.deleted_count} vectors. Success: {result.success}")
# Example output: Deleted 3 vectors. Success: True
```

##### `async delete_by_filter(search_options)`

Deletes vectors that match the given filter criteria.

**Parameters:**

- `search_options`: `SearchOptions` - Contains filter criteria for deletion

**Returns:**

- `DeletionResult` - Object containing deletion count and success status

**Example:**

```python
search_options = tinyvec.SearchOptions(
    filter={"year": {"$eq": 2024}}
)
result = await client.delete_by_filter(search_options)
print(f"Deleted {result.deleted_count} vectors. Success: {result.success}")
```

##### `async update_by_id(items)`

Updates existing database entries with new metadata and/or vector values.

**Parameters:**

- `items`: `List[UpdateItem]` - List of items to update, each containing ID and optional metadata/vector data

**Returns:**

- `UpdateResult` - Object containing update count and success status

**Example:**

```python
import numpy as np
import tinyvec

# Create update items
update_items = [
    tinyvec.UpdateItem(
        id=1,
        vector=np.random.rand(128).astype(np.float32),
        metadata={"category": "electronics", "in_stock": True}
    ),
    tinyvec.UpdateItem(
        id=42,
        metadata={"price": 99.99, "featured": True}
    ),
    tinyvec.UpdateItem(
        id=75,
        vector=np.random.rand(128).astype(np.float32)
    )
]

# Update entries in the database
result = await client.update_by_id(update_items)
print(f"Updated {result.updated_count} entries. Success: {result.success}")
```

##### `async get_paginated(config)`

Retrieves database entries with pagination support.

**Parameters:**

- `config`: `PaginationConfig` - Configuration object containing skip and limit parameters for pagination

**Returns:**

- `List[PaginationItem]` - List of paginated items, each containing ID, metadata, and vector data

**Example:**

```python
import tinyvec

# Create a pagination configuration
config = tinyvec.PaginationConfig(skip=0, limit=10)

# Retrieve paginated results
paginated_results = await client.get_paginated(config)
print(f"Retrieved {len(paginated_results)} items")

# Process the results
for item in paginated_results:
    print(f"ID: {item.id}, Metadata: {item.metadata}")

# Get the next page
next_config = tinyvec.PaginationConfig(skip=10, limit=10)
next_page = await client.get_paginated(next_config)
print(f"Next page has {len(next_page)} items")
```

The `PaginationConfig` class accepts two parameters:

- `skip`: Number of items to skip (must be non-negative)
- `limit`: Maximum number of items to return (must be positive)

Each returned `PaginationItem` contains:

- `id`: Unique identifier for the vector
- `metadata`: Associated metadata (as a JSON-serializable value)
- `vector`: The actual vector data as a list of floats

##### `async get_index_stats()`

Retrieves statistics about the database.

**Parameters:**

- None

**Returns:**

- `Stats` - Database statistics

**Example:**

```python
stats = await client.get_index_stats()
print(
  f"Database has {stats.vector_count} vectors with {stats.dimensions} dimensions"
)
# Example output: Database has 47 vectors with 512 dimensions
```

### Supporting Classes

#### `DeletionResult`

Result from delete operations.

**Properties:**

- `deleted_count`: `int` - Number of vectors deleted
- `success`: `bool` - Whether the operation was successful

**Example:**

```python
result = await client.delete_by_ids([1, 2, 3])
if result.success:
    print(f"Successfully deleted {result.deleted_count} vectors")
else:
    print("Deletion operation failed")
# Example output: Successfully deleted 3 vectors
```

#### `ClientConfig`

Configuration for the vector database.

**Parameters:**

- `dimensions`: `int` - The dimensionality of vectors to be stored

**Example:**

```python
config = tinyvec.ClientConfig(dimensions=512)
```

#### `Insertion`

Class representing a vector to be inserted.

**Parameters:**

- `vector`: `Union[List[float], np.ndarray]` - The vector data
- `metadata`: `Dict` - JSON-serializable metadata associated with the vector

**Example:**

```python
insertion = tinyvec.Insertion(
    vector=np.random.rand(512).astype(np.float32),
    metadata={"category": "example"}
)
```

#### `SearchOptions`

Options for search queries, including filtering.

**Parameters:**

- `filter`: `Dict` - Filter criteria in MongoDB-like query syntax

Available filter operators:

- `$eq`: Matches values equal to a specified value
- `$gt`: Matches values greater than a specified value
- `$gte`: Matches values greater than or equal to a specified value
- `$in`: Matches any values specified in an array
- `$lt`: Matches values less than a specified value
- `$lte`: Matches values less than or equal to a specified value
- `$ne`: Matches values not equal to a specified value
- `$nin`: Matches none of the values specified in an array

Filters can be nested for complex queries.

**Example:**

```python
# Simple filter
search_options = tinyvec.SearchOptions(
    filter={"make": {"$eq": "Toyota"}}
)

# Complex nested filter
search_options = tinyvec.SearchOptions(
    filter={
        "category": {
            "subcategory": {
                "value": {"$gt": 200}
            }
        },
        "tags": {"$in": ["premium", "featured"]}
    }
)
```

#### `SearchResult`

Class representing a search result.

**Properties:**

- `similarity`: `float` - Cosine similarity score
- `id`: `int` - ID of the matched vector
- `metadata`: `Dict | None` - Metadata associated with the matched vector

**Example:**

```python
# Results from a search query
for result in results:
    print(f"ID: {result.id}, Similarity: {result.similarity}, Metadata: {result.metadata}")
# Example output:
# ID: 34, Similarity: 0.8109967303276062, Metadata: {'category': 'example', 'name': 'item-34'}
# ID: 46, Similarity: 0.789353609085083, Metadata: {'category': 'example', 'name': 'item-46'}
# ID: 22, Similarity: 0.7870827913284302, Metadata: {'category': 'example', 'name': 'item-22'}
```

#### `Stats`

Class representing database statistics.

**Properties:**

- `vector_count`: `int` - Number of vectors in the database
- `dimensions`: `int` - Dimensionality of the vectors

**Example:**

```python
stats = await client.get_index_stats()
print(f"Vector count: {stats.vector_count}, Dimensions: {stats.dimensions}")
```
