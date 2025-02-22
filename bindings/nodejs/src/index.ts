import {
  search as nativeSearch,
  insertVectors as nativeInsert,
  connect as nativeConnect,
  getIndexStats as nativeGetIndexStats,
  updateDbFileConnection as nativeUpdateDbFileConnection,
} from "../build/Release/tinyvec.node";
import * as fs from "fs";
import * as fsPromises from "fs/promises";
import path from "path";

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

  static connect(filePath: string, config?: TinyVecConfig): TinyVecClient {
    const absolutePath = ensureAbsolutePath(filePath);
    const fExists = fileExists(absolutePath);
    if (!fExists) {
      const vectorCount = 0;
      const dimensions = config?.dimensions ?? 0;
      const buffer = Buffer.alloc(8);
      buffer.writeInt32LE(vectorCount, 0);
      buffer.writeInt32LE(dimensions, 4);

      // Open with 'wx' flag - creates a new file and fails if it exists
      const fd = fs.openSync(absolutePath, "wx");
      fs.writeSync(fd, buffer);
      // Force flush to disk
      fs.fsyncSync(fd);
      fs.closeSync(fd);

      // Create empty metadata files
      const idxFd = fs.openSync(absolutePath + ".idx", "wx");
      const metaFd = fs.openSync(absolutePath + ".meta", "wx");
      // Force flush these
      fs.fsyncSync(idxFd);
      fs.fsyncSync(metaFd);
      fs.closeSync(idxFd);
      fs.closeSync(metaFd);
    }
    nativeConnect(absolutePath, config);
    return new TinyVecClient(absolutePath, config);
  }

  async search<TMeta = any>(
    query: Float32Array,
    topK: number
  ): Promise<TinyVecSearchResult<TMeta>[]> {
    const isFloat32Array = isFloat32ArrayInstance(query);
    if (!isFloat32Array) {
      throw new Error("Query vector must be a Float32Array");
    }

    if (query.length !== this.dimensions) {
      throw new Error(
        "Query vector must have the same dimensions as the database"
      );
    }
    return await nativeSearch<TMeta>(query, topK, this.filePath);
  }

  async insert(data: TinyVecInsertion[]): Promise<number> {
    if (!data || !data.length) return 0;
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

    try {
      // Copy original files to temp
      await Promise.all([
        fsPromises.copyFile(origFiles.base, tempFiles.base),
        fsPromises.copyFile(origFiles.idx, tempFiles.idx),
        fsPromises.copyFile(origFiles.meta, tempFiles.meta),
      ]);

      // Insert data
      const inserted = await nativeInsert(this.filePath, data, this.dimensions);

      if (inserted <= 0) {
        return 0;
      }

      // Atomic rename of temp files to original
      await Promise.all([
        fsPromises.rename(tempFiles.base, origFiles.base),
        fsPromises.rename(tempFiles.idx, origFiles.idx),
        fsPromises.rename(tempFiles.meta, origFiles.meta),
      ]);

      // Update DB file connection
      nativeUpdateDbFileConnection(this.filePath);

      return inserted;
    } catch (error) {
      throw error;
    } finally {
      // Clean up temp files regardless of success or failure
      await Promise.all([
        fsPromises.unlink(tempFiles.base).catch(() => {}),
        fsPromises.unlink(tempFiles.idx).catch(() => {}),
        fsPromises.unlink(tempFiles.meta).catch(() => {}),
      ]);
    }
  }

  async getIndexStats() {
    return await nativeGetIndexStats(this.filePath);
  }
}

function fileExists(filePath: string): boolean {
  return fs.existsSync(filePath);
}

function isFloat32ArrayInstance(arr: any): boolean {
  return arr instanceof Float32Array;
}

function ensureAbsolutePath(filePath: string): string {
  if (!path.isAbsolute(filePath)) {
    // Convert relative path to absolute using current working directory
    return path.resolve(process.cwd(), filePath);
  }
  return filePath;
}

export default TinyVecClient;
