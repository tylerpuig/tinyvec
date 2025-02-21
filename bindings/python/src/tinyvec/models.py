from dataclasses import dataclass
from .types import VectorInput


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
    vector: VectorInput
    metadata: dict | None


@dataclass
class TinyVecConfig:
    dimensions: int
