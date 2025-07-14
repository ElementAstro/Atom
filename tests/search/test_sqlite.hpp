#ifndef ATOM_SEARCH_TEST_SQLITE_HPP
#define ATOM_SEARCH_TEST_SQLITE_HPP

#include "atom/search/sqlite.hpp"

#include <gtest/gtest.h>

#include <cstdio> // For std::remove
#include <string>
#include <vector>

using namespace atom::search;

class SqliteDBTest : public ::testing::Test {
protected:
    const std::string test_db_path = "test_sqlite.db";
    std::unique_ptr<SqliteDB> db;

    void SetUp() override {
        // Ensure the database file does not exist before starting
        std::remove(test_db_path.c_str());

        // Create a new database connection
        db = std::make_unique<SqliteDB>(test_db_path);

        // Create a test table
        ASSERT_TRUE(db->executeQuery(
            "CREATE TABLE test_table (id INTEGER PRIMARY KEY, name TEXT, value REAL);"));
        ASSERT_TRUE(db->executeQuery(
            "CREATE TABLE another_table (key TEXT UNIQUE, data BLOB);"));
    }

    void TearDown() override {
        // Close the database connection (unique_ptr handles deletion)
        db.reset();

        // Remove the database file
        std::remove(test_db_path.c_str());
    }
};

TEST_F(SqliteDBTest, ConstructorCreatesFile) {
    // SetUp already creates the file, just check if it exists
    FILE* file = fopen(test_db_path.c_str(), "r");
    ASSERT_NE(file, nullptr);
    fclose(file);
}

TEST_F(SqliteDBTest, ConstructorThrowsOnInvalidPath) {
    // Attempt to create a database in a non-existent directory
    EXPECT_THROW(SqliteDB("/nonexistent_dir/invalid.db"), SQLiteException);
}

TEST_F(SqliteDBTest, ExecuteQuery) {
    // Test inserting data
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));

    // Test selecting data
    auto results = db->selectData("SELECT name FROM test_table;");
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0][0], "Alice");
    EXPECT_EQ(results[1][0], "Bob");

    // Test invalid query
    EXPECT_THROW(static_cast<void>(db->executeQuery("SELECT * FROM non_existent_table;")), SQLiteException);
}

TEST_F(SqliteDBTest, ExecuteParameterizedQuery) {
    // Test inserting data with parameters
    ASSERT_TRUE(db->executeParameterizedQuery("INSERT INTO test_table (name, value) VALUES (?, ?);", "Charlie", 3.3));
    ASSERT_TRUE(db->executeParameterizedQuery("INSERT INTO test_table (name, value) VALUES (?, ?);", "David", 4.4));

    auto results = db->selectData("SELECT name, value FROM test_table WHERE name = 'Charlie';");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0][0], "Charlie");
    EXPECT_EQ(results[0][1], "3.3"); // Note: SQLite stores REAL as double, string conversion might vary slightly

    // Test with different parameter types
    ASSERT_TRUE(db->executeParameterizedQuery("INSERT INTO another_table (key, data) VALUES (?, ?);", "binary_key", std::vector<unsigned char>{1, 2, 3}));
    auto blob_results = db->selectData("SELECT key FROM another_table WHERE key = 'binary_key';");
    ASSERT_EQ(blob_results.size(), 1);
    EXPECT_EQ(blob_results[0][0], "binary_key");

    // Test invalid query with parameters
    EXPECT_THROW(static_cast<void>(db->executeParameterizedQuery("INSERT INTO non_existent_table (name) VALUES (?);", "Invalid")), SQLiteException);

    // Test wrong number of parameters
    EXPECT_THROW(static_cast<void>(db->executeParameterizedQuery("INSERT INTO test_table (name) VALUES (?, ?);", "Too many", 1)), SQLiteException);
    EXPECT_THROW(static_cast<void>(db->executeParameterizedQuery("INSERT INTO test_table (name, value) VALUES (?, ?);", "Too few")), SQLiteException);
}

