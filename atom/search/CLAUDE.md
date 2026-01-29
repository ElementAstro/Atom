# atom/search - Search and Caching Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **search**

---

## Module Overview

The **atom::search** module provides search functionality, caching systems, and database integration for the Atom framework. It offers efficient in-memory caching, full-text search capabilities, and pluggable database backends.

### Key Features

- **LRU Cache**: Thread-safe Least Recently Used cache with TTL support
- **TTL Cache**: Time-To-Live based caching
- **Resource Cache**: Specialized caching for file and memory resources
- **Search Engine**: Full-text search with tokenization and scoring
- **Database Integration**: SQLite and MySQL support with connection pooling
- **Thread Safety**: All components are thread-safe with fine-grained locking

---

## Directory Structure

```
atom/search/
├── core/              # Core search functionality
│   ├── types.hpp
│   ├── exceptions.hpp
│   ├── document.hpp
│   ├── document.cpp
│   ├── tokenizer.hpp
│   ├── tokenizer.cpp
│   ├── scoring.hpp
│   ├── scoring.cpp
│   ├── search_engine.hpp
│   ├── search_engine.cpp
│   └── search.hpp
├── cache/             # Caching implementations
│   ├── types.hpp
│   ├── exceptions.hpp
│   ├── cache.hpp
│   ├── lru_cache.hpp
│   ├── ttl_cache.hpp
│   └── resource_cache.hpp
├── database/          # Database backends
│   ├── types.hpp
│   ├── base.hpp
│   ├── sqlite.hpp
│   ├── sqlite.cpp
│   ├── mysql.hpp
│   ├── mysql.cpp
│   ├── query_builder.hpp
│   ├── statement_cache.hpp
│   ├── connection_pool.hpp
│   └── retry.hpp
└── root headers (compatibility)
    ├── cache.hpp
    ├── lru.hpp
    ├── ttl.hpp
    ├── sqlite.hpp
    ├── mysql.hpp
    └── search.hpp
```

---

## Core Components

### LRU Cache

Thread-safe Least Recently Used cache with optional TTL support:

```cpp
#include "atom/search/cache/lru_cache.hpp"

using namespace atom::search;

// Create LRU cache with capacity of 1000 items
ThreadSafeLRUCache<std::string, std::string> cache(1000);

// Insert items
cache.put("key1", "value1");

// Insert with TTL (5 minutes)
cache.put("key2", "value2", std::chrono::seconds(300));

// Get items
auto value = cache.get("key1");
if (value) {
    std::cout << "Found: " << *value << "\n";
}

// Check existence
if (cache.contains("key2")) {
    std::cout << "key2 exists\n";
}

// Get statistics
auto stats = cache.getStatistics();
std::cout << "Hit rate: " << stats.hitRate << "%\n";
std::cout << "Load factor: " << stats.loadFactor << "\n";
```

### TTL Cache

Time-To-Live based cache:

```cpp
#include "atom/search/cache/ttl_cache.hpp"

using namespace atom::search;

// Create TTL cache with 60 second default TTL
TTLCache<int, std::string> cache(std::chrono::seconds(60));

// Insert with custom TTL
cache.put(1, "one", std::chrono::seconds(30));

// Get item (auto-expires after TTL)
auto value = cache.get(1);
if (value) {
    std::cout << *value << "\n";
}
```

### Search Engine

Full-text search with tokenization and scoring:

```cpp
#include "atom/search/core/search_engine.hpp"

using namespace atom::search;

SearchEngine engine;

// Add documents
Document doc1;
doc1.id = "1";
doc1.title = "Hello World";
doc1.content = "This is a sample document";
engine.addDocument(doc1);

Document doc2;
doc2.id = "2";
doc2.title = "Goodbye World";
doc2.content = "Another sample document";
engine.addDocument(doc2);

// Search
auto results = engine.search("sample document");
for (const auto& result : results) {
    std::cout << result.id << " (score: " << result.score << ")\n";
}
```

### SQLite Database

Thread-safe SQLite wrapper with prepared statement caching:

```cpp
#include "atom/search/database/sqlite.hpp"

using namespace atom::search;

// Open database
SqliteDB db("test.db");

// Execute query
db.executeQuery("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");

// Execute with parameters
db.executeParameterizedQuery("INSERT INTO users (name) VALUES (?)", "Alice");

// Select data
auto results = db.selectData("SELECT * FROM users");
for (const auto& row : results) {
    std::cout << "ID: " << row[0] << ", Name: " << row[1] << "\n";
}

// Transaction support
db.withTransaction([&]() {
    db.executeParameterizedQuery("INSERT INTO users (name) VALUES (?)", "Bob");
    db.executeParameterizedQuery("INSERT INTO users (name) VALUES (?)", "Charlie");
});

// Single value queries
auto count = db.getIntValue("SELECT COUNT(*) FROM users");
```

