// Native addon function types
export type NativeSearchFunction = <TMeta = any>(
  query: Float32Array,
  topK: number,
  filePath: string
) => Promise<TinyVecSearchResult<TMeta>[]>;

export type NativeInsertFunction = (
  filePath: string,
  vectors: TinyVecInsertion[],
  dimensions: number
) => Promise<number>;

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

export type TinyVecSearchResult<TMeta = any> = {
  index: number;
  similarity: number;
  metadata: TMeta;
};

export type IndexFileStats = {
  vectors: number;
  dimensions: number;
};

export type TinyVecConfig = {
  dimensions: number;
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
