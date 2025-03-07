import lancedb
from qdrant_client import QdrantClient
from qdrant_client.http import models
import tinyvec
import chromadb
import sqlite3
import sqlite_vec
import numpy as np
import random
import asyncio
import struct
import os
import json

from constants import (
    LANCEDB_PATH, COLLECTION_NAME, INSERTION_COUNT, QDRANT_PATH, DIMENSIONS, SQLITE_VEC_PATH, TINYVEC_PATH, CHROMA_PATH)


def serialize_f32(vector: list[float]) -> bytes:
    """serializes a list of floats into a compact "raw bytes" format"""
    return struct.pack("%sf" % len(vector), *vector)


def ensure_directories_exist():
    """Create all necessary database directories, recreating them if they exist."""
    directories = [
        os.path.dirname(QDRANT_PATH),
        os.path.dirname(SQLITE_VEC_PATH),
        os.path.dirname(TINYVEC_PATH),
        LANCEDB_PATH,
        CHROMA_PATH
    ]

    for directory in directories:
        # Remove directory if it exists
        if os.path.exists(directory):
            import shutil
            shutil.rmtree(directory)
            print(f"Removed existing directory: {directory}")

        # Create the directory
        os.makedirs(directory, exist_ok=True)
        print(f"Created directory: {directory}")


def generate_metadata(index):
    """
    Generate metadata with multiple fields including nested structure and arrays.
    Using modulo to make half of the entries have different values.

    Args:
        index: The index of the item
    """
    # Rich metadata structure for other databases
    metadata = {
        "id": index,
        "created_at": "2025-03-05T12:00:00Z",
        "tags": ["vector", "embedding", "test"],
        # Array of numbers
        "numeric_values": [random.random() for _ in range(5)],
        "nested": {
            "level1": {
                "level2": "nested value",
                "priority": random.randint(1, 10)
            }
        }
    }

    # For half the entries (using modulo), add different values
    if index % 2 == 0:
        metadata["type"] = "even"
        metadata["category"] = "category_a"
        metadata["nested"]["level1"]["status"] = "active"
        metadata["importance"] = "high"
        metadata["amount"] = 100
    else:
        metadata["type"] = "odd"
        metadata["category"] = "category_b"
        metadata["nested"]["level1"]["status"] = "pending"
        metadata["importance"] = "medium"
        metadata["amount"] = 200

    return metadata


def generate_chroma_metadata(index):
    """
    Generate metadata specifically formatted for ChromaDB.

    Args:
        index: The index of the item
    """
    # ChromaDB doesn't support arrays or nested objects in metadata
    # Flatten the structure and convert arrays to strings
    metadata = {
        "id": index,
        "created_at": "2025-03-05T12:00:00Z",
        "tags": "vector,embedding,test",  # Convert array to string
        "numeric_values": ",".join([str(round(random.random(), 4)) for _ in range(5)]),
        "nested_level2": "nested value",
        "nested_priority": random.randint(1, 10)
    }

    if index % 2 == 0:
        metadata["type"] = "even"
        metadata["category"] = "category_a"
        metadata["nested_status"] = "active"
        metadata["importance"] = "high"
        metadata["amount"] = 100
    else:
        metadata["type"] = "odd"
        metadata["category"] = "category_b"
        metadata["nested_status"] = "pending"
        metadata["importance"] = "medium"
        metadata["amount"] = 200

    return metadata


