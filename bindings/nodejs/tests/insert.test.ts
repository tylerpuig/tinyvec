import TinyVecClient, { type TinyVecInsertion } from "../src/index";
import fs from "fs/promises";
import path from "path";
import os from "os";

describe("TinyVecClient Insert", () => {
  let tempDir: string;
  let dbPath: string;
  let client: TinyVecClient | null;

  function generateRandomVector(dimensions: number) {
    const vector = new Float32Array(dimensions);
    for (let i = 0; i < dimensions; i++) {
      vector[i] = Math.random();
    }
    return vector;
  }

  beforeEach(async () => {
    tempDir = await fs.mkdtemp(path.join(os.tmpdir(), "tinyvec-test-"));
    dbPath = path.join(tempDir, "test.db");
    client = await TinyVecClient.connect(dbPath, { dimensions: 128 });
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

  test("should insert a single vector successfully", async () => {
    const insertion: TinyVecInsertion = {
      vector: generateRandomVector(128),
      metadata: { id: 1 },
    };

    const inserted = await client!.insert([insertion]);
    expect(inserted).toBe(1);

    const stats = await client!.getIndexStats();
    expect(stats.vectors).toBe(1);
    expect(stats.dimensions).toBe(128);
  });

  test("should insert multiple vectors successfully", async () => {
    const INSERTION_COUNT = 10;
    const insertions: TinyVecInsertion[] = [];

    for (let i = 0; i < INSERTION_COUNT; i++) {
      insertions.push({
        vector: generateRandomVector(128),
        metadata: { id: i },
      });
    }

    const inserted = await client!.insert(insertions);
    expect(inserted).toBe(INSERTION_COUNT);

    const stats = await client!.getIndexStats();
    expect(stats.vectors).toBe(INSERTION_COUNT);
    expect(stats.dimensions).toBe(128);
  });

  test("should handle batch inserts with varying metadata", async () => {
    const insertions: TinyVecInsertion[] = [
      {
        vector: generateRandomVector(128),
        metadata: { id: 1, type: "text", content: "hello" },
      },
      {
        vector: generateRandomVector(128),
        metadata: { id: 2, type: "image", size: "large" },
      },
      {
        vector: generateRandomVector(128),
        metadata: { id: 3, type: "audio", duration: 120 },
      },
    ];

    const inserted = await client!.insert(insertions);
    expect(inserted).toBe(3);

    const stats = await client!.getIndexStats();
    expect(stats.vectors).toBe(3);
  });

  test("should handle empty insertion array", async () => {
    const inserted = await client!.insert([]);
    expect(inserted).toBe(0);

    const stats = await client!.getIndexStats();
    expect(stats.vectors).toBe(0);
  });

  test("should maintain consistency after multiple insert operations", async () => {
    // First batch
    const firstBatch: TinyVecInsertion[] = Array(5)
      .fill(null)
      .map((_, i) => ({
        vector: generateRandomVector(128),
        metadata: { id: i, batch: 1 },
      }));

    // Second batch
    const secondBatch: TinyVecInsertion[] = Array(3)
      .fill(null)
      .map((_, i) => ({
        vector: generateRandomVector(128),
        metadata: { id: i + 5, batch: 2 },
      }));

    await client!.insert(firstBatch);
    await client!.insert(secondBatch);

    const stats = await client!.getIndexStats();
    expect(stats.vectors).toBe(8); // 5 + 3
    expect(stats.dimensions).toBe(128);
  });
});
