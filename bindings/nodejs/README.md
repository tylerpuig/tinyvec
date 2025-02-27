# TinyVecDB Node.js API Documentation

This document provides a comprehensive overview of the TinyVecDB Node.js API.

## Table of Contents

- [Installation](#installation)
- [Core Concepts](#core-concepts)
- [Basic Usage](#basic-usage)
- [API Reference](#api-reference)
  - [TinyVecClient](#tinyvecclient)

## Installation

```bash
npm install tinyvecdb
```

## Core Concepts

TinyVecDB is an embedded vector database that emphasizes speed, low memory usage, and simplicity. The core of TinyVecDB is written in C, and this library provides a Node.js binding to that engine. The key concepts are:

- **Embeddings**: Fixed-dimension float vectors (e.g., 512 dimensions)
- **Metadata**: JSON-serializable data associated with each vector
- **Similarity Search**: Finding the nearest neighbors to a query vector using cosine similarity

## Basic Usage

```typescript
import { TinyVecClient, type TinyVecInsertion } from "tinyvecdb";

async function example() {
  // Connect to database (will create the file if it doesn't exist)
  const client = TinyVecClient.connect("./vectors.db", { dimensions: 512 });

  // Create a sample vector
  const vector = new Float32Array(512);
  for (let i = 0; i < 512; i++) {
    vector[i] = Math.random();
  }

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

  // Search for similar vectors
  const results = await client.search(vector, 5);

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
- `config?`: `TinyVecConfig`(optional) - Configuration options

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
    metadata: { id: "doc1", title: "Example Document" },
  },
]);
```

##### `async search(queryVector, topK)`

Searches for the most similar vectors to the query vector.

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

**Returns:**

- `Promise<`TinyVecResult[]`>` - Array of search results

**Example:**

```typescript
const results = await client.search(queryVector, 10);
```

##### `async getIndexStats()`

Retrieves statistics about the database.

**Parameters:**

- None

**Returns:**

- `Promise<`TinyVecStats`>` - Database statistics

**Example:**

```typescript
const stats = await client.getIndexStats();
console.log(
  `Database has ${stats.vectors} vectors with ${stats.dimensions} dimensions`
);
```