---

## Public Interfaces

### ThreadSafeLRUCache Class

```cpp
template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class ThreadSafeLRUCache {
public:
    using BatchKeyType = std::vector<Key>;
    using BatchValueType = std::vector<std::shared_ptr<Value>>;

    struct CacheStatistics {
        size_t hitCount{0};
        size_t missCount{0};
        float hitRate{0.0f};
        size_t size{0};
        size_t maxSize{0};
        float loadFactor{0.0f};
    };

    // Construction
    explicit ThreadSafeLRUCache(size_t max_size);

    // Core operations
    std::optional<Value> get(const Key& key);
    std::shared_ptr<Value> getShared(const Key& key) noexcept;
    BatchValueType getBatch(const BatchKeyType& keys) noexcept;
    bool contains(const Key& key) const noexcept;

    void put(const Key& key, Value value,
             std::optional<std::chrono::seconds> ttl = std::nullopt);
    void putBatch(const std::vector<KeyValuePair>& items,
                  std::optional<std::chrono::seconds> ttl = std::nullopt);

    bool erase(const Key& key) noexcept;
    void clear() noexcept;

    // Accessors
    std::vector<Key> keys() const;
    std::vector<Value> values() const;
    std::optional<KeyValuePair> popLru() noexcept;

    size_t size() const noexcept;
    size_t capacity() const noexcept;
    bool empty() const noexcept;

    // Statistics
    CacheStatistics getStatistics() const noexcept;
    void resetStatistics() noexcept;

    // Async operations
    std::future<std::optional<Value>> asyncGet(const Key& key);
    std::future<void> asyncPut(const Key& key, Value value,
                              std::optional<std::chrono::seconds> ttl = std::nullopt);

    // Callbacks
    void setInsertCallback(std::function<void(const Key&, const Value&)> callback);
    void setEraseCallback(std::function<void(const Key&, const Value&)> callback);
};
```

### SqliteDB Class

```cpp
class SqliteDB {
public:
    using RowData = Vector<String>;
    using ResultSet = Vector<RowData>;

    // Construction
    explicit SqliteDB(std::string_view dbPath);

    // Query execution
    [[nodiscard]] bool executeQuery(std::string_view query);

    template <typename... Args>
    [[nodiscard]] bool executeParameterizedQuery(std::string_view query,
                                                 Args&&... params);

    // Data retrieval
    [[nodiscard]] ResultSet selectData(std::string_view query);

    template <typename... Args>
    [[nodiscard]] ResultSet selectParameterizedData(std::string_view query,
                                                    Args&&... params);

    // Single value retrieval
    template <typename T>
    [[nodiscard]] std::optional<T> getSingleValue(std::string_view query);

    [[nodiscard]] std::optional<int> getIntValue(std::string_view query);
    [[nodiscard]] std::optional<double> getDoubleValue(std::string_view query);
    [[nodiscard]] std::optional<String> getTextValue(std::string_view query);

    // Transactions
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();
    void withTransaction(const std::function<void()>& operations);

    // Database info
    [[nodiscard]] bool isConnected() const noexcept;
    [[nodiscard]] bool tableExists(std::string_view tableName);
    [[nodiscard]] ResultSet getTableSchema(std::string_view tableName);
    [[nodiscard]] std::vector<String> getTables();

    // Utility
    [[nodiscard]] bool vacuum();
    [[nodiscard]] bool analyze();
    [[nodiscard]] bool integrityCheck();
    [[nodiscard]] bool backup(std::string_view destPath);
    [[nodiscard]] static std::string getVersion();
};
```

---

## Dependencies

### Required Dependencies

- **atom::type**: Type utilities
- **atom::io**: I/O operations
- **spdlog**: Logging framework (compiled library)
- **SQLite3**: Database engine (required on non-MSVC)

### Optional Dependencies

- **libmariadb**: MySQL/MariaDB support
- **fmt**: Enhanced formatting

### Platform-Specific Notes

**Windows (MSVC):**

- SQLite3 is optional but recommended
- Uses bundled SQLite3 if found via `find_package(SQLite3)`

**Linux/macOS:**

- SQLite3 is required (via libsqlite3-dev)

---

## Build Configuration

### CMake Options

```cmake
# Build search module
-DBUILD_SEARCH=ON

# The module automatically finds SQLite3 and libmariadb
```

### Database Backend Detection

```bash
# Check SQLite3 detection
cmake -B build
# Look for: "SQLite3 found" or "SQLite3 not found"

# Check MariaDB detection
# Look for: "Found libmariadb: X.X.X" or "libmariadb not found"
```

---

## Usage Examples

### Cache with Persistence

