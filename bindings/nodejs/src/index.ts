import {
  // type TinyVecSearchResult,
  // type IndexFileStats,
  // type TinyVecInsertion,
  search as nativeSearch,
  insertVectors as nativeInsert,
  connect as nativeConnect,
  getIndexStats as nativeGetIndexStats,
  updateMmaps as nativeUpdateMmaps,
} from "../build/Release/tinyvec.node";
import fs from "fs/promises";

export type TinyVecConfig = {
  dimensions: number;
};

export type TinyVecInsertion = {
  vector: Float32Array;
  metadata: Record<string, any>;
};

export type TinyVecSearchResult<TMeta = any> = {
  index: number;
  score: number;
  metadata: TMeta;
};

export type IndexFileStats = {
  vectors: number;
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

      // Open with 'wx' flag - creates a new file and fails if it exists
      const fileHandle = await fs.open(filePath, "wx");
      await fileHandle.write(buffer);
      // Force flush to disk
      await fileHandle.sync();
      await fileHandle.close();

      // Create empty metadata files - use 'wx' here too
      const idxHandle = await fs.open(filePath + ".idx", "wx");
      const metaHandle = await fs.open(filePath + ".meta", "wx");
      // Force flush these too
      await idxHandle.sync();
      await metaHandle.sync();
      await idxHandle.close();
      await metaHandle.close();
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

  async insert(data: TinyVecInsertion[]): Promise<number> {
    const basePath = this.filePath;
    const origFiles = {
      base: basePath,
      idx: `${basePath}.idx`,
      meta: `${basePath}.meta`,
    };

    const tempFiles = {
      base: `${basePath}.temp`,
      idx: `${basePath}.idx.temp`,
      meta: `${basePath}.meta.temp`,
    };

    let inserted: number = 0;

    try {
      // Copy original files to temp
      await Promise.all([
        fs.copyFile(origFiles.base, tempFiles.base),
        fs.copyFile(origFiles.idx, tempFiles.idx),
        fs.copyFile(origFiles.meta, tempFiles.meta),
      ]);

      // Insert data
      inserted = await nativeInsert(this.filePath, data, this.dimensions);
      console.log("inserted", inserted);

      if (inserted <= 0) {
        return 0;
      }

      // Atomic rename of temp files to original
      await Promise.all([
        fs.rename(tempFiles.base, origFiles.base),
        fs.rename(tempFiles.idx, origFiles.idx),
        fs.rename(tempFiles.meta, origFiles.meta),
      ]);

      // Update mmaps
      nativeUpdateMmaps(this.filePath);

      return inserted;
    } catch (error) {
      console.log(error);
      throw error;
    } finally {
      // Clean up temp files regardless of success or failure
      await Promise.all([
        fs.unlink(tempFiles.base).catch(() => {}),
        fs.unlink(tempFiles.idx).catch(() => {}),
        fs.unlink(tempFiles.meta).catch(() => {}),
      ]);
    }
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
