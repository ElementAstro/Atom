#include "atom/extra/pugixml/modern_xml.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace atom::extra::pugixml;

// Helper function to create test XML files
void create_test_xml_file(const std::string& filename,
                          const std::string& content) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "Created test file: " << filename << std::endl;
    } else {
        std::cerr << "Failed to create test file: " << filename << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== PugiXML Basic Usage Example ===" << std::endl;

        // Create test XML files
        create_test_xml_file("books.xml",
                             R"(<?xml version="1.0" encoding="UTF-8"?>
<library>
    <book id="1" category="fiction">
        <title>The Great Gatsby</title>
        <author>F. Scott Fitzgerald</author>
        <year>1925</year>
        <price currency="USD">12.99</price>
        <description>A classic American novel set in the Jazz Age.</description>
    </book>
    <book id="2" category="science">
        <title>A Brief History of Time</title>
        <author>Stephen Hawking</author>
        <year>1988</year>
        <price currency="USD">15.99</price>
        <description>A popular science book about cosmology.</description>
    </book>
    <book id="3" category="programming">
        <title>The C++ Programming Language</title>
        <author>Bjarne Stroustrup</author>
        <year>2013</year>
        <price currency="USD">59.99</price>
        <description>The definitive guide to C++ by its creator.</description>
    </book>
</library>)");

        create_test_xml_file("config.xml",
                             R"(<?xml version="1.0" encoding="UTF-8"?>
<configuration>
    <database>
        <host>localhost</host>
        <port>5432</port>
        <name>myapp</name>
        <credentials>
            <username>admin</username>
            <password>secret123</password>
        </credentials>
        <pool>
            <min_connections>5</min_connections>
            <max_connections>20</max_connections>
            <timeout>30</timeout>
        </pool>
    </database>
    <server>
        <bind_address>0.0.0.0</bind_address>
        <port>8080</port>
        <ssl enabled="true">
            <cert_file>/path/to/cert.pem</cert_file>
            <key_file>/path/to/key.pem</key_file>
        </ssl>
    </server>
    <logging>
        <level>info</level>
        <file>/var/log/myapp.log</file>
        <max_size>10MB</max_size>
    </logging>
</configuration>)");

        // 1. Basic document loading and parsing
        std::cout << "\n1. Basic Document Loading and Parsing:" << std::endl;
        {
            try {
                auto doc = Document::from_file("books.xml");
                std::cout << "Successfully loaded books.xml" << std::endl;

                auto root = doc.root();
                std::cout << "Root element: " << root.name() << std::endl;

                // Count books
                auto books = root.children("book");
                std::cout << "Number of books: " << books.size() << std::endl;

            } catch (const ParseException& e) {
                std::cerr << "Parse error: " << e.what() << std::endl;
            }
        }

        // 2. Node traversal and data extraction
        std::cout << "\n2. Node Traversal and Data Extraction:" << std::endl;
        {
            auto doc = Document::from_file("books.xml");
            auto root = doc.root();

            std::cout << "Book catalog:" << std::endl;
            for (const auto& book : root.children("book")) {
                auto id = book.attribute("id").as_int();
                auto category = book.attribute("category").as_string();
                auto title = book.child("title").text().as_string();
                auto author = book.child("author").text().as_string();
                auto year = book.child("year").text().as_int();
                auto price = book.child("price").text().as_double();
                auto currency =
                    book.child("price").attribute("currency").as_string();

                std::cout << "  Book #" << id << " (" << category << ")"
                          << std::endl;
                std::cout << "    Title: " << title << std::endl;
                std::cout << "    Author: " << author << std::endl;
                std::cout << "    Year: " << year << std::endl;
                std::cout << "    Price: " << price << " " << currency
                          << std::endl;
                std::cout << std::endl;
            }
        }

        // 3. XPath queries
        std::cout << "\n3. XPath Queries:" << std::endl;
        {
            auto doc = Document::from_file("books.xml");

            // Find all fiction books
            auto fiction_books =
                doc.select_nodes("//book[@category='fiction']");
            std::cout << "Fiction books:" << std::endl;
            for (const auto& node : fiction_books) {
                auto title = node.node().child("title").text().as_string();
                std::cout << "  " << title << std::endl;
            }

            // Find books published after 1950
            auto modern_books = doc.select_nodes("//book[year > 1950]");
            std::cout << "\nBooks published after 1950:" << std::endl;
            for (const auto& node : modern_books) {
                auto title = node.node().child("title").text().as_string();
                auto year = node.node().child("year").text().as_int();
                std::cout << "  " << title << " (" << year << ")" << std::endl;
            }

            // Find expensive books (price > 20)
            auto expensive_books = doc.select_nodes("//book[price > 20]");
            std::cout << "\nExpensive books (> $20):" << std::endl;
            for (const auto& node : expensive_books) {
                auto title = node.node().child("title").text().as_string();
                auto price = node.node().child("price").text().as_double();
                std::cout << "  " << title << " ($" << price << ")"
                          << std::endl;
            }
        }

        // 4. Document modification
        std::cout << "\n4. Document Modification:" << std::endl;
        {
            auto doc = Document::from_file("books.xml");
            auto root = doc.root();

            // Add a new book
            auto new_book = root.append_child("book");
            new_book.append_attribute("id") = "4";
            new_book.append_attribute("category") = "mystery";

            new_book.append_child("title").text() = "The Maltese Falcon";
            new_book.append_child("author").text() = "Dashiell Hammett";
            new_book.append_child("year").text() = "1930";

            auto price_node = new_book.append_child("price");
            price_node.append_attribute("currency") = "USD";
            price_node.text() = "14.99";

            new_book.append_child("description").text() =
                "A classic detective novel.";

            std::cout << "Added new book: The Maltese Falcon" << std::endl;

            // Modify existing book
            auto first_book = root.child("book");
            auto description = first_book.child("description");
            description.text() =
                "Updated: A masterpiece of American literature.";

            std::cout << "Updated description of first book" << std::endl;

            // Save modified document
            doc.save_file("books_modified.xml");
            std::cout << "Saved modified document to books_modified.xml"
                      << std::endl;
        }

        // 5. Configuration file parsing
        std::cout << "\n5. Configuration File Parsing:" << std::endl;
        {
            auto doc = Document::from_file("config.xml");
            auto root = doc.root();

            // Database configuration
            auto db = root.child("database");
            std::cout << "Database Configuration:" << std::endl;
            std::cout << "  Host: " << db.child("host").text().as_string()
                      << std::endl;
            std::cout << "  Port: " << db.child("port").text().as_int()
                      << std::endl;
            std::cout << "  Name: " << db.child("name").text().as_string()
                      << std::endl;

            auto credentials = db.child("credentials");
            std::cout << "  Username: "
                      << credentials.child("username").text().as_string()
                      << std::endl;
            std::cout << "  Password: "
                      << credentials.child("password").text().as_string()
                      << std::endl;

            auto pool = db.child("pool");
            std::cout << "  Pool min: "
                      << pool.child("min_connections").text().as_int()
                      << std::endl;
            std::cout << "  Pool max: "
                      << pool.child("max_connections").text().as_int()
                      << std::endl;

            // Server configuration
            auto server = root.child("server");
            std::cout << "\nServer Configuration:" << std::endl;
            std::cout << "  Bind address: "
                      << server.child("bind_address").text().as_string()
                      << std::endl;
            std::cout << "  Port: " << server.child("port").text().as_int()
                      << std::endl;

            auto ssl = server.child("ssl");
            auto ssl_enabled = ssl.attribute("enabled").as_bool();
            std::cout << "  SSL enabled: " << (ssl_enabled ? "yes" : "no")
                      << std::endl;
            if (ssl_enabled) {
                std::cout << "  Cert file: "
                          << ssl.child("cert_file").text().as_string()
                          << std::endl;
                std::cout << "  Key file: "
                          << ssl.child("key_file").text().as_string()
                          << std::endl;
            }
        }

        // 6. Creating documents from scratch
        std::cout << "\n6. Creating Documents from Scratch:" << std::endl;
        {
            auto doc = Document::create_empty();
            auto root = doc.append_child("employees");

            // Add employees
            std::vector<std::tuple<std::string, std::string, int, double>>
                employees = {{"John Doe", "Software Engineer", 30, 75000.0},
                             {"Jane Smith", "Product Manager", 35, 85000.0},
                             {"Bob Johnson", "Designer", 28, 65000.0}};

            for (const auto& [name, position, age, salary] : employees) {
                auto employee = root.append_child("employee");
                employee.append_child("name").text() = name.c_str();
                employee.append_child("position").text() = position.c_str();
                employee.append_child("age").text() = age;
                employee.append_child("salary").text() = salary;
            }

            doc.save_file("employees.xml");
            std::cout << "Created employees.xml with " << employees.size()
                      << " employees" << std::endl;
        }

        // 7. Error handling and validation
        std::cout << "\n7. Error Handling and Validation:" << std::endl;
        {
            // Try to load invalid XML
            create_test_xml_file("invalid.xml", R"(<?xml version="1.0"?>
<root>
    <unclosed_tag>
    <another>content</another>
</root>)");

            try {
                auto doc = Document::from_file("invalid.xml");
                std::cout << "Loaded invalid XML (unexpected)" << std::endl;
            } catch (const ParseException& e) {
                std::cout << "Expected parse error: " << e.what() << std::endl;
            }

            // Try to access non-existent file
            try {
                auto doc = Document::from_file("non_existent.xml");
                std::cout << "Loaded non-existent file (unexpected)"
                          << std::endl;
            } catch (const ParseException& e) {
                std::cout << "Expected file error: " << e.what() << std::endl;
            }

            // Safe attribute/text access
            auto doc = Document::from_file("books.xml");
            auto root = doc.root();
            auto first_book = root.child("book");

            // Safe access to non-existent attribute
            auto non_existent_attr = first_book.attribute("non_existent");
            std::cout << "Non-existent attribute value: '"
                      << non_existent_attr.as_string() << "'" << std::endl;

            // Safe access to non-existent child
            auto non_existent_child = first_book.child("non_existent");
            std::cout << "Non-existent child text: '"
                      << non_existent_child.text().as_string() << "'"
                      << std::endl;
        }

        // 8. Different parsing options
        std::cout << "\n8. Different Parsing Options:" << std::endl;
        {
            std::string xml_with_whitespace = R"(<?xml version="1.0"?>
<root>
    <item>   Content with whitespace   </item>
    <item>
        Multi-line
        content
    </item>
</root>)";

            // Parse with default options
            std::cout << "Default parsing:" << std::endl;
            auto doc1 = Document::from_string(xml_with_whitespace);
            auto items1 = doc1.root().children("item");
            for (const auto& item : items1) {
                std::cout << "  Item: '" << item.text().as_string() << "'"
                          << std::endl;
            }

            // Parse with trimmed whitespace
            std::cout << "\nTrimmed whitespace parsing:" << std::endl;
            LoadOptions trim_options;
            trim_options.trim_whitespace();
            auto doc2 =
                Document::from_string(xml_with_whitespace, trim_options);
            auto items2 = doc2.root().children("item");
            for (const auto& item : items2) {
                std::cout << "  Item: '" << item.text().as_string() << "'"
                          << std::endl;
            }
        }

        // 9. Namespace handling
        std::cout << "\n9. Namespace Handling:" << std::endl;
        {
            std::string xml_with_namespaces = R"(<?xml version="1.0"?>
<root xmlns:book="http://example.com/book" xmlns:author="http://example.com/author">
    <book:catalog>
        <book:item id="1">
            <book:title>Sample Book</book:title>
            <author:name>Sample Author</author:name>
        </book:item>
    </book:catalog>
</root>)";

            auto doc = Document::from_string(xml_with_namespaces);
            auto root = doc.root();

            // Access namespaced elements
            auto catalog = root.child("book:catalog");
            if (catalog) {
                std::cout << "Found catalog element" << std::endl;
                auto item = catalog.child("book:item");
                if (item) {
                    auto title = item.child("book:title").text().as_string();
                    auto author = item.child("author:name").text().as_string();
                    std::cout << "  Title: " << title << std::endl;
                    std::cout << "  Author: " << author << std::endl;
                }
            }
        }

        // Cleanup test files
        std::cout << "\nCleaning up test files..." << std::endl;
        std::filesystem::remove("books.xml");
        std::filesystem::remove("config.xml");
        std::filesystem::remove("books_modified.xml");
        std::filesystem::remove("employees.xml");
        std::filesystem::remove("invalid.xml");

        std::cout << "\n=== PugiXML Basic Usage Example Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