TEST_F(SqliteDBTest, SelectData) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));

    auto results = db->selectData("SELECT id, name, value FROM test_table ORDER BY id;");
    ASSERT_EQ(results.size(), 2);
    ASSERT_EQ(results[0].size(), 3);
    EXPECT_EQ(results[0][0], "1");
    EXPECT_EQ(results[0][1], "Alice");
    EXPECT_EQ(results[0][2], "1.1");
    EXPECT_EQ(results[1][0], "2");
    EXPECT_EQ(results[1][1], "Bob");
    EXPECT_EQ(results[1][2], "2.2");

    // Test selecting from empty table
    auto empty_results = db->selectData("SELECT * FROM another_table;");
    EXPECT_TRUE(empty_results.empty());

    // Test invalid select query
    EXPECT_THROW(static_cast<void>(db->selectData("SELECT * FROM non_existent_table;")), SQLiteException);
}

TEST_F(SqliteDBTest, SelectParameterizedData) {
    ASSERT_TRUE(db->executeParameterizedQuery("INSERT INTO test_table (name, value) VALUES (?, ?);", "Charlie", 3.3));
    ASSERT_TRUE(db->executeParameterizedQuery("INSERT INTO test_table (name, value) VALUES (?, ?);", "David", 4.4));

    auto results = db->selectParameterizedData("SELECT id, name, value FROM test_table WHERE name = ? ORDER BY id;", "Charlie");
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results[0].size(), 3);
    EXPECT_EQ(results[0][1], "Charlie");

    auto empty_results = db->selectParameterizedData("SELECT * FROM test_table WHERE name = ?;", "NonExistent");
    EXPECT_TRUE(empty_results.empty());

    // Test invalid query with parameters
    EXPECT_THROW(static_cast<void>(db->selectParameterizedData("SELECT * FROM non_existent_table WHERE name = ?;", "Invalid")), SQLiteException);
}

TEST_F(SqliteDBTest, GetIntValue) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));

    auto id_opt = db->getIntValue("SELECT id FROM test_table WHERE name = 'Alice';");
    ASSERT_TRUE(id_opt.has_value());
    EXPECT_EQ(id_opt.value(), 1);

    // Test non-existent row
    auto non_existent_opt = db->getIntValue("SELECT id FROM test_table WHERE name = 'Charlie';");
    EXPECT_FALSE(non_existent_opt.has_value());

    // Test NULL value
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name) VALUES ('NullTest');"));
    auto null_value_opt = db->getIntValue("SELECT value FROM test_table WHERE name = 'NullTest';");
    EXPECT_FALSE(null_value_opt.has_value());

    // Test wrong data type (selecting text as int)
    auto wrong_type_opt = db->getIntValue("SELECT name FROM test_table WHERE name = 'Alice';");
    EXPECT_FALSE(wrong_type_opt.has_value()); // Should return nullopt if type doesn't match expected int

    // Test invalid query
    EXPECT_THROW(static_cast<void>(db->getIntValue("SELECT id FROM non_existent_table;")), SQLiteException);
}

TEST_F(SqliteDBTest, GetDoubleValue) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));

    auto value_opt = db->getDoubleValue("SELECT value FROM test_table WHERE name = 'Alice';");
    ASSERT_TRUE(value_opt.has_value());
    EXPECT_DOUBLE_EQ(value_opt.value(), 1.1);

    // Test non-existent row
    auto non_existent_opt = db->getDoubleValue("SELECT value FROM test_table WHERE name = 'Charlie';");
    EXPECT_FALSE(non_existent_opt.has_value());

    // Test NULL value
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name) VALUES ('NullTest');"));
    auto null_value_opt = db->getDoubleValue("SELECT value FROM test_table WHERE name = 'NullTest';");
    EXPECT_FALSE(null_value_opt.has_value());

    // Test wrong data type (selecting text as double)
    auto wrong_type_opt = db->getDoubleValue("SELECT name FROM test_table WHERE name = 'Alice';");
    EXPECT_FALSE(wrong_type_opt.has_value()); // Should return nullopt if type doesn't match expected double

    // Test invalid query
    EXPECT_THROW(static_cast<void>(db->getDoubleValue("SELECT value FROM non_existent_table;")), SQLiteException);
}

