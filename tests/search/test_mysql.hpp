#ifndef ATOM_SEARCH_TEST_MYSQL_HPP
#define ATOM_SEARCH_TEST_MYSQL_HPP

#include <gtest/gtest.h>

// Only compile MySQL tests if MariaDB is available
#ifdef ATOM_HAS_MARIADB

#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

#include "atom/search/database/mysql.hpp"

using namespace atom::database;

class MySQLDBTest : public ::testing::Test {
protected:
    std::unique_ptr<MysqlDB> db;
    ConnectionParams testParams;

    void SetUp() override {
        // Setup test connection parameters
        // These should be configured for your test environment
        testParams.host = "localhost";
        testParams.user = "test_user";
        testParams.password = "test_password";
        testParams.database = "test_database";
        testParams.port = 3306;
        testParams.connectTimeout = 10;
        testParams.readTimeout = 10;
        testParams.writeTimeout = 10;
        testParams.autoReconnect = true;
        testParams.charset = "utf8mb4";

        try {
            db = std::make_unique<MysqlDB>(testParams);
            if (!db->connect()) {
                GTEST_SKIP() << "MySQL connection failed - skipping MySQL tests";
                return;
            }

            // Create test table
            ASSERT_TRUE(db->executeQuery(R"(
                CREATE TABLE IF NOT EXISTS test_users (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    name VARCHAR(100) NOT NULL,
                    email VARCHAR(255) UNIQUE,
                    age INT,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
                ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
            )"));

            // Clean up any existing test data
            db->executeQuery("DELETE FROM test_users");

        } catch (const MySQLException& e) {
            GTEST_SKIP() << "MySQL setup failed: " << e.what();
        }
    }

    void TearDown() override {
        if (db && db->isConnected()) {
            // Clean up test data
            try {
                db->executeQuery("DELETE FROM test_users");
                db->disconnect();
            } catch (...) {
                // Ignore cleanup errors
            }
        }
    }
};

// Connection Management Tests
TEST_F(MySQLDBTest, BasicConnection) {
    EXPECT_TRUE(db->isConnected());
    EXPECT_NO_THROW(db->disconnect());
    EXPECT_FALSE(db->isConnected());
    EXPECT_TRUE(db->connect());
    EXPECT_TRUE(db->isConnected());
}

TEST_F(MySQLDBTest, ConnectionWithParams) {
    auto db2 = std::make_unique<MysqlDB>(testParams.host, testParams.user,
                                         testParams.password, testParams.database,
                                         testParams.port);
    EXPECT_TRUE(db2->connect());
    EXPECT_TRUE(db2->isConnected());
    db2->disconnect();
}

TEST_F(MySQLDBTest, InvalidConnection) {
    ConnectionParams invalidParams = testParams;
    invalidParams.password = "wrong_password";

    auto invalidDB = std::make_unique<MysqlDB>(invalidParams);
    EXPECT_FALSE(invalidDB->connect());
    EXPECT_FALSE(invalidDB->isConnected());
}

// Basic CRUD Operations
TEST_F(MySQLDBTest, BasicInsert) {
    EXPECT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('Alice', 'alice@test.com', 25)"
    ));
    EXPECT_GT(db->getLastInsertId(), 0);
    EXPECT_EQ(db->getAffectedRows(), 1);
}

TEST_F(MySQLDBTest, BasicSelect) {
    // Insert test data
    ASSERT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('Bob', 'bob@test.com', 30)"
    ));

    // Select data
    auto result = db->executeQueryWithResults("SELECT * FROM test_users WHERE name = 'Bob'");
    ASSERT_TRUE(result.next());

    Row row = result.getCurrentRow();
    EXPECT_EQ(row.getString(1), "Bob");
    EXPECT_EQ(row.getString(2), "bob@test.com");
    EXPECT_EQ(row.getInt(3), 30);

    EXPECT_FALSE(result.next()); // Should be only one row
}

TEST_F(MySQLDBTest, BasicUpdate) {
    // Insert and update
    ASSERT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('Charlie', 'charlie@test.com', 35)"
    ));

    EXPECT_TRUE(db->executeQuery("UPDATE test_users SET age = 36 WHERE name = 'Charlie'"));
    EXPECT_EQ(db->getAffectedRows(), 1);

    // Verify update
    auto result = db->executeQueryWithResults("SELECT age FROM test_users WHERE name = 'Charlie'");
    ASSERT_TRUE(result.next());
    EXPECT_EQ(result.getCurrentRow().getInt(0), 36);
}

