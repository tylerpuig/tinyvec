import { TinyVecClient } from "../src/index";
import type { TinyVecInsertion } from "../src/types";
import fs from "fs/promises";
import path from "path";
import { generateRandomVector } from "./utils";

describe("TinyVecClient Connect", () => {
  let tempDir: string;
  let dbPath: string;
  let client: TinyVecClient | null;

  beforeEach(async () => {
    // Create main temp directory if it doesn't exist
    await fs.mkdir("temp").catch(() => {});

    // Create a temporary directory for each test inside 'temp'
    tempDir = path.join("temp", `test-${Date.now()}`);
    await fs.mkdir(tempDir);

    dbPath = path.join(tempDir, "test.db");
    client = null;
  });

  afterEach(async () => {
    // Add delay to ensure all file operations are complete
    await new Promise((resolve) => setTimeout(resolve, 100));

    try {
      // Clean up the temporary directory and all files after each test
      await fs.rm(tempDir, { recursive: true, force: true });
    } catch (error) {
      // console.warn("Cleanup failed:", error);
      // If immediate cleanup fails, retry after a delay
      await new Promise((resolve) => setTimeout(resolve, 500));
      await fs.rm(tempDir, { recursive: true, force: true }).catch(() => {});
    }
  });

  afterAll(async () => {
    // Add delay to ensure all file operations are complete
    await new Promise((resolve) => setTimeout(resolve, 100));

    try {
      // Clean up the entire temp directory after all tests complete
      await fs.rm("temp", { recursive: true, force: true });
    } catch (error) {
      // If immediate cleanup fails, retry after a delay
      await new Promise((resolve) => setTimeout(resolve, 500));
      await fs.rm("temp", { recursive: true, force: true }).catch(() => {
        console.warn("Failed to clean up temp directory:", error);
      });
    }
  });

  const checkFilesExist = async (basePath: string) => {
    const files = await Promise.all([
      fs
        .access(`${basePath}.idx`)
        .then(() => true)
        .catch(() => false),
      fs
        .access(`${basePath}.meta`)
        .then(() => true)
        .catch(() => false),
    ]);
    return files;
  };

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
    client = TinyVecClient.connect(dbPath, { dimensions: 128 });

    const indexStats = await client.getIndexStats();
    expect(indexStats.vectors).toBe(0);
    expect(indexStats.dimensions).toBe(128);

    const [idxExists, metaExists] = await checkFilesExist(dbPath);

    expect(idxExists).toBe(true);
    expect(metaExists).toBe(true);
    expect(client).toBeInstanceOf(TinyVecClient);
  });

  test("should initialize header with correct values", async () => {
    client = TinyVecClient.connect(dbPath, { dimensions: 128 });

    const header = await readInitialHeader(dbPath);
    expect(header.vectorCount).toBe(0);
    expect(header.dimensions).toBe(128);

    const indexStats = await client.getIndexStats();
    expect(indexStats.vectors).toBe(0);
    expect(indexStats.dimensions).toBe(128);
  });

  test("should handle missing dimensions in config", async () => {
    client = TinyVecClient.connect(dbPath);

    const header = await readInitialHeader(dbPath);
    expect(header.vectorCount).toBe(0);
    expect(header.dimensions).toBe(0);
  });

  test("should handle missing dimensions in config and update header file dimensions on first insert", async () => {
    client = TinyVecClient.connect(dbPath);

    const indexStats = await client.getIndexStats();
    expect(indexStats.vectors).toBe(0);
    expect(indexStats.dimensions).toBe(0);

    const header = await readInitialHeader(dbPath);
    expect(header.vectorCount).toBe(0);
    expect(header.dimensions).toBe(0);

    const insertions: TinyVecInsertion[] = [
      {
        vector: generateRandomVector(128),
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

  test("should convert relative path to absolute", async () => {
    const relativePath = "./test.db";
    const absolutePath = path.resolve(process.cwd(), relativePath);

    try {
      client = TinyVecClient.connect(relativePath, { dimensions: 128 });
      const exists = await fs
        .access(absolutePath)
        .then(() => true)
        .catch(() => false);

      expect(exists).toBe(true);
    } finally {
      // Clean up the files in current working directory
      await Promise.all([
        fs.unlink(absolutePath).catch(() => {}),
        fs.unlink(`${absolutePath}.idx`).catch(() => {}),
        fs.unlink(`${absolutePath}.meta`).catch(() => {}),
      ]);
    }
  });

  test("should create empty metadata files", async () => {
    client = TinyVecClient.connect(dbPath, { dimensions: 128 });

    const idxStats = await fs.stat(`${dbPath}.idx`);
    const metaStats = await fs.stat(`${dbPath}.meta`);

    expect(idxStats.size).toBe(0);
    expect(metaStats.size).toBe(0);
  });

  test("should not overwrite existing database", async () => {
    // Create first instance
    client = TinyVecClient.connect(dbPath, { dimensions: 128 });

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
    client = TinyVecClient.connect(dbPath, { dimensions: 256 });

    const header = await readInitialHeader(dbPath);
    expect(header.vectorCount).toBe(42);
    expect(header.dimensions).toBe(128);
  });

  test("should throw an error if dimensions is not a number", async () => {
    const incorrectConfig = { dimensions: "128" };
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