TEST_F(SqliteDBTest, GetTextValue) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));

    auto name_opt = db->getTextValue("SELECT name FROM test_table WHERE name = 'Alice';");
    ASSERT_TRUE(name_opt.has_value());
    EXPECT_EQ(name_opt.value(), "Alice");

    // Test non-existent row
    auto non_existent_opt = db->getTextValue("SELECT name FROM test_table WHERE name = 'Charlie';");
    EXPECT_FALSE(non_existent_opt.has_value());

    // Test NULL value
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (value) VALUES (99.9);"));
    auto null_value_opt = db->getTextValue("SELECT name FROM test_table WHERE value = 99.9;");
    EXPECT_FALSE(null_value_opt.has_value());

    // Test wrong data type (selecting int as text)
    auto wrong_type_opt = db->getTextValue("SELECT id FROM test_table WHERE name = 'Alice';");
    ASSERT_TRUE(wrong_type_opt.has_value()); // SQLite often converts int/real to text
    EXPECT_EQ(wrong_type_opt.value(), "1");

    // Test invalid query
    EXPECT_THROW(static_cast<void>(db->getTextValue("SELECT name FROM non_existent_table;")), SQLiteException);
}

TEST_F(SqliteDBTest, SearchData) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));

    // Test finding existing data
    EXPECT_TRUE(db->searchData("SELECT 1 FROM test_table WHERE name = ?;", "Alice"));
    EXPECT_TRUE(db->searchData("SELECT 1 FROM test_table WHERE value = ?;", "2.2"));

    // Test not finding non-existent data
    EXPECT_FALSE(db->searchData("SELECT 1 FROM test_table WHERE name = ?;", "Charlie"));
    EXPECT_FALSE(db->searchData("SELECT 1 FROM test_table WHERE value = ?;", "99.9"));

    // Test invalid query
    EXPECT_THROW(static_cast<void>(db->searchData("SELECT 1 FROM non_existent_table WHERE name = ?;", "Invalid")), SQLiteException);
}

TEST_F(SqliteDBTest, UpdateData) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));

    // Update one row
    int changes = db->updateData("UPDATE test_table SET value = 1.5 WHERE name = 'Alice';");
    EXPECT_EQ(changes, 1);
    auto value_opt = db->getDoubleValue("SELECT value FROM test_table WHERE name = 'Alice';");
    ASSERT_TRUE(value_opt.has_value());
    EXPECT_DOUBLE_EQ(value_opt.value(), 1.5);

    // Update multiple rows
    changes = db->updateData("UPDATE test_table SET value = value + 10;");
    EXPECT_EQ(changes, 2);
    auto value_alice = db->getDoubleValue("SELECT value FROM test_table WHERE name = 'Alice';");
    ASSERT_TRUE(value_alice.has_value());
    EXPECT_DOUBLE_EQ(value_alice.value(), 11.5);
    auto value_bob = db->getDoubleValue("SELECT value FROM test_table WHERE name = 'Bob';");
    ASSERT_TRUE(value_bob.has_value());
    EXPECT_DOUBLE_EQ(value_bob.value(), 12.2);

    // Update non-existent row
    changes = db->updateData("UPDATE test_table SET value = 99 WHERE name = 'Charlie';");
    EXPECT_EQ(changes, 0);

    // Test invalid query
    EXPECT_THROW(static_cast<void>(db->updateData("UPDATE non_existent_table SET value = 1;")), SQLiteException);
}

TEST_F(SqliteDBTest, DeleteData) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Charlie', 3.3);"));

    // Delete one row
    int changes = db->deleteData("DELETE FROM test_table WHERE name = 'Alice';");
    EXPECT_EQ(changes, 1);
    EXPECT_FALSE(db->searchData("SELECT 1 FROM test_table WHERE name = ?;", "Alice"));
    EXPECT_EQ(db->selectData("SELECT * FROM test_table;").size(), 2);

    // Delete multiple rows
    changes = db->deleteData("DELETE FROM test_table WHERE value > 2.0;");
    EXPECT_EQ(changes, 2); // Deletes Bob (2.2) and Charlie (3.3)
    EXPECT_FALSE(db->searchData("SELECT 1 FROM test_table WHERE name = ?;", "Bob"));
    EXPECT_FALSE(db->searchData("SELECT 1 FROM test_table WHERE name = ?;", "Charlie"));
    EXPECT_TRUE(db->selectData("SELECT * FROM test_table;").empty());

    // Delete non-existent row
    changes = db->deleteData("DELETE FROM test_table WHERE name = 'David';");
    EXPECT_EQ(changes, 0);

    // Test invalid query
    EXPECT_THROW(static_cast<void>(db->deleteData("DELETE FROM non_existent_table;")), SQLiteException);
}

