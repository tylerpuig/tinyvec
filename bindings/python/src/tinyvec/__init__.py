"""
Tinyvec - A tiny vector database for Python
"""

__version__ = "0.2.0"

from .client import TinyVecClient
from .models import ClientConfig, SearchResult, Insertion, IndexStats, SearchOptions

__all__ = [
    'TinyVecClient',
    'ClientConfig',
    'SearchResult',
    'Insertion',
    'IndexStats',
    'SearchOptions'
]
