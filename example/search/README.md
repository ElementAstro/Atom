# Atom Search Examples

This directory contains comprehensive examples demonstrating all features and capabilities of the Atom Search module. The examples are organized into logical categories and build upon each other to provide a complete understanding of the available functionality.

## Directory Structure

The examples are organized to mirror the structure of `atom/search/`, with each subdirectory containing examples for the corresponding module. Each module typically has both basic and comprehensive examples.

```text
example/search/
├── README.md                                    # This file
├── CMakeLists.txt                               # Build configuration
├── core/                                        # Core search engine examples
│   ├── search_basic.cpp                        # Basic search engine usage
│   └── search_engine_comprehensive.cpp         # Advanced search features
├── cache/                                       # Caching system examples
│   ├── cache_basic.cpp                         # Basic ResourceCache usage
│   ├── resource_cache_comprehensive.cpp        # Advanced ResourceCache features
│   ├── lru_basic.cpp                           # Basic LRU cache usage
│   ├── lru_cache_comprehensive.cpp             # Advanced LRU cache features
│   ├── ttl_basic.cpp                           # Basic TTL cache usage
│   └── ttl_cache_comprehensive.cpp             # Advanced TTL cache features
├── database/                                    # Database integration examples
│   ├── sqlite_basic.cpp                        # Basic SQLite usage
│   ├── sqlite_comprehensive.cpp                # Advanced SQLite features
│   └── mysql_comprehensive.cpp                 # MySQL integration
├── integration/                                 # Integration examples
│   └── search_cache_database_integration.cpp   # Combined usage patterns
└── advanced/                                    # Advanced features
    ├── async_batch_statistics.cpp              # Async operations & statistics
    └── error_handling_edge_cases.cpp           # Error handling patterns
```

## Example Types

### Basic Examples

Simple, focused examples demonstrating core functionality of each component. These are great starting points for learning the API.

### Comprehensive Examples

In-depth examples covering advanced features, edge cases, and best practices. These demonstrate production-ready usage patterns.

## Example Categories

### 1. Core Search Engine (`core/`)

Demonstrates the main search engine functionality from `atom/search/core/search.cpp`.

#### search_basic.cpp

- Basic document creation and indexing
- Simple search operations (by tag, by content)
- Document retrieval and display
- Introduction to the search API

#### search_engine_comprehensive.cpp

- Document creation and validation
- Basic search operations (by tag, content, multiple tags, boolean)
- Advanced search features (fuzzy search, autocomplete)
- Persistence with save and load operations
- Click tracking and result ranking
- Error handling and edge cases

### 2. Caching Systems (`cache/`)

Demonstrates the caching implementations from `atom/search/cache/`.

#### cache_basic.cpp

- Basic ResourceCache operations (insert, get, remove)
- Simple resource management
- Cache statistics
- Introduction to the ResourceCache API

#### resource_cache_comprehensive.cpp

- Advanced cache operations (insert, get, remove)
- Resource expiration handling
- LRU eviction policy
- Asynchronous operations
- Batch operations
- Serialization and persistence (text and JSON)
- Event callbacks and statistics
- Configuration and cleanup

#### lru_basic.cpp

- Basic LRU cache operations
- Simple eviction policy demonstration
- Cache hit/miss tracking
- Introduction to the LRU cache API

#### lru_cache_comprehensive.cpp

- Thread-safe LRU cache operations
- TTL and expiration handling with default TTL
- Batch put/get and prefetch
- Async get/put and persistence
- Callbacks (insert/erase/clear) and statistics
- Keys/values iteration and dynamic resizing
- Thread-safety demo and edge cases

#### ttl_basic.cpp

- Basic TTL cache operations
- Time-based expiration
- Simple cache cleanup
- Introduction to the TTL cache API

#### ttl_cache_comprehensive.cpp

- TTL-based cache expiration and automatic cleanup
- Complex data types and move semantics
- Batch operations and compute pattern (get_or_compute)
- Emplace construction and shared access
- TTL management (update_ttl, get_remaining_ttl) and force_cleanup
- Eviction callbacks and configuration management
- Capacity reservation and statistics reset
- Thread safety demonstrations

### 3. Database Integration (`database/`)

Demonstrates database backends from `atom/search/database/`.

#### sqlite_basic.cpp

- Basic SQLite database operations
- Simple CRUD operations
- Query execution
- Introduction to the SQLite wrapper API

#### sqlite_comprehensive.cpp

- Database creation and connection
- CRUD operations with error handling
- Parameterized queries for security
- Transaction management
- Full-text search capabilities
- Data validation and integrity checks
- Performance optimization techniques

#### mysql_comprehensive.cpp

- MySQL connection and configuration
- Prepared statements for secure queries
- Transaction management with commit/rollback
- Connection pooling and error handling
- Batch operations for performance
- Advanced MySQL features

### 4. Integration Examples (`integration/`)

#### search_cache_database_integration.cpp

- Real-world document management system
- Combines search engine, cache, and database
- Performance optimization with caching
- Asynchronous operations
- Statistics and monitoring
- Error handling and recovery

### 5. Advanced Features (`advanced/`)

#### async_batch_statistics.cpp

- Asynchronous search operations with futures
- Batch processing for high-throughput scenarios
- Real-time statistics monitoring
- Performance optimization techniques
- Concurrent cache operations
- Load balancing and resource management