TEST_F(MySQLDBTest, BasicDelete) {
    // Insert and delete
    ASSERT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('David', 'david@test.com', 40)"
    ));

    EXPECT_TRUE(db->executeQuery("DELETE FROM test_users WHERE name = 'David'"));
    EXPECT_EQ(db->getAffectedRows(), 1);

    // Verify deletion
    auto result = db->executeQueryWithResults("SELECT * FROM test_users WHERE name = 'David'");
    EXPECT_FALSE(result.next());
}

// Prepared Statement Tests
TEST_F(MySQLDBTest, PreparedStatementInsert) {
    auto stmt = db->prepareStatement(
        "INSERT INTO test_users (name, email, age) VALUES (?, ?, ?)"
    );

    ASSERT_NE(stmt, nullptr);

    stmt->bindString(0, "Eve")
         .bindString(1, "eve@test.com")
         .bindInt(2, 28);

    EXPECT_TRUE(stmt->execute());
    EXPECT_GT(db->getLastInsertId(), 0);
}

TEST_F(MySQLDBTest, PreparedStatementSelect) {
    // Insert test data
    ASSERT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('Frank', 'frank@test.com', 32)"
    ));

    auto stmt = db->prepareStatement("SELECT * FROM test_users WHERE age > ?");
    ASSERT_NE(stmt, nullptr);

    stmt->bindInt(0, 30);
    EXPECT_TRUE(stmt->execute());

    auto result = stmt->getResultSet();
    ASSERT_TRUE(result.next());
    EXPECT_EQ(result.getCurrentRow().getString(1), "Frank");
}

TEST_F(MySQLDBTest, PreparedStatementBatch) {
    auto stmt = db->prepareStatement(
        "INSERT INTO test_users (name, email, age) VALUES (?, ?, ?)"
    );

    ASSERT_NE(stmt, nullptr);

    // Insert multiple records
    std::vector<std::tuple<std::string, std::string, int>> users = {
        {"Grace", "grace@test.com", 27},
        {"Henry", "henry@test.com", 45},
        {"Ivy", "ivy@test.com", 29}
    };

    for (const auto& [name, email, age] : users) {
        stmt->bindString(0, name)
             .bindString(1, email)
             .bindInt(2, age);
        EXPECT_TRUE(stmt->execute());
    }

    // Verify all records were inserted
    auto result = db->executeQueryWithResults("SELECT COUNT(*) FROM test_users");
    ASSERT_TRUE(result.next());
    EXPECT_EQ(result.getCurrentRow().getInt(0), 3);
}

// Transaction Tests
TEST_F(MySQLDBTest, BasicTransaction) {
    EXPECT_TRUE(db->beginTransaction());

    EXPECT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('Jack', 'jack@test.com', 33)"
    ));

    EXPECT_TRUE(db->commitTransaction());

    // Verify data was committed
    auto result = db->executeQueryWithResults("SELECT * FROM test_users WHERE name = 'Jack'");
    EXPECT_TRUE(result.next());
}

TEST_F(MySQLDBTest, TransactionRollback) {
    EXPECT_TRUE(db->beginTransaction());

    EXPECT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('Kate', 'kate@test.com', 26)"
    ));

    EXPECT_TRUE(db->rollbackTransaction());

    // Verify data was rolled back
    auto result = db->executeQueryWithResults("SELECT * FROM test_users WHERE name = 'Kate'");
    EXPECT_FALSE(result.next());
}

TEST_F(MySQLDBTest, TransactionIsolation) {
    // Test different isolation levels
    EXPECT_TRUE(db->setTransactionIsolation(TransactionIsolation::READ_COMMITTED));

    EXPECT_TRUE(db->beginTransaction());
    EXPECT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('Leo', 'leo@test.com', 31)"
    ));

    // In a real scenario, another connection wouldn't see this data yet
    EXPECT_TRUE(db->commitTransaction());

    auto result = db->executeQueryWithResults("SELECT * FROM test_users WHERE name = 'Leo'");
    EXPECT_TRUE(result.next());
}

// Error Handling Tests
TEST_F(MySQLDBTest, InvalidQuery) {
    EXPECT_FALSE(db->executeQuery("INVALID SQL SYNTAX"));
    EXPECT_FALSE(db->getLastError().empty());
}

TEST_F(MySQLDBTest, ConstraintViolation) {
    // Insert first user
    ASSERT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('Mike', 'mike@test.com', 34)"
    ));

    // Try to insert duplicate email (should fail due to UNIQUE constraint)
    EXPECT_FALSE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('Mike2', 'mike@test.com', 35)"
    ));
    EXPECT_FALSE(db->getLastError().empty());
}

TEST_F(MySQLDBTest, InvalidTableAccess) {
    EXPECT_FALSE(db->executeQuery("SELECT * FROM nonexistent_table"));
    EXPECT_FALSE(db->getLastError().empty());
}

