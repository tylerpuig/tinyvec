import { TinyVecClient } from "../src/index";
import type { TinyVecInsertion } from "../src/types";
import fs from "fs/promises";
import path from "path";
import * as testUtils from "./utils";

const DIMENSIONS = 128;

describe("TinyVecClient Connect", () => {
  let tempDir: string;
  let dbPath: string;

  beforeEach(async () => {
    const hash = testUtils.createRandomMD5Hash();
    tempDir = path.join("temp", hash);
    await fs.mkdir(tempDir);
  });

  beforeAll(async () => {
    await fs.mkdir("temp").catch(() => {});
  });

  const readInitialHeader = async (filePath: string) => {
    let fileHandle;
    try {
      fileHandle = await fs.open(filePath, "r");
      const buffer = Buffer.alloc(8);
      await fileHandle.read(buffer, 0, 8, 0);
      return {
        vectorCount: buffer.readInt32LE(0),
        dimensions: buffer.readInt32LE(4),
      };
    } finally {
      await fileHandle?.close();
    }
  };

  test("should create new database files when they don't exist", async () => {
    const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    const indexStats = await client.getIndexStats();
    expect(indexStats.vectors).toBe(0);
    expect(indexStats.dimensions).toBe(128);

    expect(client).toBeInstanceOf(TinyVecClient);
  });

  test("should initialize header with correct values", async () => {
    const dbPath = testUtils.getRandDbPath(tempDir);
    const client = TinyVecClient.connect(dbPath, { dimensions: 128 });

    const header = await readInitialHeader(dbPath);
    expect(header.vectorCount).toBe(0);
    expect(header.dimensions).toBe(128);

    const indexStats = await client.getIndexStats();
    expect(indexStats.vectors).toBe(0);
    expect(indexStats.dimensions).toBe(128);
  });

  test("should handle missing dimensions in config", async () => {
    const newDbPath = tempDir + Date.now();
    const newClient = TinyVecClient.connect(newDbPath);

    const header = await readInitialHeader(newDbPath);
    expect(header.vectorCount).toBe(0);
    expect(header.dimensions).toBe(0);
  });

  test("should handle missing dimensions in config and update header file dimensions on first insert", async () => {
    const dbPath = testUtils.getRandDbPath(tempDir);
    const client = TinyVecClient.connect(dbPath, { dimensions: DIMENSIONS });

    const indexStats = await client.getIndexStats();
    expect(indexStats.vectors).toBe(0);
    expect(indexStats.dimensions).toBe(DIMENSIONS);

    const header = await readInitialHeader(dbPath);
    expect(header.vectorCount).toBe(0);
    expect(header.dimensions).toBe(DIMENSIONS);

    const insertions: TinyVecInsertion[] = [
      {
        vector: testUtils.generateRandomVector(128),
        metadata: { id: 1 },
      },
    ];

    const inserted = await client.insert(insertions);
    expect(inserted).toBe(1);

    const updatedHeader = await readInitialHeader(dbPath);
    expect(updatedHeader.vectorCount).toBe(1);
    expect(updatedHeader.dimensions).toBe(128);

    const stats = await client.getIndexStats();
    expect(stats.vectors).toBe(1);
    expect(stats.dimensions).toBe(128);
  });

  test("should not overwrite existing database", async () => {
    // Create first instance
    const dbPath = testUtils.getRandDbPath(tempDir);
    const client = TinyVecClient.connect(dbPath, { dimensions: 128 });

    // Modify the header to check if it persists
    let fileHandle;
    try {
      fileHandle = await fs.open(dbPath, "r+");
      const buffer = Buffer.alloc(8);
      buffer.writeInt32LE(42, 0); // Set vector count to 42
      buffer.writeInt32LE(128, 4); // Keep dimensions at 128
      await fileHandle.write(buffer, 0, 8, 0);
    } finally {
      await fileHandle?.close();
    }

    // Connect again
    const newClient = TinyVecClient.connect(dbPath, { dimensions: 256 });

    const header = await readInitialHeader(dbPath);
    expect(header.vectorCount).toBe(42);
    expect(header.dimensions).toBe(128);
  });

  test("should throw an error if dimensions is not a number", async () => {
    const incorrectConfig = { dimensions: "128" };
    const dbPath = testUtils.getRandDbPath(tempDir);
    expect(() =>
      // @ts-ignore
      TinyVecClient.connect(dbPath, incorrectConfig)
    ).toThrow("Dimensions must be a positive number.");

    const nullDimensionsConfig = { dimensions: null };
    expect(() =>
      // @ts-ignore
      TinyVecClient.connect(dbPath, nullDimensionsConfig)
    ).toThrow("Dimensions must be a positive number.");
  });
});
