import * as fs from "fs";
import path from "path";
import type { NumericArray, JsonValue } from "./types";

export function fileExists(filePath: string): boolean {
  return fs.existsSync(filePath);
}

export function isFloat32ArrayInstance(arr: any): boolean {
  return arr instanceof Float32Array;
}

export function ensureAbsolutePath(filePath: string): string {
  if (!path.isAbsolute(filePath)) {
    // Convert relative path to absolute using current working directory
    return path.resolve(process.cwd(), filePath);
  }
  return filePath;
}

export function convertToFloat32Array(
  arr: NumericArray,
  index?: number,
  metadata?: JsonValue
): Float32Array {
  try {
    // If it's already a Float32Array, return it
    if (arr instanceof Float32Array) {
      return arr;
    }

    // If it's another TypedArray, we can convert it directly
    if (ArrayBuffer.isView(arr)) {
      return new Float32Array(arr);
    }

    // If it's a regular array, convert it
    if (Array.isArray(arr)) {
      return new Float32Array(arr);
    }

    throw new Error("Unsupported array type");
  } catch (error) {
    const errorMessage = error instanceof Error ? error.message : String(error);
    throw new VectorConversionError(
      `Vector conversion failed${
        index !== undefined ? ` at index ${index}` : ""
      }: ${errorMessage}`,
      index,
      metadata
    );
  }
}

export class VectorConversionError extends Error {
  constructor(
    message: string,
    public index?: number,
    public metadata?: JsonValue
  ) {
    super(message);
    this.name = "VectorConversionError";
  }
}
