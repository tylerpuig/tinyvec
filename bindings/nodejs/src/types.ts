// Native addon function types
export type NativeSearchFunction = <TMeta = any>(
  query: Float32Array,
  topK: number,
  filePath: string,
  options?: Record<string, any>
) => Promise<TinyVecSearchResult<TMeta>[]>;

export type NativeInsertFunction = (
  filePath: string,
  vectors: TinyVecInsertion[],
  dimensions: number
) => Promise<number>;

export type DeleteVectorsByIdsFunction = (
  filePath: string,
  ids: number[]
) => Promise<DeletionResult>;

export type DeleteVectorsByFilterFunction = (
  filePath: string,
  jsonFilter: string
) => Promise<DeletionResult>;

export type NativeConnectFunction = (
  filePath: string,
  config?: TinyVecConfig
) => { filePath: string };

export type NativeGetIndexStatsFunction = (
  filePath: string
) => Promise<IndexFileStats>;

export type NativeUpdateDbFileConnectionFunction = (
  filePath: string
) => boolean;

export type NativeUpdateVectorsByIdFunction = (
  filePath: string,
  items: UpdateItem[]
) => Promise<UpdateResult>;

export type PaginationItem<TMeta = any> = {
  id: number;
  metadata: TMeta;
  vector: Float32Array;
};

export type PaginatedResults<TMeta = any> = {
  results: PaginationItem<TMeta>[];
  count: number;
};

export type NativeGetPaginatedVectorsFunction = (
  filePath: string,
  options: string
) => Promise<PaginationItem[]>;

export type TinyVecSearchResult<TMeta = any> = {
  id: number;
  similarity: number;
  metadata: TMeta;
};

export type DeletionResult = {
  deletedCount: number;
  success: boolean;
};

export type UpdateResult = {
  updatedCount: number;
  success: boolean;
};

export type DeleteByFilterOptions = {
  filter: Record<string, any>;
};

export type IndexFileStats = {
  vectors: number;
  dimensions: number;
};

export type TinyVecConfig = {
  dimensions: number;
};

export type TinyVecSearchOptions = {
  filter?: Record<string, any>;
};

export type NumericArray =
  | number[]
  | Float32Array
  | Float64Array
  | Int8Array
  | Int16Array
  | Int32Array
  | Uint8Array
  | Uint16Array
  | Uint32Array;

export type JsonValue =
  | string
  | number
  | boolean
  | null
  | JsonObject
  | JsonArray;
interface JsonObject {
  [key: string]: JsonValue;
}
interface JsonArray extends Array<JsonValue> {}

export type TinyVecInsertion = {
  vector: NumericArray;
  metadata: JsonValue;
};

type OnlyMetadataUpdateItem = {
  id: number;
  metadata: JsonValue;
  vector?: NumericArray;
};

type OnlyVectorUpdateItem = {
  id: number;
  vector: NumericArray;
  metadata?: JsonValue;
};

export type UpdateItem = {
  id: number;
} & (OnlyMetadataUpdateItem | OnlyVectorUpdateItem);
