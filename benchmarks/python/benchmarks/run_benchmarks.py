import pandas as pd
from typing import List, Dict
import os
import json
import subprocess
from pathlib import Path
import time

PYTHON_DIR = str(Path(__file__).parent.parent)
os.environ['PYTHONPATH'] = f"{PYTHON_DIR}"


def run_search_benchmarks(search_scripts: List[str], output_file: str = 'metrics.json'):
    """
    Run multiple search benchmark scripts using Poetry environment
    """
    processes = []
    env = os.environ.copy()

    # Start all processes using Poetry
    for script in search_scripts:
        print(f"Starting benchmark: {script}")
        # Use poetry run python for each script
        process = subprocess.Popen(
            ['poetry', 'run', 'python', script],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
            cwd=str(Path(__file__).parent)
        )
        processes.append((script, process))

        stdout, stderr = process.communicate()

        if process.returncode != 0:
            print(f"Error running {script}:")
            print(stderr.decode())
        else:
            print(f"Completed: {script}")
            print(stdout.decode())

        time.sleep(1)
    # Wait for all processes to complete
    # for script, process in processes:
    #     stdout, stderr = process.communicate()
    #     if process.returncode != 0:
    #         print(f"Error running {script}:")
    #         print(stderr.decode())
    #     else:
    #         print(f"Completed: {script}")
    #         print(stdout.decode())  # Print any stdout output


def format_memory_diff(initial: Dict[str, float], final: Dict[str, float]) -> Dict[str, float]:
    """Calculate memory differences between initial and final states."""
    return {
        k: final[k] - initial[k]
        for k in initial.keys()
    }


def parse_results(metrics_file: str = 'metrics.json') -> pd.DataFrame:
    """
    Parse and format the benchmark results into a readable table.
    """
    if not os.path.exists(metrics_file):
        raise FileNotFoundError(f"No results file found at {metrics_file}")

    with open(metrics_file, 'r') as f:
        results = json.load(f)

    # Format results into a table
    formatted_results = []
    for result in results:
        memory_diff = format_memory_diff(
            result['initial_memory'], result['final_memory'])
        formatted_results.append({
            'Database': result.get('database_title', 'Unknown'),
            'Query Time (ms)': round(result['query_time'] * 1000, 2),
            'RSS Δ (MB)': round(memory_diff['rss_mb'], 2),
            'VMS Δ (MB)': round(memory_diff['vms_mb'], 2),
            'Shared Δ (MB)': round(memory_diff['shared_mb'], 2),
            'Private Δ (MB)': round(memory_diff['private_mb'], 2)
        })

    return pd.DataFrame(formatted_results)


def main():
    # Define your search scripts
    search_scripts = [
        'clients/qdrant_search.py',
        'clients/tinyvec_search.py',
        'clients/chroma_search.py',
        'clients/sqlite_vec_search.py',
        'clients/lance_search.py'
    ]

    # Run all benchmarks
    run_search_benchmarks(search_scripts)

    # Parse and display results
    try:
        results_df = parse_results()
        print("\nBenchmark Results:")
        print(results_df.to_string(index=False))

        # Optionally save to CSV
        # results_df.to_csv('benchmark_results.csv', index=False)
    except FileNotFoundError as e:
        print(f"Error: {e}")
    except Exception as e:
        print(f"Error parsing results: {e}")


if __name__ == "__main__":
    main()
