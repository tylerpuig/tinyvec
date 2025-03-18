import numpy as np
import random
from typing import List


def generate_random_vector(dimensions: int) -> np.ndarray:
    """Generate a random vector with given dimensions."""
    return np.random.rand(dimensions).astype(np.float32)


def generate_random_vector_list(dimensions: int) -> List[float]:
    """Generate a random vector with given dimensions."""
    return [random.random() for _ in range(dimensions)]
