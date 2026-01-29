#ifndef ATOM_SEARCH_TEST_SQLITE_HPP
#define ATOM_SEARCH_TEST_SQLITE_HPP

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

#include "atom/search/database/sqlite.hpp"

using namespace atom::search;

class SqliteDBTest : public ::testing::Test {
protected:
    std::unique_ptr<SqliteDB> db;
    std::string testDbPath;

    void SetUp() override {
        // Use in-memory database for most tests
        testDbPath = ":memory:";
        db = std::make_unique<SqliteDB>(testDbPath);

        // Create test table
        ASSERT_TRUE(db->executeQuery(R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                email TEXT UNIQUE,
                age INTEGER,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )"));
    }

    void TearDown() override {
        db.reset();
        // Clean up file-based test databases
        if (testDbPath != ":memory:" && std::filesystem::exists(testDbPath)) {
            std::filesystem::remove(testDbPath);
        }
    }

    void createFileBasedDB(const std::string& filename) {
        testDbPath = filename;
        db = std::make_unique<SqliteDB>(testDbPath);

        // Create test table
        ASSERT_TRUE(db->executeQuery(R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                email TEXT UNIQUE,
                age INTEGER,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )"));
    }
};

// Basic CRUD Operations Tests
TEST_F(SqliteDBTest, BasicInsert) {
    EXPECT_TRUE(
        db->executeQuery("INSERT INTO users (name, email, age) VALUES "
                         "('Alice', 'alice@test.com', 25)"));
    EXPECT_GT(db->getLastInsertRowId(), 0);
    EXPECT_EQ(db->getChanges(), 1);
}

TEST_F(SqliteDBTest, BasicSelect) {
    // Insert test data
    ASSERT_TRUE(
        db->executeQuery("INSERT INTO users (name, email, age) VALUES ('Bob', "
                         "'bob@test.com', 30)"));

    // Select data
    auto result = db->selectData("SELECT * FROM users WHERE name = 'Bob'");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0][1], "Bob");
    EXPECT_EQ(result[0][2], "bob@test.com");
    EXPECT_EQ(result[0][3], "30");
}

TEST_F(SqliteDBTest, BasicUpdate) {
    // Insert and update
    ASSERT_TRUE(
        db->executeQuery("INSERT INTO users (name, email, age) VALUES "
                         "('Charlie', 'charlie@test.com', 35)"));
    EXPECT_TRUE(
        db->executeQuery("UPDATE users SET age = 36 WHERE name = 'Charlie'"));
    EXPECT_EQ(db->getChanges(), 1);

    // Verify update
    auto result =
        db->selectData("SELECT age FROM users WHERE name = 'Charlie'");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0][0], "36");
}

TEST_F(SqliteDBTest, BasicDelete) {
    // Insert and delete
    ASSERT_TRUE(
        db->executeQuery("INSERT INTO users (name, email, age) VALUES "
                         "('David', 'david@test.com', 40)"));
    EXPECT_TRUE(db->executeQuery("DELETE FROM users WHERE name = 'David'"));
    EXPECT_EQ(db->getChanges(), 1);

    // Verify deletion
    auto result = db->selectData("SELECT * FROM users WHERE name = 'David'");
    EXPECT_EQ(result.size(), 0);
}

// Parameterized Query Tests
TEST_F(SqliteDBTest, ParameterizedInsert) {
    const char* name = "Eve";
    const char* email = "eve@test.com";
    EXPECT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name, email,
        28));
    EXPECT_GT(db->getLastInsertRowId(), 0);
}

TEST_F(SqliteDBTest, ParameterizedSelect) {
    // Insert test data
    const char* name = "Frank";
    const char* email = "frank@test.com";
    ASSERT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name, email,
        32));

    // Parameterized select
    auto result =
        db->selectParameterizedData("SELECT * FROM users WHERE age > ?", 30);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0][1], "Frank");
}

TEST_F(SqliteDBTest, ParameterizedUpdate) {
    const char* name = "Grace";
    const char* email = "grace@test.com";
    ASSERT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name, email,
        27));

    const char* update_name = "Grace";
    EXPECT_TRUE(db->executeParameterizedQuery(
        "UPDATE users SET age = ? WHERE name = ?", 28, update_name));
    EXPECT_EQ(db->getChanges(), 1);
}

TEST_F(SqliteDBTest, ParameterizedDelete) {
    const char* name = "Henry";
    const char* email = "henry@test.com";
    ASSERT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name, email,
        45));

    EXPECT_TRUE(
        db->executeParameterizedQuery("DELETE FROM users WHERE age > ?", 40));
    EXPECT_EQ(db->getChanges(), 1);
}

// Transaction Tests
TEST_F(SqliteDBTest, BasicTransaction) {
    EXPECT_NO_THROW(db->beginTransaction());

    const char* name = "Ivy";
    const char* email = "ivy@test.com";
    EXPECT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name, email,
        29));

    EXPECT_NO_THROW(db->commitTransaction());

    // Verify data was committed
    auto result = db->selectData("SELECT * FROM users WHERE name = 'Ivy'");
    EXPECT_EQ(result.size(), 1);
}

