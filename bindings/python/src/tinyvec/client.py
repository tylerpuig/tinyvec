from concurrent.futures import ThreadPoolExecutor
import asyncio
from typing import List
import json
import ctypes
import os
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


class TinyVecClient:
    def __init__(self):
        self.executor = ThreadPoolExecutor(max_workers=1)
        self.file_path: str | None = None
        self.encoded_path: bytes | None = None
        self.dimensions: int | None = None

    def connect(self, file_path: str, config: TinyVecConfig):
        absolute_path = ensure_absolute_path(file_path)

        if not file_exists(absolute_path):
            create_db_files(absolute_path, config.dimensions)

        # Store the encoded path we use for connection
        try:
            self.encoded_path = absolute_path.encode('utf-8')
        except UnicodeEncodeError as e:
            raise RuntimeError(
                f"Failed to encode path: {absolute_path}") from e

        # initialize_vector_file(file_path, config)
        connection = lib.create_tiny_vec_connection(
            self.encoded_path, config.dimensions)

        if not connection:
            raise RuntimeError("Failed to create connection")

        # Store the readable path and dimensions
        self.file_path = absolute_path  # Store original string
        self.dimensions = connection.contents.dimensions

    def disconnect(self):
        result = lib.remove_from_connection_pool(self.encoded_path)
        print('disconect result: ', result)

    async def search(self, query_vec: VectorInput, top_k: int) -> List[TinyVecResult]:
        def run_query():

            query_vec_float32 = get_float32_array(query_vec)

            results_ptr = lib.get_top_k(
                self.encoded_path,
                query_vec_float32,
                top_k
            )

            if query_vec_float32.shape[0] != self.dimensions:
                raise RuntimeError(
                    f"Query vector must have {self.dimensions} dimensions, got {query_vec_float32.shape[0]}"
                )

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
            if dimensions is None:
                dimensions = len(vectors[0].vector)

            # Pre-filter vectors with correct dimensions
            # valid_vectors = [vec for vec in vectors if len(
            #     vec.vector) == dimensions]
            # vec_count = len(valid_vectors)

            # if vec_count == 0:
            #     return 0

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
            # Pre-process vectors to get valid count
            valid_vectors = []
            for vec in vectors:
                try:
                    vec_float32 = get_float32_array(vec.vector)
                    if len(vec_float32) != dimensions:
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
