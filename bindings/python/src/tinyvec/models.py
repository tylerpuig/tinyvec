from dataclasses import dataclass
from .types import VectorInput


@dataclass
class IndexStats:
    vector_count: int
    dimensions: int


@dataclass
class SearchResult:
    similarity: float
    index: int
    metadata: dict | None


@dataclass
class Insertion:
    vector: VectorInput
    metadata: dict | None


@dataclass
class ClientConfig:
    dimensions: int | None = None


@dataclass
class SearchOptions:
    filter: dict | None = None


@dataclass
class DeletionResult:
    deleted_count: int
    success: bool
