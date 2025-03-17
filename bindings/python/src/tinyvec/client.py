from concurrent.futures import ThreadPoolExecutor
import asyncio
from typing import List, Optional, Dict, Any
import json
import ctypes
import os
import struct
import ctypes
import shutil
from typing import List, cast
from .core.utils import create_db_files, file_exists, ensure_absolute_path, get_float32_array, write_file_header, prepare_db_update_items
from .types import VectorInput


from .core.ctypes_bindings import lib, VecResult, MetadataBytes
from .models import (
    ClientConfig,
    SearchResult,
    Insertion,
    IndexStats,
    SearchOptions,
    DeletionResult,
    UpdateResult,
    UpdateItem
)


base_tinyvec_config = ClientConfig(dimensions=0)


class TinyVecClient:
    def __init__(self):
        self.executor = ThreadPoolExecutor(max_workers=1)
        self.file_path: str | None = None
        self.encoded_path: bytes | None = None
        self.dimensions: int | None = None

    def connect(self, file_path: str, config: ClientConfig = base_tinyvec_config):
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

    async def search(self, query_vec: VectorInput, top_k: int, options: Optional[SearchOptions] = None) -> List[SearchResult]:

        def run_query():
            query_vec_float32 = get_float32_array(query_vec)

            # If options with filter is provided, convert it to a JSON string
            if options and options.filter:
                filter_str = json.dumps(options.filter).encode('utf-8')

                # Call the version of get_top_k that accepts options
                results_ptr = lib.get_top_k_with_filter(
                    self.encoded_path,
                    query_vec_float32,
                    top_k,
                    filter_str
                )
            else:
                # Call the original version without options
                results_ptr = lib.get_top_k(
                    self.encoded_path,
                    query_vec_float32,
                    top_k
                )

            # Check if results_ptr is NULL
            if not results_ptr:
                return []

            db_results = results_ptr.contents
            result_count = min(db_results.count, top_k)

            results: List[SearchResult] = []

            for i in range(result_count):
                result = db_results.results[i]
                metadata_struct = result.metadata

                try:
                    if metadata_struct.data and metadata_struct.length > 0:
                        metadata_bytes = bytes(
                            metadata_struct.data[:metadata_struct.length])
                        metadata = json.loads(metadata_bytes.decode('utf-8'))
                    else:
                        metadata = None
                except:
                    metadata = None

                results.append(SearchResult(
                    similarity=result.similarity,
                    id=result.index,
                    metadata=metadata
                ))

            return results

        return await asyncio.get_event_loop().run_in_executor(
            self.executor,
            run_query
        )

    async def insert(self, vectors: List[Insertion]) -> int:
        def insert_data():
            if len(vectors) == 0:
                return 0

            dimensions = self.dimensions
            if dimensions == 0:
                dimensions = len(vectors[0].vector)

            base_path = self.file_path
            temp_file_path = f"{base_path}.temp"

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
                # Copy temp file

                shutil.copyfile(self.file_path, temp_file_path)

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

                # Rename temp file to original
                os.replace(temp_file_path, self.file_path)

                # Update DB file connection
                lib.update_db_file_connection(self.encoded_path)

                return inserted
            except Exception as e:
                raise e
            finally:
                try:
                    os.unlink(temp_file_path)
                except FileNotFoundError:
                    pass
        return await asyncio.get_event_loop().run_in_executor(
            self.executor,
            insert_data
        )

    async def delete_by_ids(self, ids: List[int]) -> DeletionResult:
        if not ids or len(ids) == 0:
            return DeletionResult(deleted_count=0, success=False)

        def run_delete_ids_native() -> int:
            # Convert Python list to C array for C function call
            c_array = (ctypes.c_int * len(ids))(*ids)

            # Call the C function to perform deletion
            deleted_count = lib.delete_data_by_ids(
                self.encoded_path, c_array, len(ids))

            return deleted_count

        if not self.file_path:
            return DeletionResult(deleted_count=0, success=False)

        temp_file_path = f"{self.file_path}.temp"
        try:
            # Get current index stats
            index_stats = await self.get_index_stats()
            if not index_stats or index_stats.vector_count == 0:
                return DeletionResult(deleted_count=0, success=False)

            # Write the header to the temp file
            write_file_header(
                temp_file_path,
                index_stats.vector_count,
                index_stats.dimensions
            )

            # Execute the native deletion function in the executor
            deleted_count = await asyncio.get_event_loop().run_in_executor(
                self.executor,
                run_delete_ids_native  # Use the native function that returns an int
            )

            # Rename temp file to original
            os.replace(temp_file_path, self.file_path)

            # Update DB file connection
            lib.update_db_file_connection(self.encoded_path)

            return DeletionResult(deleted_count=deleted_count, success=True)

        except Exception as e:
            return DeletionResult(deleted_count=0, success=False)
        finally:
            # Clean up temp file regardless of success or failure
            try:
                if os.path.exists(temp_file_path):
                    os.unlink(temp_file_path)
            except:
                # Ignore errors during cleanup
                pass

    async def delete_by_filter(self, search_options: SearchOptions) -> DeletionResult:
        if not search_options:
            raise ValueError("No options provided")

        if not search_options.filter:
            raise ValueError("No filter provided")

        if not self.file_path:
            return DeletionResult(deleted_count=0, success=False)

        temp_file_path = f"{self.file_path}.temp"

        filter_str = json.dumps(search_options.filter).encode('utf-8')

        def run_delete_by_filter() -> int:
            deleted_count = lib.delete_data_by_filter(
                self.encoded_path, filter_str)
            if not deleted_count:
                return 0
            return deleted_count

        try:
            # Get current index stats
            index_stats = await self.get_index_stats()
            if not index_stats or index_stats.vector_count == 0:
                return DeletionResult(deleted_count=0, success=False)

            # Write the header to the temp file
            write_file_header(
                temp_file_path,
                index_stats.vector_count,
                index_stats.dimensions
            )

            # Call the C function to perform deletion
            deleted_count = await asyncio.get_event_loop().run_in_executor(
                self.executor,
                run_delete_by_filter
            )

            # Match Node.js behavior
            if deleted_count == 0:
                lib.update_db_file_connection(self.encoded_path)
                return DeletionResult(deleted_count=0, success=False)

            # Rename temp file to original
            os.replace(temp_file_path, self.file_path)

            # Update DB file connection
            lib.update_db_file_connection(self.encoded_path)

            return DeletionResult(deleted_count=deleted_count, success=True)

        except Exception as e:
            raise e
        finally:
            # Clean up temp file regardless of success or failure
            try:
                if os.path.exists(temp_file_path):
                    os.unlink(temp_file_path)
            except:
                # Ignore errors during cleanup
                pass

    async def update_by_id(self, items: List[UpdateItem]) -> UpdateResult:
        if not items:
            return UpdateResult(updated_count=0, success=False)

        index_stats = await self.get_index_stats()
        if not index_stats or index_stats.vector_count == 0:

            return UpdateResult(updated_count=0, success=False)

        def update_items_by_id():
            try:
                c_items, vector_refs, metadata_refs = prepare_db_update_items(
                    items)

                updated_count = lib.batch_update_items_by_id(
                    self.encoded_path, c_items, len(items))

                if updated_count <= 0:
                    return UpdateResult(updated_count=0, success=False)

                # Update DB file connection
                lib.update_db_file_connection(self.encoded_path)

                return UpdateResult(updated_count=updated_count, success=True)

            except Exception as e:
                raise e

        return await asyncio.get_event_loop().run_in_executor(
            self.executor,
            update_items_by_id
        )

    async def get_index_stats(self) -> IndexStats:
        def run_stats():
            stats = lib.get_index_stats(self.encoded_path)
            if not stats:
                raise RuntimeError("Failed to get index stats")
            return IndexStats(
                vector_count=stats.vector_count,
                dimensions=stats.dimensions
            )

        stats = await asyncio.get_event_loop().run_in_executor(
            self.executor,
            run_stats
        )

        return stats
