import lancedb
from qdrant_client import QdrantClient
from qdrant_client.http import models
from tinyvec import TinyVecClient, TinyVecConfig, TinyVecInsertion
import chromadb
import sqlite3
import sqlite_vec
import numpy as np
import random
import asyncio
import struct
import os

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


async def main():
    ensure_directories_exist()

    lance_db_connection = lancedb.connect(LANCEDB_PATH)

    data_rows = []
    for i in range(INSERTION_COUNT):
        row = {
            "vector": np.random.rand(DIMENSIONS),
            "text": f"New text {i}",
            "metadata": {"id": f"{i}"}
        }
        data_rows.append(row)

    # create lancedb table with data
    lance_db_connection.create_table(
        COLLECTION_NAME, data=data_rows, mode="overwrite")

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
        # Convert numpy array to list and ensure it's a list of floats
        points.append(
            models.PointStruct(
                id=i,
                vector=[random.random() for _ in range(DIMENSIONS)],
                payload={"metadata": {"id": i}}
            )
        )

    # Batch insert points
    qdrant_client.upsert(
        collection_name=COLLECTION_NAME,
        points=points
    )

    sqlite_db = sqlite3.connect(
        SQLITE_VEC_PATH)
    sqlite_db.enable_load_extension(True)
    sqlite_vec.load(sqlite_db)
    sqlite_db.enable_load_extension(False)

    sqlite_db.execute("DROP TABLE IF EXISTS vec_items")
    sqlite_db.execute(
        f"CREATE VIRTUAL TABLE vec_items USING vec0(embedding float[{DIMENSIONS}])"
    )
    sqlite_db.commit()

    with sqlite_db:
        sqlite_db.executemany(
            "INSERT INTO vec_items(rowid, embedding) VALUES (?, ?)",
            [(i, serialize_f32([random.random() for _ in range(DIMENSIONS)]))
             for i in range(INSERTION_COUNT)]
        )

    chroma_client = chromadb.PersistentClient(
        CHROMA_PATH)
    chroma_collection = chroma_client.create_collection(
        COLLECTION_NAME, embedding_function=None)
    chroma_embeddings = []
    chroma_docs = []
    chroma_ids = []
    for i in range(INSERTION_COUNT):
        chroma_embeddings.append(np.random.random(512).astype(np.float32))
        chroma_docs.append("item-" + str(i))
        chroma_ids.append(str(i))

    chroma_batch_size = 5_000

    for i in range(0, INSERTION_COUNT, chroma_batch_size):
        # Handle the last batch correctly
        end_idx = min(i + chroma_batch_size, INSERTION_COUNT)

        # Add the current batch
        chroma_collection.add(
            embeddings=chroma_embeddings[i:end_idx],
            documents=chroma_docs[i:end_idx],
            ids=chroma_ids[i:end_idx]
        )

    tinyvec_client = TinyVecClient()
    tinyvec_client.connect(TINYVEC_PATH, TinyVecConfig(dimensions=DIMENSIONS))

    tinyvec_insertions = []
    for i in range(INSERTION_COUNT):
        tinyvec_insertions.append(TinyVecInsertion(
            vector=[random.random() for _ in range(DIMENSIONS)],
            metadata={"id": i}
        ))
    await tinyvec_client.insert(tinyvec_insertions)


if __name__ == "__main__":
    asyncio.run(main())