TEST_F(SqliteDBTest, TransactionRollback) {
    EXPECT_NO_THROW(db->beginTransaction());

    const char* name = "Jack";
    const char* email = "jack@test.com";
    EXPECT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name, email,
        33));

    EXPECT_NO_THROW(db->rollbackTransaction());

    // Verify data was rolled back
    auto result = db->selectData("SELECT * FROM users WHERE name = 'Jack'");
    EXPECT_EQ(result.size(), 0);
}

// Error Handling Tests
TEST_F(SqliteDBTest, InvalidQuery) {
    EXPECT_THROW(db->executeQuery("INVALID SQL SYNTAX"), SQLiteException);
}

TEST_F(SqliteDBTest, ConstraintViolation) {
    // Insert first user
    const char* name1 = "Kate";
    const char* email = "kate@test.com";
    ASSERT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name1, email,
        26));

    // Try to insert duplicate email (should fail due to UNIQUE constraint)
    const char* name2 = "Kate2";
    EXPECT_THROW(db->executeParameterizedQuery(
                     "INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
                     name2, email, 27),
                 SQLiteException);
}

TEST_F(SqliteDBTest, InvalidTableAccess) {
    EXPECT_THROW(db->selectData("SELECT * FROM nonexistent_table"),
                 SQLiteException);
}

// File Operations Tests
TEST_F(SqliteDBTest, FileBasedDatabase) {
    std::string filename =
        "test_file_db_" + std::to_string(std::time(nullptr)) + ".sqlite";
    createFileBasedDB(filename);

    // Insert data
    const char* name = "Leo";
    const char* email = "leo@test.com";
    EXPECT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name, email,
        31));

    // Close and reopen database
    db.reset();
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Allow file to be released
    db = std::make_unique<SqliteDB>(filename);

    // Verify data persisted
    auto result = db->selectData("SELECT * FROM users WHERE name = 'Leo'");
    EXPECT_EQ(result.size(), 1);

    // Cleanup
    db.reset();
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Allow file to be released
    std::filesystem::remove(filename);
}

// Pagination Tests
TEST_F(SqliteDBTest, Pagination) {
    // Insert multiple records
    for (int i = 1; i <= 10; ++i) {
        ASSERT_TRUE(db->executeParameterizedQuery(
            "INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
            "User" + std::to_string(i),
            "user" + std::to_string(i) + "@test.com", 20 + i));
    }

    // Test pagination
    auto page1 =
        db->selectDataWithPagination("SELECT * FROM users ORDER BY id", 3, 0);
    EXPECT_EQ(page1.size(), 3);

    auto page2 =
        db->selectDataWithPagination("SELECT * FROM users ORDER BY id", 3, 3);
    EXPECT_EQ(page2.size(), 3);

    // Verify different records
    EXPECT_NE(page1[0][0], page2[0][0]);  // Different IDs
}

// Search and Validation Tests
TEST_F(SqliteDBTest, SearchData) {
    // Insert test data
    const char* name = "Mike";
    const char* email = "mike@test.com";
    ASSERT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name, email,
        34));

    // Search for existing data
    EXPECT_TRUE(db->searchData("SELECT * FROM users WHERE name = ?", "Mike"));

    // Search for non-existing data
    EXPECT_FALSE(
        db->searchData("SELECT * FROM users WHERE name = ?", "NonExistent"));
}

TEST_F(SqliteDBTest, ValidateData) {
    // Insert test data
    const char* name = "Nina";
    const char* email = "nina@test.com";
    ASSERT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name, email,
        24));

    // Valid validation
    EXPECT_TRUE(db->validateData(
        "SELECT * FROM users WHERE name = 'Nina'",
        "SELECT COUNT(*) > 0 FROM users WHERE name = 'Nina' AND age > 0"));

    // Invalid validation
    EXPECT_FALSE(db->validateData(
        "SELECT * FROM users WHERE name = 'Nina'",
        "SELECT COUNT(*) > 0 FROM users WHERE name = 'Nina' AND age < 0"));
}

// Edge Cases and Boundary Tests
TEST_F(SqliteDBTest, EmptyResults) {
    auto result = db->selectData("SELECT * FROM users WHERE 1 = 0");
    EXPECT_EQ(result.size(), 0);
}

TEST_F(SqliteDBTest, NullValues) {
    EXPECT_TRUE(db->executeQuery(
        "INSERT INTO users (name, email, age) VALUES ('Oscar', NULL, NULL)"));

    auto result = db->selectData("SELECT * FROM users WHERE name = 'Oscar'");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0][1], "Oscar");
    // Note: NULL values handling depends on implementation
}

TEST_F(SqliteDBTest, LargeData) {
    std::string largeName(1000, 'A');
    EXPECT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", largeName,
        "large@test.com", 25));

    auto result =
        db->selectData("SELECT name FROM users WHERE email = 'large@test.com'");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0][0].size(), 1000);
}

TEST_F(SqliteDBTest, SpecialCharacters) {
    std::string specialName = "Test'\"\\Name";
    EXPECT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", specialName,
        "special@test.com", 25));

    auto result = db->selectData(
        "SELECT name FROM users WHERE email = 'special@test.com'");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0][0], specialName);
}

