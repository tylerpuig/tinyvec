from benchmarks.utils import (
    generate_random_embeddings, warmup_memory, get_stable_memory_usage,
    get_avg_search_time,  save_metrics, QueryMetrics
)
from benchmarks.constants import (
    DIMENSIONS, SLEEP_TIME, QUERY_ITERATIONS, COLLECTION_NAME, QDRANT_PATH, TOP_K
)
from typing import List, Union
import time
import numpy as np
from qdrant_client.http import models
from qdrant_client import QdrantClient
import sys
from pathlib import Path
sys.path.append(str(Path(__file__).parent.parent.parent))


def main():
    warmup_memory()
    init_memory = get_stable_memory_usage()

    # Initialize client
    qdrant = QdrantClient(
        path=QDRANT_PATH)

    time.sleep(SLEEP_TIME)
    # Generate a random query vector
    query_vec = generate_random_embeddings(1, DIMENSIONS)[0]

    # Perform search and measure time
    print("Performing search...")
    query_times: List[float] = []
    for _ in range(QUERY_ITERATIONS):
        start_time = time.time()
        qdrant.query_points(
            collection_name=COLLECTION_NAME,
            query=query_vec,
            limit=TOP_K
        )

        end_time = time.time()
        search_time = end_time - start_time
        query_times.append(search_time)
        print(f"\nSearch completed in {search_time * 1000:.2f}ms")

    avg_search_time = get_avg_search_time(query_times)

    time.sleep(SLEEP_TIME)

    final_memory = get_stable_memory_usage()

    query_metrics = QueryMetrics(
        database_title="Qdrant",
        query_time=avg_search_time,
        initial_memory=init_memory,
        final_memory=final_memory,
        benchmark_type="Vector Search"
    )

    save_metrics(query_metrics)


if __name__ == "__main__":
    main()
