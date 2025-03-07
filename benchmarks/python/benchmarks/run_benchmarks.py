import pandas as pd
from typing import List, Dict, Optional
import os
import json
import subprocess
from pathlib import Path
import time
import argparse
import sys

PYTHON_DIR = str(Path(__file__).parent.parent)
os.environ['PYTHONPATH'] = f"{PYTHON_DIR}"

# Define script paths
VECTOR_SEARCH_SCRIPTS = [
    'search/qdrant_search.py',
    'search/tinyvec_search.py',
    'search/chroma_search.py',
    'search/sqlite_vec_search.py',
    'search/lance_search.py'
]

FILTER_SEARCH_SCRIPTS = [
    'search_filter/qdrant_search.py',
    'search_filter/tinyvec_search.py',
    'search_filter/chroma_search.py',
    # 'search_filter/sqlite_vec_search.py',
    'search_filter/lance_search.py'
]


def run_search_benchmarks(search_scripts: List[str], output_file: str = 'metrics.json', suffix: Optional[str] = None):
    """
    Run multiple search benchmark scripts using Poetry environment

    Args:
        search_scripts: List of script paths to run
        output_file: File to save metrics
        suffix: Optional suffix to add to output file (e.g., "_filter")
    """
    # Create a unique metrics file for each type of benchmark if suffix is provided
    if suffix:
        base_name, ext = os.path.splitext(output_file)
        output_file = f"{base_name}{suffix}{ext}"

    # Remove existing metrics file if it exists
    if os.path.exists(output_file):
        os.remove(output_file)

    processes = []
    env = os.environ.copy()

    # Set the output file in environment so scripts can use it
    env['METRICS_FILE'] = output_file

    # Start all processes using Poetry
    for script in search_scripts:
        script_path = Path(script)
        if not script_path.exists():
            print(f"Warning: Script {script} does not exist. Skipping.")
            continue

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

    return output_file


def format_memory_diff(initial: Dict[str, float], final: Dict[str, float]) -> Dict[str, float]:
    """Calculate memory differences between initial and final states."""
    return {
        k: final[k] - initial[k]
        for k in initial.keys()
    }


def parse_results(metrics_file: str = 'metrics.json', benchmark_type: str = 'Vector Search') -> pd.DataFrame:
    """
    Parse and format the benchmark results into a readable table.

    Args:
        metrics_file: Path to metrics file
        benchmark_type: Type of benchmark for labeling
    """

    script_dir = os.path.dirname(os.path.abspath(__file__))
    metrics_file = os.path.join(script_dir, 'metrics.json')
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
            'Benchmark Type': benchmark_type,
            'Database': result.get('database_title', 'Unknown'),
            'Query Time (ms)': round(result['query_time'] * 1000, 2),
            'RSS Δ (MB)': round(memory_diff['rss_mb'], 2),
            'VMS Δ (MB)': round(memory_diff['vms_mb'], 2),
            'Shared Δ (MB)': round(memory_diff['shared_mb'], 2),
            'Private Δ (MB)': round(memory_diff['private_mb'], 2)
        })

    return pd.DataFrame(formatted_results)


def setup_cli():
    """Set up command line interface for benchmark runner"""
    parser = argparse.ArgumentParser(
        description='Run vector database search benchmarks')

    parser.add_argument('--vector', action='store_true',
                        help='Run pure vector search benchmarks')
    parser.add_argument('--filter', action='store_true',
                        help='Run metadata filter search benchmarks')
    parser.add_argument('--all', action='store_true',
                        help='Run both vector and filter search benchmarks')
    parser.add_argument('--output', type=str, default='metrics.json',
                        help='Base name for output metrics file')
    parser.add_argument('--save-csv', type=str, default=None,
                        help='Save results to CSV file')

    args = parser.parse_args()

    # If no options provided, default to interactive mode
    if not (args.vector or args.filter or args.all):
        return interactive_mode()

    return args


def interactive_mode():
    """Interactive CLI mode when no arguments are provided"""
    # Create a parser and get a namespace object to maintain type compatibility
    parser = argparse.ArgumentParser()
    parser.add_argument('--vector', action='store_true')
    parser.add_argument('--filter', action='store_true')
    parser.add_argument('--all', action='store_true')
    parser.add_argument('--output', type=str)
    parser.add_argument('--save-csv', type=str)

    # Parse empty args to get a properly typed namespace
    args = parser.parse_args([])

    print("Vector Database Benchmark Runner")
    print("================================")
    print("Please select benchmark types to run:")
    print("1. Pure Vector Search")
    print("2. Metadata Filter Search")
    print("3. Both")
    print("4. Exit")

    choice = input("Enter your choice (1-4): ")

    if choice == '1':
        args.vector = True
        args.filter = False
        args.all = False
    elif choice == '2':
        args.vector = False
        args.filter = True
        args.all = False
    elif choice == '3':
        args.vector = False
        args.filter = False
        args.all = True
    elif choice == '4':
        print("Exiting...")
        sys.exit(0)
    else:
        print("Invalid choice. Exiting.")
        sys.exit(1)

    # Ask for output file
    default_output = 'metrics.json'
    output_file = input(
        f"Enter metrics output file name [default: {default_output}]: ")
    args.output = output_file if output_file else default_output

    # Ask for CSV export
    save_csv = input("Save results to CSV? (y/n) [default: n]: ").lower()
    if save_csv == 'y':
        csv_name = input(
            "Enter CSV filename [default: benchmark_results.csv]: ")
        args.save_csv = csv_name if csv_name else 'benchmark_results.csv'
    else:
        args.save_csv = None

    return args


def main():
    args = setup_cli()

    results_dfs = []

    # Run vector search benchmarks
    if args.vector or args.all:
        print("\n=== Running Pure Vector Search Benchmarks ===")
        vector_metrics = run_search_benchmarks(
            VECTOR_SEARCH_SCRIPTS, args.output, "_vector")
        vector_results = parse_results(vector_metrics, "Vector Search")
        results_dfs.append(vector_results)

        print("\nVector Search Results:")
        print(vector_results.to_string(index=False))

    # Run filter search benchmarks
    if args.filter or args.all:
        print("\n=== Running Metadata Filter Search Benchmarks ===")
        filter_metrics = run_search_benchmarks(
            FILTER_SEARCH_SCRIPTS, args.output, "_filter")
        filter_results = parse_results(filter_metrics, "Filter Search")
        results_dfs.append(filter_results)

        print("\nFilter Search Results:")
        print(filter_results.to_string(index=False))

    # Combine and display all results
    if results_dfs:
        all_results = pd.concat(results_dfs, ignore_index=True)

        print("\nAll Benchmark Results:")
        print(all_results.to_string(index=False))

        # Save to CSV if requested
        if args.save_csv:
            all_results.to_csv(args.save_csv, index=False)
            print(f"\nResults saved to {args.save_csv}")


if __name__ == "__main__":
    main()