// Connection Pool Tests
TEST_F(MySQLDBTest, ConnectionReconnect) {
    EXPECT_TRUE(db->isConnected());

    // Force disconnect
    db->disconnect();
    EXPECT_FALSE(db->isConnected());

    // Reconnect
    EXPECT_TRUE(db->connect());
    EXPECT_TRUE(db->isConnected());

    // Should be able to execute queries again
    EXPECT_TRUE(db->executeQuery("SELECT 1"));
}

// Performance Tests
TEST_F(MySQLDBTest, BulkInsert) {
    auto start = std::chrono::high_resolution_clock::now();

    EXPECT_TRUE(db->beginTransaction());

    auto stmt = db->prepareStatement(
        "INSERT INTO test_users (name, email, age) VALUES (?, ?, ?)"
    );

    for (int i = 0; i < 1000; ++i) {
        stmt->bindString(0, "BulkUser" + std::to_string(i))
             .bindString(1, "bulk" + std::to_string(i) + "@test.com")
             .bindInt(2, 20 + (i % 50));
        EXPECT_TRUE(stmt->execute());
    }

    EXPECT_TRUE(db->commitTransaction());

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Verify all records were inserted
    auto result = db->executeQueryWithResults("SELECT COUNT(*) FROM test_users");
    ASSERT_TRUE(result.next());
    EXPECT_EQ(result.getCurrentRow().getInt(0), 1000);

    // Performance should be reasonable (less than 10 seconds for 1000 inserts)
    EXPECT_LT(duration.count(), 10000);
}

// Concurrency Tests
TEST_F(MySQLDBTest, ConcurrentConnections) {
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Launch multiple connection threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i, &successCount]() {
            try {
                auto threadDB = std::make_unique<MysqlDB>(testParams);
                if (threadDB->connect()) {
                    // Each thread inserts some data
                    for (int j = 0; j < 10; ++j) {
                        if (threadDB->executeQuery(
                            "INSERT INTO test_users (name, email, age) VALUES "
                            "('" + std::string("Thread") + std::to_string(i) + "User" + std::to_string(j) + "', "
                            "'thread" + std::to_string(i) + "user" + std::to_string(j) + "@test.com', " +
                            std::to_string(25 + j) + ")"
                        )) {
                            successCount++;
                        }
                    }
                    threadDB->disconnect();
                }
            } catch (...) {
                // Some operations may fail due to concurrency
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount, 25); // At least half should succeed
}

TEST_F(MySQLDBTest, ConcurrentTransactions) {
    std::vector<std::thread> threads;
    std::atomic<int> commitCount{0};

    // Launch multiple transaction threads
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, i, &commitCount]() {
            try {
                auto threadDB = std::make_unique<MysqlDB>(testParams);
                if (threadDB->connect()) {
                    if (threadDB->beginTransaction()) {
                        bool allSuccess = true;
                        for (int j = 0; j < 5; ++j) {
                            if (!threadDB->executeQuery(
                                "INSERT INTO test_users (name, email, age) VALUES "
                                "('" + std::string("TxThread") + std::to_string(i) + "User" + std::to_string(j) + "', "
                                "'txthread" + std::to_string(i) + "user" + std::to_string(j) + "@test.com', " +
                                std::to_string(30 + j) + ")"
                            )) {
                                allSuccess = false;
                                break;
                            }
                        }

                        if (allSuccess && threadDB->commitTransaction()) {
                            commitCount++;
                        } else {
                            threadDB->rollbackTransaction();
                        }
                    }
                    threadDB->disconnect();
                }
            } catch (...) {
                // Handle exceptions
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(commitCount, 0); // At least one transaction should succeed
}

// Advanced Features Tests
TEST_F(MySQLDBTest, StoredProcedure) {
    // Create a simple stored procedure
    EXPECT_TRUE(db->executeQuery(R"(
        CREATE PROCEDURE IF NOT EXISTS GetUsersByAge(IN min_age INT)
        BEGIN
            SELECT * FROM test_users WHERE age >= min_age ORDER BY age;
        END
    )"));

    // Insert test data
    ASSERT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES "
        "('Nina', 'nina@test.com', 24), "
        "('Oscar', 'oscar@test.com', 35), "
        "('Paul', 'paul@test.com', 42)"
    ));

    // Call stored procedure
    auto result = db->executeQueryWithResults("CALL GetUsersByAge(30)");

    int count = 0;
    while (result.next()) {
        EXPECT_GE(result.getCurrentRow().getInt(3), 30); // age column
        count++;
    }
    EXPECT_EQ(count, 2); // Should return Oscar and Paul

    // Clean up
    db->executeQuery("DROP PROCEDURE IF EXISTS GetUsersByAge");
}