TEST_F(SqliteDBTest, Transactions) {
    // Test successful transaction
    db->beginTransaction();
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Tx1', 10.1);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Tx2', 10.2);"));
    db->commitTransaction();

    auto results_commit = db->selectData("SELECT name FROM test_table WHERE name LIKE 'Tx%';");
    ASSERT_EQ(results_commit.size(), 2);

    // Test rollback
    db->beginTransaction();
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Tx3', 10.3);"));
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Tx4', 10.4);"));
    db->rollbackTransaction();

    auto results_rollback = db->selectData("SELECT name FROM test_table WHERE name LIKE 'Tx%';");
    ASSERT_EQ(results_rollback.size(), 2); // Tx3 and Tx4 should not be present

    // Test nested transactions (SQLite doesn't support true nested transactions,
    // but BEGIN/COMMIT/ROLLBACK handle savepoints implicitly)
    db->beginTransaction(); // Outer
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Outer', 20.1);"));
    db->beginTransaction(); // Inner (becomes a savepoint)
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Inner', 20.2);"));
    db->commitTransaction(); // Commits inner savepoint
    db->commitTransaction(); // Commits outer transaction

    auto results_nested = db->selectData("SELECT name FROM test_table WHERE name IN ('Outer', 'Inner');");
    ASSERT_EQ(results_nested.size(), 2);

    // Test rollback of inner transaction
    db->beginTransaction(); // Outer
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('OuterRollback', 30.1);"));
    db->beginTransaction(); // Inner
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('InnerRollback', 30.2);"));
    db->rollbackTransaction(); // Rollback inner savepoint
    db->commitTransaction(); // Commit outer transaction

    auto results_nested_rollback = db->selectData("SELECT name FROM test_table WHERE name IN ('OuterRollback', 'InnerRollback');");
    ASSERT_EQ(results_nested_rollback.size(), 1); // Only OuterRollback should be present
    EXPECT_EQ(results_nested_rollback[0][0], "OuterRollback");

    // Test transaction errors
    db->beginTransaction();
    EXPECT_THROW(static_cast<void>(db->executeQuery("INVALID SQL;")), SQLiteException);
    // The transaction is likely in an error state now, rollback should be safe
    db->rollbackTransaction();
}

TEST_F(SqliteDBTest, WithTransaction) {
    // Test successful transaction with lambda
    bool success = false;
    EXPECT_NO_THROW(db->withTransaction([&]() {
        ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('WithTx1', 40.1);"));
        ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('WithTx2', 40.2);"));
        success = true;
    }));
    EXPECT_TRUE(success);
    auto results_withtx = db->selectData("SELECT name FROM test_table WHERE name LIKE 'WithTx%';");
    ASSERT_EQ(results_withtx.size(), 2);

    // Test transaction rollback on exception
    bool exception_caught = false;
    EXPECT_THROW(db->withTransaction([&]() {
        ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('WithTx3', 40.3);"));
        ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('WithTx4', 40.4);"));
        throw std::runtime_error("Simulated error"); // This should trigger rollback
    }), std::runtime_error);

    auto results_withtx_rollback = db->selectData("SELECT name FROM test_table WHERE name LIKE 'WithTx%';");
    ASSERT_EQ(results_withtx_rollback.size(), 2); // Tx3 and Tx4 should not be present
}

TEST_F(SqliteDBTest, ValidateData) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));

    // Test successful validation
    EXPECT_TRUE(db->validateData(
        "UPDATE test_table SET value = 1.5 WHERE name = 'Alice';",
        "SELECT COUNT(*) FROM test_table WHERE name = 'Alice' AND value = 1.5;"
    ));
    auto value_opt = db->getDoubleValue("SELECT value FROM test_table WHERE name = 'Alice';");
    ASSERT_TRUE(value_opt.has_value());
    EXPECT_DOUBLE_EQ(value_opt.value(), 1.5);

    // Test validation failure (main query succeeds, validation query fails)
    EXPECT_FALSE(db->validateData(
        "UPDATE test_table SET value = 2.0 WHERE name = 'Alice';",
        "SELECT COUNT(*) FROM test_table WHERE name = 'Alice' AND value = 99.9;" // This will be 0
    ));
    // The main query should still have executed
    value_opt = db->getDoubleValue("SELECT value FROM test_table WHERE name = 'Alice';");
    ASSERT_TRUE(value_opt.has_value());
    EXPECT_DOUBLE_EQ(value_opt.value(), 2.0);

    // Test main query failure (validation query is not run)
    EXPECT_THROW(static_cast<void>(db->validateData(
        "UPDATE non_existent_table SET value = 1;",
        "SELECT 1;"
    )), SQLiteException);

    // Test invalid validation query
    EXPECT_THROW(static_cast<void>(db->validateData(
        "SELECT 1;",
        "INVALID VALIDATION QUERY;"
    )), SQLiteException);
}

