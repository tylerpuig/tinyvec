declare module "*/tinyvec.node" {
  export type TinyVecSearchResult<TMeta = any> = {
    index: number;
    score: number;
    metadata: TMeta;
  };

  export type IndexFileStats = {
    vectors: number;
    dimensions: number;
  };

  export type TinyVecInsertion = {
    vector: Float32Array;
    metadata: Record<string, any>;
  };

  export function search<TMeta = any>(
    query: Float32Array,
    topK: number,
    filePath: string
  ): Promise<Array<TinyVecSearchResult<TMeta>>>;

  export function insertVectors(
    filePath: string,
    vectors: TinyVecInsertion[],
    dimensions: number
  ): Promise<number>;

  export function connect(
    filePath: string,
    config?: TinyVecConfig
  ): Promise<{ filePath: string }>;

  export function updateDbFileConnection(filePath: string): boolean;

  export function getIndexStats(filePath: string): Promise<IndexFileStats>;
}
