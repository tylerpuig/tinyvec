// declare module "*/tinyvec.node" {
//   import type {
//     TinyVecSearchResult,
//     IndexFileStats,
//     TinyVecConfig,
//     TinyVecInsertion,
//   } from "./types";
//   export function search<TMeta = any>(
//     query: Float32Array,
//     topK: number,
//     filePath: string
//   ): Promise<TinyVecSearchResult<TMeta>[]>;

//   export function insertVectors(
//     filePath: string,
//     vectors: TinyVecInsertion[],
//     dimensions: number
//   ): Promise<number>;

//   export function connect(
//     filePath: string,
//     config?: TinyVecConfig
//   ): { filePath: string };

//   export function updateDbFileConnection(filePath: string): boolean;

//   export function getIndexStats(filePath: string): Promise<IndexFileStats>;
// }
