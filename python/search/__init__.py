"""
Atom Search Module

This module provides comprehensive search, caching, and database functionality for the Atom project.

Modules:
    search: Core search engine with document indexing and querying capabilities
    cache: Resource cache with TTL expiration and LRU eviction
    lru: Thread-safe LRU cache with advanced features
    ttl: Time-to-live cache with configurable behavior and eviction callbacks
    mysql: MySQL/MariaDB database interface with connection management
    sqlite: SQLite database interface with thread-safe operations

Examples:
    >>> from atom.search import SearchEngine, Document
    >>> from atom.search.cache import StringCache
    >>> from atom.search.mysql import MysqlDB
    >>> from atom.search.sqlite import SqliteDB
    >>> from atom.search.ttl import TTLCache, CacheConfig

    # Search functionality
    >>> engine = SearchEngine()
    >>> doc = Document("1", "Hello world", ["greeting", "example"])
    >>> engine.addDocument(doc)

    # Caching functionality
    >>> cache = StringCache(5000, 100)  # 5-second TTL, 100 items max
    >>> cache.put("key", "value")

    # Database functionality
    >>> db = SqliteDB("example.db")
    >>> db.execute_query("CREATE TABLE test (id INTEGER, name TEXT)")
"""

# Import core search functionality
try:
    from .search import SearchEngine, Document
    __all__ = ["SearchEngine", "Document"]
except ImportError:
    __all__ = []

# Import cache modules
try:
    from . import cache
    __all__.extend(["cache"])
except ImportError:
    pass

try:
    from . import lru
    __all__.extend(["lru"])
except ImportError:
    pass

try:
    from . import ttl
    __all__.extend(["ttl"])
except ImportError:
    pass

# Import database modules
try:
    from . import mysql
    __all__.extend(["mysql"])
except ImportError:
    pass

try:
    from . import sqlite
    __all__.extend(["sqlite"])
except ImportError:
    pass

# Version information
__version__ = "1.0.0"
__author__ = "Atom Project"
__description__ = "Comprehensive search, caching, and database functionality"
