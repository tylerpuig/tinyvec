import path from "path";
import * as tinyvecUtils from "./utils";
import * as tinyvecTypes from "./types";
import * as fs from "fs";
import * as fsPromises from "fs/promises";

const gyp = require("node-gyp-build");
const nativeModule = gyp(path.resolve(__dirname, ".."));

const nativeSearch = nativeModule.search as tinyvecTypes.NativeSearchFunction;
const nativeInsert =
  nativeModule.insertVectors as tinyvecTypes.NativeInsertFunction;
const nativeConnect =
  nativeModule.connect as tinyvecTypes.NativeConnectFunction;
const nativeGetIndexStats =
  nativeModule.getIndexStats as tinyvecTypes.NativeGetIndexStatsFunction;
const nativeUpdateDbFileConnection =
  nativeModule.updateDbFileConnection as tinyvecTypes.NativeUpdateDbFileConnectionFunction;

class TinyVecClient {
  private filePath: string;
  private dimensions: number = 0;

  private constructor(filePath: string, config?: tinyvecTypes.TinyVecConfig) {
    this.filePath = filePath;
    if (config?.dimensions) {
      this.dimensions = config.dimensions;
    }
  }

  static connect(
    filePath: string,
    config?: tinyvecTypes.TinyVecConfig
  ): TinyVecClient {
    const absolutePath = tinyvecUtils.ensureAbsolutePath(filePath);
    const fExists = tinyvecUtils.fileExists(absolutePath);

    if (config?.dimensions !== undefined) {
      if (typeof config.dimensions !== "number" || config.dimensions <= 0) {
        throw new Error("Dimensions must be a positive number.");
      }
    }
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
    query: tinyvecTypes.NumericArray,
    topK: number
  ): Promise<tinyvecTypes.TinyVecSearchResult<TMeta>[]> {
    if (typeof topK !== "number" || topK <= 0) {
      throw new Error("Top K must be a positive number.");
    }
    const float32Array = tinyvecUtils.convertToFloat32Array(query);
    return await nativeSearch<TMeta>(float32Array, topK, this.filePath);
  }

  async insert(data: tinyvecTypes.TinyVecInsertion[]): Promise<number> {
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
      const indexStats = await nativeGetIndexStats(this.filePath);
      if (indexStats.dimensions === 0) {
        const buffer = Buffer.alloc(4);
        const dimensions = this.dimensions || data[0].vector.length;
        buffer.writeInt32LE(dimensions, 0);

        const fd = fs.openSync(this.filePath, "r+");
        fs.writeSync(fd, buffer, 0, buffer.length, 4);
        // Force flush to disk
        fs.fsyncSync(fd);
        fs.closeSync(fd);
      }
      // Convert data with index and metadata context
      const convertedData = data.map((item, index) => {
        try {
          return {
            ...item,
            vector: tinyvecUtils.convertToFloat32Array(
              item.vector,
              index,
              item.metadata
            ),
          };
        } catch (error) {
          if (error instanceof tinyvecUtils.VectorConversionError) {
            throw error;
          }
          throw new tinyvecUtils.VectorConversionError(
            `Failed to convert vector at index ${index}`,
            index,
            item.metadata
          );
        }
      });

      // Copy original files to temp
      await Promise.all([
        fsPromises.copyFile(origFiles.base, tempFiles.base),
        fsPromises.copyFile(origFiles.idx, tempFiles.idx),
        fsPromises.copyFile(origFiles.meta, tempFiles.meta),
      ]);

      // Insert data
      const inserted = await nativeInsert(
        this.filePath,
        convertedData,
        this.dimensions
      );

      if (inserted <= 0) {
        return 0;
      }

      // "Atomic" rename of temp files to original
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

  async getIndexStats(): Promise<tinyvecTypes.IndexFileStats> {
    return await nativeGetIndexStats(this.filePath);
  }
}

export { TinyVecClient };
// export * from "./types";
export type {
  TinyVecInsertion,
  TinyVecSearchResult,
  IndexFileStats,
  TinyVecConfig,
  JsonValue,
} from "./types";