#### error_handling_edge_cases.cpp

- Comprehensive error handling patterns
- Edge case scenarios and recovery mechanisms
- Input validation and sanitization
- Resource exhaustion testing
- Concurrent error handling
- Logging and debugging techniques

## Building the Examples

### Prerequisites

- C++20 compatible compiler
- CMake 3.10 or higher
- Atom Search library

### Build Instructions

1. **Build all examples:**

   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

2. **Build specific category:**

   ```bash
   cmake -DATOM_EXAMPLE_SEARCH_BUILD_ALL=OFF -DATOM_EXAMPLE_SEARCH_CORE_SEARCH_ENGINE_COMPREHENSIVE=ON ..
   make
   ```

3. **Available build options:**
   - `ATOM_EXAMPLE_SEARCH_BUILD_ALL` - Build all examples (default: ON)
   - `ATOM_EXAMPLE_SEARCH_CORE_*` - Core examples
   - `ATOM_EXAMPLE_SEARCH_CACHE_*` - Cache examples
   - `ATOM_EXAMPLE_SEARCH_DATABASE_*` - Database examples
   - `ATOM_EXAMPLE_SEARCH_INTEGRATION_*` - Integration examples
   - `ATOM_EXAMPLE_SEARCH_ADVANCED_*` - Advanced examples

### Generated Executables

Examples are built with the naming convention: `search_[category]_[example_name]`

**Core Examples:**

- `search_core_search_basic`
- `search_core_search_engine_comprehensive`

**Cache Examples:**

- `search_cache_cache_basic`
- `search_cache_resource_cache_comprehensive`
- `search_cache_lru_basic`
- `search_cache_lru_cache_comprehensive`
- `search_cache_ttl_basic`
- `search_cache_ttl_cache_comprehensive`

**Database Examples:**

- `search_database_sqlite_basic`
- `search_database_sqlite_comprehensive`
- `search_database_mysql_comprehensive`

**Integration Examples:**

- `search_integration_search_cache_database_integration`

**Advanced Examples:**

- `search_advanced_async_batch_statistics`
- `search_advanced_error_handling_edge_cases`

## Running the Examples

### Basic Usage

Start with the basic examples to learn the fundamentals:

```bash
# Run basic search engine example
./search_core_search_basic

# Run basic cache examples
./search_cache_cache_basic
./search_cache_lru_basic
./search_cache_ttl_basic

# Run basic database example
./search_database_sqlite_basic
```

Then explore the comprehensive examples for advanced features:

```bash
# Run comprehensive search engine example
./search_core_search_engine_comprehensive

# Run comprehensive cache examples
./search_cache_resource_cache_comprehensive
./search_cache_lru_cache_comprehensive
./search_cache_ttl_cache_comprehensive

# Run comprehensive database examples
./search_database_sqlite_comprehensive
./search_database_mysql_comprehensive

# Run integration example
./search_integration_search_cache_database_integration

# Run advanced examples
./search_advanced_async_batch_statistics
./search_advanced_error_handling_edge_cases
```

### Example Output

Each example provides detailed output showing:

- Step-by-step operations
- Performance metrics
- Error handling demonstrations
- Statistics and monitoring information

## Key Features Demonstrated

### Search Engine Features

- ✅ Document management (add, update, remove)
- ✅ Multiple search types (tag, content, boolean, fuzzy)
- ✅ Autocomplete functionality
- ✅ Persistence and serialization
- ✅ Click tracking and ranking
- ✅ Multithreaded operations

### Caching Features

- ✅ LRU eviction policy
- ✅ TTL-based expiration
- ✅ Thread-safe operations
- ✅ Batch processing
- ✅ Event callbacks
- ✅ Performance monitoring
- ✅ Dynamic resizing

### Database Features

- ✅ SQLite and MySQL support
- ✅ Parameterized queries
- ✅ Transaction management
- ✅ Full-text search
- ✅ Connection pooling
- ✅ Error handling
- ✅ Performance optimization

### Advanced Features

- ✅ Asynchronous operations
- ✅ Batch processing
- ✅ Real-time statistics
- ✅ Concurrent access
- ✅ Resource management
- ✅ Error recovery

## Best Practices Demonstrated

1. **Error Handling**: Comprehensive exception handling patterns
2. **Resource Management**: Proper RAII and memory management
3. **Performance**: Optimization techniques and monitoring
4. **Thread Safety**: Safe concurrent operations
5. **Modularity**: Clean separation of concerns
6. **Documentation**: Well-commented code with clear explanations

## Troubleshooting

### Common Issues

1. **Compilation Errors**: Ensure C++20 support and proper include paths
2. **Missing Dependencies**: Verify Atom Search library is properly installed
3. **Database Connection**: Check database server status and credentials
4. **Performance Issues**: Monitor system resources and adjust thread counts

### Getting Help

- Check the inline documentation in each example
- Review the error messages for specific guidance
- Consult the main Atom Search documentation
- Examine the integration example for real-world usage patterns

## Contributing

When adding new examples:

1. Follow the established naming conventions
2. Include comprehensive documentation
3. Add appropriate error handling
4. Update the CMakeLists.txt file
5. Update this README with the new example information

## License

These examples are part of the Atom Search project and follow the same licensing terms.