TEST_F(SqliteDBTest, SelectDataWithPagination) {
    // Insert 10 rows
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(db->executeParameterizedQuery("INSERT INTO test_table (name, value) VALUES (?, ?);", "Item" + std::to_string(i), i * 1.0));
    }

    // Get first 5 items
    auto results1 = db->selectDataWithPagination("SELECT name FROM test_table ORDER BY id", 5, 0);
    ASSERT_EQ(results1.size(), 5);
    EXPECT_EQ(results1[0][0], "Item0");
    EXPECT_EQ(results1[4][0], "Item4");

    // Get next 5 items
    auto results2 = db->selectDataWithPagination("SELECT name FROM test_table ORDER BY id", 5, 5);
    ASSERT_EQ(results2.size(), 5);
    EXPECT_EQ(results2[0][0], "Item5");
    EXPECT_EQ(results2[4][0], "Item9");

    // Get items with limit > total
    auto results3 = db->selectDataWithPagination("SELECT name FROM test_table ORDER BY id", 20, 0);
    ASSERT_EQ(results3.size(), 10);

    // Get items with offset > total
    auto results4 = db->selectDataWithPagination("SELECT name FROM test_table ORDER BY id", 5, 10);
    EXPECT_TRUE(results4.empty());

    // Get items with limit = 0
    auto results5 = db->selectDataWithPagination("SELECT name FROM test_table ORDER BY id", 0, 0);
    EXPECT_TRUE(results5.empty());

    // Test invalid limit/offset
    EXPECT_THROW(static_cast<void>(db->selectDataWithPagination("SELECT name FROM test_table", -1, 0)), SQLiteException);
    EXPECT_THROW(static_cast<void>(db->selectDataWithPagination("SELECT name FROM test_table", 10, -1)), SQLiteException);

    // Test invalid query
    EXPECT_THROW(static_cast<void>(db->selectDataWithPagination("SELECT name FROM non_existent_table", 5, 0)), SQLiteException);
}

TEST_F(SqliteDBTest, SetErrorMessageCallback) {
    std::string captured_error_message;
    db->setErrorMessageCallback([&](std::string_view msg) {
        captured_error_message = msg;
    });

    // Execute an invalid query to trigger an error
    EXPECT_THROW(static_cast<void>(db->executeQuery("SELECT * FROM non_existent_table;")), SQLiteException);

    // Check if the callback captured the error message
    // The exact message might vary slightly depending on SQLite version and context
    EXPECT_FALSE(captured_error_message.empty());
    // A more robust check might look for a substring known to be in the error
    // e.g., EXPECT_NE(captured_error_message.find("non_existent_table"), std::string::npos);
}

TEST_F(SqliteDBTest, IsConnected) {
    EXPECT_TRUE(db->isConnected());
    db.reset(); // Explicitly close the connection
    EXPECT_FALSE(db->isConnected());
}

TEST_F(SqliteDBTest, GetLastInsertRowId) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    EXPECT_EQ(db->getLastInsertRowId(), 1);

    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));
    EXPECT_EQ(db->getLastInsertRowId(), 2);

    // Test after a non-insert query
    static_cast<void>(db->selectData("SELECT * FROM test_table;"));
    // The value should persist from the last insert, but this is implementation dependent
    // A safer test is to check after another insert
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Charlie', 3.3);"));
    EXPECT_EQ(db->getLastInsertRowId(), 3);

    // Test on empty table or after only non-insert queries (might return 0 or previous value)
    // The documentation says it's the rowid of the most recent successful INSERT
    // If no inserts have occurred, the result is undefined or 0.
    // Let's clear and test.
    TearDown(); // Remove the database file
    SetUp();    // Recreate the database and tables
    // After clear, there might not be a "last insert"
    // The behavior here depends on the underlying sqlite3_last_insert_rowid()
    // It's often 0 if no inserts have happened on the connection.
    // We can't strictly assert 0, but we can assert it doesn't throw if connected.
    EXPECT_NO_THROW(static_cast<void>(db->getLastInsertRowId()));
}

