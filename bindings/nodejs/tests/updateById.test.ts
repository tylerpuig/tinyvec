import { type TinyVecInsertion, type UpdateItem } from "../src/index";
import fs from "fs/promises";
import path from "path";
import * as testUtils from "./utils";

const DIMENSIONS = 128;

describe("TinyVecClient Update By Id Operations", () => {
  let tempDir: string;

  beforeAll(async () => {
    await fs.mkdir("temp").catch(() => {});
  });

  beforeEach(async () => {
    const hash = testUtils.createRandomMD5Hash();
    tempDir = path.join("temp", hash);
    await fs.mkdir(tempDir);
  });

  test("should update a single vector entry with both vector and metadata", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    // Insert some test data
    const insertions: TinyVecInsertion[] = [];
    for (let i = 0; i < 1; i++) {
      insertions.push({
        vector: testUtils.generateRandomVector(DIMENSIONS),
        metadata: {
          originalId: 5,
          name: `Original name`,
          category: "Original category",
          items: [1, 2, 3],
        },
      });
    }

    const inserted = await newClient.insert(insertions);
    expect(inserted).toBe(insertions.length);
    // Arrange
    const idToUpdate = 1;
    const newVector = testUtils.generateRandomVector(DIMENSIONS);
    type NewMetadata = {
      originalId: number;
      name: string;
      category: string;
      items: number[];
    };
    const newMetadata = {
      originalId: 6,
      name: "Updated Item",
      category: "updated",
      items: [4, 5, 6],
    };

    const upsertItems: UpdateItem[] = [
      {
        id: idToUpdate,
        vector: newVector,
        metadata: newMetadata,
      },
    ];

    const updateResult = await newClient.updateById(upsertItems);

    // Assert
    expect(updateResult.success).toBe(true);
    expect(updateResult.updatedCount).toBe(1);

    const queryVector = testUtils.generateRandomVector(DIMENSIONS);
    const searchResults = await newClient.search<NewMetadata>(queryVector, 1);
    const item = searchResults[0];

    // Verify the update
    expect(item).not.toBeNull();
    expect(item.metadata.category).toEqual(newMetadata.category);
    expect(item.metadata.items).toEqual(newMetadata.items);
    expect(item.metadata.name).toEqual(newMetadata.name);
  });

  test("should update multiple vector entries", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const idsToUpdate = [1, 2, 3];

    const itemsToInsert = idsToUpdate.map((id) => ({
      vector: testUtils.generateRandomVector(DIMENSIONS),
      metadata: {
        originalId: id,
        name: `Batch Updated Item ${id}`,
        category: "original-category",
      },
    }));

    const inserted = await newClient.insert(itemsToInsert);
    expect(inserted).toBe(idsToUpdate.length);

    const updateItems: UpdateItem[] = itemsToInsert.map((item, i) => ({
      id: idsToUpdate[i],
      vector: testUtils.generateRandomVector(DIMENSIONS),
      metadata: {
        originalId: item.metadata.originalId,
        name: `Updated Item ${item.metadata.originalId}`,
        category: "updated-category",
      },
    }));

    const updateResults = await newClient.updateById(updateItems);

    expect(updateResults.success).toBe(true);
    expect(updateResults.updatedCount).toBe(idsToUpdate.length);

    // Verify the updates
    const searchResults = await newClient.search(
      testUtils.generateRandomVector(DIMENSIONS),
      3
    );
    expect(searchResults.length).toBe(3);

    for (const id of idsToUpdate) {
      const item = searchResults.find((r) => r.metadata.originalId === id);
      expect(item).toBeTruthy();
      expect(item?.metadata.category).toBe("updated-category");
      expect(item?.metadata.name).toBe(`Updated Item ${id}`);
    }
  });

  test("should update only vector without changing metadata", async () => {
    const idToUpdate = 1;
    const ids = Array.from({ length: 10 }, (_, i) => i + 1);
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    const insertItems: TinyVecInsertion[] = ids.map((id) => ({
      vector: testUtils.generateRandomVector(DIMENSIONS),
      metadata: {
        originalId: id,
        name: `Original Item ${id}`,
        category: "original-category",
      },
    }));

    const insertions = await newClient.insert(insertItems);
    expect(insertions).toBe(10);

    const newVector = testUtils.generateRandomVector(DIMENSIONS);
    const updateItems: UpdateItem[] = [
      {
        id: idToUpdate,
        vector: newVector,
      },
    ];

    const updateResults = await newClient.updateById(updateItems);
    expect(updateResults.success).toBe(true);
    expect(updateResults.updatedCount).toBe(1);

    const searchResults = await newClient.search(
      testUtils.generateRandomVector(DIMENSIONS),
      10
    );
    expect(searchResults.length).toBe(10);

    const item = searchResults.find(
      (r) => r?.metadata?.originalId === idToUpdate
    );

    expect(item?.metadata?.category).toBe("original-category");
    expect(item?.metadata?.originalId).toBe(1);
    expect(item?.metadata?.name).toBe(`Original Item ${idToUpdate}`);
  });

  test("should update only vector without changing metadata", async () => {
    const idToUpdate = 1;
    const ids = Array.from({ length: 10 }, (_, i) => i + 1);
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    const insertItems: TinyVecInsertion[] = ids.map((id) => ({
      vector: testUtils.generateRandomVector(DIMENSIONS),
      metadata: {
        originalId: id,
        name: `Original Item ${id}`,
        category: "original-category",
      },
    }));

    const insertions = await newClient.insert(insertItems);
    expect(insertions).toBe(10);

    const newVector = testUtils.generateRandomVector(DIMENSIONS);
    const updateItems: UpdateItem[] = [
      {
        id: idToUpdate,
        vector: newVector,
      },
    ];

    const updateResults = await newClient.updateById(updateItems);
    expect(updateResults.success).toBe(true);
    expect(updateResults.updatedCount).toBe(1);

    const searchResults = await newClient.search(
      testUtils.generateRandomVector(DIMENSIONS),
      10
    );
    expect(searchResults.length).toBe(10);

    const item = searchResults.find(
      (r) => r?.metadata?.originalId === idToUpdate
    );

    expect(item?.metadata?.category).toBe("original-category");
    expect(item?.metadata?.originalId).toBe(1);
    expect(item?.metadata?.name).toBe(`Original Item ${idToUpdate}`);
  });

  test("should update only metadata without changing vector", async () => {
    const idToUpdate = 2;
    const ids = Array.from({ length: 10 }, (_, i) => i + 1);
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    const insertItems: TinyVecInsertion[] = ids.map((id) => ({
      vector: testUtils.generateRandomVector(DIMENSIONS),
      metadata: {
        originalId: id,
        name: `Original Item ${id}`,
        category: "original-category",
      },
    }));

    const insertions = await newClient.insert(insertItems);
    expect(insertions).toBe(10);

    const newMetadata = {
      originalId: idToUpdate,
      name: "Metadata Only Update",
      category: "meta-updated",
      timestamp: new Date().toISOString(),
    };

    const updateItems: UpdateItem[] = [
      {
        id: idToUpdate,
        metadata: newMetadata,
      },
    ];

    const updateResults = await newClient.updateById(updateItems);
    expect(updateResults.success).toBe(true);
    expect(updateResults.updatedCount).toBe(1);

    const searchResults = await newClient.search(
      testUtils.generateRandomVector(DIMENSIONS),
      10
    );
    expect(searchResults.length).toBe(10);

    const item = searchResults.find(
      (r) => r?.metadata?.originalId === idToUpdate
    );

    // Check if metadata was updated
    expect(item?.metadata?.category).toBe("meta-updated");
    expect(item?.metadata?.name).toBe("Metadata Only Update");
  });

  test("should fail gracefully when trying to update non-existent ID", async () => {
    const nonExistentId = 9999;
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    const updateItems: UpdateItem[] = [
      {
        id: nonExistentId,
        vector: testUtils.generateRandomVector(DIMENSIONS),
        metadata: { name: "Doesn't exist" },
      },
    ];

    const updateResults = await newClient.updateById(updateItems);

    // Should indicate no updates
    expect(updateResults.updatedCount).toBe(0);
    expect(updateResults.success).toBe(false);
  });

  test("should handle mixed success and failure for batch updates", async () => {
    const ids = Array.from({ length: 10 }, (_, i) => i + 1);
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    const insertItems: TinyVecInsertion[] = ids.map((id) => ({
      vector: testUtils.generateRandomVector(DIMENSIONS),
      metadata: {
        originalId: id,
        name: `Original Item ${id}`,
        category: "original-category",
      },
    }));

    const insertions = await newClient.insert(insertItems);
    expect(insertions).toBe(10);

    // Valid IDs that exist and one that doesn't
    const validIds = [3, 4];
    const invalidId = 9999;

    const updateItems: UpdateItem[] = [
      ...validIds.map((id) => ({
        id,
        metadata: {
          originalId: id,
          name: `Valid Update ${id}`,
          category: "original-category",
        },
      })),
      {
        id: invalidId,
        metadata: { name: "Invalid Update" },
      },
    ];

    const updateResults = await newClient.updateById(updateItems);

    // Overall operation succeeds but only valid IDs are updated
    expect(updateResults.success).toBe(true);
    expect(updateResults.updatedCount).toBe(validIds.length);

    const searchResults = await newClient.search(
      testUtils.generateRandomVector(DIMENSIONS),
      10
    );

    // Verify only valid IDs were updated
    for (const id of validIds) {
      const item = searchResults.find((r) => r?.metadata?.originalId === id);
      expect(item?.metadata?.name).toBe(`Valid Update ${id}`);
    }
  });

  test("should preserve search findability after vector update", async () => {
    const idToUpdate = 5;
    const ids = Array.from({ length: 10 }, (_, i) => i + 1);
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

    const insertItems: TinyVecInsertion[] = ids.map((id) => ({
      vector: testUtils.generateRandomVector(DIMENSIONS),
      metadata: {
        originalId: id,
        name: `Original Item ${id}`,
        category: "original-category",
      },
    }));

    const insertions = await newClient.insert(insertItems);
    expect(insertions).toBe(10);

    const distinctiveVector = Array.from({ length: DIMENSIONS }, () => 1);

    const updateItems: UpdateItem[] = [
      {
        id: idToUpdate,
        vector: distinctiveVector,
      },
    ];

    const updateResults = await newClient.updateById(updateItems);
    expect(updateResults.success).toBe(true);
    expect(updateResults.updatedCount).toBe(1);

    // Now search with the same distinctive vector
    const searchResults = await newClient.search(distinctiveVector, 1);

    // Should return exactly one result that matches our updated ID
    expect(searchResults.length).toBe(1);
    expect(searchResults[0]?.metadata?.originalId).toBe(idToUpdate);
    expect(searchResults[0].similarity).toBeCloseTo(1, 5); // Should be very close to 1 (perfect match)

    // Metadata should be preserved
    expect(searchResults[0]?.metadata?.category).toBe("original-category");
    expect(searchResults[0]?.metadata?.name).toBe(
      `Original Item ${idToUpdate}`
    );
  });

  test("should not update any vectors with empty vector & metadata", async () => {
    const idToUpdate = 1;

    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    const insertItems: TinyVecInsertion[] = [
      {
        vector: testUtils.generateRandomVector(DIMENSIONS),
        metadata: {
          originalId: idToUpdate,
          name: `Original Item ${idToUpdate}`,
          category: "original-category",
        },
      },
    ];

    const insertions = await newClient.insert(insertItems);
    expect(insertions).toBe(1);

    const updateItems: UpdateItem[] = [
      {
        id: idToUpdate,
      },
    ];

    const updateResults = await newClient.updateById(updateItems);
    expect(updateResults.success).toBe(false);
  });
});