```cpp
#include "atom/search/cache/lru_cache.hpp"

using namespace atom::search;

ThreadSafeLRUCache<std::string, std::string> cache(1000);

// Save to file
cache.saveToFile("cache.dat", [](const std::string& key, const std::string& value) {
    return key + ":" + value;
});

// Load from file
cache.loadFromFile("cache.dat", [](const std::string& line) {
    auto pos = line.find(':');
    return std::make_pair(line.substr(0, pos), line.substr(pos + 1));
});
```

### Batch Operations

```cpp
#include "atom/search/cache/lru_cache.hpp"

using namespace atom::search;

ThreadSafeLRUCache<int, std::string> cache(1000);

// Batch insert
std::vector<std::pair<int, std::string>> items = {
    {1, "one"}, {2, "two"}, {3, "three"}
};
cache.putBatch(items);

// Batch get
std::vector<int> keys = {1, 2, 3, 4};
auto values = cache.getBatch(keys);
for (size_t i = 0; i < keys.size(); ++i) {
    if (values[i]) {
        std::cout << keys[i] << " -> " << *values[i] << "\n";
    }
}
```

### Database Connection Pooling

```cpp
#include "atom/search/database/sqlite.hpp"
#include "atom/search/database/connection_pool.hpp"

using namespace atom::search;

// Create connection pool
ConnectionPool<SqliteDB> pool(
    []() { return std::make_unique<SqliteDB>("test.db"); },
    5  // 5 connections
);

// Acquire connection
auto conn = pool.acquire();
conn->executeQuery("CREATE TABLE IF NOT EXISTS users ...");

// Connection automatically returned to pool
```

---

## Performance Considerations

### Cache Sizing

```cpp
// Good: Size based on expected usage
ThreadSafeLRUCache<std::string, std::string> cache(
    1000,  // Approximate number of items
    std::chrono::seconds(300)  // 5 minute TTL
);

// Bad: Too small causes thrashing
ThreadSafeLRUCache<std::string, std::string> cache(10);  // Too small!

// Bad: Too large wastes memory
ThreadSafeLRUCache<std::string, std::string> cache(10000000);  // Too big!
```

### Database Query Optimization

```cpp
// Good: Use prepared statements (parameterized queries)
db.executeParameterizedQuery("SELECT * FROM users WHERE id = ?", userId);

// Bad: String concatenation (SQL injection risk + slower)
db.executeQuery("SELECT * FROM users WHERE id = " + std::to_string(userId));

// Good: Use transactions for bulk inserts
db.withTransaction([&]() {
    for (const auto& item : items) {
        db.executeParameterizedQuery("INSERT INTO table VALUES (?, ?)", item.a, item.b);
    }
});
```

---

## Testing

### Test Organization

Tests are located in `tests/search/`:

- `test_cache.cpp`: Cache implementation tests
- `test_lru.cpp`: LRU cache tests
- `test_ttl.cpp`: TTL cache tests
- `test_database.cpp`: Database tests
- `test_search.cpp`: Search engine tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run search tests
ctest -R search_ --output-on-failure
```

---

## Thread Safety

### Cache Thread Safety

All cache operations are thread-safe:

- Multiple threads can call `get()`, `put()`, `erase()` concurrently
- Fine-grained locking minimizes contention
- Reader-writer locks allow concurrent reads

### Database Thread Safety

`SqliteDB` uses the ThreadSafeMixin for thread-safe operations:

- Each connection has its own mutex
- Prepared statements are cached per connection
- Use connection pooling for multi-threaded access

---

## Error Handling

### Cache Exceptions

```cpp
try {
    cache.put(key, value);
} catch (const LRUCacheException& e) {
    std::cerr << "Cache error: " << e.what() << "\n";
}
```

### Database Exceptions

```cpp
try {
    db.executeQuery(query);
} catch (const DatabaseException& e) {
    std::cerr << "Database error: " << e.what() << "\n";
}
```

---

## Best Practices

### Cache Usage

**DO:**

- Set appropriate cache sizes based on usage patterns
- Use TTL for time-sensitive data
- Monitor hit rates to optimize cache size
- Use callbacks for cache eviction logging

**DON'T:**

- Store large objects in cache (use pointers instead)
- Set cache size too small (causes thrashing)
- Forget to handle cache misses

### Database Usage

**DO:**

- Always use parameterized queries
- Use transactions for bulk operations
- Close connections when done
- Use connection pooling for multi-threaded access

**DON'T:**

- Concatenate SQL strings (SQL injection risk)
- Forget to commit transactions
- Hold connections open longer than necessary

---

## Related Modules

- **atom::io**: File I/O for cache persistence
- **atom::async**: Async cache operations
- **atom::type**: Type utilities for variant values

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented cache, database, search components
- Added usage examples and best practices

---

**Maintained By:** Atom Framework Team
