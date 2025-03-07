import sqlite3
import sqlite_vec

from typing import List
import struct
from random import random

from benchmarks.constants import DIMENSIONS, QUERY_ITERATIONS, SLEEP_TIME, SQLITE_VEC_PATH
from benchmarks.utils import (
    warmup_memory, get_stable_memory_usage,
    get_avg_search_time,  save_metrics, QueryMetrics
)
import time
import sys
from pathlib import Path
sys.path.append(str(Path(__file__).parent.parent.parent))


def serialize_f32(vector: List[float]) -> bytes:
    """serializes a list of floats into a compact "raw bytes" format"""
    return struct.pack("%sf" % len(vector), *vector)


def main():

    warmup_memory()

    init_memory = get_stable_memory_usage()

    db = sqlite3.connect(
        SQLITE_VEC_PATH)
    db.enable_load_extension(True)
    sqlite_vec.load(db)

    query_times: List[float] = []
    random_vector = [random() for _ in range(DIMENSIONS)]
    for i in range(QUERY_ITERATIONS):
        start_time = time.time()
        db.execute(
            """
        SELECT
            rowid,
            distance
        FROM vec_items
        WHERE embedding MATCH ?
        ORDER BY distance
        LIMIT 10
        """,
            [serialize_f32(random_vector)],
        ).fetchall()

        end_time = time.time()
        total_time = end_time - start_time
        query_times.append(total_time)
        print(f"Query time: {total_time * 1000:.2f}ms")

    avg_time = get_avg_search_time(query_times)
    time.sleep(SLEEP_TIME)

    final_memory = get_stable_memory_usage()

    query_metrics = QueryMetrics(
        database_title="SQLite-Vec",
        query_time=avg_time,
        initial_memory=init_memory,
        final_memory=final_memory,
        benchmark_type="Vector Search"
    )

    save_metrics(query_metrics)


if __name__ == "__main__":
    main()
