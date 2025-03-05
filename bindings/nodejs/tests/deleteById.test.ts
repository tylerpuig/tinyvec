import { TinyVecClient, type TinyVecInsertion } from "../src/index";
import fs from "fs/promises";
import path from "path";
import * as testUtils from "./utils";

const DIMENSIONS = 128;
const INSERTIONS = 10;

type DeleteByIdTestItem = {
  text: string;
  category: string;
};

async function insertTestVectors(client: TinyVecClient) {
  const testData: DeleteByIdTestItem[] = [];

  for (let i = 0; i < INSERTIONS; i++) {
    testData.push({
      text: `document_${i}`,
      category: i % 2 === 0 ? "even" : "odd",
    });
  }

  const insertions: TinyVecInsertion[] = testData.map((item) => ({
    vector: testUtils.generateRandomVector(DIMENSIONS),
    metadata: item,
  }));

  const insertCount = await client.insert(insertions);
  return insertCount;
}

describe("TinyVecClient Delete By Id", () => {
  let tempDir: string;

  beforeEach(async () => {
    // Create main temp directory if it doesn't exist
    await fs.mkdir("temp").catch(() => {});

    // Create a temporary directory for each test inside 'temp'
    tempDir = path.join("temp", `test-${Date.now()}`);
    await fs.mkdir(tempDir);
  });

  // Basic exact match filter test
  test("remove single vector by id", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);

    const initialStats = await newClient.getIndexStats();
    expect(initialStats.vectors).toBe(INSERTIONS);
    expect(initialStats.dimensions).toBe(DIMENSIONS);

    const searchResult = await newClient.search<DeleteByIdTestItem>(
      testUtils.generateRandomVector(DIMENSIONS),
      1,
      {
        filter: {
          category: { $eq: "even" },
        },
      }
    );

    expect(searchResult).toHaveLength(1);
    expect(searchResult[0]?.metadata.category).toBe("even");

    const itemId = searchResult[0].index;

    const deleteByIdResult = await newClient.deleteByIds([itemId]);

    expect(deleteByIdResult.deletedCount).toBe(1);
    expect(deleteByIdResult.success).toBe(true);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(INSERTIONS - 1);
    expect(stats.dimensions).toBe(DIMENSIONS);
  });

  test("remove multiple vectors by id", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);

    const initialStats = await newClient.getIndexStats();
    expect(initialStats.vectors).toBe(INSERTIONS);
    expect(initialStats.dimensions).toBe(DIMENSIONS);

    const searchResult = await newClient.search<DeleteByIdTestItem>(
      testUtils.generateRandomVector(DIMENSIONS),
      4,
      {
        filter: {
          category: { $eq: "even" },
        },
      }
    );

    expect(searchResult.length).toBeGreaterThan(0);
    expect(searchResult.every((r) => r.metadata.category === "even")).toBe(
      true
    );

    const itemIds = searchResult.map((r) => r.index);

    const deleteByIdResult = await newClient.deleteByIds(itemIds);

    expect(deleteByIdResult.deletedCount).toBe(itemIds.length);
    expect(deleteByIdResult.success).toBe(true);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(INSERTIONS - itemIds.length);
    expect(stats.dimensions).toBe(DIMENSIONS);
  });

  test("properly handle empty array of IDs", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);

    const initialStats = await newClient.getIndexStats();
    expect(initialStats.vectors).toBe(INSERTIONS);
    expect(initialStats.dimensions).toBe(DIMENSIONS);

    const deleteByIdResult = await newClient.deleteByIds([]);

    expect(deleteByIdResult.deletedCount).toBe(0);
    expect(deleteByIdResult.success).toBe(false);

    const stats = await newClient.getIndexStats();
    expect(stats.vectors).toBe(INSERTIONS);
    expect(stats.dimensions).toBe(DIMENSIONS);
  });
});
