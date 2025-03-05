import { TinyVecClient } from "../src/index";
import path from "path";
import crypto from "crypto";

export function generateRandomVector(dimensions: number) {
  const vector = new Float32Array(dimensions);
  for (let i = 0; i < dimensions; i++) {
    vector[i] = Math.random();
  }
  return vector;
}

export function getTinyvecClient(tempDir: string, dims: number) {
  return TinyVecClient.connect(getRandDbPath(tempDir), { dimensions: dims });
}

export function getRandDbPath(tempDir: string) {
  const randInt = Math.floor(Math.random() * 1000);
  const dbPath = path.join(tempDir, `${Date.now()}-${randInt}.db`);
  return dbPath;
}

export function normalizeVector(vector: Float32Array) {
  const norm = Math.sqrt(vector.reduce((acc, val) => acc + val * val, 0));
  const epsilon = 1e-12;
  for (let i = 0; i < vector.length; i++) {
    vector[i] = vector[i] / (norm + epsilon);
  }
  return vector;
}

export function createRandomMD5Hash(): string {
  // Generate random data to hash
  const randomBytes = crypto.randomBytes(32); // Generate 32 random bytes

  // Create the MD5 hash
  const hash = crypto.createHash("md5");
  hash.update(randomBytes);
  return hash.digest("hex");
}
