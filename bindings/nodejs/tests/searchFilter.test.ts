import { TinyVecClient, type TinyVecInsertion } from "../src/index";
import fs from "fs/promises";
import path from "path";
import * as testUtils from "./utils";

const DIMENSIONS = 128;
describe("TinyVecClient Filter Operations", () => {
  let tempDir: string;

  beforeAll(async () => {
    await fs.mkdir("temp").catch(() => {});
  });

  beforeEach(async () => {
    const hash = testUtils.createRandomMD5Hash();
    tempDir = path.join("temp", hash);
    await fs.mkdir(tempDir);
  });

  // Basic exact match filter test
  test("filter with $eq operator", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);
    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        brand: { $eq: "Pear" },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(results.every((r) => r?.metadata?.brand === "Pear")).toBe(true);
  });

  // Test $ne (not equal) operator
  test("filter with $ne operator", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        brand: { $ne: "Pear" },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(results.every((r) => r.metadata.brand !== "Pear")).toBe(true);
  });

  // Test $gt (greater than) operator with numbers
  test("filter with $gt operator", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        year: { $gt: 2020 },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(results.every((r) => r.metadata.year > 2020)).toBe(true);
  });

  // Test $gte (greater than or equal) operator
  test("filter with $gte operator", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        year: { $gte: 2020 },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(results.every((r) => r.metadata.year >= 2020)).toBe(true);
  });

  // Test $lt (less than) operator
  test("filter with $lt operator", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        year: { $lt: 2020 },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(results.every((r) => r.metadata.year < 2020)).toBe(true);
  });

  // Test $lte (less than or equal) operator
  test("filter with $lte operator", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        year: { $lte: 2020 },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(results.every((r) => r.metadata.year <= 2020)).toBe(true);
  });

  test("filter with $exists operator", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const noBrandResults = await newClient.search<TestMetadata>(
      searchVector,
      10,
      {
        filter: {
          brand: { $exists: false },
        },
      }
    );

    expect(noBrandResults.length).toEqual(0);

    const hasBrandResults = await newClient.search<TestMetadata>(
      searchVector,
      10,
      {
        filter: {
          brand: { $exists: true },
        },
      }
    );

    expect(hasBrandResults.length).toBeGreaterThan(0);
    expect(hasBrandResults.every((r) => r.metadata.brand !== undefined)).toBe(
      true
    );
  });

  // Test $in operator (value must be in an array)
  test("filter with $in operator", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const searchItems = ["Pear", "Nexus"];

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        brand: { $in: searchItems },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(results.every((r) => searchItems.includes(r.metadata.brand))).toBe(
      true
    );
  });

  // Test $nin operator (value must not be in an array)
  test("filter with $nin operator", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const searchItems = ["Pear", "Nexus"];

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        brand: { $nin: searchItems },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(results.every((r) => !searchItems.includes(r.metadata.brand))).toBe(
      true
    );
  });

  // Test nested object property filtering
  test("filter with nested property using dot notation", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        specs: {
          storage: { $lt: 200 },
        },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(results.every((r) => r.metadata.specs.storage < 200)).toBe(true);
  });

  // Test array element filtering with $in
  test("filter for values in array field", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        ratings: { $in: [4] },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(results.every((r) => r.metadata.ratings.includes(4))).toBe(true);
  });

  // Test combined filters
  test("filter with multiple conditions combined", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        brand: { $eq: "Pear" },
        year: { $gte: 2020 },
        inStock: { $eq: true },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(
      results.every(
        (r) =>
          r.metadata.brand === "Pear" &&
          r.metadata.year >= 2020 &&
          r.metadata.inStock === true
      )
    ).toBe(true);
  });

  // Test type mismatch (shouldn't return results when querying with wrong type)
  test("should not return results when filter type doesn't match metadata type", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        year: { $eq: "2020" }, // Using string instead of number
      },
    });

    // Should not match anything as we're comparing string to number
    expect(results.length).toBe(0);
  });

  // Test nested complex query
  test("filter with nested query structure", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        brand: { $eq: "Pear" },
        specs: {
          condition: { $eq: "excellent" },
        },
        inStock: { $eq: true },
      },
    });

    expect(results.length).toBeGreaterThan(0);
    expect(
      results.every(
        (r) =>
          r.metadata.brand === "Pear" &&
          r.metadata.specs.condition === "excellent" &&
          r.metadata.inStock === true
      )
    ).toBe(true);
  });

  // Test the specific example from your code
  test("exact match for Pear pPhone Pro 2024", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    const results = await newClient.search<TestMetadata>(searchVector, 10, {
      filter: {
        model: { $eq: "pPhone Pro" },
        year: { $eq: 2024 },
        brand: { $eq: "Pear" },
      },
    });

    expect(results.length).toBe(1);
    expect(results[0].metadata.brand).toBe("Pear");
    expect(results[0].metadata.model).toBe("pPhone Pro");
    expect(results[0].metadata.year).toBe(2024);
  });

  // Test for deeply nested object
  test("filter for very deeply nested object", async () => {
    const newClient = testUtils.getTinyvecClient(tempDir, DIMENSIONS);
    await insertTestVectors(newClient);
    const searchVector = createVector(3);

    type DeeplyNestedType = {
      item: {
        inner: {
          another: {
            value: number;
          };
        };
      };
    };

    const insertions: TinyVecInsertion[] = [];
    for (let i = 0; i < 10; i++) {
      const item: DeeplyNestedType = {
        item: {
          inner: {
            another: {
              value: i,
            },
          },
        },
      };
      insertions.push({
        vector: createVector(DIMENSIONS),
        metadata: item,
      });
    }

    const inserted = await newClient.insert(insertions);
    expect(inserted).toBe(10);

    const gteResults = await newClient.search<DeeplyNestedType>(
      searchVector,
      10,
      {
        filter: {
          item: {
            inner: {
              another: {
                value: { $gte: 4 },
              },
            },
          },
        },
      }
    );

    expect(gteResults.length).toBeGreaterThan(0);
    expect(
      gteResults.every((r) => r.metadata.item.inner.another.value >= 4)
    ).toBe(true);

    const lteResults = await newClient.search<DeeplyNestedType>(
      searchVector,
      10,
      {
        filter: {
          item: {
            inner: {
              another: {
                value: { $lte: 4 },
              },
            },
          },
        },
      }
    );

    expect(lteResults.length).toBeGreaterThan(0);
    expect(
      lteResults.every((r) => r.metadata.item.inner.another.value <= 4)
    ).toBe(true);
  });
});

