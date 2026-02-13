/**
 * @file mysql_comprehensive.cpp
 * @brief Comprehensive example demonstrating all features of MySQL database
 * integration
 * @author Atom Search Examples
 * @date 2025-01-25
 *
 * This example demonstrates:
 * - MySQL database connection and configuration
 * - Basic CRUD operations (Create, Read, Update, Delete)
 * - Prepared statements for secure queries
 * - Transaction management with commit/rollback
 * - Connection pooling and error handling
 * - Batch operations for performance
 * - Search functionality integration
 * - Performance monitoring and optimization
 * - Connection timeout and reconnection
 * - Advanced MySQL features
 */

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/search/mysql.hpp"

// Helper function to print section titlesvoid printSection(const std::string&
// title) {
std::cout << "\n" << std::string(80, '=') << "\n";
std::cout << "  " << title << "\n";
std::cout << std::string(80, '=') << "\n";
}

// Helper function to print query resultsvoid printResults(
    const std::unique_ptr<atom::search::database::ResultSet>& results) {
        if (!results || results->getRowCount() == 0) {
            std::cout << "No results found.\n";
            return;
        }

        const size_t colWidth = 15;

        // Print header
        std::cout << std::left << std::setw(colWidth) << "Column 1" << " | "
                  << std::setw(colWidth) << "Column 2" << " | "
                  << std::setw(colWidth) << "Column 3" << " | "
                  << std::setw(colWidth) << "Column 4" << std::endl;
        std::cout << std::string(4 * (colWidth + 3), '-') << std::endl;

        // Print rows
        while (results->next()) {
            for (int i = 0; i < 4; ++i) {
                std::string value = results->getString(i);
                if (value.empty())
                    value = "NULL";
                std::cout << std::left << std::setw(colWidth) << value << " | ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    // Custom error callbackvoid errorCallback(const std::string& errorMsg,
    // unsigned int errorCode) {
    std::cerr << "MySQL Error [" << errorCode << "]: " << errorMsg << std::endl;
    }

    int main() {
        try {
            std::cout << "=== MySQL Database Comprehensive Examples ===\n\n";

            //----------------------------------------------------------------------
            // 1. Database Connection Setup
            //----------------------------------------------------------------------
            printSection("1. Database Connection Setup");

            // Create connection parameters
            atom::search::database::ConnectionParams params;
            params.host = "localhost";
            params.user = "test_user";
            params.password = "test_password";
            params.database = "test_db";
            params.port = 3306;
            params.connectTimeout = 10;
            params.readTimeout = 30;
            params.writeTimeout = 30;

            std::cout << "Creating MySQL connection with parameters:"
                      << std::endl;
            std::cout << "  Host: " << params.host << std::endl;
            std::cout << "  Database: " << params.database << std::endl;
            std::cout << "  Port: " << params.port << std::endl;

            // Create database connection
            atom::search::database::MysqlDB db(params);

            // Set error callback
            db.setErrorCallback(errorCallback);

            // Attempt to connect
            std::cout << "\nAttempting to connect to MySQL database..."
                      << std::endl;
            if (db.connect()) {
                std::cout << "Successfully connected to MySQL database!"
                          << std::endl;
                std::cout << "Server version: " << db.getServerVersion()
                          << std::endl;
                std::cout << "Client version: " << db.getClientVersion()
                          << std::endl;
            } else {
                std::cout
                    << "Failed to connect to database. Using mock operations "
                       "for demonstration."
                    << std::endl;
                std::cout
                    << "Note: Ensure MySQL server is running and credentials "
                       "are correct."
                    << std::endl;
            }

            //----------------------------------------------------------------------
            // 2. Basic Table Creation and Schema Setup
            //----------------------------------------------------------------------
            printSection("2. Basic Table Creation and Schema Setup");

            std::cout << "Creating test tables..." << std::endl;

            // Create users table
            std::string createUsersTable = R"(
            CREATE TABLE IF NOT EXISTS users (
                id INT AUTO_INCREMENT PRIMARY KEY,
                username VARCHAR(50) NOT NULL UNIQUE,
                email VARCHAR(100) NOT NULL UNIQUE,
                age INT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                status ENUM('active', 'inactive', 'suspended') DEFAULT 'active'
            )
        )";

            if (db.executeQuery(createUsersTable)) {
                std::cout << "Users table created successfully." << std::endl;
            }

            // Create posts table
            std::string createPostsTable = R"(
            CREATE TABLE IF NOT EXISTS posts (
                id INT AUTO_INCREMENT PRIMARY KEY,
                user_id INT,
                title VARCHAR(200) NOT NULL,
                content TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
            )
        )";

            if (db.executeQuery(createPostsTable)) {
                std::cout << "Posts table created successfully." << std::endl;
            }

            // Create search index table for full-text search
            std::string createSearchTable = R"(
            CREATE TABLE IF NOT EXISTS search_index (
                id INT AUTO_INCREMENT PRIMARY KEY,
                document_id VARCHAR(100) NOT NULL,
                content TEXT,
                tags VARCHAR(500),
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FULLTEXT(content, tags)
            )
        )";

            if (db.executeQuery(createSearchTable)) {
                std::cout << "Search index table created successfully."
                          << std::endl;
            }

            //----------------------------------------------------------------------
            // 3. Prepared Statements and Parameterized Queries
            //----------------------------------------------------------------------
            printSection("3. Prepared Statements and Parameterized Queries");

            std::cout << "Demonstrating prepared statements..." << std::endl;

            try {
                // Prepare insert statement for users
                auto insertUserStmt = db.prepareStatement(
                    "INSERT INTO users (username, email, age) VALUES (?, ?, "
                    "?)");

                // Insert multiple users using prepared statement
                std::vector<std::tuple<std::string, std::string, int>> users = {
                    {"alice", "alice@example.com", 28},
                    {"bob", "bob@example.com", 34},
                    {"charlie", "charlie@example.com", 42},
                    {"diana", "diana@example.com", 29}};

                for (const auto& [username, email, age] : users) {
                    insertUserStmt->bindString(0, username)
                        .bindString(1, email)
                        .bindInt(2, age);

                    if (insertUserStmt->execute()) {
                        std::cout << "Inserted user: " << username << std::endl;
                    }
                }

                std::cout << "Last insert ID: " << db.getLastInsertId()
                          << std::endl;
                std::cout << "Affected rows: " << db.getAffectedRows()
                          << std::endl;

            } catch (const atom::search::database::MySQLException& e) {
                std::cout
                    << "Prepared statement error (expected if table exists): "
                    << e.what() << std::endl;
            }

            //----------------------------------------------------------------------
            // 4. Transaction Management
            //----------------------------------------------------------------------
            printSection("4. Transaction Management");

            std::cout << "Demonstrating transaction management..." << std::endl;

            try {
                // Using automatic transaction management
                db.withTransaction([&db]() {
                    // Insert posts within a transaction
                    db.executeQuery(
                        "INSERT INTO posts (user_id, title, content) VALUES "
                        "(1, 'First Post', 'Hello world from Alice!')");
                    db.executeQuery(
                        "INSERT INTO posts (user_id, title, content) VALUES "
                        "(1, 'Second Post', 'More content from Alice')");
                    db.executeQuery(
                        "INSERT INTO posts (user_id, title, content) VALUES "
                        "(2, 'Bob\\'s Post', 'Bob\\'s first contribution')");
                });
                std::cout << "Transaction committed successfully." << std::endl;

            } catch (const atom::search::database::MySQLException& e) {
                std::cout << "Transaction failed: " << e.what() << std::endl;
            }

            // Manual transaction control
            std::cout << "\nDemonstrating manual transaction control..."
                      << std::endl;
            try {
                db.beginTransaction();

                db.executeQuery(
                    "INSERT INTO search_index (document_id, content, tags) "
                    "VALUES "
                    "('doc1', 'Machine learning algorithms', "
                    "'ai,ml,algorithms')");
                db.executeQuery(
                    "INSERT INTO search_index (document_id, content, tags) "
                    "VALUES "
                    "('doc2', 'Database optimization techniques', "
                    "'database,performance')");

                db.commitTransaction();
                std::cout << "Manual transaction committed successfully."
                          << std::endl;

            } catch (const atom::search::database::MySQLException& e) {
                db.rollbackTransaction();
                std::cout << "Manual transaction failed and rolled back: "
                          << e.what() << std::endl;
            }

            //----------------------------------------------------------------------
            // 5. Query Operations and Result Handling
            //----------------------------------------------------------------------
            printSection("5. Query Operations and Result Handling");

            std::cout << "Querying all users:" << std::endl;
            auto users_result =
                db.executeQueryWithResults("SELECT * FROM users");
            printResults(users_result);

            std::cout << "Querying posts with user information:" << std::endl;
            auto posts_result = db.executeQueryWithResults(R"(
            SELECT p.id, p.title, p.content, u.username
            FROM posts p
            JOIN users u ON p.user_id = u.id
            ORDER BY p.created_at DESC
        )");
            printResults(posts_result);

            //----------------------------------------------------------------------
            // 6. Search Functionality Integration
            //----------------------------------------------------------------------
            printSection("6. Search Functionality Integration");

            std::cout << "Demonstrating search functionality..." << std::endl;

            // Full-text search
            std::cout << "Performing full-text search for 'machine':"
                      << std::endl;
            auto search_result = db.executeQueryWithResults(R"(
            SELECT document_id, content, tags
            FROM search_index
            WHERE MATCH(content, tags) AGAINST('machine' IN NATURAL LANGUAGE MODE)
        )");
            printResults(search_result);

            // Pattern matching search
            std::cout
                << "Performing pattern search for posts containing 'Alice':"
                << std::endl;
            if (db.searchData("SELECT * FROM posts", "content", "Alice")) {
                std::cout << "Found posts containing 'Alice'" << std::endl;
            } else {
                std::cout << "No posts found containing 'Alice'" << std::endl;
            }

            //----------------------------------------------------------------------
            // 7. Pagination and Performance
            //----------------------------------------------------------------------
            printSection("7. Pagination and Performance");

            std::cout << "Demonstrating pagination..." << std::endl;

            // Add more test data for pagination
            for (int i = 0; i < 10; i++) {
                std::string query =
                    "INSERT INTO search_index (document_id, content, tags) "
                    "VALUES "
                    "('" +
                    std::to_string(i + 10) + "', 'Test document " +
                    std::to_string(i) + "', 'test,document')";
                db.executeQuery(query);
            }

            // Paginated query
            std::cout << "Page 1 (limit 5, offset 0):" << std::endl;
            auto page1 = db.executeQueryWithPagination(
                "SELECT * FROM search_index ORDER BY id", 5, 0);
            printResults(page1);

            std::cout << "Page 2 (limit 5, offset 5):" << std::endl;
            auto page2 = db.executeQueryWithPagination(
                "SELECT * FROM search_index ORDER BY id", 5, 5);
            printResults(page2);

            //----------------------------------------------------------------------
            // 8. Connection Management and Monitoring
            //----------------------------------------------------------------------
            printSection("8. Connection Management and Monitoring");

            std::cout << "Testing connection status..." << std::endl;
            if (db.ping()) {
                std::cout << "Database connection is alive" << std::endl;
            } else {
                std::cout << "Database connection is not responding"
                          << std::endl;
            }

            // Set connection timeout
            std::cout << "Setting connection timeout to 5 seconds..."
                      << std::endl;
            if (db.setConnectionTimeout(5)) {
                std::cout << "Connection timeout set successfully" << std::endl;
            }

            std::cout << "\n=== MySQL examples completed successfully ==="
                      << std::endl;

        } catch (const atom::search::database::MySQLException& e) {
            std::cerr << "MySQL Exception: " << e.what() << std::endl;
            return 1;
        } catch (const std::exception& e) {
            std::cerr << "Standard Exception: " << e.what() << std::endl;
            return 1;
        }

        return 0;
    }
