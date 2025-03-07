# TinyVecDB Node.js API Documentation

This document provides a comprehensive overview of the TinyVecDB Node.js API.

## Table of Contents

- [Installation](#installation)
- [Core Concepts](#core-concepts)
- [Basic Usage](#basic-usage)
- [API Reference](#api-reference)
  - [TinyVecClient](#tinyvecclient)
  - [Supporting Types](#supporting-types)

## Installation

```bash
npm install tinyvecdb
```

## Core Concepts

TinyVecDB is an embedded vector database that emphasizes speed, low memory usage, and simplicity. The core of TinyVecDB is written in C, and this library provides a Node.js binding to that engine. The key concepts are:

- **Embeddings**: Fixed-dimension float vectors (e.g., 512 dimensions)
- **Metadata**: JSON-serializable data associated with each vector
- **Similarity Search**: Finding the nearest neighbors to a query vector using cosine similarity
- **Filtering**: Query vectors based on metadata attributes

## Basic Usage

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

## API Reference

### TinyVecClient

The main class you'll interact with is `TinyVecClient`. It provides all methods for managing the vector database.

#### Static Methods

##### `TinyVecClient.connect(path, config?)`

Creates and connects to a TinyVecDB database.

**Parameters:**

- `path`: `string` - Path to the database file
- `config?`: `TinyVecConfig` (optional) - Configuration options

**Example:**

```typescript
const client = TinyVecClient.connect("./vectors.db", { dimensions: 512 });
```

#### Instance Methods

##### `async insert(vectors)`

Inserts vectors with metadata into the database. Each metadata item must be a JSON-serializable object.

**Parameters:**

- `vectors`: `TinyVecInsertion[]` - Array of vectors to insert

**Returns:**

- `Promise<number>` - The number of vectors successfully inserted

**Example:**

```typescript
const vector = new Float32Array(512).fill(0.1);
const count = await client.insert([
  {
    vector,
    metadata: {
      documentId: "doc1",
      title: "Example Document",
      category: "reference",
    },
  },
]);
```

##### `async search(queryVector, topK, searchOptions?)`

Searches for the most similar vectors to the query vector, with optional filtering.

**Parameters:**

- `queryVector`: `NumericArray`  
  A query vector to search for, which can be any of the following types:
  ```ts
  type NumericArray =
    | number[]
    | Float32Array
    | Float64Array
    | Int8Array
    | Int16Array
    | Int32Array
    | Uint8Array
    | Uint16Array
    | Uint32Array;
  ```
  Internally, it will be converted to a Float32Array for similarity calculations.
- `topK`: `number` - Number of results to return
- `searchOptions?`: `TinyVecSearchOptions` (optional) - Contains filter criteria for the search

**Returns:**

- `Promise<TinyVecResult[]>` - Array of search results

**Example:**

```typescript
// Search without filtering
const results = await client.search(queryVector, 10);

// Search with filtering
const searchOptions: TinyVecSearchOptions = {
  filter: { year: { $eq: 2024 } },
};
const filteredResults = await client.search(queryVector, 10, searchOptions);
```

##### `async deleteByIds(ids)`

Deletes vectors by their IDs.

**Parameters:**

- `ids`: `number[]` - Array of vector IDs to delete

**Returns:**

- `Promise<TinyVecDeletionResult>` - Object containing deletion count and success status

**Example:**

```typescript
const result = await client.deleteByIds([1, 2, 3]);
console.log(
  `Deleted ${result.deletedCount} vectors. Success: ${result.success}`
);
// Example output: Deleted 3 vectors. Success: true
```

##### `async deleteByFilter(searchOptions)`

Deletes vectors that match the given filter criteria.

**Parameters:**

- `searchOptions`: `TinyVecSearchOptions` - Contains filter criteria for deletion

**Returns:**

- `Promise<TinyVecDeletionResult>` - Object containing deletion count and success status

**Example:**

```typescript
const searchOptions: TinyVecSearchOptions = {
  filter: { year: { $eq: 2024 } },
};
const result = await client.deleteByFilter(searchOptions);
console.log(
  `Deleted ${result.deletedCount} vectors. Success: ${result.success}`
);
```

##### `async getIndexStats()`

Retrieves statistics about the database.

**Parameters:**

- None

**Returns:**

- `Promise<TinyVecStats>` - Database statistics

**Example:**

```typescript
const stats = await client.getIndexStats();
console.log(
  `Database has ${stats.vectors} vectors with ${stats.dimensions} dimensions`
);
```

### Supporting Types

#### `TinyVecDeletionResult`

Result from delete operations.

**Properties:**

- `deletedCount`: `number` - Number of vectors deleted
- `success`: `boolean` - Whether the operation was successful

**Example:**

```typescript
const result = await client.deleteByIds([1, 2, 3]);
if (result.success) {
  console.log(`Successfully deleted ${result.deletedCount} vectors`);
} else {
  console.log("Deletion operation failed");
}
```

#### `TinyVecConfig`

Configuration for the vector database.

**Properties:**

- `dimensions`: `number` - The dimensionality of vectors to be stored

**Example:**

```typescript
const config: TinyVecConfig = { dimensions: 512 };
```

#### `TinyVecInsertion`

Interface representing a vector to be inserted.

**Properties:**

- `vector`: `NumericArray` - The vector data
- `metadata`: `Record<string, any>` - JSON-serializable metadata associated with the vector

**Example:**

```typescript
const insertion: TinyVecInsertion = {
  vector: new Float32Array(512).fill(0.1),
  metadata: { category: "example" },
};
```

#### `TinyVecSearchOptions`

Options for search queries, including filtering.

**Properties:**

- `filter`: `object` - Filter criteria in MongoDB-like query syntax

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

```typescript
// Simple filter
const searchOptions: TinyVecSearchOptions = {
  filter: { make: { $eq: "Toyota" } },
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

#### `TinyVecResult`

Interface representing a search result.

**Properties:**

- `similarity`: `number` - Cosine similarity score
- `index`: `number` - ID of the matched vector
- `metadata`: `Record<string, any> | null` - Metadata associated with the matched vector

**Example:**

```typescript
// Results from a search query
for (const result of results) {
  console.log(
    `ID: ${result.index}, Similarity: ${result.similarity}, Metadata:`,
    result.metadata
  );
}
// Example output:
// ID: 34, Similarity: 0.8109967303276062, Metadata: { category: 'example', name: 'item-34' }
// ID: 46, Similarity: 0.789353609085083, Metadata: { category: 'example', name: 'item-46' }
// ID: 22, Similarity: 0.7870827913284302, Metadata: { category: 'example', name: 'item-22' }
```

#### `TinyVecStats`

Interface representing database statistics.

**Properties:**

- `vectors`: `number` - Number of vectors in the database
- `dimensions`: `number` - Dimensionality of the vectors

**Example:**

```typescript
const stats = await client.getIndexStats();
console.log(`Vector count: ${stats.vectors}, Dimensions: ${stats.dimensions}`);
```
