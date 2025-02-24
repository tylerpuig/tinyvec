from concurrent.futures import ThreadPoolExecutor
import asyncio
from typing import List
import json
import ctypes
import os
import struct
from typing import List, cast
from .core.utils import create_db_files, file_exists, ensure_absolute_path, get_float32_array
from .types import VectorInput


from .core.ctypes_bindings import lib, VecResult, MetadataBytes
from .models import (
    TinyVecConfig,
    TinyVecResult,
    TinyVecInsertion,
    TinyVecIndexStats
)


base_tinyvec_config = TinyVecConfig(dimensions=0)


class TinyVecClient:
    def __init__(self):
        self.executor = ThreadPoolExecutor(max_workers=1)
        self.file_path: str | None = None
        self.encoded_path: bytes | None = None
        self.dimensions: int | None = None

    def connect(self, file_path: str, config: TinyVecConfig = base_tinyvec_config):
        absolute_path = ensure_absolute_path(file_path)

        dimensions = config.dimensions or 0

        if not file_exists(absolute_path):
            create_db_files(absolute_path, dimensions)

        # Store the encoded path we use for connection
        try:
            self.encoded_path = absolute_path.encode('utf-8')
        except UnicodeEncodeError as e:
            raise RuntimeError(
                f"Failed to encode path: {absolute_path}") from e

        connection = lib.create_tiny_vec_connection(
            self.encoded_path, dimensions)

        if not connection:
            raise RuntimeError("Failed to create connection")

        # Store the readable path and dimensions
        self.file_path = absolute_path  # Store original string
        self.dimensions = connection.contents.dimensions

    async def search(self, query_vec: VectorInput, top_k: int) -> List[TinyVecResult]:
        def run_query():
            query_vec_float32 = get_float32_array(query_vec)

            results_ptr = lib.get_top_k(
                self.encoded_path,
                query_vec_float32,
                top_k
            )

            # Check if results_ptr is NULL
            if not results_ptr:
                return []

            results: List[TinyVecResult] = []
            for i in range(top_k):
                result = cast(VecResult, results_ptr[i])
                metadata_struct = cast(MetadataBytes, result.metadata)

                try:
                    if metadata_struct.data and metadata_struct.length > 0:
                        metadata_bytes = bytes(
                            metadata_struct.data[:metadata_struct.length])
                        metadata = json.loads(metadata_bytes.decode('utf-8'))
                    else:
                        metadata = None
                except:
                    metadata = None

                results.append(TinyVecResult(
                    similarity=result.similarity,
                    index=result.index,
                    metadata=metadata
                ))

            return results

        return await asyncio.get_event_loop().run_in_executor(
            self.executor,
            run_query
        )

    async def insert(self, vectors: List[TinyVecInsertion]):

        def insert_data():
            if len(vectors) == 0:
                return 0

            dimensions = self.dimensions
            if dimensions == 0:
                dimensions = len(vectors[0].vector)

            base_path = self.file_path
            orig_files = {
                'base': base_path,
                'idx': f"{base_path}.idx",
                'meta': f"{base_path}.meta"
            }

            temp_files = {
                'base': f"{base_path}.temp",
                'idx': f"{base_path}.idx.temp",
                'meta': f"{base_path}.meta.temp"
            }

            if not self.file_path:
                raise ValueError("File path is not set")

            with open(self.file_path, 'rb') as f:
                # Skip first 4 bytes (vector count)
                f.seek(4)
                current_dimensions = struct.unpack('<i', f.read(4))[0]

            # If dimensions are 0, update them
            if current_dimensions == 0:
                # Determine dimensions from either provided parameter or first vector
                if dimensions is not None:
                    new_dimensions = dimensions
                elif vectors and len(vectors) > 0:
                    new_dimensions = len(vectors[0].vector)
                else:
                    raise ValueError(
                        "Cannot determine dimensions - no data or dimensions provided")

                # Create buffer with new dimensions
                dimensions_buffer = struct.pack('<i', new_dimensions)
                self.dimensions = new_dimensions

                # Open file in read+write mode and update dimensions at offset 4
                with open(self.file_path, 'r+b') as f:
                    f.seek(4)  # Position to write dimensions
                    f.write(dimensions_buffer)
                    f.flush()
                    os.fsync(f.fileno())  # Force flush to disk

            # Pre-process vectors to get valid count
            valid_vectors = []
            for vec in vectors:
                try:
                    vec_float32 = get_float32_array(vec.vector)
                    if len(vec_float32) != dimensions and dimensions != 0:
                        continue
                    valid_vectors.append((vec, vec_float32))
                except (ValueError, TypeError) as e:
                    continue

            vec_count = len(valid_vectors)
            if vec_count == 0:
                return 0
            try:
                # Copy original files to temp first
                for orig, temp in zip(orig_files.values(), temp_files.values()):
                    with open(orig, 'rb') as src, open(temp, 'wb') as dst:
                        dst.write(src.read())

                # Create array of pointers to float arrays
                vec_array = (ctypes.POINTER(ctypes.c_float) * vec_count)()
                metadata_array = (ctypes.c_char_p * vec_count)()
                metadata_lengths = (ctypes.c_size_t * vec_count)()

                # Keep references to prevent garbage collection
                vector_buffers = []
                for i, (vec, vec_float32) in enumerate(valid_vectors):
                    # Create contiguous buffer and keep reference
                    vec_buffer = vec_float32.ctypes.data_as(
                        ctypes.POINTER(ctypes.c_float))
                    # Keep reference to prevent garbage collection
                    vector_buffers.append(vec_float32)
                    vec_array[i] = vec_buffer

                    # Process metadata
                    md_str = json.dumps(
                        vec.metadata) if vec.metadata is not None else ""
                    md_bytes = md_str.encode('utf-8')
                    metadata_array[i] = md_bytes
                    metadata_lengths[i] = len(md_bytes)

                # Call the C function
                inserted = lib.insert_data(
                    self.encoded_path,    # file_path
                    vec_array,           # vectors (array of pointers)
                    metadata_array,      # metadatas
                    metadata_lengths,    # metadata_lengths
                    vec_count,          # vec_count
                    dimensions          # dimensions
                )

                if inserted <= 0:
                    return 0

                # Atomic rename of temp files to original
                for temp, orig in zip(temp_files.values(), orig_files.values()):
                    # os.replace is atomic on Unix systems
                    os.replace(temp, orig)

                # Update DB file connection
                lib.update_db_file_connection(self.encoded_path)

                return inserted
            except Exception as e:
                raise e
            finally:
                for temp in temp_files.values():
                    try:
                        os.unlink(temp)
                    except FileNotFoundError:
                        pass

                # Clear ctypes arrays
                if 'vec_array' in locals():
                    for ptr in vec_array:
                        if ptr:
                            del ptr
                if 'metadata_array' in locals():
                    for ptr in metadata_array:
                        if ptr:
                            del ptr
        return await asyncio.get_event_loop().run_in_executor(
            self.executor,
            insert_data
        )

    async def get_index_stats(self) -> TinyVecIndexStats:
        def run_stats():
            stats = lib.get_index_stats(self.encoded_path)
            if not stats:
                raise RuntimeError("Failed to get index stats")
            return TinyVecIndexStats(
                vector_count=stats.vector_count,
                dimensions=stats.dimensions
            )

        stats = await asyncio.get_event_loop().run_in_executor(
            self.executor,
            run_stats
        )

        return stats