async def main():
    ensure_directories_exist()

    # === LanceDB ===
    lance_db_connection = lancedb.connect(LANCEDB_PATH)

    # For LanceDB, move "type" field to top level instead of in metadata
    data_rows = []
    for i in range(INSERTION_COUNT):
        metadata = generate_metadata(i)

        # Extract "type" from metadata to make it a top-level column
        type_value = metadata.pop("type")  # Remove from metadata

        amount_value = 100 if i % 2 == 0 else 200

        row = {
            "vector": np.random.rand(DIMENSIONS),
            "text": f"New text {i}",
            "type": type_value,  # Add as separate column
            "metadata": metadata,
            "amount": amount_value
        }
        data_rows.append(row)

    # Create lancedb table with data
    lance_db_connection.create_table(
        COLLECTION_NAME, data=data_rows, mode="overwrite")

    # === Qdrant ===
    qdrant_client = QdrantClient(path=QDRANT_PATH)
    qdrant_client.create_collection(
        collection_name=COLLECTION_NAME,
        vectors_config=models.VectorParams(
            size=DIMENSIONS,
            distance=models.Distance.COSINE,
        )
    )

    points = []
    for i in range(INSERTION_COUNT):
        metadata = generate_metadata(i)
        points.append(
            models.PointStruct(
                id=i,
                vector=[random.random() for _ in range(DIMENSIONS)],
                payload=metadata  # Using the same metadata structure
            )
        )

    # Batch insert points
    qdrant_client.upsert(
        collection_name=COLLECTION_NAME,
        points=points
    )

    # === SQLite ===
    sqlite_db = sqlite3.connect(SQLITE_VEC_PATH)
    sqlite_db.enable_load_extension(True)
    sqlite_vec.load(sqlite_db)
    sqlite_db.enable_load_extension(False)

    sqlite_db.execute("DROP TABLE IF EXISTS vec_items")
    sqlite_db.execute(
        f"CREATE VIRTUAL TABLE vec_items USING vec0(embedding float[{DIMENSIONS}])"
    )

    # Add a metadata table for SQLite
    sqlite_db.execute("DROP TABLE IF EXISTS item_metadata")
    sqlite_db.execute(
        "CREATE TABLE item_metadata (id INTEGER PRIMARY KEY, metadata TEXT)"
    )

    sqlite_db.commit()

    with sqlite_db:
        # Insert vectors
        sqlite_db.executemany(
            "INSERT INTO vec_items(rowid, embedding) VALUES (?, ?)",
            [(i, serialize_f32([random.random() for _ in range(DIMENSIONS)]))
             for i in range(INSERTION_COUNT)]
        )

        # Insert metadata as JSON strings
        for i in range(INSERTION_COUNT):
            metadata = generate_metadata(i)
            sqlite_db.execute(
                "INSERT INTO item_metadata(id, metadata) VALUES (?, ?)",
                (i, json.dumps(metadata))
            )

    # === ChromaDB ===
    chroma_client = chromadb.PersistentClient(CHROMA_PATH)
    chroma_collection = chroma_client.create_collection(
        COLLECTION_NAME, embedding_function=None)

    chroma_embeddings = []
    chroma_docs = []
    chroma_ids = []
    chroma_metadatas = []

    for i in range(INSERTION_COUNT):
        chroma_embeddings.append(np.random.random(512).astype(np.float32))
        chroma_docs.append("item-" + str(i))
        chroma_ids.append(str(i))
        # Use the specific Chroma-compatible metadata format
        chroma_metadatas.append(generate_chroma_metadata(i))

    chroma_batch_size = 5_000

    for i in range(0, INSERTION_COUNT, chroma_batch_size):
        # Handle the last batch correctly
        end_idx = min(i + chroma_batch_size, INSERTION_COUNT)

        # Add the current batch with metadata
        chroma_collection.add(
            embeddings=chroma_embeddings[i:end_idx],
            documents=chroma_docs[i:end_idx],
            ids=chroma_ids[i:end_idx],
            metadatas=chroma_metadatas[i:end_idx]
        )

    # === TinyVec ===
    tinyvec_client = tinyvec.TinyVecClient()
    tinyvec_client.connect(
        TINYVEC_PATH, tinyvec.ClientConfig(dimensions=DIMENSIONS))

    tinyvec_insertions = []
    for i in range(INSERTION_COUNT):
        metadata = generate_metadata(i)
        tinyvec_insertions.append(tinyvec.Insertion(
            vector=[random.random() for _ in range(DIMENSIONS)],
            metadata=metadata
        ))
    await tinyvec_client.insert(tinyvec_insertions)


if __name__ == "__main__":
    asyncio.run(main())
