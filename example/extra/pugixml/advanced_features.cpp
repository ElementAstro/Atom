#include "atom/extra/pugixml/modern_xml.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace atom::extra::pugixml;
using namespace std::chrono_literals;

// Helper function to create test XML files
void create_test_xml_file(const std::string& filename,
                          const std::string& content) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "Created test file: " << filename << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== PugiXML Advanced Features Example ===" << std::endl;

        // 1. Complex XPath queries and expressions
        std::cout << "\n1. Complex XPath Queries and Expressions:" << std::endl;
        {
            create_test_xml_file("complex_data.xml", R"(<?xml version="1.0"?>
<company>
    <department name="Engineering" budget="1000000">
        <employee id="1" salary="75000" experience="5">
            <name>John Doe</name>
            <skills>
                <skill level="expert">C++</skill>
                <skill level="intermediate">Python</skill>
                <skill level="beginner">JavaScript</skill>
            </skills>
        </employee>
        <employee id="2" salary="85000" experience="8">
            <name>Jane Smith</name>
            <skills>
                <skill level="expert">Python</skill>
                <skill level="expert">Machine Learning</skill>
                <skill level="intermediate">C++</skill>
            </skills>
        </employee>
    </department>
    <department name="Marketing" budget="500000">
        <employee id="3" salary="65000" experience="3">
            <name>Bob Johnson</name>
            <skills>
                <skill level="expert">Marketing</skill>
                <skill level="intermediate">Analytics</skill>
            </skills>
        </employee>
    </department>
</company>)");

            auto doc = Document::from_file("complex_data.xml");

            // Complex XPath queries
            std::cout << "Advanced XPath queries:" << std::endl;

            // Find employees with salary > 70000
            auto high_earners = doc.select_nodes("//employee[@salary > 70000]");
            std::cout << "  High earners (>$70k): " << high_earners.size()
                      << std::endl;
            for (const auto& node : high_earners) {
                auto name = node.node().child("name").text().as_string();
                auto salary = node.node().attribute("salary").as_int();
                std::cout << "    " << name << " ($" << salary << ")"
                          << std::endl;
            }

            // Find employees with expert-level C++ skills
            auto cpp_experts = doc.select_nodes(
                "//employee[skills/skill[@level='expert' and text()='C++']]");
            std::cout << "  C++ experts: " << cpp_experts.size() << std::endl;
            for (const auto& node : cpp_experts) {
                auto name = node.node().child("name").text().as_string();
                std::cout << "    " << name << std::endl;
            }

            // Calculate total budget
            auto departments = doc.select_nodes("//department");
            int total_budget = 0;
            for (const auto& dept : departments) {
                total_budget += dept.node().attribute("budget").as_int();
            }
            std::cout << "  Total company budget: $" << total_budget
                      << std::endl;

            // Find departments with budget > average
            double avg_budget =
                static_cast<double>(total_budget) / departments.size();
            auto high_budget_depts = doc.select_nodes(
                "//department[@budget > " +
                std::to_string(static_cast<int>(avg_budget)) + "]");
            std::cout << "  High-budget departments (>$"
                      << static_cast<int>(avg_budget)
                      << "): " << high_budget_depts.size() << std::endl;
        }

        // 2. Document building with fluent API
        std::cout << "\n2. Document Building with Fluent API:" << std::endl;
        {
            auto doc = Document::create_empty();

            // Build a complex document structure
            auto root = doc.append_child("catalog");
            root.append_attribute("version") = "2.0";
            root.append_attribute("xmlns") = "http://example.com/catalog";

            // Products section
            auto products = root.append_child("products");

            std::vector<std::tuple<std::string, std::string, double, int>>
                product_data = {{"Laptop", "Electronics", 999.99, 50},
                                {"Book", "Education", 29.99, 200},
                                {"Coffee Mug", "Kitchen", 12.99, 100}};

            for (const auto& [name, category, price, stock] : product_data) {
                auto product = products.append_child("product");
                product.append_attribute("category") = category.c_str();
                product.append_attribute("in_stock") =
                    (stock > 0 ? "true" : "false");

                product.append_child("name").text() = name.c_str();
                product.append_child("price").text() = price;
                product.append_child("stock").text() = stock;

                // Add metadata
                auto metadata = product.append_child("metadata");
                metadata.append_child("created").text() = "2024-01-01";
                metadata.append_child("updated").text() = "2024-01-15";
            }

            // Categories section
            auto categories = root.append_child("categories");
            std::vector<std::string> category_list = {"Electronics",
                                                      "Education", "Kitchen"};
            for (const auto& cat : category_list) {
                auto category = categories.append_child("category");
                category.append_attribute("name") = cat.c_str();
                category.append_attribute("active") = "true";
            }

            doc.save_file("built_catalog.xml");
            std::cout << "Built complex catalog with " << product_data.size()
                      << " products" << std::endl;
        }

        // 3. XML transformation and manipulation
        std::cout << "\n3. XML Transformation and Manipulation:" << std::endl;
        {
            auto doc = Document::from_file("complex_data.xml");

            // Transform: Add performance ratings based on salary and experience
            auto employees = doc.select_nodes("//employee");
            for (const auto& emp_node : employees) {
                auto employee = emp_node.node();
                auto salary = employee.attribute("salary").as_int();
                auto experience = employee.attribute("experience").as_int();

                // Calculate performance score
                int performance_score = (salary / 1000) + (experience * 2);

                auto performance = employee.append_child("performance");
                performance.append_attribute("score") = performance_score;

                std::string rating;
                if (performance_score >= 90)
                    rating = "excellent";
                else if (performance_score >= 75)
                    rating = "good";
                else if (performance_score >= 60)
                    rating = "average";
                else
                    rating = "needs_improvement";

                performance.append_attribute("rating") = rating.c_str();
            }

            // Remove employees with low performance
            auto low_performers = doc.select_nodes(
                "//employee[performance/@rating='needs_improvement']");
            std::cout << "Removing " << low_performers.size()
                      << " low performers" << std::endl;
            for (const auto& node : low_performers) {
                node.node().parent().remove_child(node.node());
            }

            doc.save_file("transformed_data.xml");
            std::cout << "Transformed and saved employee data" << std::endl;
        }

        // 4. Custom XML serialization for C++ objects
        std::cout << "\n4. Custom XML Serialization:" << std::endl;
        {
            // Define a simple class hierarchy
            struct Address {
                std::string street, city, country;
                int zip_code;

                void to_xml(Node& parent) const {
                    auto addr = parent.append_child("address");
                    addr.append_child("street").text() = street.c_str();
                    addr.append_child("city").text() = city.c_str();
                    addr.append_child("country").text() = country.c_str();
                    addr.append_child("zip_code").text() = zip_code;
                }

                static Address from_xml(const Node& addr_node) {
                    Address addr;
                    addr.street = addr_node.child("street").text().as_string();
                    addr.city = addr_node.child("city").text().as_string();
                    addr.country =
                        addr_node.child("country").text().as_string();
                    addr.zip_code = addr_node.child("zip_code").text().as_int();
                    return addr;
                }
            };

            struct Person {
                std::string name, email;
                int age;
                Address address;
                std::vector<std::string> hobbies;

                void to_xml(Node& parent) const {
                    auto person = parent.append_child("person");
                    person.append_child("name").text() = name.c_str();
                    person.append_child("email").text() = email.c_str();
                    person.append_child("age").text() = age;

                    address.to_xml(person);

                    auto hobbies_node = person.append_child("hobbies");
                    for (const auto& hobby : hobbies) {
                        hobbies_node.append_child("hobby").text() =
                            hobby.c_str();
                    }
                }

                static Person from_xml(const Node& person_node) {
                    Person person;
                    person.name = person_node.child("name").text().as_string();
                    person.email =
                        person_node.child("email").text().as_string();
                    person.age = person_node.child("age").text().as_int();
                    person.address =
                        Address::from_xml(person_node.child("address"));

                    for (const auto& hobby :
                         person_node.child("hobbies").children("hobby")) {
                        person.hobbies.push_back(hobby.text().as_string());
                    }
                    return person;
                }
            };

            // Create and serialize objects
            std::vector<Person> people = {
                {"Alice Johnson",
                 "alice@example.com",
                 30,
                 {"123 Main St", "New York", "USA", 10001},
                 {"reading", "hiking", "photography"}},
                {"Bob Smith",
                 "bob@example.com",
                 25,
                 {"456 Oak Ave", "Los Angeles", "USA", 90210},
                 {"gaming", "cooking", "music"}}};

            auto doc = Document::create_empty();
            auto root = doc.append_child("people");

            for (const auto& person : people) {
                person.to_xml(root);
            }

            doc.save_file("serialized_people.xml");
            std::cout << "Serialized " << people.size() << " people to XML"
                      << std::endl;

            // Deserialize back
            auto loaded_doc = Document::from_file("serialized_people.xml");
            std::vector<Person> loaded_people;

            for (const auto& person_node :
                 loaded_doc.root().children("person")) {
                loaded_people.push_back(Person::from_xml(person_node));
            }

            std::cout << "Deserialized " << loaded_people.size()
                      << " people from XML" << std::endl;
            for (const auto& person : loaded_people) {
                std::cout << "  " << person.name << " (" << person.age << ") - "
                          << person.hobbies.size() << " hobbies" << std::endl;
            }
        }

        // 5. Performance testing and optimization
        std::cout << "\n5. Performance Testing:" << std::endl;
        {
            // Create a large XML document
            auto doc = Document::create_empty();
            auto root = doc.append_child("large_dataset");

            const int num_records = 10000;
            std::cout << "Creating large XML with " << num_records
                      << " records..." << std::endl;

            auto start_time = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < num_records; ++i) {
                auto record = root.append_child("record");
                record.append_attribute("id") = i;
                record.append_child("name").text() =
                    ("Record_" + std::to_string(i)).c_str();
                record.append_child("value").text() = i * 1.5;
                record.append_child("category").text() =
                    ("Category_" + std::to_string(i % 10)).c_str();
            }

            auto build_time = std::chrono::high_resolution_clock::now();
            auto build_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    build_time - start_time);

            std::cout << "Document creation time: " << build_duration.count()
                      << " ms" << std::endl;

            // Save performance test
            doc.save_file("large_dataset.xml");
            auto save_time = std::chrono::high_resolution_clock::now();
            auto save_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    save_time - build_time);

            std::cout << "Document save time: " << save_duration.count()
                      << " ms" << std::endl;

            // Load performance test
            auto load_start = std::chrono::high_resolution_clock::now();
            auto loaded_doc = Document::from_file("large_dataset.xml");
            auto load_end = std::chrono::high_resolution_clock::now();
            auto load_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    load_end - load_start);

            std::cout << "Document load time: " << load_duration.count()
                      << " ms" << std::endl;

            // XPath query performance
            auto query_start = std::chrono::high_resolution_clock::now();
            auto results =
                loaded_doc.select_nodes("//record[@id > 5000 and @id < 5100]");
            auto query_end = std::chrono::high_resolution_clock::now();
            auto query_duration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    query_end - query_start);

            std::cout << "XPath query time: " << query_duration.count() << " μs"
                      << std::endl;
            std::cout << "Query results: " << results.size() << " records"
                      << std::endl;
        }

        // 6. Multi-threaded XML processing
        std::cout << "\n6. Multi-threaded XML Processing:" << std::endl;
        {
            // Create multiple XML documents for parallel processing
            std::vector<std::string> xml_files;
            for (int i = 0; i < 4; ++i) {
                std::string filename =
                    "thread_data_" + std::to_string(i) + ".xml";
                xml_files.push_back(filename);

                auto doc = Document::create_empty();
                auto root = doc.append_child("data");

                for (int j = 0; j < 1000; ++j) {
                    auto item = root.append_child("item");
                    item.append_attribute("id") = (i * 1000 + j);
                    item.append_child("value").text() = (i * 1000 + j) * 2;
                }

                doc.save_file(filename);
            }

            // Process files in parallel
            std::vector<std::future<int>> futures;

            for (const auto& filename : xml_files) {
                futures.push_back(
                    std::async(std::launch::async, [filename]() -> int {
                        auto doc = Document::from_file(filename);
                        auto items = doc.select_nodes("//item");

                        int sum = 0;
                        for (const auto& item : items) {
                            sum += item.node().child("value").text().as_int();
                        }

                        std::cout << "Thread processed " << filename << ": "
                                  << items.size() << " items, sum = " << sum
                                  << std::endl;
                        return sum;
                    }));
            }

            // Collect results
            int total_sum = 0;
            for (auto& future : futures) {
                total_sum += future.get();
            }

            std::cout << "Total sum from all threads: " << total_sum
                      << std::endl;
        }

        // 7. XML validation and schema checking
        std::cout << "\n7. XML Validation and Schema Checking:" << std::endl;
        {
            // Simple validation function
            auto validate_book_xml =
                [](const Document& doc) -> std::vector<std::string> {
                std::vector<std::string> errors;

                auto root = doc.root();
                if (root.name() != std::string("library")) {
                    errors.push_back("Root element must be 'library'");
                    return errors;
                }

                for (const auto& book : root.children("book")) {
                    if (!book.attribute("id")) {
                        errors.push_back(
                            "Book missing required 'id' attribute");
                    }

                    if (!book.child("title")) {
                        errors.push_back(
                            "Book missing required 'title' element");
                    }

                    if (!book.child("author")) {
                        errors.push_back(
                            "Book missing required 'author' element");
                    }

                    auto year = book.child("year");
                    if (year) {
                        int year_val = year.text().as_int();
                        if (year_val < 1000 || year_val > 2024) {
                            errors.push_back("Invalid year: " +
                                             std::to_string(year_val));
                        }
                    }
                }

                return errors;
            };

            // Test with valid XML
            create_test_xml_file("valid_book.xml", R"(<?xml version="1.0"?>
<library>
    <book id="1">
        <title>Valid Book</title>
        <author>Valid Author</author>
        <year>2020</year>
    </book>
</library>)");

            auto valid_doc = Document::from_file("valid_book.xml");
            auto valid_errors = validate_book_xml(valid_doc);
            std::cout << "Valid XML errors: " << valid_errors.size()
                      << std::endl;

            // Test with invalid XML
            create_test_xml_file("invalid_book.xml", R"(<?xml version="1.0"?>
<library>
    <book>
        <title>Invalid Book</title>
        <year>3000</year>
    </book>
</library>)");

            auto invalid_doc = Document::from_file("invalid_book.xml");
            auto invalid_errors = validate_book_xml(invalid_doc);
            std::cout << "Invalid XML errors: " << invalid_errors.size()
                      << std::endl;
            for (const auto& error : invalid_errors) {
                std::cout << "  " << error << std::endl;
            }
        }

        // Cleanup test files
        std::cout << "\nCleaning up test files..." << std::endl;
        std::filesystem::remove("complex_data.xml");
        std::filesystem::remove("built_catalog.xml");
        std::filesystem::remove("transformed_data.xml");
        std::filesystem::remove("serialized_people.xml");
        std::filesystem::remove("large_dataset.xml");
        std::filesystem::remove("valid_book.xml");
        std::filesystem::remove("invalid_book.xml");

        for (int i = 0; i < 4; ++i) {
            std::filesystem::remove("thread_data_" + std::to_string(i) +
                                    ".xml");
        }

        std::cout << "\n=== PugiXML Advanced Features Example Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
