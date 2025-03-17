# TinyVecDB

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![npm](https://img.shields.io/npm/v/tinyvecdb)](https://www.npmjs.com/package/tinyvecdb)
[![PyPI](https://img.shields.io/pypi/v/tinyvecdb)](https://pypi.org/project/tinyvecdb/)

A lightweight, embedded vector database focused on simplicity and speed. TinyVecDB provides vector storage and similarity search without the complexity of client-server architectures.

## 🚀 Features

- **Embedded**: No server setup or maintenance required
- **Simple API**: Get started with just a few lines of code
- **Cross-platform**: Works seamlessly on Windows, macOS, and Linux
- **Lightweight**: Minimal memory footprint and resource usage
- **Offline-capable**: Great for desktop and mobile applications

## 🧪 Performance

TinyVecDB is optimized for performance:

- **4-10x faster** query times compared to alternatives
- **Minimal memory footprint** during operations
- **Efficient metadata filtering** with vector search

Check out our detailed benchmarks:

- [Vector Search Performance](benchmarks/vector-search.md)
- [Vector Search with Metadata Filtering](benchmarks/vector-search-with-filter.md)

## 🛠️ Installation

### JavaScript/TypeScript

```bash
npm install tinyvecdb
```

### Python

```bash
pip install tinyvecdb
```

## 📝 Quick Start

### TypeScript/JavaScript

```typescript
import {
  TinyVecClient,
  type TinyVecInsertion,
  type TinyVecSearchOptions,
} from "tinyvecdb";

async function example() {
  // Connect to database (will create the file if it doesn't exist)
  const client = TinyVecClient.connect("./vectors.db", { dimensions: 512 });

  // Create sample vectors
  const insertions: TinyVecInsertion[] = [];
  for (let i = 0; i < 50; i++) {
    // Using Float32Array (more efficient)
    const vec = new Float32Array(512);
    for (let j = 0; j < 512; j++) {
      vec[j] = Math.random();
    }

    // Or using standard JavaScript arrays
    // const vec = Array.from({ length: 512 }, () => Math.random());

    insertions.push({
      vector: vec,
      metadata: { name: `item-${i}`, category: "example" },
    });
  }

  // Insert vectors
  const inserted = await client.insert(insertions);
  console.log("Inserted:", inserted);

  // Create a query vector
  const queryVector = new Float32Array(512);
  for (let i = 0; i < 512; i++) {
    queryVector[i] = Math.random();
  }

  // Search for similar vectors (without filtering)
  const results = await client.search(queryVector, 5);
  // Example results:
  /*
  [
    {
      id: 8,
      similarity: 0.801587700843811,
      metadata: { name: 'item-8', category: 'example' }
    },
    {
      id: 16,
      similarity: 0.7834401726722717,
      metadata: { name: 'item-16', category: 'example' }
    },
    {
      id: 5,
      similarity: 0.7815409898757935,
      metadata: { name: 'item-5', category: 'example' }
    },
    ...
  ]
  */

  // Search with filtering
  const searchOptions: TinyVecSearchOptions = {
    filter: { category: { $eq: "example" } },
  };
  const filteredResults = await client.search(queryVector, 5, searchOptions);

  // Delete items by ID
  const deleteByIdResult = await client.deleteByIds([1, 2, 3]);
  console.log("Delete by ID result:", deleteByIdResult);
  // Example output: Delete by ID result: { deletedCount: 3, success: true }

  // Delete by metadata filter
  const deleteByFilterResult = await client.deleteByFilter({
    filter: { category: { $eq: "example" } },
  });
  console.log("Delete by filter result:", deleteByFilterResult);
  // Example output: Delete by filter result: { deletedCount: 47, success: true }

  // Get database statistics
  const stats = await client.getIndexStats();
  console.log(
    `Database has ${stats.vectors} vectors with ${stats.dimensions} dimensions`
  );
}
```

### Python

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

## Key API Methods

- **connect**: Initialize the database with configuration
- **insert**: Add vectors with metadata
- **search**: Find similar vectors using cosine similarity
- **deleteByIds**: Remove vectors by their IDs
- **deleteByFilter**: Remove vectors that match specific filter criteria
- **getIndexStats**: Returns database statistics (vector count and dimensions)
- **updateById**: Update existing vectors with new metadata and/or vector data

## MongoDB-like Filtering

TinyVecDB supports MongoDB-like query syntax for filtering vectors based on their metadata. These filters can be used with both the `search` and `deleteByFilter` methods.

### Available Filter Operators

- `$eq`: Matches values equal to a specified value
- `$gt`: Matches values greater than a specified value
- `$gte`: Matches values greater than or equal to a specified value
- `$in`: Matches any values specified in an array
- `$lt`: Matches values less than a specified value
- `$lte`: Matches values less than or equal to a specified value
- `$ne`: Matches values not equal to a specified value
- `$nin`: Matches none of the values specified in an array

Filters can be nested for complex queries.

### Filtering Examples

```typescript
// Simple filter - find vectors with metadata where category equals "electronics"
const searchOptions: TinyVecSearchOptions = {
  filter: { category: { $eq: "electronics" } },
};

// Array inclusion filter - find vectors with any of the specified tags
const inclusionFilter: TinyVecSearchOptions = {
  filter: {
    tags: { $in: ["premium", "featured", "new"] },
  },
};

// Complex nested filter
const complexSearchOptions: TinyVecSearchOptions = {
  filter: {
    category: {
      subcategory: {
        value: { $gt: 200 },
      },
    },
    tags: { $in: ["premium", "featured"] },
  },
};
```

### Using Filters with Search

```typescript
// Create a query vector
const queryVector = new Float32Array(512);
for (let i = 0; i < 512; i++) {
  queryVector[i] = Math.random();
}

// Search with filter
const searchOptions: TinyVecSearchOptions = {
  filter: { category: { $eq: "electronics" } },
};

const filteredResults = await client.search(queryVector, 10, searchOptions);
console.log("Found items in electronics category:", filteredResults.length);
```

### Using Filters with Deletion

```typescript
// Delete all vectors with a specific category
const deleteOptions: TinyVecSearchOptions = {
  filter: { category: { $eq: "outdated" } },
};

const deleteResult = await client.deleteByFilter(deleteOptions);
console.log(`Cleaned up ${deleteResult.deletedCount} outdated items`);

// Delete vectors with low ratings
const lowRatingOptions: TinyVecSearchOptions = {
  filter: { rating: { $lt: 3.0 } },
};

const cleanupResult = await client.deleteByFilter(lowRatingOptions);
console.log(`Removed ${cleanupResult.deletedCount} low-rated items`);
```

## Vector Input Formats

- **Python**: Both NumPy arrays and Python lists are accepted, but all vectors are converted to NumPy float32 arrays before insertion for optimal performance.
- **Node.js**: Regular arrays and TypedArrays (Float32Array, etc.) are accepted, but all vectors are converted to Float32Array before insertion.

## Metadata Flexibility

- **Flexible metadata**: You can include any serializable data in the metadata field, including deeply nested objects, arrays, primitive types, and complex data structures.
- **Use cases**: Ideal for storing document IDs, timestamps, text snippets, tags, category hierarchies, or any contextual information you need to retrieve with your vectors.
- **Size considerations**: While there's no strict size limit on metadata, keeping metadata compact will improve performance for large datasets.

## Database Initialization

- **Automatic file creation**: TinyVecDB automatically creates all necessary storage files when connecting if they don't exist.
- **Dynamic dimension detection**: If `dimensions` is not specified in the configuration, TinyVecDB will automatically use the length of the first inserted vector to update the database.
- **Flexible configuration**: The config object is optional. When omitted, TinyVecDB will determine appropriate settings based on your first vector insertion while ensuring consistency for subsequent operations.

## 📚 Language Bindings

- [Node.js/JavaScript](./bindings/nodejs)
- [Python](./bindings/python)

## 🔧 Configuration Options

| Option     | Description           | Default                              |
| ---------- | --------------------- | ------------------------------------ |
| path       | Path to database file | required                             |
| dimensions | Dimension of vectors  | optional (derived from first vector) |

## 📋 Use Cases

| Good Use Cases                           | Less Ideal Use Cases                                |
| ---------------------------------------- | --------------------------------------------------- |
| ✅ Local semantic search in desktop apps | ❌ Very complex multi-join query requireme          |
| ✅ Rapid prototyping and MVPs            | ❌ Systems needing extensive admin tools/monitoring |
| ✅ Self-contained applications           | ❌ Cloud-native distributed architectures           |
| ✅ Resource-constrained environments     | ❌ Applications requiring real-time replication     |

## 🧪 Additional Performance Notes

- **C Core Engine**: Built with a native C core for maximum efficiency
- Minimal memory footprint
- Optimized similarity calculations with CPU-specific instructions
- Fast insert and query operations

## 📖 Documentation

- [Python binding documentation](https://github.com/tylerpuig/tinyvec/tree/main/bindings/python/README.md)
- [Node.js/JavaScript binding documentation](https://github.com/tylerpuig/tinyvec/tree/main/bindings/nodejs/README.md)

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

For more details on how to contribute to this project, please read our [CONTRIBUTING.md](CONTRIBUTING.md) guide.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
