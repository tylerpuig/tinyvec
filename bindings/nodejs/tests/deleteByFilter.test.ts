import { type TinyVecInsertion } from "../src/index";
import fs from "fs/promises";
import path from "path";
import * as testUtils from "./utils";
import { insertTestVectors, type TestMetadata } from "./searchFilter.test";

const DIMENSIONS = 128;
describe("TinyVecClient Delete Operations", () => {
  let tempDir: string;

  beforeAll(async () => {
    await fs.mkdir("temp").catch(() => {});
  });

  beforeEach(async () => {
    const hash = testUtils.createRandomMD5Hash();
    tempDir = path.join("temp", hash);
    await fs.mkdir(tempDir);
  });

  // Basic exact match deletion test
  test("deleteByFilter with $eq operator", async () => {
    const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(client);

    // Delete Pear phones
    const deleteResult = await client.deleteByFilter({
      filter: {
        brand: { $eq: "Pear" },
      },
    });

    // Verify deletion result
    expect(deleteResult.success).toBe(true);
    expect(deleteResult.deletedCount).toBe(3); // We should have 3 Pear phones in test data

    // Verify deletion by searching for Pear phones
    // const searchVector = testUtils.generateRandomVector(DIMENSIONS);
    // const searchResults = await client.search<TestMetadata>(searchVector, 10, {
    //   filter: {
    //     brand: { $eq: "Pear" },
    //   },
    // });

    // // Should no longer have any Pear phones
    // expect(searchResults.length).toBe(0);
  });

  // Test deletion with $ne (not equal) operator
  test("deleteByFilter with $ne operator", async () => {
    const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(client);

    // Get count of non-Pear phones before deletion
    const searchVector = testUtils.generateRandomVector(DIMENSIONS);
    const beforeResults = await client.search<TestMetadata>(searchVector, 10, {
      filter: {
        brand: { $ne: "Pear" },
      },
    });
    const nonPearCount = beforeResults.length;

    // Delete non-Pear phones
    const deleteResult = await client.deleteByFilter({
      filter: {
        brand: { $ne: "Pear" },
      },
    });

    // Verify deletion result
    expect(deleteResult.success).toBe(true);
    expect(deleteResult.deletedCount).toBe(nonPearCount);

    // Verify only Pear phones remain
    const afterResults = await client.search<TestMetadata>(searchVector, 10);
    expect(afterResults.length).toBe(3); // Should only have the 3 Pear phones left
    expect(afterResults.every((r) => r.metadata.brand === "Pear")).toBe(true);
  });

  // Test deletion with $gt (greater than) operator
  test("deleteByFilter with $gt operator", async () => {
    const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(client);

    // Delete phones from after 2020
    const deleteResult = await client.deleteByFilter({
      filter: {
        year: { $gt: 2020 },
      },
    });

    // Verify deletion
    const searchVector = testUtils.generateRandomVector(DIMENSIONS);
    const searchResults = await client.search<TestMetadata>(
      searchVector,
      10,
      {}
    );

    // All remaining phones should be from 2020 or earlier
    expect(searchResults.every((r) => r.metadata.year <= 2020)).toBe(true);
    // Should have deleted phones from 2021, 2022, and 2024
    expect(deleteResult.deletedCount).toBeGreaterThan(0);
  });

  // Test deletion with combined filter conditions
  test("deleteByFilter with multiple conditions", async () => {
    const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(client);

    // Delete expensive Pear phones
    const deleteResult = await client.deleteByFilter({
      filter: {
        brand: { $eq: "Pear" },
        price: { $gt: 1000 },
      },
    });

    // Should delete just the pPhone Pro (2024)
    expect(deleteResult.deletedCount).toBe(1);

    // Verify remaining Pear phones are all under $1000
    const searchVector = testUtils.generateRandomVector(DIMENSIONS);
    const searchResults = await client.search<TestMetadata>(searchVector, 10, {
      filter: {
        brand: { $eq: "Pear" },
      },
    });

    expect(searchResults.length).toBe(2); // Should have 2 Pear phones left
    expect(searchResults.every((r) => r.metadata.price <= 1000)).toBe(true);
  });

  // // Test deletion based on nested properties
  // test("deleteByFilter with nested object properties", async () => {
  //   const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
  //   await insertTestVectors(client);

  //   // Delete phones with high storage
  //   const deleteResult = await client.deleteByFilter({
  //     filter: {
  //       specs: {
  //         storage: { $gte: 256 },
  //       },
  //     },
  //   });

  //   // Verify deletion
  //   const searchVector = testUtils.generateRandomVector(DIMENSIONS);
  //   const searchResults = await client.search<TestMetadata>(searchVector, 10);

  //   // All remaining phones should have less than 256GB storage
  //   expect(searchResults.every((r) => r.metadata.specs.storage < 256)).toBe(
  //     true
  //   );
  //   expect(deleteResult.deletedCount).toBeGreaterThan(0);
  // });

  // // Test deletion with $exists operator
  // test("deleteByFilter with $exists operator", async () => {
  //   const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);

  //   // Insert one record with missing fields
  //   const normalVector = testUtils.generateRandomVector(DIMENSIONS);
  //   await client.insert([
  //     {
  //       vector: normalVector,
  //       metadata: {
  //         id: 100,
  //         brand: "TestBrand",
  //         // Intentionally missing model
  //         year: 2023,
  //         price: 900,
  //         inStock: true,
  //         specs: { storage: 128, previousOwners: 0, condition: "excellent" },
  //         ratings: [4.5],
  //       } as any,
  //     },
  //   ]);

  //   // Also insert regular test vectors
  //   await insertTestVectors(client);

  //   // Delete records where model doesn't exist
  //   const deleteResult = await client.deleteByFilter({
  //     filter: {
  //       model: { $exists: false },
  //     },
  //   });

  //   // Should have deleted just the one record
  //   expect(deleteResult.deletedCount).toBe(1);

  //   // Verify all remaining records have model field
  //   const searchVector = testUtils.generateRandomVector(DIMENSIONS);
  //   const searchResults = await client.search<TestMetadata>(searchVector, 20);
  //   expect(searchResults.every((r) => r.metadata.model !== undefined)).toBe(
  //     true
  //   );
  // });

  // // Test deletion with $in operator
  // test("deleteByFilter with $in operator", async () => {
  //   const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
  //   await insertTestVectors(client);

  //   // Delete specific brands
  //   const deleteResult = await client.deleteByFilter({
  //     filter: {
  //       brand: { $in: ["Nexus", "Pear"] },
  //     },
  //   });

  //   // Should have deleted Nexus and Pear phones
  //   expect(deleteResult.deletedCount).toBe(5); // 2 Nexus + 3 Pear

  //   // Verify deletion
  //   const searchVector = testUtils.generateRandomVector(DIMENSIONS);
  //   const searchResults = await client.search<TestMetadata>(searchVector, 10);

  //   // Should not have any Nexus or Pear phones
  //   expect(
  //     searchResults.some((r) => ["Nexus", "Pear"].includes(r.metadata.brand))
  //   ).toBe(false);
  // });

  // // Test deletion with boolean field
  // test("deleteByFilter with boolean field", async () => {
  //   const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
  //   await insertTestVectors(client);

  //   // Delete out-of-stock phones
  //   const deleteResult = await client.deleteByFilter({
  //     filter: {
  //       inStock: { $eq: false },
  //     },
  //   });

  //   // Verify deletion
  //   const searchVector = testUtils.generateRandomVector(DIMENSIONS);
  //   const searchResults = await client.search<TestMetadata>(searchVector, 10);

  //   // All remaining phones should be in stock
  //   expect(searchResults.every((r) => r.metadata.inStock === true)).toBe(true);
  //   expect(deleteResult.deletedCount).toBeGreaterThan(0);
  // });

  // // Test failed deletion (no matches)
  // test("deleteByFilter with no matches", async () => {
  //   const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
  //   await insertTestVectors(client);

  //   // Try to delete phones with non-existent brand
  //   const deleteResult = await client.deleteByFilter({
  //     filter: {
  //       brand: { $eq: "NonExistentBrand" },
  //     },
  //   });

  //   // Should report success but zero deletions
  //   expect(deleteResult.success).toBe(false);
  //   expect(deleteResult.deletedCount).toBe(0);

  //   // Verify all records still exist
  //   const searchVector = testUtils.generateRandomVector(DIMENSIONS);
  //   const searchResults = await client.search<TestMetadata>(searchVector, 10);
  //   expect(searchResults.length).toBe(10); // All 10 test phones should remain
  // });

  // // Test complex nested filter deletion
  // test("deleteByFilter with complex nested filters", async () => {
  //   const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
  //   await insertTestVectors(client);

  //   // Delete high-end phones with specific condition
  //   const deleteResult = await client.deleteByFilter({
  //     filter: {
  //       price: { $gte: 900 },
  //       specs: {
  //         condition: { $in: ["excellent", "new"] },
  //         storage: { $gte: 256 },
  //       },
  //       inStock: { $eq: true },
  //     },
  //   });

  //   // Verify deletion
  //   const searchVector = testUtils.generateRandomVector(DIMENSIONS);
  //   const searchResults = await client.search<TestMetadata>(searchVector, 10, {
  //     filter: {
  //       price: { $gte: 900 },
  //       specs: {
  //         condition: { $in: ["excellent", "new"] },
  //         storage: { $gte: 256 },
  //       },
  //       inStock: { $eq: true },
  //     },
  //   });

  //   // Should no longer find any matching phones
  //   expect(searchResults.length).toBe(0);
  //   expect(deleteResult.deletedCount).toBeGreaterThan(0);
  // });

  // // Test deletion and reinsert
  // test("deleteByFilter and reinsert", async () => {
  //   const client = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
  //   await insertTestVectors(client);

  //   // Delete all Pear phones
  //   const deleteResult = await client.deleteByFilter({
  //     filter: {
  //       brand: { $eq: "Pear" },
  //     },
  //   });

  //   expect(deleteResult.deletedCount).toBe(3);

  //   // Reinsert some Pear phones with different data
  //   const newPhones: TinyVecInsertion[] = [
  //     {
  //       vector: testUtils.generateRandomVector(DIMENSIONS),
  //       metadata: {
  //         id: 101,
  //         brand: "Pear",
  //         model: "pPhone Ultra",
  //         year: 2025,
  //         price: 1499,
  //         features: ["wireless charging", "satellite", "AI assistant"],
  //         inStock: true,
  //         specs: { storage: 2048, previousOwners: 0, condition: "new" },
  //         ratings: [5, 5, 5],
  //       },
  //     },
  //     {
  //       vector: testUtils.generateRandomVector(DIMENSIONS),
  //       metadata: {
  //         id: 102,
  //         brand: "Pear",
  //         model: "pPhone Mini Ultra",
  //         year: 2025,
  //         price: 1299,
  //         features: ["wireless charging", "satellite"],
  //         inStock: true,
  //         specs: { storage: 1024, previousOwners: 0, condition: "new" },
  //         ratings: [4.9, 5, 4.8],
  //       },
  //     },
  //   ];

  //   const insertResult = await client.insert(newPhones);
  //   expect(insertResult).toBe(2);

  //   // Search for Pear phones and verify new data
  //   const searchVector = testUtils.generateRandomVector(DIMENSIONS);
  //   const searchResults = await client.search<TestMetadata>(searchVector, 10, {
  //     filter: {
  //       brand: { $eq: "Pear" },
  //     },
  //   });

  //   expect(searchResults.length).toBe(2);
  //   expect(searchResults.every((r) => r.metadata.year === 2025)).toBe(true);
  // });
});
