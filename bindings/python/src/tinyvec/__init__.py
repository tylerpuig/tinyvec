"""
Tinyvec - A tiny vector database for Python
"""

__version__ = "0.2.2"

from .client import TinyVecClient
from .models import ClientConfig, SearchResult, Insertion, IndexStats, SearchOptions, UpdateItem

__all__ = [
    'TinyVecClient',
    'ClientConfig',
    'SearchResult',
    'Insertion',
    'IndexStats',
    'SearchOptions',
    'UpdateItem'
]
