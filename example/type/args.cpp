#include <iostream>
#include <memory_resource>
#include <string>
#include <vector>

#include "atom/type/args.hpp"

// Custom type for demonstration
struct Person {
    std::string name;
    int age;
    double salary;

    Person() = default;
    Person(std::string n, int a, double s)
        : name(std::move(n)), age(a), salary(s) {}

    bool operator==(const Person& other) const {
        return name == other.name && age == other.age && salary == other.salary;
    }

    friend std::ostream& operator<<(std::ostream& os, const Person& p) {
        return os << "Person{name='" << p.name << "', age=" << p.age
                  << ", salary=" << p.salary << "}";
    }
};

// Helper function to print section headers
void print_header(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
    std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// Validator functions for demonstration
bool validate_positive_int(const atom::any_type& value) {
    try {
#ifdef ATOM_USE_BOOST
        int val = boost::any_cast<int>(value);
#else
        int val = std::any_cast<int>(value);
#endif
        return val > 0;
    } catch (...) {
        return false;
    }
}

bool validate_non_empty_string(const atom::any_type& value) {
    try {
#ifdef ATOM_USE_BOOST
        std::string val = boost::any_cast<std::string>(value);
#else
        std::string val = std::any_cast<std::string>(value);
#endif
        return !val.empty();
    } catch (...) {
        return false;
    }
}

bool validate_salary_range(const atom::any_type& value) {
    try {
#ifdef ATOM_USE_BOOST
        double val = boost::any_cast<double>(value);
#else
        double val = std::any_cast<double>(value);
#endif
        return val >= 0.0 && val <= 1000000.0;
    } catch (...) {
        return false;
    }
}

int main() {
    std::cout << "Args Container Usage Examples" << std::endl;
    std::cout << "=============================" << std::endl;

    // 1. Basic Construction and Operations
    print_header("Basic Construction and Operations");

    atom::Args args;
    std::cout << "Created empty Args container" << std::endl;
    std::cout << "Is empty: " << (args.empty() ? "Yes" : "No") << std::endl;
    std::cout << "Size: " << args.size() << std::endl;

    // Basic setting and getting
    args.set("name", std::string("John Doe"));
    args.set("age", 30);
    args.set("salary", 75000.50);
    args.set("is_employed", true);

    std::cout << "\nAfter setting values:" << std::endl;
    std::cout << "Size: " << args.size() << std::endl;
    std::cout << "Name: " << args.get<std::string>("name") << std::endl;
    std::cout << "Age: " << args.get<int>("age") << std::endl;
    std::cout << "Salary: " << args.get<double>("salary") << std::endl;
    std::cout << "Is employed: "
              << (args.get<bool>("is_employed") ? "Yes" : "No") << std::endl;

    // 2. Type Checking and Safe Access
    print_header("Type Checking and Safe Access");

    std::cout << "Type checks:" << std::endl;
    std::cout << "  'name' is string: "
              << (args.isType<std::string>("name") ? "Yes" : "No") << std::endl;
    std::cout << "  'age' is int: " << (args.isType<int>("age") ? "Yes" : "No")
              << std::endl;
    std::cout << "  'age' is string: "
              << (args.isType<std::string>("age") ? "Yes" : "No") << std::endl;

    // Safe access with defaults
    std::cout << "\nSafe access with defaults:" << std::endl;
    std::cout << "  Existing key 'age': " << args.getOr<int>("age", -1)
              << std::endl;
    std::cout << "  Non-existing key 'height': "
              << args.getOr<double>("height", 0.0) << std::endl;

    // Optional access
    std::cout << "\nOptional access:" << std::endl;
    auto opt_name = args.getOptional<std::string>("name");
    auto opt_missing = args.getOptional<std::string>("missing");

    std::cout << "  Optional name: " << (opt_name ? *opt_name : "Not found")
              << std::endl;
    std::cout << "  Optional missing: "
              << (opt_missing ? *opt_missing : "Not found") << std::endl;

    // 3. Validation System
    print_header("Validation System");

    atom::Args validated_args;

    // Set up validators
    validated_args.setValidator("age", validate_positive_int);
    validated_args.setValidator("name", validate_non_empty_string);
    validated_args.setValidator("salary", validate_salary_range);

    std::cout << "Setting up validators for age (positive), name (non-empty), "
                 "salary (0-1M)"
              << std::endl;

    // Valid values
    try {
        validated_args.set("age", 25);
        validated_args.set("name", std::string("Alice"));
        validated_args.set("salary", 80000.0);
        std::cout << "✓ All valid values accepted" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✗ Unexpected error: " << e.what() << std::endl;
    }

    // Invalid values
    std::cout << "\nTesting invalid values:" << std::endl;

    try {
        validated_args.set("age", -5);
        std::cout << "✗ Should have failed: negative age accepted" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ Correctly rejected negative age: " << e.what()
                  << std::endl;
    }

    try {
        validated_args.set("name", std::string(""));
        std::cout << "✗ Should have failed: empty name accepted" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ Correctly rejected empty name: " << e.what()
                  << std::endl;
    }

    try {
        validated_args.set("salary", 2000000.0);
        std::cout << "✗ Should have failed: excessive salary accepted"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ Correctly rejected excessive salary: " << e.what()
                  << std::endl;
    }

    // 4. Batch Operations
    print_header("Batch Operations");

    atom::Args batch_args;

    // Batch set with initializer list
    std::cout << "Setting multiple values with initializer list:" << std::endl;
    batch_args.set({{"config_name", std::string("MyApp")},
                    {"version", std::string("1.0.0")},
                    {"debug_mode", true},
                    {"max_connections", 100},
                    {"timeout", 30.5}});

    std::cout << "Batch set completed. Size: " << batch_args.size()
              << std::endl;

    // Batch get with multiple keys
    std::vector<atom::string_view_type> keys = {"config_name", "version",
                                                "missing_key"};
    auto string_values = batch_args.get<std::string>(keys);

    std::cout << "\nBatch get results:" << std::endl;
    for (size_t i = 0; i < keys.size(); ++i) {
        std::cout << "  " << keys[i] << ": ";
        if (string_values[i]) {
            std::cout << *string_values[i];
        } else {
            std::cout << "Not found or wrong type";
        }
        std::cout << std::endl;
    }

    // 5. Custom Types and Complex Objects
    print_header("Custom Types and Complex Objects");

    atom::Args custom_args;

    // Store custom objects
    Person person1("Alice Johnson", 28, 65000.0);
    Person person2("Bob Smith", 35, 85000.0);

    custom_args.set("employee1", person1);
    custom_args.set("employee2", person2);

    // Store containers
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};

    custom_args.set("numbers", numbers);
    custom_args.set("names", names);

    std::cout << "Stored custom objects and containers" << std::endl;

    // Retrieve and display
    auto retrieved_person1 = custom_args.get<Person>("employee1");
    auto retrieved_numbers = custom_args.get<std::vector<int>>("numbers");
    auto retrieved_names = custom_args.get<std::vector<std::string>>("names");

    std::cout << "Retrieved person1: " << retrieved_person1 << std::endl;
    std::cout << "Retrieved numbers: ";
    for (int num : retrieved_numbers) {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    std::cout << "Retrieved names: ";
    for (const auto& name : retrieved_names) {
        std::cout << name << " ";
    }
    std::cout << std::endl;

    // 6. Memory Pool Usage
    print_header("Memory Pool Usage");

    // Create a custom memory resource
    std::pmr::monotonic_buffer_resource pool(1024);
    atom::Args pool_args(&pool);

    std::cout << "Created Args with custom memory pool (1024 bytes)"
              << std::endl;

    // Add some data
    for (int i = 0; i < 10; ++i) {
        pool_args.set("key" + std::to_string(i), i * i);
    }

    std::cout << "Added 10 key-value pairs to pool-backed Args" << std::endl;
    std::cout << "Pool Args size: " << pool_args.size() << std::endl;

    // 7. Iteration and Inspection
    print_header("Iteration and Inspection");

    atom::Args iter_args;
    iter_args.set("alpha", 1);
    iter_args.set("beta", std::string("test"));
    iter_args.set("gamma", 3.14);
    iter_args.set("delta", true);

    std::cout << "Iterating through Args container:" << std::endl;
    std::cout << "Size: " << iter_args.size() << std::endl;
    std::cout << "Contains 'alpha': "
              << (iter_args.contains("alpha") ? "Yes" : "No") << std::endl;
    std::cout << "Contains 'missing': "
              << (iter_args.contains("missing") ? "Yes" : "No") << std::endl;

    // 8. Error Handling and Edge Cases
    print_header("Error Handling and Edge Cases");

    atom::Args error_args;
    error_args.set("test_value", 42);

    // Type mismatch error
    std::cout << "Testing error handling:" << std::endl;
    try {
        auto wrong_type = error_args.get<std::string>("test_value");
        std::cout << "✗ Should have failed: got " << wrong_type << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ Correctly caught type mismatch: " << e.what()
                  << std::endl;
    }

    // Missing key error
    try {
        auto missing = error_args.get<int>("missing_key");
        std::cout << "✗ Should have failed: got " << missing << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ Correctly caught missing key: " << e.what()
                  << std::endl;
    }

    // 9. Macro Interface
    print_header("Macro Interface");

    atom::Args macro_args;

    // Using convenience macros
    SET_ARGUMENT(macro_args, username, std::string("john_doe"));
    SET_ARGUMENT(macro_args, user_id, 12345);
    SET_ARGUMENT(macro_args, is_admin, false);

    std::cout << "Set values using macros" << std::endl;

    // Get values using macros
    auto username = GET_ARGUMENT(macro_args, username, std::string);
    auto user_id = GET_ARGUMENT(macro_args, user_id, int);
    auto is_admin = GET_ARGUMENT(macro_args, is_admin, bool);

    std::cout << "Username: " << username << std::endl;
    std::cout << "User ID: " << user_id << std::endl;
    std::cout << "Is Admin: " << (is_admin ? "Yes" : "No") << std::endl;

    // Check existence using macros
    std::cout << "Has username: "
              << (HAS_ARGUMENT(macro_args, username) ? "Yes" : "No")
              << std::endl;
    std::cout << "Has email: "
              << (HAS_ARGUMENT(macro_args, email) ? "Yes" : "No") << std::endl;

    // Remove using macros
    REMOVE_ARGUMENT(macro_args, user_id);
    std::cout << "After removing user_id, has user_id: "
              << (HAS_ARGUMENT(macro_args, user_id) ? "Yes" : "No")
              << std::endl;

    std::cout << "\nAll Args examples completed successfully!" << std::endl;
    return 0;
}
