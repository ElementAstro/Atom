# Atom Search Examples

This directory contains comprehensive examples demonstrating all features and capabilities of the Atom Search module. The examples are organized into logical categories and build upon each other to provide a complete understanding of the available functionality.

## Directory Structure

```
example/search/
├── README.md                           # This file
├── CMakeLists.txt                      # Build configuration
├── core/                               # Core search engine examples
│   └── search_engine_comprehensive.cpp
├── cache/                              # Caching system examples
│   ├── resource_cache_comprehensive.cpp
│   ├── lru_cache_comprehensive.cpp
│   └── ttl_cache_comprehensive.cpp
├── database/                           # Database integration examples
│   ├── sqlite_comprehensive.cpp
│   └── mysql_comprehensive.cpp
├── integration/                        # Integration examples
│   └── search_cache_database_integration.cpp
├── advanced/                           # Advanced features
│   ├── async_batch_statistics.cpp
│   └── error_handling_edge_cases.cpp
└── [legacy examples]                   # Backward compatibility
    ├── search.cpp
    ├── cache.cpp
    ├── lru.cpp
    ├── ttl.cpp
    └── sqlite.cpp
```

## Example Categories

### 1. Core Search Engine (`core/`)

**search_engine_comprehensive.cpp**

- Document creation and validation
- Basic search operations (by tag, content, multiple tags, boolean)
- Advanced search features (fuzzy search, autocomplete)
- Persistence with save and load operations
- Click tracking and result ranking
- Error handling and edge cases

### 2. Caching Systems (`cache/`)

**resource_cache_comprehensive.cpp**

- Basic cache operations (insert, get, remove)
- Resource expiration handling
- LRU eviction policy
- Asynchronous operations
- Batch operations
- Serialization and persistence
- Event callbacks and statistics

**lru_cache_comprehensive.cpp**

- Thread-safe LRU cache operations
- TTL and expiration handling
- Concurrent access from multiple threads
- Callbacks for monitoring cache events
- Dynamic resizing and performance optimization
- Prefetching and error handling

**ttl_cache_comprehensive.cpp**

- TTL-based cache expiration
- Automatic cleanup mechanisms
- Complex data type storage
- Batch operations for efficiency
- Performance monitoring and statistics
- Thread safety demonstrations

### 3. Database Integration (`database/`)

**sqlite_comprehensive.cpp**

- Database creation and connection
- CRUD operations with error handling
- Parameterized queries for security
- Transaction management
- Full-text search capabilities
- Data validation and integrity checks
- Performance optimization techniques

**mysql_comprehensive.cpp**

- MySQL connection and configuration
- Prepared statements for secure queries
- Transaction management with commit/rollback
- Connection pooling and error handling
- Batch operations for performance
- Advanced MySQL features

### 4. Integration Examples (`integration/`)

**search_cache_database_integration.cpp**

- Real-world document management system
- Combines search engine, cache, and database
- Performance optimization with caching
- Asynchronous operations
- Statistics and monitoring
- Error handling and recovery

### 5. Advanced Features (`advanced/`)

**async_batch_statistics.cpp**

- Asynchronous search operations with futures
- Batch processing for high-throughput scenarios
- Real-time statistics monitoring
- Performance optimization techniques
- Concurrent cache operations
- Load balancing and resource management

**error_handling_edge_cases.cpp**

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

- `search_core_search_engine_comprehensive`
- `search_cache_resource_cache_comprehensive`
- `search_cache_lru_cache_comprehensive`
- `search_cache_ttl_cache_comprehensive`
- `search_database_sqlite_comprehensive`
- `search_database_mysql_comprehensive`
- `search_integration_search_cache_database_integration`
- `search_advanced_async_batch_statistics`

## Running the Examples

### Basic Usage

```bash
# Run core search engine example
./search_core_search_engine_comprehensive

# Run cache example
./search_cache_lru_cache_comprehensive

# Run database example
./search_database_sqlite_comprehensive

# Run integration example
./search_integration_search_cache_database_integration
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
