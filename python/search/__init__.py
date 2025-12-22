"""
Atom Search Module

This module provides comprehensive search, caching, and database functionality for the Atom project.

The module is organized into submodules matching the C++ structure:

Submodules:
    core: Core search engine with document indexing and querying capabilities
        - search: SearchEngine, Document classes
    cache: Caching implementations
        - cache: Resource cache with TTL expiration
        - lru: Thread-safe LRU cache with advanced features
        - ttl: Time-to-live cache with configurable behavior
    database: Database interfaces
        - sqlite: SQLite database interface with thread-safe operations
        - mysql: MySQL/MariaDB database interface with connection management

Examples:
    >>> from atom.search import SearchEngine, Document
    >>> from atom.search.cache import lru, ttl, cache
    >>> from atom.search.database import mysql, sqlite

    # Search functionality
    >>> engine = SearchEngine()
    >>> doc = Document("1", "Hello world", ["greeting", "example"])
    >>> engine.addDocument(doc)

    # Caching functionality (new structure)
    >>> from atom.search.cache.lru import StringCache
    >>> cache = StringCache(100)  # 100 items max
    >>> cache.put("key", "value")

    # Database functionality (new structure)
    >>> from atom.search.database.sqlite import SqliteDB
    >>> db = SqliteDB("example.db")
    >>> db.execute_query("CREATE TABLE test (id INTEGER, name TEXT)")
"""

import warnings

__version__ = "1.0.0"
__author__ = "Atom Project"
__description__ = "Comprehensive search, caching, and database functionality"

# Import core search functionality for backward compatibility
try:
    from .core.search import Document, SearchEngine

    __all__ = ["SearchEngine", "Document"]
except ImportError as e:
    warnings.warn(
        f"Failed to import core search module: {e}", ImportWarning, stacklevel=2
    )
    __all__ = []

# Import submodule packages
try:
    from . import cache as cache_pkg

    __all__.append("cache")
except ImportError as e:
    warnings.warn(f"Failed to import cache module: {e}", ImportWarning, stacklevel=2)
    cache_pkg = None

try:
    from . import core

    __all__.append("core")
except ImportError as e:
    warnings.warn(f"Failed to import core module: {e}", ImportWarning, stacklevel=2)
    core = None

try:
    from . import database

    __all__.append("database")
except ImportError as e:
    warnings.warn(f"Failed to import database module: {e}", ImportWarning, stacklevel=2)
    database = None

# Backward compatibility aliases - import individual modules at top level
try:
    from .cache import cache, lru, ttl

    __all__.extend(["lru", "ttl"])
except ImportError:
    pass

try:
    from .database import mysql, sqlite

    __all__.extend(["mysql", "sqlite"])
except ImportError:
    pass


def get_available_modules():
    """Get a list of successfully imported modules.

    Returns:
        List of module names that were successfully imported.
    """
    modules = []
    for module_name in ["cache", "core", "database", "lru", "ttl", "mysql", "sqlite"]:
        if globals().get(module_name) is not None:
            modules.append(module_name)
    return modules