TEST_F(SqliteDBTest, GetChanges) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    EXPECT_EQ(db->getChanges(), 1);

    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));
    EXPECT_EQ(db->getChanges(), 1);

    int changes = db->updateData("UPDATE test_table SET value = value + 1;");
    EXPECT_EQ(changes, 2);
    EXPECT_EQ(db->getChanges(), 2);

    changes = db->deleteData("DELETE FROM test_table WHERE name = 'Alice';");
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(db->getChanges(), 1);

    // Select queries should not change the count
    static_cast<void>(db->selectData("SELECT * FROM test_table;"));
    EXPECT_EQ(db->getChanges(), 1); // Still 1 from the last delete

    // Test on non-existent rows
    static_cast<void>(db->updateData("UPDATE test_table SET value = 99 WHERE name = 'Charlie';"));
    EXPECT_EQ(db->getChanges(), 0);
    static_cast<void>(db->deleteData("DELETE FROM test_table WHERE name = 'Charlie';"));
    EXPECT_EQ(db->getChanges(), 0);
}

TEST_F(SqliteDBTest, GetTotalChanges) {
    EXPECT_EQ(db->getTotalChanges(), 0); // Should be 0 initially

    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Alice', 1.1);"));
    EXPECT_EQ(db->getTotalChanges(), 1);

    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name, value) VALUES ('Bob', 2.2);"));
    EXPECT_EQ(db->getTotalChanges(), 2);

    int changes = db->updateData("UPDATE test_table SET value = value + 1;");
    EXPECT_EQ(changes, 2);
    EXPECT_EQ(db->getTotalChanges(), 4); // 2 inserts + 2 updates

    changes = db->deleteData("DELETE FROM test_table WHERE name = 'Alice';");
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(db->getTotalChanges(), 5); // 4 + 1 delete

    // Select queries should not change the total count
    static_cast<void>(db->selectData("SELECT * FROM test_table;"));
    EXPECT_EQ(db->getTotalChanges(), 5);

    // Test on non-existent rows (should not increase total changes)
    static_cast<void>(db->updateData("UPDATE test_table SET value = 99 WHERE name = 'Charlie';"));
    EXPECT_EQ(db->getTotalChanges(), 5);
    static_cast<void>(db->deleteData("DELETE FROM test_table WHERE name = 'Charlie';"));
    EXPECT_EQ(db->getTotalChanges(), 5);
}

TEST_F(SqliteDBTest, TableExists) {
    EXPECT_TRUE(db->tableExists("test_table"));
    EXPECT_TRUE(db->tableExists("another_table"));
    EXPECT_FALSE(db->tableExists("non_existent_table"));
    EXPECT_FALSE(db->tableExists("")); // Test empty name
}

TEST_F(SqliteDBTest, GetTableSchema) {
    auto schema = db->getTableSchema("test_table");
    ASSERT_EQ(schema.size(), 3); // id, name, value

    // Check column names and types (order might vary slightly, but usually matches creation)
    // CID | Name  | Type    | Not Null | Default | PK
    // 0   | id    | INTEGER | 0        | NULL    | 1
    // 1   | name  | TEXT    | 0        | NULL    | 0
    // 2   | value | REAL    | 0        | NULL    | 0

    // Check first column (id)
    ASSERT_EQ(schema[0].size(), 6);
    EXPECT_EQ(schema[0][1], "id");
    EXPECT_EQ(schema[0][2], "INTEGER");
    EXPECT_EQ(schema[0][5], "1"); // PK

    // Check second column (name)
    ASSERT_EQ(schema[1].size(), 6);
    EXPECT_EQ(schema[1][1], "name");
    EXPECT_EQ(schema[1][2], "TEXT");

    // Check third column (value)
    ASSERT_EQ(schema[2].size(), 6);
    EXPECT_EQ(schema[2][1], "value");
    EXPECT_EQ(schema[2][2], "REAL");

    // Test non-existent table
    EXPECT_THROW(static_cast<void>(db->getTableSchema("non_existent_table")), SQLiteException);
}

