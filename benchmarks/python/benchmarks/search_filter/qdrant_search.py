from benchmarks.utils import (
    generate_random_embeddings, get_stable_memory_usage, warmup_memory,
    get_avg_search_time, get_memory_usage, save_metrics, QueryMetrics
)
from benchmarks.constants import (
    DIMENSIONS, SLEEP_TIME, QUERY_ITERATIONS, TOP_K, QDRANT_PATH, COLLECTION_NAME
)
import numpy as np
from qdrant_client.http import models
from qdrant_client import QdrantClient
from typing import List
import asyncio
import time
import sys
from pathlib import Path
sys.path.append(str(Path(__file__).parent.parent))


async def main():
    warmup_memory()
    init_memory = get_stable_memory_usage()

    # Connect to Qdrant
    qdrant_client = QdrantClient(path=QDRANT_PATH)

    time.sleep(SLEEP_TIME)

    # Generate random query vector
    query_vec = generate_random_embeddings(1, DIMENSIONS)[0].tolist()
    query_times: List[float] = []

    # Perform search and measure time
    print("Performing search with metadata filter...")

    # Basic filter for even type entries
    filter_even = models.Filter(
        must=[
            models.FieldCondition(
                key="type",
                match=models.MatchValue(value="even"),
            ),
        ]
    )

    filter_gt = models.Filter(
        must=[
            models.FieldCondition(
                key="amount",
                range=models.Range(
                    gt=100
                )
            ),
        ]
    )

    filters: List[models.Filter] = [filter_even, filter_gt]
    query_vec = generate_random_embeddings(1, DIMENSIONS)[0]

    def run_bench(filters: List[models.Filter]):
        for metadata_filter in filters:
            for i in range(QUERY_ITERATIONS):
                start_query_time = time.time()

                qdrant_client.search(
                    collection_name=COLLECTION_NAME,
                    query_vector=query_vec,
                    limit=TOP_K,
                    query_filter=metadata_filter
                )

                end_query_time = time.time()
                total_time = end_query_time - start_query_time
                print(f"Query {i+1} time: {total_time * 1000:.2f}ms")
                query_times.append(total_time)

    run_bench(filters)

    avg_search_time = get_avg_search_time(query_times)

    time.sleep(SLEEP_TIME)

    final_memory = get_stable_memory_usage()

    query_metrics = QueryMetrics(
        database_title="Qdrant",
        query_time=avg_search_time,
        initial_memory=init_memory,
        final_memory=final_memory,
        benchmark_type="Metadata Filter"
    )

    save_metrics(query_metrics)


if __name__ == "__main__":
    asyncio.run(main())
