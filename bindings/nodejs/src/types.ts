export type TinyVecSearchResult<TMeta = any> = {
  index: number;
  score: number;
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
