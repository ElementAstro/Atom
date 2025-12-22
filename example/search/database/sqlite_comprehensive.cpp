/**
 * @file sqlite_comprehensive.cpp
 * @brief Comprehensive example demonstrating all features of SQLite database
 * integration
 * @author Atom Search Examples
 * @date 2025-01-25
 *
 * This example demonstrates:
 * - SQLite database creation and connection
 * - Basic CRUD operations with error handling
 * - Parameterized queries for security
 * - Transaction management with automatic rollback
 * - Full-text search capabilities
 * - Pagination and result handling
 * - Data validation and integrity checks
 * - Performance optimization techniques
 * - Move semantics and resource management
 * - Integration with search functionality
 */

#include <cassert>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/search/sqlite.hpp"

// Helper function to print section titlesvoid printSection(const std::string&
// title) {
std::cout << "\n" << std::string(80, '=') << "\n";
std::cout << "  " << title << "\n";
std::cout << std::string(80, '=') << "\n";
}

// Helper function to print query resultsvoid printResults(const
// atom::search::database::SqliteDB::ResultSet& results) {
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
              << std::string(results[0].size() * (colWidth + 3), '-') << "\n";
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

// Custom error callbackvoid errorCallback(std::string_view errorMsg) {
std::cerr << "SQLite Error: " << errorMsg << std::endl;
}

