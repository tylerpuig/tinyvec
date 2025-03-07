import chromadb
import time

from benchmarks.utils import (
    generate_random_embeddings, warmup_memory, get_stable_memory_usage,
    get_avg_search_time, save_metrics, QueryMetrics
)
from benchmarks.constants import (
    DIMENSIONS, SLEEP_TIME, QUERY_ITERATIONS, COLLECTION_NAME, CHROMA_PATH
)


def main():
    warmup_memory()
    init_memory = get_stable_memory_usage()

    client = chromadb.PersistentClient(
        CHROMA_PATH)
    collection = client.get_collection(
        COLLECTION_NAME, embedding_function=None)

    query_times: list[float] = []
    query_vec = generate_random_embeddings(1, DIMENSIONS)[0]

    # Metadata filter similar to TinyVec example
    metadata_filter_eq: chromadb.Where = {"type": {"$eq": "even"}}
    metadata_filter_gt: chromadb.Where = {"amount": {"$gt": 100}}

    filters = [metadata_filter_eq, metadata_filter_gt]

    def run_bench(filters: list[chromadb.Where]):
        for metadata_filter in filters:
            print(f"Running benchmark with filter: {metadata_filter}")
            # query_times: list[float] = []
            for _ in range(QUERY_ITERATIONS):
                start_query_time = time.time()
                collection.query(
                    query_embeddings=query_vec,
                    n_results=10,
                    where=metadata_filter
                )
                end_query_time = time.time()
                total_time = end_query_time - start_query_time
                print(f"Query time: {total_time * 1000:.2f}ms")
                query_times.append(total_time)

    run_bench(filters)

    avg_search_time = get_avg_search_time(query_times)

    time.sleep(SLEEP_TIME)

    final_memory = get_stable_memory_usage()

    query_metrics = QueryMetrics(
        database_title="Chroma",
        query_time=avg_search_time,
        initial_memory=init_memory,
        final_memory=final_memory,
        benchmark_type="Metadata Filter"
    )

    save_metrics(query_metrics)


if __name__ == "__main__":
    main()
