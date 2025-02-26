# TinyVec Python API Documentation

This document provides a comprehensive overview of the TinyVec Python API.

## Table of Contents

- [Installation](#installation)
- [Core Concepts](#core-concepts)
- [Basic Usage](#basic-usage)
- [API Reference](#api-reference)
  - [TinyVecClient](#tinyvecclient)

## Installation

```bash
pip install tinyvec
```

## Core Concepts

TinyVec is an embedded vector database that emphasizes speed, low memory usage, and simplicity. The core of TinyVec is written in C, and this library provides a Python binding to that engine. The key concepts are:

- **Embeddings**: Fixed-dimension float vectors (e.g., 512 dimensions)
- **Metadata**: JSON-serializable data associated with each vector
- **Similarity Search**: Finding the nearest neighbors to a query vector using cosine similarity

## Basic Usage

```python
import asyncio
import random
import numpy as np
from tinyvec import TinyVecClient, TinyVecConfig, TinyVecInsertion

async def example():
    # Connect to database (will create the file if it doesn't exist)
    client = TinyVecClient()
    config = TinyVecConfig(dimensions=512)
    client.connect("./vectors.db", config)

    # Create a sample vector
    vector = np.random.rand(512).astype(np.float32)

    insertions = []
    for i in range(50):
        # Using NumPy (more efficient)
        vec = np.random.rand(512).astype(np.float32)
        # Or using standard Python lists
        # vec = [random.random() for _ in range(512)]

        insertions.append(TinyVecInsertion(
            vector=vec,
            metadata={"id": i}
        ))

    # Insert vectors
    inserted = await client.insert(insertions)
    print("Inserted:", inserted)

    # Search for similar vectors
    results = await client.search(vector, 5)

    # Example output of search results:
    """
    [TinyVecResult(similarity=0.801587700843811, index=8, metadata={'id': 8}),
     TinyVecResult(similarity=0.7834401726722717, index=16, metadata={'id': 16}),
     TinyVecResult(similarity=0.7815409898757935, index=5, metadata={'id': 5}),
     ...]
    """

if __name__ == "__main__":
    asyncio.run(example())
```

## API Reference

### TinyVecClient

The main class you'll interact with is `TinyVecClient`. It provides all methods for managing the vector database.

#### Constructor and Connection

##### `TinyVecClient()`

Creates a new TinyVec client instance.

**Example:**

```python
client = TinyVecClient()
```

##### `connect(path, config)`

Connects to a TinyVec database.

**Parameters:**

- `path`: `str` - Path to the database file
- `config`: `TinyVecConfig` - Configuration options

**Example:**

```python
config = TinyVecConfig(dimensions=512)
client.connect("./vectors.db", config)
```

#### Instance Methods

##### `async insert(vectors)`

Inserts vectors with metadata into the database. Each metadata item must be a JSON-serializable object.

**Parameters:**

- `vectors`: `List[TinyVecInsertion]` - List of vectors to insert

**Returns:**

- `int` - The number of vectors successfully inserted

**Example:**

```python
vector = np.zeros(512, dtype=np.float32) + 0.1
count = await client.insert([
  TinyVecInsertion(
    vector=vector,
    metadata={"id": "doc1", "title": "Example Document"}
  )
])
```

##### `async search(query_vector, top_k)`

Searches for the most similar vectors to the query vector.

**Parameters:**

- `query_vector`: `Union[List[float], np.ndarray]`  
  A query vector to search for, which can be any of the following types:

  - Python list of numbers
  - NumPy array (any numeric dtype)

  Internally, it will be converted to a float32 array for similarity calculations.

- `top_k`: `int` - Number of results to return

**Returns:**

- `List[TinyVecResult]` - List of search results

**Example:**

```python
results = await client.search(query_vector, 10)
```

##### `async get_index_stats()`

Retrieves statistics about the database.

**Parameters:**

- None

**Returns:**

- `TinyVecStats` - Database statistics

**Example:**

```python
stats = await client.get_index_stats()
print(
  f"Database has {stats.vector_count} vectors with {stats.dimensions} dimensions"
)
```