int main() {
    try {
        std::cout << "=== SQLite Database Comprehensive Examples ===\n\n";

        //----------------------------------------------------------------------
        // 1. Database Creation and Connection
        //----------------------------------------------------------------------
        printSection("1. Database Creation and Connection");

        // Database path (in-memory for this example)
        const std::string dbPath = ":memory:";
        std::cout << "Creating SQLite database connection..." << std::endl;
        std::cout << "Database path: " << dbPath << std::endl;

        // Create a database connection
        atom::search::database::SqliteDB db(dbPath);

        // Set error callback
        std::cout << "Setting error callback..." << std::endl;
        db.setErrorMessageCallback(errorCallback);

        // Check connection status
        if (db.isConnected()) {
            std::cout << "Successfully connected to SQLite database."
                      << std::endl;
        } else {
            std::cerr << "Failed to connect to database!" << std::endl;
            return 1;
        }

        //----------------------------------------------------------------------
        // 2. Schema Creation and Table Setup
        //----------------------------------------------------------------------
        printSection("2. Schema Creation and Table Setup");

        std::cout << "Creating comprehensive database schema..." << std::endl;

        // Create users table with various data types
        bool success = db.executeQuery(R"(
            CREATE TABLE users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT NOT NULL UNIQUE,
                email TEXT UNIQUE,
                age INTEGER CHECK(age >= 0 AND age <= 150),
                balance REAL DEFAULT 0.0,
                is_active BOOLEAN DEFAULT 1,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                metadata TEXT
            )
        )");

        if (success) {
            std::cout << "Users table created successfully." << std::endl;
        }

        // Create posts table with foreign key relationship
        success = db.executeQuery(R"(
            CREATE TABLE posts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER NOT NULL,
                title TEXT NOT NULL,
                content TEXT,
                view_count INTEGER DEFAULT 0,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
            )
        )");

        if (success) {
            std::cout << "Posts table created successfully." << std::endl;
        }

        // Create full-text search table
        success = db.executeQuery(R"(
            CREATE VIRTUAL TABLE documents_fts USING fts5(
                document_id,
                title,
                content,
                tags
            )
        )");

        if (success) {
            std::cout << "Full-text search table created successfully."
                      << std::endl;
        }

        // Create indexes for performance
        db.executeQuery("CREATE INDEX idx_users_email ON users(email)");
        db.executeQuery("CREATE INDEX idx_posts_user_id ON posts(user_id)");
        db.executeQuery(
            "CREATE INDEX idx_posts_created_at ON posts(created_at)");

        std::cout << "Database indexes created for performance optimization."
                  << std::endl;

        //----------------------------------------------------------------------
        // 3. Parameterized Queries and Data Insertion
        //----------------------------------------------------------------------
        printSection("3. Parameterized Queries and Data Insertion");

        std::cout << "Inserting data using parameterized queries..."
                  << std::endl;

        // Insert users with parameterized queries
        std::vector<std::tuple<std::string, std::string, int, double>> users = {
            {"alice", "alice@example.com", 28, 1500.50},
            {"bob", "bob@example.com", 34, 2300.75},
            {"charlie", "charlie@example.com", 42, 890.25},
            {"diana", "diana@example.com", 29, 3200.00},
            {"eve", "eve@example.com", 31, 1750.80}};

        for (const auto& [username, email, age, balance] : users) {
            success = db.executeParameterizedQuery(
                "INSERT INTO users (username, email, age, balance) VALUES (?, "
                "?, ?, ?)",
                username, email, age, balance);

            if (success) {
                std::cout << "Inserted user: " << username
                          << " (ID: " << db.getLastInsertRowId() << ")"
                          << std::endl;
            }
        }

        std::cout << "Total changes made: " << db.getChanges() << std::endl;

        //----------------------------------------------------------------------
        // 4. Transaction Management
        //----------------------------------------------------------------------
        printSection("4. Transaction Management");

        std::cout << "Demonstrating transaction management..." << std::endl;

        try {
            // Using automatic transaction management
            db.withTransaction([&db]() {
                // Insert multiple posts within a transaction
                db.executeParameterizedQuery(
                    "INSERT INTO posts (user_id, title, content) VALUES (?, ?, "
                    "?)",
                    1, "Alice's First Post",
                    "Welcome to my blog! This is my first post.");
                db.executeParameterizedQuery(
                    "INSERT INTO posts (user_id, title, content) VALUES (?, ?, "
                    "?)",
                    1, "Alice's Second Post",
                    "Here's another interesting article.");
                db.executeParameterizedQuery(
                    "INSERT INTO posts (user_id, title, content) VALUES (?, ?, "
                    "?)",
                    2, "Bob's Technical Guide",
                    "A comprehensive guide to database optimization.");
                db.executeParameterizedQuery(
                    "INSERT INTO posts (user_id, title, content) VALUES (?, ?, "
                    "?)",
                    3, "Charlie's Thoughts",
                    "Some philosophical musings on technology.");
            });
            std::cout << "Transaction committed successfully." << std::endl;

        } catch (const atom::search::database::SQLiteException& e) {
            std::cout << "Transaction failed: " << e.what() << std::endl;
        }

        // Manual transaction control with error simulation
        std::cout << "\nDemonstrating manual transaction with rollback..."
                  << std::endl;
        try {
            db.beginTransaction();

            // Insert valid data
            db.executeParameterizedQuery(
                "INSERT INTO posts (user_id, title, content) VALUES (?, ?, ?)",
                4, "Diana's Post", "This should be inserted.");

            // Simulate an error (invalid user_id)
            db.executeParameterizedQuery(
                "INSERT INTO posts (user_id, title, content) VALUES (?, ?, ?)",
                999, "Invalid Post", "This should cause a rollback.");

            db.commitTransaction();

        } catch (const atom::search::database::SQLiteException& e) {
            std::cout << "Error occurred, rolling back transaction: "
                      << e.what() << std::endl;
            db.rollbackTransaction();
        }

        //----------------------------------------------------------------------
        // 5. Query Operations and Result Handling
        //----------------------------------------------------------------------
        printSection("5. Query Operations and Result Handling");

        std::cout << "Querying all users:" << std::endl;
        auto users_result = db.selectData("SELECT * FROM users ORDER BY id");
        printResults(users_result);

        std::cout << "Querying posts with user information (JOIN):"
                  << std::endl;
        auto posts_result = db.selectData(R"(
            SELECT p.id, p.title, p.content, u.username, p.created_at
            FROM posts p
            JOIN users u ON p.user_id = u.id
            ORDER BY p.created_at DESC
        )");
        printResults(posts_result);

        //----------------------------------------------------------------------
        // 6. Scalar Value Retrieval
        //----------------------------------------------------------------------
        printSection("6. Scalar Value Retrieval");

        // Get integer value
        auto userCount = db.getIntValue("SELECT COUNT(*) FROM users");
        if (userCount) {
            std::cout << "Total users: " << *userCount << std::endl;
        }

        // Get double value
        auto avgAge = db.getDoubleValue("SELECT AVG(age) FROM users");
        if (avgAge) {
            std::cout << "Average user age: " << std::fixed
                      << std::setprecision(2) << *avgAge << std::endl;
        }

        // Get text value
        auto oldestUser = db.getTextValue(
            "SELECT username FROM users ORDER BY age DESC LIMIT 1");
        if (oldestUser) {
            std::cout << "Oldest user: " << *oldestUser << std::endl;
        }

        // Get total balance
        auto totalBalance = db.getDoubleValue("SELECT SUM(balance) FROM users");
        if (totalBalance) {
            std::cout << "Total user balance: $" << std::fixed
                      << std::setprecision(2) << *totalBalance << std::endl;
        }

        //----------------------------------------------------------------------
        // 7. Full-Text Search Capabilities
        //----------------------------------------------------------------------
        printSection("7. Full-Text Search Capabilities");

        std::cout << "Setting up full-text search data..." << std::endl;

        // Insert documents for full-text search
        std::vector<
            std::tuple<std::string, std::string, std::string, std::string>>
            documents = {
                {"doc1", "Machine Learning Basics",
                 "Introduction to machine learning algorithms and concepts",
                 "ai,ml,algorithms,tutorial"},
                {"doc2", "Database Optimization",
                 "Advanced techniques for optimizing database performance",
                 "database,performance,sql,optimization"},
                {"doc3", "Web Development Guide",
                 "Complete guide to modern web development practices",
                 "web,html,css,javascript,development"},
                {"doc4", "Data Science Tutorial",
                 "Learn data science with Python and machine learning",
                 "python,data,science,ml,analytics"},
                {"doc5", "System Architecture",
                 "Designing scalable and robust system architectures",
                 "architecture,scalability,design,systems"}};

        for (const auto& [doc_id, title, content, tags] : documents) {
            db.executeParameterizedQuery(
                "INSERT INTO documents_fts (document_id, title, content, tags) "
                "VALUES (?, ?, ?, ?)",
                doc_id, title, content, tags);
        }

        std::cout << "Performing full-text search for 'machine learning':"
                  << std::endl;
        auto search_result = db.selectData(R"(
            SELECT document_id, title, content
            FROM documents_fts
            WHERE documents_fts MATCH 'machine learning'
            ORDER BY rank
        )");
        printResults(search_result);

        std::cout << "Performing full-text search for 'database OR web':"
                  << std::endl;
        auto search_result2 = db.selectData(R"(
            SELECT document_id, title, tags
            FROM documents_fts
            WHERE documents_fts MATCH 'database OR web'
            ORDER BY rank
        )");
        printResults(search_result2);

        //----------------------------------------------------------------------
        // 8. Data Validation and Search
        //----------------------------------------------------------------------
        printSection("8. Data Validation and Search");

        // Search for specific data
        std::cout << "Searching for users with 'alice' in their data:"
                  << std::endl;
        bool found = db.searchData("SELECT * FROM users", "alice");
        std::cout << "Search result: " << (found ? "Found" : "Not found")
                  << std::endl;

        // Validate data integrity
        std::cout << "\nValidating data integrity..." << std::endl;
        bool isValid =
            db.validateData("SELECT * FROM users WHERE username = 'alice'",
                            "SELECT COUNT(*) > 0 FROM users WHERE username = "
                            "'alice' AND age > 0");
        std::cout << "Data validation result: "
                  << (isValid ? "Valid" : "Invalid") << std::endl;

        return 0;

    } catch (const atom::search::database::SQLiteException& e) {
        std::cerr << "SQLite Exception: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Standard Exception: " << e.what() << std::endl;
        return 1;
    }
}
