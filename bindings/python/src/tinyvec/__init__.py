"""
Tinyvec - A tiny vector database for Python
"""

__version__ = "0.1.3"

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
