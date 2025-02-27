# TinyVecDB

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A lightweight, embedded vector database focused on simplicity and speed. TinyVecDB provides vector storage and similarity search without the complexity of client-server architectures.

## 🚀 Features

- **Embedded**: No server setup or maintenance required
- **Simple API**: Get started with just a few lines of code
- **Cross-platform**: Works seamlessly on Windows, macOS, and Linux
- **Lightweight**: Minimal memory footprint and resource usage
- **Offline-capable**: Great for desktop and mobile applications

## 📋 Use Cases

| Good Use Cases                           | Less Ideal Use Cases                                     |
| ---------------------------------------- | -------------------------------------------------------- |
| ✅ Local semantic search in desktop apps | ❌ Complex query filters requiring SQL-like capabilities |
| ✅ Rapid prototyping and MVPs            | ❌ Systems needing extensive admin tools/monitoring      |
| ✅ Self-contained applications           | ❌ Cloud-native distributed architectures                |
| ✅ Resource-constrained environments     | ❌ Applications requiring real-time replication          |

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
  type TinyVecConfig,
} from "tinyvecdb";

async function main() {
  try {
    // Initialize client and configuration
    const config: TinyVecConfig = {
      dimensions: 512,
    };
    const client = TinyVecClient.connect("tinyvec.db", config);

    // Create vector insertions
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
        metadata: { id: i },
      });
    }

    // Insert vectors
    const inserted = await client.insert(insertions);
    console.log("Inserted:", inserted);

    // Query top k similar vectors
    const queryVec = Array.from({ length: 512 }, () => Math.random());
    const results = await client.search(queryVec, 10);
    console.log(results);

    // Get database statistics
    const indexStats = await client.getIndexStats();
    console.log(
      "vectors:",
      indexStats.vectors,
      "dimensions:",
      indexStats.dimensions
    );

    // Example output of search results:
    /*
    [
      {
        index: 8,
        similarity: 0.801587700843811,
        metadata: { id: 8 }
      },
      {
        index: 16,
        similarity: 0.7834401726722717,
        metadata: { id: 16 }
      },
      {
        index: 5,
        similarity: 0.7815409898757935,
        metadata: { id: 5 }
      },
      ...
    ]
    */
  } catch (error) {
    console.error("Error:", error);
  }
}

main();
```

### Python

```python
import asyncio
import random
import numpy as np
from tinyvec import TinyVecClient, TinyVecConfig, TinyVecInsertion

async def main():
    # Initialize client and configuration
    client = TinyVecClient()
    config = TinyVecConfig(dimensions=512)

    # Connect to database
    client.connect("tinyvec.db", config)

    # Create vector insertions
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
    print('Inserted:', inserted)

    # Query top k similar vectors
    query_vec = [random.random() for _ in range(512)]
    results = await client.search(query_vec, 10)
    print(results)

    # Get database statistics
    index_stats = await client.get_index_stats()
    print('vectors:', index_stats.vector_count,
          'dimensions:', index_stats.dimensions)

    # Example output of search results:
    """
    [TinyVecResult(similarity=0.8109967303276062, index=34, metadata={'id': 34}),
     TinyVecResult(similarity=0.789353609085083, index=46, metadata={'id': 46}),
     TinyVecResult(similarity=0.7870827913284302, index=22, metadata={'id': 22}),
     ...]
    """

if __name__ == "__main__":
    asyncio.run(main())
```

## Key API Methods

- **connect**: Initialize the database with configuration
- **insert**: Add vectors with metadata
- **search**: Find similar vectors using cosine similarity
- **getIndexStats**: Returns database statistics (vector count and dimensions)

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

## 🧪 Performance

TinyVecDB is optimized for performance on resource-constrained devices:

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
