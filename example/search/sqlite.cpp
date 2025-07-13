#include "atom/search/sqlite.hpp"
#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Helper function to print query results
void printResults(const SqliteDB::ResultSet& results) {
    if (results.empty()) {
        std::cout << "No results found.\n";
        return;
    }

    // Calculate column widths for pretty printing
    const size_t colWidth = 15;

    // Print header row (first row of first result)
    if (!results.empty() && !results[0].empty()) {
        for (size_t i = 0; i < results[0].size(); ++i) {
            std::cout << std::left << std::setw(colWidth)
                      << "Column " + std::to_string(i) << " | ";
        }
        std::cout << "\n"
                  << std::string(results[0].size() * (colWidth + 3), '-')
                  << "\n";
    }

    // Print all rows
    for (const auto& row : results) {
        for (const auto& cell : row) {
            std::cout << std::left << std::setw(colWidth) << cell << " | ";
        }
        std::cout << '\n';
    }
    std::cout << "\n";
}

// Custom error callback
void errorCallback(std::string_view errorMsg) {
    std::cerr << "SQL Error: " << errorMsg << std::endl;
}

int main() {
    try {
        // Database path (in-memory for this example)
        const std::string dbPath = ":memory:";

        std::cout << "=== SqliteDB Comprehensive Examples ===\n\n";

        // Create a database connection
        std::cout << "Creating database connection...\n";
        SqliteDB db(dbPath);

        // Set error callback
        std::cout << "Setting error callback...\n";
        db.setErrorMessageCallback(errorCallback);

        // Check connection status
        if (db.isConnected()) {
            std::cout << "Successfully connected to database.\n\n";
        } else {
            std::cerr << "Failed to connect to database!\n";
            return 1;
        }

        // --- Basic Operations ---
        std::cout << "=== Creating Test Tables ===\n";

        // Create tables
        bool success = db.executeQuery(
            "CREATE TABLE users ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL,"
            "  email TEXT UNIQUE,"
            "  age INTEGER,"
            "  status TEXT DEFAULT 'active'"
            ");");

        if (success) {
            std::cout << "Users table created successfully.\n";
        }

        success = db.executeQuery(
            "CREATE TABLE posts ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  user_id INTEGER,"
            "  title TEXT NOT NULL,"
            "  content TEXT,"
            "  created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
            "  FOREIGN KEY (user_id) REFERENCES users (id)"
            ");");

        if (success) {
            std::cout << "Posts table created successfully.\n\n";
        }

        // --- Transaction Example ---
        std::cout << "=== Transaction Example ===\n";
        std::cout << "Inserting initial users using transaction...\n";

        try {
            db.withTransaction([&db]() {
                // Insert multiple users within a transaction
                db.executeQuery(
                    "INSERT INTO users (name, email, age) VALUES ('Alice', "
                    "'alice@example.com', 28);");
                db.executeQuery(
                    "INSERT INTO users (name, email, age) VALUES ('Bob', "
                    "'bob@example.com', 34);");
                db.executeQuery(
                    "INSERT INTO users (name, email, age) VALUES ('Charlie', "
                    "'charlie@example.com', 42);");
                db.executeQuery(
                    "INSERT INTO users (name, email, age) VALUES ('Diana', "
                    "'diana@example.com', 29);");
            });
            std::cout << "Transaction committed successfully.\n";
        } catch (const SQLiteException& e) {
            std::cerr << "Transaction failed: " << e.what() << "\n";
        }

        // --- Manual Transaction Control ---
        std::cout << "\nDemonstrating manual transaction control...\n";

        try {
            db.beginTransaction();
            db.executeQuery(
                "INSERT INTO posts (user_id, title, content) VALUES (1, 'First "
                "Post', 'Hello world!');");
            db.executeQuery(
                "INSERT INTO posts (user_id, title, content) VALUES (1, "
                "'Second Post', 'More content here.');");
            db.executeQuery(
                "INSERT INTO posts (user_id, title, content) VALUES (2, 'My "
                "Post', 'Bob''s first post.');");
            db.commitTransaction();
            std::cout << "Manual transaction committed successfully.\n";
        } catch (const SQLiteException& e) {
            // This would run if an error occurred
            db.rollbackTransaction();
            std::cerr << "Manual transaction failed and was rolled back: "
                      << e.what() << "\n";
        }

        // --- Select Data Example ---
        std::cout << "\n=== Select Data Example ===\n";
        std::cout << "Querying all users:\n";

        auto users = db.selectData("SELECT * FROM users;");
        printResults(users);

        // --- Parameterized Query Example ---
        std::cout << "=== Parameterized Query Example ===\n";
        std::cout << "Using parameterized query to insert a new user...\n";

        success = db.executeParameterizedQuery(
            "INSERT INTO users (name, email, age) VALUES (?, ?, ?);", "Eve",
            "eve@example.com", 31);

        if (success) {
            std::cout << "Parameterized insert successful.\n";
            std::cout << "Last insert row ID: " << db.getLastInsertRowId()
                      << "\n";
            std::cout << "Changes made: " << db.getChanges() << "\n\n";
        }

        // --- Get Scalar Values Examples ---
        std::cout << "=== Get Scalar Values Examples ===\n";

        // Get integer value
        auto userCount = db.getIntValue("SELECT COUNT(*) FROM users;");
        if (userCount) {
            std::cout << "User count: " << *userCount << "\n";
        }

        // Get double value
        auto avgAge = db.getDoubleValue("SELECT AVG(age) FROM users;");
        if (avgAge) {
            std::cout << "Average user age: " << *avgAge << "\n";
        }

        // Get text value
        auto oldestUser = db.getTextValue(
            "SELECT name FROM users ORDER BY age DESC LIMIT 1;");
        if (oldestUser) {
            std::cout << "Oldest user: " << *oldestUser << "\n\n";
        }

        // --- Search Example ---
        std::cout << "=== Search Example ===\n";

        bool found = db.searchData("SELECT * FROM users;", "Alice");
        std::cout << "Search for 'Alice': " << (found ? "Found" : "Not found")
                  << "\n";

        found = db.searchData("SELECT * FROM users;", "NotInDatabase");
        std::cout << "Search for 'NotInDatabase': "
                  << (found ? "Found" : "Not found") << "\n\n";

        // --- Update Example ---
        std::cout << "=== Update Example ===\n";

        int rowsAffected = db.updateData(
            "UPDATE users SET status = 'inactive' WHERE age > 35;");
        std::cout << "Users marked inactive: " << rowsAffected << "\n";

        std::cout << "Updated users:\n";
        users = db.selectData("SELECT * FROM users;");
        printResults(users);

        // --- Delete Example ---
        std::cout << "=== Delete Example ===\n";

        // First insert a user we'll delete
        db.executeQuery(
            "INSERT INTO users (name, email, age) VALUES ('Temporary', "
            "'temp@example.com', 25);");

        rowsAffected =
            db.deleteData("DELETE FROM users WHERE name = 'Temporary';");
        std::cout << "Deleted " << rowsAffected << " user(s).\n\n";

        // --- Validation Example ---
        std::cout << "=== Validation Example ===\n";

        bool isValid =
            db.validateData("SELECT * FROM users WHERE name = 'Alice';",
                            "SELECT COUNT(*) > 0 FROM users WHERE name = "
                            "'Alice' AND age < 30;");

        std::cout << "Validation check (Alice age < 30): "
                  << (isValid ? "Valid" : "Invalid") << "\n\n";

        // --- Pagination Example ---
        std::cout << "=== Pagination Example ===\n";

        // Insert more data for pagination demo
        db.withTransaction([&db]() {
            for (int i = 0; i < 10; i++) {
                db.executeParameterizedQuery(
                    "INSERT INTO users (name, email, age) VALUES (?, ?, ?);",
                    "User" + std::to_string(i),
                    "user" + std::to_string(i) + "@example.com", 20 + i);
            }
        });

        std::cout << "Page 1 (limit 5, offset 0):\n";
        auto page1 = db.selectDataWithPagination(
            "SELECT * FROM users ORDER BY id;", 5, 0);
        printResults(page1);

        std::cout << "Page 2 (limit 5, offset 5):\n";
        auto page2 = db.selectDataWithPagination(
            "SELECT * FROM users ORDER BY id;", 5, 5);
        printResults(page2);

        // --- Error Handling Examples ---
        std::cout << "=== Error Handling Examples ===\n";

        try {
            // Intentionally cause an error with invalid SQL
            db.executeQuery("SELECT * FROMM users;");
        } catch (const SQLiteException& e) {
            std::cout << "Expected error caught: " << e.what() << "\n";
        }

        try {
            // Intentionally cause an error with constraint violation
            db.executeQuery(
                "INSERT INTO users (name, email, age) VALUES ('Alice', "
                "'alice@example.com', 30);");
        } catch (const SQLiteException& e) {
            std::cout << "Expected constraint error caught: " << e.what()
                      << "\n\n";
        }

        // --- Move Semantics Examples ---
        std::cout << "=== Move Semantics Examples ===\n";

        // Move constructor
        SqliteDB movedDb = std::move(db);
        std::cout << "Database moved via move constructor.\n";

        // Check the moved database still works
        auto movedResult = movedDb.selectData("SELECT COUNT(*) FROM users;");
        if (!movedResult.empty() && !movedResult[0].empty()) {
            std::cout << "Moved database query successful. User count: "
                      << movedResult[0][0] << "\n";
        }

        // Move assignment
        SqliteDB anotherDb(":memory:");
        anotherDb = std::move(movedDb);
        std::cout << "Database moved via move assignment.\n";

        // Check the moved database still works
        auto anotherResult =
            anotherDb.selectData("SELECT COUNT(*) FROM users;");
        if (!anotherResult.empty() && !anotherResult[0].empty()) {
            std::cout << "Second moved database query successful. User count: "
                      << anotherResult[0][0] << "\n";
        }

        std::cout << "\n=== Examples completed successfully ===\n";

    } catch (const SQLiteException& e) {
        std::cerr << "SQLite Exception: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Standard Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