TEST_F(SqliteDBTest, Vacuum) {
    // VACUUM is hard to test for correctness without checking file size changes
    // or internal state, but we can test that it executes without throwing.
    EXPECT_NO_THROW(static_cast<void>(db->vacuum()));
    EXPECT_TRUE(db->vacuum()); // Should return true on success
}

TEST_F(SqliteDBTest, Analyze) {
    // ANALYZE is hard to test for correctness, but we can test that it executes
    // without throwing.
    EXPECT_NO_THROW(static_cast<void>(db->analyze()));
    EXPECT_TRUE(db->analyze()); // Should return true on success
}

TEST_F(SqliteDBTest, MoveConstructor) {
    ASSERT_TRUE(db->executeQuery("INSERT INTO test_table (name) VALUES ('Original');"));
    EXPECT_TRUE(db->tableExists("test_table"));
    EXPECT_EQ(db->selectData("SELECT COUNT(*) FROM test_table;")[0][0], "1");

    SqliteDB moved_db = std::move(*db);

    // The original unique_ptr is now null, so db is null.
    // The moved_db should now manage the connection and data.
    EXPECT_TRUE(moved_db.tableExists("test_table"));
    EXPECT_EQ(moved_db.selectData("SELECT COUNT(*) FROM test_table;")[0][0], "1");

    // Attempting operations on the moved-from object (db) should be safe
    // because the unique_ptr is null.
    db.reset(); // Explicitly reset the original unique_ptr
    EXPECT_EQ(db, nullptr);
}

TEST_F(SqliteDBTest, MoveAssignment) {
    auto db1 = std::make_unique<SqliteDB>("db1.db");
    ASSERT_TRUE(db1->executeQuery("CREATE TABLE t1 (id INTEGER);"));
    ASSERT_TRUE(db1->executeQuery("INSERT INTO t1 VALUES (1);"));

    auto db2 = std::make_unique<SqliteDB>("db2.db");
    ASSERT_TRUE(db2->executeQuery("CREATE TABLE t2 (name TEXT);"));
    ASSERT_TRUE(db2->executeQuery("INSERT INTO t2 VALUES ('A');"));
    ASSERT_TRUE(db2->executeQuery("INSERT INTO t2 VALUES ('B');"));

    EXPECT_TRUE(db1->tableExists("t1"));
    EXPECT_FALSE(db1->tableExists("t2"));
    EXPECT_TRUE(db2->tableExists("t2"));
    EXPECT_FALSE(db2->tableExists("t1"));

    *db1 = std::move(*db2); // Move assign db2 to db1

    // db1 should now have the contents of db2
    EXPECT_FALSE(db1->tableExists("t1")); // Original table should be gone
    EXPECT_TRUE(db1->tableExists("t2"));
    EXPECT_EQ(db1->selectData("SELECT COUNT(*) FROM t2;")[0][0], "2");

    // db2 unique_ptr is now null
    db2.reset();

    // Clean up the files created for this test
    std::remove("db1.db");
    std::remove("db2.db");
}

TEST_F(SqliteDBTest, CheckConnectionThrowsWhenDisconnected) {
    db.reset(); // Explicitly close the connection

    // Most operations should now throw SQLiteException
    EXPECT_THROW(static_cast<void>(db->executeQuery("SELECT 1;")), SQLiteException);
    EXPECT_THROW(static_cast<void>(db->selectData("SELECT 1;")), SQLiteException);
    EXPECT_THROW(db->beginTransaction(), SQLiteException);
    EXPECT_THROW(static_cast<void>(db->getLastInsertRowId()), SQLiteException);
    EXPECT_THROW(static_cast<void>(db->getChanges()), SQLiteException);
    EXPECT_THROW(static_cast<void>(db->getTotalChanges()), SQLiteException);
    EXPECT_THROW(static_cast<void>(db->tableExists("any")), SQLiteException);
    EXPECT_THROW(static_cast<void>(db->getTableSchema("any")), SQLiteException);
    EXPECT_THROW(static_cast<void>(db->vacuum()), SQLiteException);
    EXPECT_THROW(static_cast<void>(db->analyze()), SQLiteException);
    // rollbackTransaction() is designed not to throw
    EXPECT_NO_THROW(db->rollbackTransaction());
    // withTransaction() will throw the exception from beginTransaction
    EXPECT_THROW(db->withTransaction([](){}), SQLiteException);
}

#endif // ATOM_SEARCH_TEST_SQLITE_HPP