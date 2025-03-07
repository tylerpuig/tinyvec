import { type TinyVecInsertion } from "../src/index";
import fs from "fs/promises";
import path from "path";
import * as testUtils from "./utils";

const DIMENSIONS = 128;
describe("TinyVecClient Insert", () => {
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

  test("should insert a single vector successfully", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const insertion: TinyVecInsertion = {
      vector: testUtils.generateRandomVector(128),
      metadata: { id: 1 },
    };

    const inserted = await newClient.insert([insertion]);
    expect(inserted).toBe(1);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(1);
    expect(stats.dimensions).toBe(128);
  });

  test("should insert multiple vectors successfully", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const INSERTION_COUNT = 10;
    const insertions: TinyVecInsertion[] = [];

    for (let i = 0; i < INSERTION_COUNT; i++) {
      insertions.push({
        vector: testUtils.generateRandomVector(128),
        metadata: { id: i },
      });
    }

    const inserted = await newClient.insert(insertions);
    expect(inserted).toBe(INSERTION_COUNT);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(INSERTION_COUNT);
    expect(stats.dimensions).toBe(128);
  });

  test("should handle batch inserts with varying metadata", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const insertions: TinyVecInsertion[] = [
      {
        vector: testUtils.generateRandomVector(128),
        metadata: { id: 1, type: "text", content: "hello" },
      },
      {
        vector: testUtils.generateRandomVector(128),
        metadata: { id: 2, type: "image", size: "large" },
      },
      {
        vector: testUtils.generateRandomVector(128),
        metadata: { id: 3, type: "audio", duration: 120 },
      },
    ];

    const inserted = await newClient.insert(insertions);
    expect(inserted).toBe(3);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(3);
  });

  test("should convert number arrays to Float32 arrays", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const insertions: TinyVecInsertion[] = [
      {
        vector: [...testUtils.generateRandomVector(128)],
        metadata: { id: 1, type: "text", content: "hello" },
      },
      {
        vector: [...testUtils.generateRandomVector(128)],
        metadata: { id: 2, type: "image", size: "large" },
      },
      {
        vector: [...testUtils.generateRandomVector(128)],
        metadata: { id: 3, type: "audio", duration: 120 },
      },
    ];

    const inserted = await newClient.insert(insertions);
    expect(inserted).toBe(3);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(3);
  });

  test("should handle empty insertion array", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const inserted = await newClient.insert([]);
    expect(inserted).toBe(0);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(0);
  });

  test("should maintain consistency after multiple insert operations", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    // First batch
    const firstBatch: TinyVecInsertion[] = Array(5)
      .fill(null)
      .map((_, i) => ({
        vector: testUtils.generateRandomVector(128),
        metadata: { id: i, batch: 1 },
      }));

    // Second batch
    const secondBatch: TinyVecInsertion[] = Array(3)
      .fill(null)
      .map((_, i) => ({
        vector: testUtils.generateRandomVector(128),
        metadata: { id: i + 5, batch: 2 },
      }));

    await newClient.insert(firstBatch);
    await newClient.insert(secondBatch);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(8); // 5 + 3
    expect(stats.dimensions).toBe(128);
  });

  test("should throw an error if insertions parameter is falsy value", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    // @ts-ignore
    const inserted = await newClient.insert(null);
    expect(inserted).toBe(0);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(0);
    expect(stats.dimensions).toBe(128);
  });

  test("should return 0 if insertions parameter is empty array", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const inserted = await newClient.insert([]);
    expect(inserted).toBe(0);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(0);
    expect(stats.dimensions).toBe(128);
  });
});
