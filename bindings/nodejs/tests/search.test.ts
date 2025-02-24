import TinyVecClient from "../src/index";
import type { TinyVecInsertion } from "../src/types";
import fs from "fs/promises";
import path from "path";
import { generateRandomVector } from "./utils";

describe("TinyVecClient Search", () => {
  let tempDir: string;
  let dbPath: string;
  let client: TinyVecClient | null;
  const DIMENSIONS = 128;

  function normalizeVector(vector: Float32Array) {
    const norm = Math.sqrt(vector.reduce((acc, val) => acc + val * val, 0));
    const epsilon = 1e-12;
    for (let i = 0; i < vector.length; i++) {
      vector[i] = vector[i] / (norm + epsilon);
    }
    return vector;
  }

  // Helper to create a vector with a specific pattern
  function createVector(baseValue: number): Float32Array {
    const vector = new Float32Array(DIMENSIONS);
    for (let i = 0; i < DIMENSIONS; i++) {
      // Create slightly different values based on baseValue
      vector[i] = baseValue + i * 0.01;
    }
    return normalizeVector(vector);
  }

  // Helper to prepare test data
  async function insertTestVectors() {
    const insertions: TinyVecInsertion[] = Array(10)
      .fill(null)
      .map((_, i) => ({
        vector: createVector(i),
        metadata: {
          id: i,
          text: `document_${i}`,
          category: i % 2 === 0 ? "even" : "odd",
        },
      }));

    await client!.insert(insertions);
    return insertions;
  }

  beforeEach(async () => {
    // Create main temp directory if it doesn't exist
    await fs.mkdir("temp").catch(() => {});

    // Create a temporary directory for each test inside 'temp'
    tempDir = path.join("temp", `test-${Date.now()}`);
    await fs.mkdir(tempDir);

    dbPath = path.join(tempDir, "test.db");
    client = TinyVecClient.connect(dbPath, { dimensions: DIMENSIONS });
  });

  afterEach(async () => {
    await new Promise((resolve) => setTimeout(resolve, 100));
    try {
      await fs.rm(tempDir, { recursive: true, force: true });
    } catch {
      await new Promise((resolve) => setTimeout(resolve, 500));
      await fs.rm(tempDir, { recursive: true, force: true }).catch(() => {});
    }
  });

  test("should find exact matches", async () => {
    const insertions = await insertTestVectors();

    // Search using the exact same vector as document_5
    const searchVector = createVector(5);
    const results = await client!.search<(typeof insertions)[0]["metadata"]>(
      searchVector,
      1
    );

    expect(results).toHaveLength(1);
    expect(results[0].metadata.id).toBe(5);
    expect(results[0].metadata.text).toBe("document_5");
  });

  test("should return correct number of results with topK", async () => {
    await insertTestVectors();

    const indexstats = await client!.getIndexStats();
    expect(indexstats.vectors).toBe(10);
    expect(indexstats.dimensions).toBe(128);

    const searchVector = createVector(3);
    const topK = 5;

    const results = await client!.search(searchVector, topK);

    expect(results).toHaveLength(topK);
  });

  test("should return empty result array if no vectors are found without dimensions in config or insertions", async () => {
    const randInt = Math.floor(Math.random() * 1000);
    const newClient = TinyVecClient.connect(dbPath + randInt.toString());
    const indexStats = await newClient.getIndexStats();
    expect(indexStats.vectors).toBe(0);
    expect(indexStats.dimensions).toBe(0);

    const searchVector = createVector(3);
    const topK = 5;

    const results = await newClient.search(searchVector, topK);

    expect(results).toHaveLength(0);
  });

  test("should throw an error if topK is less than or equal to 0", async () => {
    await insertTestVectors();
    const indexStats = await client!.getIndexStats();
    expect(indexStats.vectors).toBe(10);
    expect(indexStats.dimensions).toBe(128);

    const searchVector = createVector(3);
    const topK = -5;

    await expect(client!.search(searchVector, topK)).rejects.toThrow(
      "Top K must be a positive integer."
    );
  });

  test("should throw an error if topK is not a number", async () => {
    await insertTestVectors();
    const indexStats = await client!.getIndexStats();
    expect(indexStats.vectors).toBe(10);
    expect(indexStats.dimensions).toBe(128);

    const searchVector = createVector(3);
    const topK = "5";

    // @ts-ignore
    await expect(client!.search(searchVector, topK)).rejects.toThrow(
      "Top K must be a positive integer."
    );
  });

  test("should throw an error if search vector is not valid numeric array type", async () => {
    await insertTestVectors();
    const indexStats = await client!.getIndexStats();
    expect(indexStats.vectors).toBe(10);
    expect(indexStats.dimensions).toBe(128);

    const searchVector = {};
    const topK = 5;

    // @ts-ignore
    await expect(client!.search(searchVector, topK)).rejects.toThrow(
      "Vector conversion failed: Unsupported array type"
    );
  });

  test("should return results in order of similarity", async () => {
    await insertTestVectors();

    const searchVector = createVector(2);
    const results = await client!.search(searchVector, 3);

    // The closest vectors should be 2, then 1 or 3
    expect(results[0].metadata.id).toBe(2);

    // Check scores are in descending order (higher is more similar)
    const scores = results.map((r) => r.score);
    expect(scores).toEqual([...scores].sort((a, b) => b - a));
  });

  test("should handle search with no similar results", async () => {
    await insertTestVectors();

    // Create a very different vector
    const searchVector = new Float32Array(DIMENSIONS).fill(999);
    const results = await client!.search(searchVector, 5);

    // Should still return results, but with lower similarity scores
    expect(results).toHaveLength(5);
  });

  test("should convert an array to a Float32Array", async () => {
    await insertTestVectors();

    // Create a very different vector
    const searchVector = [...generateRandomVector(DIMENSIONS)];

    const results = await client!.search(searchVector, 5);
    expect(results).toBeTruthy();
    expect(results).toHaveLength(5);
  });

  test("should preserve metadata types in search results", async () => {
    const insertions: TinyVecInsertion[] = [
      {
        vector: createVector(0),
        metadata: {
          id: 1,
          numeric: 42,
          string: "test",
          boolean: true,
          nested: { key: "value" },
          array: [1, 2, 3],
        },
      },
    ];

    await client!.insert(insertions);

    const results = await client!.search<(typeof insertions)[0]["metadata"]>(
      createVector(0),
      1
    );

    expect(results[0].metadata).toEqual(insertions[0].metadata);
    expect(typeof results[0].metadata.numeric).toBe("number");
    expect(typeof results[0].metadata.string).toBe("string");
    expect(typeof results[0].metadata.boolean).toBe("boolean");
    expect(Array.isArray(results[0].metadata.array)).toBe(true);
  });

  test("should handle multiple searches with different topK", async () => {
    await insertTestVectors();
    const searchVector = createVector(5);

    const results1 = await client!.search(searchVector, 3);
    const results2 = await client!.search(searchVector, 7);

    expect(results1).toHaveLength(3);
    expect(results2).toHaveLength(7);

    // First 3 results should be identical in both searches
    expect(results1.map((r) => r.metadata.id)).toEqual(
      results2.slice(0, 3).map((r) => r.metadata.id)
    );
  });
});
