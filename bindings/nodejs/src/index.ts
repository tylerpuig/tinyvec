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
const nativeDeleteVectorsByIds =
  nativeModule.deleteByIds as tinyvecTypes.DeleteVectorsByIdsFunction;
const nativeDeleteVectorsByFilter =
  nativeModule.deleteByFilter as tinyvecTypes.DeleteVectorsByFilterFunction;
const nativeUpsertVectorsById =
  nativeModule.upsertById as tinyvecTypes.NativeUpdateVectorsByIdFunction;
const nativeGetPaginatedVectors =
  nativeModule.getPaginatedVectors as tinyvecTypes.NativeGetPaginatedVectorsFunction;

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
    }
    nativeConnect(absolutePath, config);
    return new TinyVecClient(absolutePath, config);
  }

  async search<TMeta = any>(
    query: tinyvecTypes.NumericArray,
    topK: number,
    options?: tinyvecTypes.TinyVecSearchOptions
  ): Promise<tinyvecTypes.TinyVecSearchResult<TMeta>[]> {
    if (typeof topK !== "number" || topK <= 0) {
      throw new Error("Top K must be a positive number.");
    }
    const float32Array = tinyvecUtils.convertToFloat32Array(query);

    if (options) {
      const filterStr = JSON.stringify(options?.filter ?? "{}");
      const optsFmt = { ...options, filter: filterStr };
      return await nativeSearch<TMeta>(
        float32Array,
        topK,
        this.filePath,
        optsFmt
      );
    }

    return await nativeSearch<TMeta>(float32Array, topK, this.filePath);
  }

  async insert(data: tinyvecTypes.TinyVecInsertion[]): Promise<number> {
    if (!data || !data.length) return 0;
    // const tempFilePath = `${this.filePath}.temp`;
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
      // await fsPromises.copyFile(this.filePath, tempFilePath);

      // Insert data
      const inserted = await nativeInsert(
        this.filePath,
        convertedData,
        this.dimensions
      );

      if (inserted <= 0) {
        return 0;
      }

      // Rename of temp file to original
      // await fsPromises.rename(tempFilePath, this.filePath);

      // Update DB file connection
      nativeUpdateDbFileConnection(this.filePath);

      return inserted;
    } catch (error) {
      throw error;
    } finally {
      // Clean up temp files regardless of success or failure
      // await fsPromises.unlink(tempFilePath).catch(() => {});
    }
  }

  async deleteByIds(ids: number[]): Promise<tinyvecTypes.DeletionResult> {
    if (!ids || !ids.length) {
      return { deletedCount: 0, success: false };
    }
    let tempFilePath = `${this.filePath}.temp`;
    try {
      // first copy the vector file to a temp file
      const indexStats = await nativeGetIndexStats(this.filePath);
      if (!indexStats || !indexStats.vectors) {
        return { deletedCount: 0, success: false };
      }
      // Write the header to the temp file
      await tinyvecUtils.writeFileHeader(
        tempFilePath,
        indexStats.vectors,
        indexStats.dimensions
      );
      const deletionResult = await nativeDeleteVectorsByIds(this.filePath, ids);

      await fsPromises.rename(tempFilePath, this.filePath);

      // Update DB file connection
      nativeUpdateDbFileConnection(this.filePath);

      return deletionResult;
    } catch (error) {
      throw error;
    } finally {
      // Clean up temp files regardless of success or failure
      await fsPromises.unlink(tempFilePath).catch(() => {});
    }
  }

  async deleteByFilter(
    options: tinyvecTypes.TinyVecSearchOptions
  ): Promise<tinyvecTypes.DeletionResult> {
    if (!options) {
      throw new Error("No options provided");
    }

    if (!options.filter) {
      throw new Error("No filter provided");
    }
    let tempFilePath = `${this.filePath}.temp`;
    try {
      const indexStats = await nativeGetIndexStats(this.filePath);
      if (!indexStats || !indexStats.vectors) {
        return { deletedCount: 0, success: false };
      }
      // Write the header to the temp file
      await tinyvecUtils.writeFileHeader(
        tempFilePath,
        indexStats.vectors,
        indexStats.dimensions
      );
      const filterStr = JSON.stringify(options.filter);
      const deletionResult = await nativeDeleteVectorsByFilter(
        this.filePath,
        filterStr
      );

      if (!deletionResult.deletedCount) {
        nativeUpdateDbFileConnection(this.filePath);
        return {
          deletedCount: 0,
          success: false,
        };
      }

      await fsPromises.rename(tempFilePath, this.filePath);

      // Update DB file connection
      nativeUpdateDbFileConnection(this.filePath);

      return deletionResult;
    } catch (error) {
      throw error;
    } finally {
      // Clean up temp files regardless of success or failure
      await fsPromises.unlink(tempFilePath).catch(() => {});
    }
  }

  async updateById(
    items: tinyvecTypes.UpdateItem[]
  ): Promise<tinyvecTypes.UpdateResult> {
    if (!items || !items.length) {
      return { updatedCount: 0, success: false };
    }
    try {
      const indexStats = await nativeGetIndexStats(this.filePath);
      if (!indexStats || !indexStats.vectors) {
        return { updatedCount: 0, success: false };
      }

      const formattedItems = items.map((item) => {
        return {
          id: item.id,
          vector: tinyvecUtils.convertToFloat32Array(
            item.vector ?? new Float32Array()
          ),
          metadata: JSON.stringify(item.metadata),
        };
      });

      const updateResult = await nativeUpsertVectorsById(
        this.filePath,
        formattedItems
      );

      // Update DB file connection
      nativeUpdateDbFileConnection(this.filePath);

      return updateResult;
    } catch (error) {
      throw error;
    }
  }

  async getPaginated<TMeta = any>(
    options: tinyvecTypes.PaginationConfig
  ): Promise<tinyvecTypes.PaginationItem<TMeta>[]> {
    if (!options) {
      throw new Error("No options provided");
    }
    if (typeof options.skip !== "number" || options.skip < 0) {
      throw new Error("Skip must be a positive number.");
    }
    if (typeof options.limit !== "number" || options.limit <= 0) {
      throw new Error("Limit must be a positive number.");
    }

    const indexStats = await this.getIndexStats();
    if (!indexStats || !indexStats.vectors) {
      return [];
    }
    if (options.skip > indexStats.vectors) {
      return [];
    }

    if (options.limit > indexStats.vectors) {
      options.limit = indexStats.vectors;
    }

    const results = await nativeGetPaginatedVectors(this.filePath, options);
    return results;
  }

  async getIndexStats(): Promise<tinyvecTypes.IndexFileStats> {
    return await nativeGetIndexStats(this.filePath);
  }
}

export { TinyVecClient };
export type {
  TinyVecInsertion,
  TinyVecSearchResult,
  IndexFileStats,
  TinyVecConfig,
  JsonValue,
  TinyVecSearchOptions,
  UpdateItem,
  PaginationConfig,
  PaginationItem,
} from "./types";
