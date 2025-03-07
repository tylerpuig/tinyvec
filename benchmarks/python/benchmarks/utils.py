import numpy as np
from typing import List, Dict
import psutil
import json
from dataclasses import dataclass
import os
import time
import gc
import sys
DIMENSIONS = 512


def generate_random_embeddings(num_vectors: int, dim: int) -> List[np.ndarray]:
    """Generate random embeddings for testing."""
    embeddings = []
    for _ in range(num_vectors):
        # Generate random vector without normalization
        vec = np.random.random(dim)
        embeddings.append(vec)
    return embeddings


def warmup_memory():
    # Force garbage collection
    gc.collect()
    # Perform some dummy operations to stabilize memory
    dummy_vec = generate_random_embeddings(10, DIMENSIONS)
    time.sleep(1)


def get_stable_memory_usage(samples: int = 5, delay: float = 0.5) -> Dict[str, float]:
    measurements = []
    for _ in range(samples):
        measurements.append(get_memory_usage())
        time.sleep(delay)

    # Average the measurements
    return {
        key: sum(m[key] for m in measurements) / len(measurements)
        for key in measurements[0].keys()
    }


def get_detailed_memory_info():
    process = psutil.Process()
    mem = process.memory_info()
    return {
        'rss_mb': mem.rss / 1024 / 1024,
        'vms_mb': mem.vms / 1024 / 1024,
        # Python-specific allocation
        'python_allocated': sys.getsizeof([])/1024/1024,
        # NumPy-specific allocation
        'numpy_allocated': np.zeros(1).nbytes/1024/1024
    }


def get_memory_usage() -> Dict[str, float]:
    process = psutil.Process()
    mem = process.memory_info()
    return {
        'rss_mb': mem.rss / 1024 / 1024,
        'vms_mb': mem.vms / 1024 / 1024,
        'shared_mb': getattr(mem, 'shared', 0) / 1024 / 1024,
        'private_mb': getattr(mem, 'private', 0) / 1024 / 1024
    }


@dataclass
class QueryMetrics:
    database_title: str
    query_time: float
    initial_memory: Dict[str, float]
    final_memory: Dict[str, float]
    benchmark_type: str


def save_metrics(metrics: QueryMetrics, filename: str = 'metrics.json'):
    # Convert metrics to dictionary
    new_metrics = {
        'initial_memory': metrics.initial_memory,
        'final_memory': metrics.final_memory,
        'query_time': metrics.query_time,
        'database_title': metrics.database_title,
        'benchmark_type': metrics.benchmark_type,
    }

    # Load existing data if file exists
    existing_metrics = []
    if os.path.exists(filename):
        with open(filename, 'r') as f:
            try:
                existing_metrics = json.load(f)
                # Ensure it's a list
                if not isinstance(existing_metrics, list):
                    existing_metrics = [existing_metrics]
            except json.JSONDecodeError:
                # Handle case where file is empty or invalid
                existing_metrics = []

    # Append new metrics
    existing_metrics.append(new_metrics)

    # Write back to file
    with open(filename, 'w') as f:
        json.dump(existing_metrics, f, indent=2)


def get_avg_search_time(times: List[float]):
    return sum(times) / len(times)
