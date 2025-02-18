from dataclasses import dataclass
import numpy as np


@dataclass
class TinyVecIndexStats:
    vector_count: int
    dimensions: int


@dataclass
class TinyVecResult:
    similarity: float
    index: int
    metadata: dict | None


@dataclass
class TinyVecInsertion:
    vector: np.ndarray
    metadata: dict | None


@dataclass
class TinyVecConfig:
    dimensions: int
