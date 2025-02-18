import os
import struct


def ensure_absolute_path(file_path: str) -> str:
    """Convert relative path to absolute if needed."""
    if not os.path.isabs(file_path):
        return os.path.abspath(file_path)
    return file_path


def create_db_files(file_path: str, dimensions: int = 0):
    """
    Create vector storage files with initial header.
    Raises FileExistsError if any of the files already exist.
    """
    absolute_path = ensure_absolute_path(file_path)
    vector_count = 0

    # Pack header data: two 32-bit integers for vector_count and dimensions
    header = struct.pack('<ii', vector_count, dimensions)

    # Creates a new file and fails if it exists
    try:
        with open(absolute_path, 'xb') as f:
            f.write(header)
            # Force flush to disk
            f.flush()
            os.fsync(f.fileno())

        # Create empty metadata files
        for suffix in ['.idx', '.meta']:
            with open(absolute_path + suffix, 'xb') as f:
                f.flush()
                os.fsync(f.fileno())

    except FileExistsError as e:
        # Clean up any partially created files
        for path in [absolute_path, absolute_path + '.idx', absolute_path + '.meta']:
            try:
                os.unlink(path)
            except FileNotFoundError:
                pass
        raise FileExistsError(
            f"One or more vector files already exist at {absolute_path}") from e


def file_exists(file_path: str) -> bool:
    """Check if a file exists at the given path."""
    return os.path.exists(file_path)
