import {
  type TinyVecSearchResult,
  type IndexFileStats,
  type VectorInsertion,
  search as nativeSearch,
  insertVectors as nativeInsert,
  connect as nativeConnect,
  getIndexStats as nativeGetIndexStats,
} from "./build/Release/tinyvec.node";
import fs from "fs/promises";

export type TinyVecConfig = {
  dimensions: number;
};

export class TinyVecClient {
  private filePath: string;
  private dimensions: number = 0;

  private constructor(filePath: string, config?: TinyVecConfig) {
    this.filePath = filePath;
    if (config?.dimensions) {
      this.dimensions = config.dimensions;
    }
  }

  static async connect(
    filePath: string,
    config?: TinyVecConfig
  ): Promise<TinyVecClient> {
    const file = await fileExists(filePath);
    if (!file) {
      const vectorCount = 0;
      const dimensions = config?.dimensions ?? 0;
      const buffer = Buffer.alloc(8);
      buffer.writeInt32LE(vectorCount, 0);
      buffer.writeInt32LE(dimensions, 4);

      // Write buffer directly instead of empty file first
      await fs.writeFile(filePath, buffer);
      await fs.writeFile(filePath + ".idx", "");
      await fs.writeFile(filePath + ".meta", "");
    }
    await nativeConnect(filePath, config);
    return new TinyVecClient(filePath, config);
  }

  async search<TMeta = any>(
    query: Float32Array,
    topK: number
  ): Promise<TinyVecSearchResult<TMeta>[]> {
    const isFloat32Array = isFloat32ArrayInstance(query);
    if (!isFloat32Array) {
      throw new Error("Query vector must be a Float32Array");
    }
    return await nativeSearch<TMeta>(query, topK, this.filePath);
  }

  async insert(data: VectorInsertion[]): Promise<number> {
    return await nativeInsert(this.filePath, data, this.dimensions);
  }

  async getIndexStats() {
    return await nativeGetIndexStats(this.filePath);
  }
}

async function fileExists(filePath: string): Promise<boolean> {
  try {
    await fs.access(filePath);
    return true;
  } catch (err) {
    return false;
  }
}

function isFloat32ArrayInstance(arr: any): boolean {
  return arr instanceof Float32Array;
}

// Default export for easier imports
export default TinyVecClient;
export type { TinyVecSearchResult, IndexFileStats };