// Concurrency Tests
TEST_F(SqliteDBTest, ConcurrentReads) {
    // Insert test data
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(db->executeParameterizedQuery(
            "INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
            "ConcurrentUser" + std::to_string(i),
            "concurrent" + std::to_string(i) + "@test.com", 20 + (i % 50)));
    }

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Launch multiple read threads
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &successCount]() {
            try {
                auto result = db->selectData("SELECT COUNT(*) FROM users");
                if (!result.empty() && std::stoi(result[0][0]) >= 100) {
                    successCount++;
                }
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount, 5);  // At least half should succeed
}

TEST_F(SqliteDBTest, ConcurrentWrites) {
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Launch multiple write threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i, &successCount]() {
            try {
                for (int j = 0; j < 10; ++j) {
                    if (db->executeParameterizedQuery(
                            "INSERT INTO users (name, email, age) VALUES (?, "
                            "?, ?)",
                            "Thread" + std::to_string(i) + "User" +
                                std::to_string(j),
                            "thread" + std::to_string(i) + "user" +
                                std::to_string(j) + "@test.com",
                            25 + j)) {
                        successCount++;
                    }
                }
            } catch (...) {
                // Some writes may fail due to concurrency
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount, 25);  // At least half should succeed
}

// Performance Tests
TEST_F(SqliteDBTest, BulkInsert) {
    auto start = std::chrono::high_resolution_clock::now();

    EXPECT_NO_THROW(db->beginTransaction());
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(db->executeParameterizedQuery(
            "INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
            "BulkUser" + std::to_string(i),
            "bulk" + std::to_string(i) + "@test.com", 20 + (i % 50)));
    }
    EXPECT_NO_THROW(db->commitTransaction());

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Verify all records were inserted
    auto result = db->selectData("SELECT COUNT(*) FROM users");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(std::stoi(result[0][0]), 1000);

    // Performance should be reasonable (less than 5 seconds for 1000 inserts)
    EXPECT_LT(duration.count(), 5000);
}

// Error Recovery Tests
TEST_F(SqliteDBTest, TransactionErrorRecovery) {
    EXPECT_NO_THROW(db->beginTransaction());

    // Valid insert
    const char* name = "ValidUser";
    const char* email = "valid@test.com";
    EXPECT_TRUE(db->executeParameterizedQuery(
        "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", name, email,
        25));

    // Invalid insert (should fail)
    EXPECT_THROW(
        db->executeQuery("INSERT INTO users (invalid_column) VALUES ('test')"),
        SQLiteException);

    // Transaction should still be active, rollback should work
    EXPECT_NO_THROW(db->rollbackTransaction());

    // Verify no data was committed
    auto result =
        db->selectData("SELECT * FROM users WHERE name = 'ValidUser'");
    EXPECT_EQ(result.size(), 0);
}

// Database State Tests
TEST_F(SqliteDBTest, DatabaseInfo) {
    // Test database introspection
    auto result =
        db->selectData("SELECT name FROM sqlite_master WHERE type='table'");
    EXPECT_GT(result.size(), 0);  // Should have at least our users table

    bool foundUsersTable = false;
    for (const auto& row : result) {
        if (row[0] == "users") {
            foundUsersTable = true;
            break;
        }
    }
    EXPECT_TRUE(foundUsersTable);
}

TEST_F(SqliteDBTest, TableSchema) {
    auto result = db->selectData("PRAGMA table_info(users)");
    EXPECT_GE(result.size(), 5);  // Should have at least 5 columns

    // Verify column names exist
    std::vector<std::string> expectedColumns = {"id", "name", "email", "age",
                                                "created_at"};
    std::set<std::string> foundColumns;

    for (const auto& row : result) {
        if (row.size() > 1) {
            foundColumns.insert(row[1]);  // Column name is in second position
        }
    }

    for (const auto& expected : expectedColumns) {
        EXPECT_TRUE(foundColumns.count(expected) > 0)
            << "Column " << expected << " not found";
    }
}

// Cleanup and Resource Management Tests
TEST_F(SqliteDBTest, MultipleConnections) {
    // Create multiple connections to the same in-memory database
    // Note: In-memory databases are per-connection, so this tests connection
    // handling
    auto db2 = std::make_unique<SqliteDB>(":memory:");

    // Both should work independently
    EXPECT_TRUE(db->executeQuery("CREATE TABLE test1 (id INTEGER)"));
    EXPECT_TRUE(db2->executeQuery("CREATE TABLE test2 (id INTEGER)"));

    EXPECT_TRUE(db->executeQuery("INSERT INTO test1 VALUES (1)"));
    EXPECT_TRUE(db2->executeQuery("INSERT INTO test2 VALUES (2)"));

    auto result1 = db->selectData("SELECT * FROM test1");
    auto result2 = db2->selectData("SELECT * FROM test2");

    EXPECT_EQ(result1.size(), 1);
    EXPECT_EQ(result2.size(), 1);
}

#endif  // ATOM_SEARCH_TEST_SQLITE_HPP
