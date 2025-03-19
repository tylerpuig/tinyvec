import { type TinyVecInsertion } from "../src/index";
import fs from "fs/promises";
import path from "path";
import * as testUtils from "./utils";

const DIMENSIONS = 128;

describe("TinyVecClient getPaginated method", () => {
  let tempDir: string;
  let dbPath: string;

  beforeAll(async () => {
    await fs.mkdir("temp").catch(() => {});
  });

  beforeEach(async () => {
    const hash = testUtils.createRandomMD5Hash();
    tempDir = path.join("temp", hash);
    await fs.mkdir(tempDir);

    dbPath = path.join(tempDir, "test.db");
  });

  afterEach(async () => {
    await fs.rm(tempDir, { recursive: true, force: true }).catch(() => {});
  });

  test("should return a single entry", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const insertion: TinyVecInsertion = {
      vector: testUtils.generateRandomVector(128),
      metadata: { id: 1, type: "text", content: "hello", paginate: true },
    };

    const inserted = await newClient.insert([insertion]);
    expect(inserted).toBe(1);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(1);
    expect(stats.dimensions).toBe(128);

    const paginatedResults = await newClient.getPaginated({
      skip: 0,
      limit: 1,
    });

    expect(paginatedResults.length).toBe(1);
    expect(paginatedResults[0].id).toBe(1);
    expect(paginatedResults[0].metadata?.type).toBe("text");
    expect(paginatedResults[0].metadata?.content).toBe("hello");
    expect(paginatedResults[0].metadata?.paginate).toBe(true);
  });

  test("should return multiple entries with correct pagination", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const insertions: TinyVecInsertion[] = Array.from(
      { length: 10 },
      (_, i) => ({
        vector: testUtils.generateRandomVector(128),
        metadata: { id: i + 1, type: "text", content: `item ${i + 1}` },
      })
    );

    const inserted = await newClient.insert(insertions);
    expect(inserted).toBe(10);

    // Get first page
    const firstPage = await newClient.getPaginated({
      skip: 0,
      limit: 3,
    });

    expect(firstPage.length).toBe(3);
    expect(firstPage[0].metadata?.id).toBe(1);
    expect(firstPage[1].metadata?.id).toBe(2);
    expect(firstPage[2].metadata?.id).toBe(3);

    // Get second page
    const secondPage = await newClient.getPaginated({
      skip: 3,
      limit: 3,
    });

    expect(secondPage.length).toBe(3);
    expect(secondPage[0].metadata?.id).toBe(4);
    expect(secondPage[1].metadata?.id).toBe(5);
    expect(secondPage[2].metadata?.id).toBe(6);

    // Get last page (with fewer items)
    const lastPage = await newClient.getPaginated({
      skip: 9,
      limit: 3,
    });

    expect(lastPage.length).toBe(1);
    expect(lastPage[0].metadata?.id).toBe(10);
  });

  test("should return empty array when database is empty", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    const paginatedResults = await newClient.getPaginated({
      skip: 0,
      limit: 10,
    });

    expect(paginatedResults).toEqual([]);
  });

  test("should return empty array when skip exceeds vector count", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const insertions: TinyVecInsertion[] = Array.from(
      { length: 5 },
      (_, i) => ({
        vector: testUtils.generateRandomVector(128),
        metadata: { id: i + 1 },
      })
    );

    const inserted = await newClient.insert(insertions);
    expect(inserted).toBe(5);

    const paginatedResults = await newClient.getPaginated({
      skip: 10,
      limit: 5,
    });

    expect(paginatedResults).toEqual([]);
  });

  test("should adjust limit to match available vectors", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const insertions: TinyVecInsertion[] = Array.from(
      { length: 3 },
      (_, i) => ({
        vector: testUtils.generateRandomVector(128),
        metadata: { id: i + 1 },
      })
    );

    const inserted = await newClient.insert(insertions);
    expect(inserted).toBe(3);

    const paginatedResults = await newClient.getPaginated({
      skip: 0,
      limit: 10, // Exceeds available vectors
    });

    expect(paginatedResults.length).toBe(3); // Should adjust to actual count
  });

  test("should handle complex metadata correctly", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const complexMetadata = {
      id: 1,
      type: "document",
      content: "complex example",
      nested: {
        property: "value",
        array: [1, 2, 3],
      },
      tags: ["important", "test"],
    };

    const insertion: TinyVecInsertion = {
      vector: testUtils.generateRandomVector(128),
      metadata: complexMetadata,
    };

    await newClient.insert([insertion]);

    const paginatedResults = await newClient.getPaginated({
      skip: 0,
      limit: 1,
    });

    expect(paginatedResults.length).toBe(1);
    expect(paginatedResults[0].metadata).toEqual(complexMetadata);
  });

  test("should throw error with invalid skip parameter", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    await expect(
      newClient.getPaginated({
        skip: -1,
        limit: 10,
      })
    ).rejects.toThrow("Skip must be a positive number.");
  });

  test("should throw error with invalid limit parameter", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    await expect(
      newClient.getPaginated({
        skip: 0,
        limit: 0,
      })
    ).rejects.toThrow("Limit must be a positive number.");

    await expect(
      newClient.getPaginated({
        skip: 0,
        limit: -5,
      })
    ).rejects.toThrow("Limit must be a positive number.");
  });

  test("should throw error when no options provided", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    // @ts-expect-error Testing runtime behavior with invalid input
    await expect(newClient.getPaginated()).rejects.toThrow(
      "No options provided"
    );

    // @ts-expect-error Testing runtime behavior with invalid input
    await expect(newClient.getPaginated(null)).rejects.toThrow(
      "No options provided"
    );
  });

  test("should handle a large dataset with proper pagination", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const totalItems = 100;
    const pageSize = 25;

    // Create a large batch of insertions
    const insertions: TinyVecInsertion[] = Array.from(
      { length: totalItems },
      (_, i) => ({
        vector: testUtils.generateRandomVector(128),
        metadata: { id: i + 1, content: `Item ${i + 1}` },
      })
    );

    await newClient.insert(insertions);

    // Verify total count
    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(totalItems);

    // Test pagination logic by fetching all pages
    let collectedItems: { id: number; metadata?: { id: number } }[] = [];
    for (let page = 0; page < Math.ceil(totalItems / pageSize); page++) {
      const pageResults = await newClient.getPaginated({
        skip: page * pageSize,
        limit: pageSize,
      });

      expect(pageResults.length).toBe(
        page === Math.floor(totalItems / pageSize)
          ? totalItems % pageSize || pageSize
          : pageSize
      );

      collectedItems = [...collectedItems, ...pageResults];
    }

    // Verify we got all items
    expect(collectedItems.length).toBe(totalItems);

    // Verify the items are in the correct order
    for (let i = 0; i < totalItems; i++) {
      expect(collectedItems[i].metadata?.id).toBe(i + 1);
    }
  });
});
