from typing import Optional
from dataclasses import dataclass
from .types import VectorInput, JsonValue


@dataclass
class IndexStats:
    vector_count: int
    dimensions: int


@dataclass
class SearchResult:
    similarity: float
    id: int
    metadata: JsonValue


@dataclass
class Insertion:
    vector: VectorInput
    metadata: JsonValue


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


@dataclass
class UpdateItem:
    id: int
    metadata: Optional[JsonValue] = None
    vector: Optional[VectorInput] = None

    def __post_init__(self):
        if self.metadata is None and self.vector is None:
            raise ValueError("Either metadata or vector must be provided")


@dataclass
class PaginationConfig:
    skip: int
    limit: int

    def __post_init__(self):
        if self.skip < 0:
            raise ValueError("Skip must be a positive number")
        if self.limit <= 0:
            raise ValueError("Limit must be a positive number")


@dataclass
class PaginationItem:
    id: int
    metadata: JsonValue
    vector: list[float]


@dataclass
class UpdateResult:
    updated_count: int
    success: bool