TEST_F(MySQLDBTest, CharsetAndEncoding) {
    // Test UTF-8 characters
    std::string unicodeName = "测试用户";
    std::string unicodeEmail = "测试@example.com";

    auto stmt = db->prepareStatement(
        "INSERT INTO test_users (name, email, age) VALUES (?, ?, ?)"
    );

    stmt->bindString(0, unicodeName)
         .bindString(1, unicodeEmail)
         .bindInt(2, 25);

    EXPECT_TRUE(stmt->execute());

    // Verify the data was stored correctly
    auto result = db->executeQueryWithResults("SELECT name, email FROM test_users WHERE age = 25");
    ASSERT_TRUE(result.next());

    Row row = result.getCurrentRow();
    EXPECT_EQ(row.getString(0), unicodeName);
    EXPECT_EQ(row.getString(1), unicodeEmail);
}

// Edge Cases and Error Recovery
TEST_F(MySQLDBTest, ConnectionTimeout) {
    ConnectionParams timeoutParams = testParams;
    timeoutParams.connectTimeout = 1; // Very short timeout
    timeoutParams.host = "192.0.2.1"; // Non-routable IP for timeout test

    auto timeoutDB = std::make_unique<MysqlDB>(timeoutParams);

    auto start = std::chrono::high_resolution_clock::now();
    bool connected = timeoutDB->connect();
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_FALSE(connected);

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    EXPECT_LE(duration.count(), 5); // Should timeout quickly
}

TEST_F(MySQLDBTest, LargeResultSet) {
    // Insert a larger dataset
    EXPECT_TRUE(db->beginTransaction());

    auto stmt = db->prepareStatement(
        "INSERT INTO test_users (name, email, age) VALUES (?, ?, ?)"
    );

    for (int i = 0; i < 100; ++i) {
        stmt->bindString(0, "LargeUser" + std::to_string(i))
             .bindString(1, "large" + std::to_string(i) + "@test.com")
             .bindInt(2, 20 + (i % 60));
        EXPECT_TRUE(stmt->execute());
    }

    EXPECT_TRUE(db->commitTransaction());

    // Query the large result set
    auto result = db->executeQueryWithResults("SELECT * FROM test_users ORDER BY id");

    int count = 0;
    while (result.next()) {
        count++;
        // Verify data integrity
        Row row = result.getCurrentRow();
        EXPECT_FALSE(row.getString(1).empty()); // name should not be empty
        EXPECT_FALSE(row.getString(2).empty()); // email should not be empty
        EXPECT_GT(row.getInt(3), 0); // age should be positive
    }

    EXPECT_EQ(count, 100);
}

TEST_F(MySQLDBTest, NullValueHandling) {
    // Insert record with NULL values
    EXPECT_TRUE(db->executeQuery(
        "INSERT INTO test_users (name, email, age) VALUES ('NullTest', NULL, NULL)"
    ));

    auto result = db->executeQueryWithResults("SELECT * FROM test_users WHERE name = 'NullTest'");
    ASSERT_TRUE(result.next());

    Row row = result.getCurrentRow();
    EXPECT_EQ(row.getString(1), "NullTest");
    EXPECT_TRUE(row.isNull(2)); // email should be NULL
    EXPECT_TRUE(row.isNull(3)); // age should be NULL
}

// Resource Management Tests
TEST_F(MySQLDBTest, StatementCleanup) {
    // Create multiple statements and ensure they're cleaned up properly
    std::vector<std::unique_ptr<PreparedStatement>> statements;

    for (int i = 0; i < 10; ++i) {
        auto stmt = db->prepareStatement("SELECT ? AS test_value");
        ASSERT_NE(stmt, nullptr);
        stmt->bindInt(0, i);
        EXPECT_TRUE(stmt->execute());
        statements.push_back(std::move(stmt));
    }

    // Statements should be automatically cleaned up when vector is destroyed
    statements.clear();

    // Database should still be functional
    EXPECT_TRUE(db->executeQuery("SELECT 1"));
}

TEST_F(MySQLDBTest, DatabaseReconnection) {
    // Simulate connection loss and recovery
    EXPECT_TRUE(db->isConnected());

    // Force disconnect
    db->disconnect();
    EXPECT_FALSE(db->isConnected());

    // Try to execute query (should fail)
    EXPECT_FALSE(db->executeQuery("SELECT 1"));

    // Reconnect and retry
    EXPECT_TRUE(db->connect());
    EXPECT_TRUE(db->isConnected());
    EXPECT_TRUE(db->executeQuery("SELECT 1"));
}

#else
// Placeholder test when MySQL is not available
TEST(MySQLDBTest, MySQLNotAvailable) {
    GTEST_SKIP() << "MySQL/MariaDB not available - MySQL tests skipped";
}
#endif // ATOM_HAS_MARIADB

#endif  // ATOM_SEARCH_TEST_MYSQL_HPP
