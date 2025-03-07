from benchmarks.constants import (
    DIMENSIONS, SLEEP_TIME, QUERY_ITERATIONS, TOP_K, TINYVEC_PATH
)
from benchmarks.utils import (
    generate_random_embeddings, get_stable_memory_usage, warmup_memory,
    get_avg_search_time, save_metrics, QueryMetrics
)
from typing import List
import tinyvec
import asyncio
import time
import sys
from pathlib import Path
sys.path.append(str(Path(__file__).parent.parent))


async def main():

    warmup_memory()
    init_memory = get_stable_memory_usage()

    client = tinyvec.TinyVecClient()
    config = tinyvec.ClientConfig(dimensions=DIMENSIONS)

    client.connect(
        TINYVEC_PATH, config)

    time.sleep(SLEEP_TIME)

    query_vec = generate_random_embeddings(1, DIMENSIONS)[0]
    query_times: List[float] = []

    search_options_eq = tinyvec.SearchOptions(
        filter={"type": {"$eq": "even"}}
    )

    search_options_gt = tinyvec.SearchOptions(
        filter={"amount": {"$gt": 100}}
    )

    filters: List[tinyvec.SearchOptions] = [
        search_options_eq, search_options_gt]

    # Perform search and measure time
    async def run_bench(opts: List[tinyvec.SearchOptions]):
        for metadata_filter in opts:
            for _ in range(QUERY_ITERATIONS):
                start_query_time = time.time()
                await client.search(query_vec, TOP_K, metadata_filter)
                end_query_time = time.time()
                total_time = end_query_time - start_query_time
                print(f"Query time: {total_time * 1000:.2f}ms")
                query_times.append(total_time)

    await run_bench(filters)

    avg_search_time = get_avg_search_time(query_times)

    time.sleep(SLEEP_TIME)

    final_memory = get_stable_memory_usage()

    query_metrics = QueryMetrics(
        database_title="TinyVec",
        query_time=avg_search_time,
        initial_memory=init_memory,
        final_memory=final_memory,
        benchmark_type="Metadata Filter"
    )

    save_metrics(query_metrics)


if __name__ == "__main__":
    asyncio.run(main())