// Helper to create a vector with a specific pattern
function createVector(baseValue: number): Float32Array {
  const vector = new Float32Array(DIMENSIONS);
  for (let i = 0; i < DIMENSIONS; i++) {
    vector[i] = baseValue + i * 0.01;
  }
  return testUtils.normalizeVector(vector);
}

export type TestMetadata = {
  id: number;
  brand: string;
  model: string;
  year: number;
  price: number;
  features: string[];
  inStock: boolean;
  specs: {
    storage: number;
    previousOwners: number;
    condition: string;
  };
  ratings: number[];
};

// Helper to prepare test data with rich metadata for filtering
export async function insertTestVectors(client: TinyVecClient) {
  const testData: TestMetadata[] = [
    {
      id: 0,
      brand: "Nexus",
      model: "Galaxy",
      year: 2020,
      price: 799,
      features: ["wireless charging", "water resistant"],
      inStock: true,
      specs: { storage: 128, previousOwners: 1, condition: "excellent" },
      ratings: [4, 5, 4.5],
    },

    {
      id: 1,
      brand: "Pear",
      model: "pPhone",
      year: 2021,
      price: 899,
      features: ["wireless charging", "portrait mode"],
      inStock: true,
      specs: { storage: 256, previousOwners: 1, condition: "excellent" },
      ratings: [4.5, 5, 4.7],
    },

    {
      id: 2,
      brand: "Pear",
      model: "pPhone Mini",
      year: 2019,
      price: 650,
      features: ["wireless charging"],
      inStock: false,
      specs: { storage: 64, previousOwners: 2, condition: "good" },
      ratings: [4, 4.2, 3.8],
    },

    {
      id: 3,
      brand: "Nexus",
      model: "Pixel",
      year: 2018,
      price: 550,
      features: ["water resistant"],
      inStock: true,
      specs: { storage: 32, previousOwners: 1, condition: "good" },
      ratings: [3.5, 4, 3.8],
    },

    {
      id: 4,
      brand: "Oceania",
      model: "Wave Pro",
      year: 2022,
      price: 1099,
      features: ["5G capability", "wireless charging", "portrait mode"],
      inStock: true,
      specs: { storage: 512, previousOwners: 0, condition: "excellent" },
      ratings: [4.8, 5, 4.9],
    },

    {
      id: 5,
      brand: "Pinnacle",
      model: "Summit",
      year: 2021,
      price: 999,
      features: ["5G capability", "wireless charging"],
      inStock: false,
      specs: { storage: 256, previousOwners: 1, condition: "excellent" },
      ratings: [4.6, 4.8, 4.7],
    },

    {
      id: 6,
      brand: "Horizon",
      model: "Edge",
      year: 2020,
      price: 850,
      features: ["portrait mode", "AI assistant"],
      inStock: true,
      specs: { storage: 128, previousOwners: 1, condition: "excellent" },
      ratings: [4.7, 4.9, 4.8],
    },

    {
      id: 7,
      brand: "Quantum",
      model: "Z Series",
      year: 2019,
      price: 799,
      features: ["portrait mode", "AI assistant"],
      inStock: true,
      specs: { storage: 256, previousOwners: 2, condition: "good" },
      ratings: [4.5, 4.6, 4.7],
    },

    {
      id: 8,
      brand: "Stellar",
      model: "X-Class",
      year: 2018,
      price: 750,
      features: ["portrait mode", "AI assistant"],
      inStock: false,
      specs: { storage: 128, previousOwners: 1, condition: "good" },
      ratings: [4.4, 4.5, 4.3],
    },

    {
      id: 9,
      brand: "Pear",
      model: "pPhone Pro",
      year: 2024,
      price: 1299,
      features: ["wireless charging", "AI assistant", "portrait mode"],
      inStock: true,
      specs: { storage: 1024, previousOwners: 0, condition: "new" },
      ratings: [5, 5, 4.9],
    },
  ];

  const insertions: TinyVecInsertion[] = [];
  for (const entry of testData) {
    insertions.push({
      vector: createVector(DIMENSIONS),
      metadata: entry,
    });
  }

  const inserted = await client.insert(insertions);
  return inserted;
}
