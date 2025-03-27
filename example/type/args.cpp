#include <cassert>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "atom/type/args.hpp"
#include "atom/type/argsview.hpp"

// Custom type for demonstration
struct Point {
    double x;
    double y;

    // Required for comparison
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    // Required for output
    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        return os << "Point(" << p.x << ", " << p.y << ")";
    }
};

// Custom type for demonstration
struct Person {
    std::string name;
    int age;

    // Comparators for Person
    bool operator==(const Person& other) const {
        return name == other.name && age == other.age;
    }

    bool operator<(const Person& other) const {
        return age < other.age || (age == other.age && name < other.name);
    }

    // Output stream operator for Person
    friend std::ostream& operator<<(std::ostream& os, const Person& p) {
        return os << "Person{name='" << p.name << "', age=" << p.age << "}";
    }
};

// Helper function to print section headers
void print_header(const std::string& title) {
    std::cout << "\n";
    std::cout << "██████████████████████████████████████████████" << std::endl;
    std::cout << "███ " << std::left << std::setw(40) << title << " ███"
              << std::endl;
    std::cout << "██████████████████████████████████████████████" << std::endl;
}

// Helper function to print sub-section headers
void print_subheader(const std::string& title) {
    std::cout << "\n----- " << title << " -----" << std::endl;
}

// Helper to print optional values
template <typename T>
void print_optional(const std::optional<T>& opt, const std::string& name) {
    std::cout << "  " << std::left << std::setw(20) << name << ": ";
    if (opt) {
        std::cout << *opt << std::endl;
    } else {
        std::cout << "[not present]" << std::endl;
    }
}

// Helper to print multiple optional values
template <typename T>
void print_optional_vector(const std::vector<std::optional<T>>& values,
                           const std::vector<std::string>& names) {
    for (size_t i = 0; i < values.size(); ++i) {
        print_optional(values[i], names[i]);
    }
}

// Helper to print tuple contents
template <typename Tuple, std::size_t... Is>
void print_tuple_impl(const Tuple& tuple, std::index_sequence<Is...>) {
    ((std::cout << (Is == 0 ? "" : ", ") << std::get<Is>(tuple)), ...);
}

template <typename... Args>
void print_tuple(const std::tuple<Args...>& tuple) {
    std::cout << "(";
    print_tuple_impl(tuple, std::index_sequence_for<Args...>{});
    std::cout << ")";
}

int main() {
    std::cout << "======================================================="
              << std::endl;
    std::cout << "        COMPREHENSIVE ATOM::ARGS USAGE EXAMPLES        "
              << std::endl;
    std::cout << "======================================================="
              << std::endl;

    // 1. Basic Usage
    print_header("Basic Operations");

    // Create an Args container
    atom::Args basic_args;

    // Set various types of values
    basic_args.set("integer", 42);
    basic_args.set("float", 3.14159f);
    basic_args.set("double", 2.71828);
    basic_args.set("string", std::string("Hello, Args!"));
    basic_args.set("bool", true);
    basic_args.set("point", Point{1.0, 2.0});

    // Get values
    std::cout << "Getting values by type:" << std::endl;
    std::cout << "  integer: " << basic_args.get<int>("integer") << std::endl;
    std::cout << "  float: " << basic_args.get<float>("float") << std::endl;
    std::cout << "  double: " << basic_args.get<double>("double") << std::endl;
    std::cout << "  string: " << basic_args.get<std::string>("string")
              << std::endl;
    std::cout << "  bool: " << std::boolalpha << basic_args.get<bool>("bool")
              << std::endl;

    Point p = basic_args.get<Point>("point");
    std::cout << "  point: " << p << std::endl;

    // Container information
    std::cout << "\nContainer info:" << std::endl;
    std::cout << "  size: " << basic_args.size() << std::endl;
    std::cout << "  is empty: " << std::boolalpha << basic_args.empty()
              << std::endl;
    std::cout << "  contains 'integer': " << basic_args.contains("integer")
              << std::endl;
    std::cout << "  contains 'missing': " << basic_args.contains("missing")
              << std::endl;

    // 2. Using Macros
    print_header("Using Convenience Macros");

    atom::Args macro_args;

    // Set values using macros
    SET_ARGUMENT(macro_args, age, 25);
    SET_ARGUMENT(macro_args, name, std::string("John Doe"));
    SET_ARGUMENT(macro_args, location, Point{10.5, 20.7});

    // Get values using macros
    int age = GET_ARGUMENT(macro_args, age, int);
    std::string name = GET_ARGUMENT(macro_args, name, std::string);
    Point location = GET_ARGUMENT(macro_args, location, Point);

    std::cout << "Values set and retrieved using macros:" << std::endl;
    std::cout << "  age: " << age << std::endl;
    std::cout << "  name: " << name << std::endl;
    std::cout << "  location: " << location << std::endl;

    // Check existence with macro
    std::cout << "  HAS_ARGUMENT(macro_args, age): " << std::boolalpha
              << HAS_ARGUMENT(macro_args, age) << std::endl;

    // Remove with macro
    REMOVE_ARGUMENT(macro_args, age);
    std::cout << "  After removal, HAS_ARGUMENT(macro_args, age): "
              << HAS_ARGUMENT(macro_args, age) << std::endl;

    // 3. Default Values
    print_header("Default Values");

    atom::Args default_args;
    default_args.set("existing", 100);

    // Get with defaults
    int existing_val = default_args.getOr("existing", -1);
    int missing_val = default_args.getOr("missing", -1);
    std::string missing_str =
        default_args.getOr("missing_str", std::string("Default String"));
    Point missing_point = default_args.getOr("missing_point", Point{0.0, 0.0});

    std::cout << "Values with defaults:" << std::endl;
    std::cout << "  existing_val: " << existing_val << std::endl;
    std::cout << "  missing_val: " << missing_val << std::endl;
    std::cout << "  missing_str: " << missing_str << std::endl;
    std::cout << "  missing_point: " << missing_point << std::endl;

    // 4. Optional Values
    print_header("Optional Values");

    atom::Args optional_args;
    optional_args.set("value1", 100);
    optional_args.set("text", std::string("Hello, Optional!"));

    // Get optional values
    std::optional<int> opt_val1 = optional_args.getOptional<int>("value1");
    std::optional<int> opt_val2 = optional_args.getOptional<int>("value2");
    std::optional<std::string> opt_text =
        optional_args.getOptional<std::string>("text");
    std::optional<double> opt_wrong_type =
        optional_args.getOptional<double>("text");

    std::cout << "Optional values:" << std::endl;
    print_optional(opt_val1, "value1");
    print_optional(opt_val2, "value2");
    print_optional(opt_text, "text");
    print_optional(opt_wrong_type, "text as double");

    // 5. Type Checking
    print_header("Type Checking");

    atom::Args type_args;
    type_args.set("number", 42);
    type_args.set("text", std::string("Hello, Type!"));

    std::cout << "Type checking:" << std::endl;
    std::cout << "  'number' is int: " << type_args.isType<int>("number")
              << std::endl;
    std::cout << "  'number' is double: " << type_args.isType<double>("number")
              << std::endl;
    std::cout << "  'text' is string: " << type_args.isType<std::string>("text")
              << std::endl;
    std::cout << "  'text' is int: " << type_args.isType<int>("text")
              << std::endl;
    std::cout << "  'missing' is int: " << type_args.isType<int>("missing")
              << std::endl;

    // 6. Batch Operations
    print_header("Batch Operations");

    print_subheader("Batch Set");

    atom::Args batch_args;

    // Create a batch of values
    std::vector<std::pair<std::string_view, int>> int_pairs = {
        {"value1", 10}, {"value2", 20}, {"value3", 30}};

    // Set values in batch
    batch_args.set(
        std::span<const std::pair<std::string_view, int>>(int_pairs));

    std::cout << "Values set in batch:" << std::endl;
    std::cout << "  value1: " << batch_args.get<int>("value1") << std::endl;
    std::cout << "  value2: " << batch_args.get<int>("value2") << std::endl;
    std::cout << "  value3: " << batch_args.get<int>("value3") << std::endl;

    print_subheader("Batch Get");

    // Get multiple values at once
    std::vector<std::string_view> keys = {"value1", "value2", "missing",
                                          "value3"};

    std::vector<std::string> key_names = {"value1", "value2", "missing",
                                          "value3"};

    std::vector<std::optional<int>> batch_result =
        batch_args.get<int>(std::span(keys));

    std::cout << "Batch get results:" << std::endl;
    print_optional_vector(batch_result, key_names);

    // 7. Validation
    print_header("Validation");

    atom::Args validated_args;

    // Set a validator for "age" that ensures value is between 0 and 120
    validated_args.setValidator("age", [](const atom::any_type& value) {
        try {
#ifdef ATOM_USE_BOOST
            int age = boost::any_cast<int>(value);
#else
            int age = std::any_cast<int>(value);
#endif
            return age >= 0 && age <= 120;
        } catch (...) {
            return false;
        }
    });

    std::cout << "Using validators:" << std::endl;

    // Try setting valid values
    try {
        validated_args.set("age", 25);
        std::cout << "  Successfully set age to 25" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Error: " << e.what() << std::endl;
    }

    // Try setting invalid values
    try {
        validated_args.set("age", 150);
        std::cout << "  Successfully set age to 150 (should not happen)"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Validation error caught: " << e.what() << std::endl;
    }

    // 8. Error Handling
    print_header("Error Handling");

    atom::Args error_args;
    error_args.set("number", 42);

    print_subheader("Accessing Non-existent Key");

    try {
        int value = error_args.get<int>("missing");
        std::cout << "  Value: " << value << " (should not happen)"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Error caught: " << e.what() << std::endl;
    }

    print_subheader("Type Mismatch");

    try {
        std::string value = error_args.get<std::string>("number");
        std::cout << "  Value: " << value << " (should not happen)"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Error caught: " << e.what() << std::endl;
    }

    // 9. Higher-Order Functions
    print_header("Higher-Order Functions");

    atom::Args transform_args;
    transform_args.set("val1", 10);
    transform_args.set("val2", 20);
    transform_args.set("val3", 30);

    print_subheader("ForEach");

    std::cout << "ForEach demonstration:" << std::endl;
    transform_args.forEach([](auto key, auto value) {
        try {
#ifdef ATOM_USE_BOOST
            int val = boost::any_cast<int>(value);
#else
            int val = std::any_cast<int>(value);
#endif
            std::cout << "  " << key << ": " << val << std::endl;
        } catch (...) {
            std::cout << "  " << key << ": [non-integer value]" << std::endl;
        }
    });

    print_subheader("Transform");

    atom::Args doubled =
        transform_args.transform([](const atom::any_type& value) {
            try {
#ifdef ATOM_USE_BOOST
                int val = boost::any_cast<int>(value);
#else
                int val = std::any_cast<int>(value);
#endif
                return atom::any_type(val * 2);
            } catch (...) {
                return value;
            }
        });

    std::cout << "Original values:" << std::endl;
    std::cout << "  val1: " << transform_args.get<int>("val1") << std::endl;
    std::cout << "  val2: " << transform_args.get<int>("val2") << std::endl;
    std::cout << "  val3: " << transform_args.get<int>("val3") << std::endl;

    std::cout << "Transformed values (doubled):" << std::endl;
    std::cout << "  val1: " << doubled.get<int>("val1") << std::endl;
    std::cout << "  val2: " << doubled.get<int>("val2") << std::endl;
    std::cout << "  val3: " << doubled.get<int>("val3") << std::endl;

    print_subheader("Filter");

    atom::Args filtered = transform_args.filter([](auto key, auto value) {
        try {
#ifdef ATOM_USE_BOOST
            int val = boost::any_cast<int>(value);
#else
            int val = std::any_cast<int>(value);
#endif
            return val > 15;  // Keep only values greater than 15
        } catch (...) {
            return false;
        }
    });

    std::cout << "Original args size: " << transform_args.size() << std::endl;
    std::cout << "Filtered args size: " << filtered.size() << std::endl;
    std::cout << "Filtered values (> 15):" << std::endl;

    filtered.forEach([](auto key, auto value) {
        try {
#ifdef ATOM_USE_BOOST
            int val = boost::any_cast<int>(value);
#else
            int val = std::any_cast<int>(value);
#endif
            std::cout << "  " << key << ": " << val << std::endl;
        } catch (...) {
            std::cout << "  " << key << ": [non-integer value]" << std::endl;
        }
    });

    // 10. Move Semantics
    print_header("Move Semantics");

    atom::Args source_args;
    source_args.set("value", 42);
    source_args.set("text", std::string("Original"));

    // Move construction
    atom::Args moved_args(std::move(source_args));

    std::cout << "After move construction:" << std::endl;
    std::cout << "  moved_args contains 'value': "
              << moved_args.contains("value") << std::endl;
    std::cout << "  moved_args['value']: " << moved_args.get<int>("value")
              << std::endl;
    std::cout << "  moved_args['text']: " << moved_args.get<std::string>("text")
              << std::endl;

    // Create a new args object
    atom::Args another_args;
    another_args.set("another", 100);

    // Move assignment
    atom::Args assigned_args;
    assigned_args = std::move(another_args);

    std::cout << "After move assignment:" << std::endl;
    std::cout << "  assigned_args contains 'another': "
              << assigned_args.contains("another") << std::endl;
    std::cout << "  assigned_args['another']: "
              << assigned_args.get<int>("another") << std::endl;

    // 11. Iterator Support
    print_header("Iterator Support");

    atom::Args iter_args;
    iter_args.set("a", 1);
    iter_args.set("b", 2);
    iter_args.set("c", 3);

    std::cout << "Iterating over args:" << std::endl;
    for (const auto& [key, value] : iter_args) {
        try {
#ifdef ATOM_USE_BOOST
            int val = boost::any_cast<int>(value);
#else
            int val = std::any_cast<int>(value);
#endif
            std::cout << "  " << key << ": " << val << std::endl;
        } catch (...) {
            std::cout << "  " << key << ": [non-integer value]" << std::endl;
        }
    }

    // 12. Items Collection
    print_header("Items Collection");

    atom::Args items_args;
    items_args.set("item1", 10);
    items_args.set("item2", 20);
    items_args.set("item3", 30);

    std::cout << "Getting all items:" << std::endl;
    auto all_items = items_args.items();
    for (const auto& [key, value] : all_items) {
        try {
#ifdef ATOM_USE_BOOST
            int val = boost::any_cast<int>(value);
#else
            int val = std::any_cast<int>(value);
#endif
            std::cout << "  " << key << ": " << val << std::endl;
        } catch (...) {
            std::cout << "  " << key << ": [non-integer value]" << std::endl;
        }
    }

    // 13. Operator[] Access
    print_header("Operator[] Access");

    atom::Args op_args;
    op_args.set("value", 42);
    op_args.set("text", std::string("Hello"));

    std::cout << "Initial values:" << std::endl;
    std::cout << "  value: " << op_args.get<int>("value") << std::endl;
    std::cout << "  text: " << op_args.get<std::string>("text") << std::endl;

    // Modify values through operator[]
    op_args.operator[]<int>("value") = 100;
    op_args.operator[]<std::string>("text") = "Modified";

    std::cout << "After modification with operator[]:" << std::endl;
    std::cout << "  value: " << op_args.get<int>("value") << std::endl;
    std::cout << "  text: " << op_args.get<std::string>("text") << std::endl;

    // Direct access to underlying any object
    atom::any_type& any_ref = op_args["new_value"];
#ifdef ATOM_USE_BOOST
    any_ref = boost::any(200);
#else
    any_ref = std::any(200);
#endif

    std::cout << "After direct assignment to any:" << std::endl;
    std::cout << "  new_value: " << op_args.get<int>("new_value") << std::endl;

// 14. Thread Safety (if enabled)
#ifdef ATOM_THREAD_SAFE
    print_header("Thread Safety");

    atom::Args thread_args;
    thread_args.set("counter", 0);

    std::cout << "Testing thread safety with concurrent access:" << std::endl;

    // Function to increment the counter in a thread
    auto increment_counter = [&thread_args](int times) {
        for (int i = 0; i < times; ++i) {
            int current = thread_args.get<int>("counter");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            thread_args.set("counter", current + 1);
        }
    };

    // Create multiple threads accessing the same Args object
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(increment_counter, 10);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "  Final counter value: " << thread_args.get<int>("counter")
              << std::endl;
    std::cout << "  Expected value: 100" << std::endl;
#endif

    // 15. Memory Pool
    print_header("Memory Pool Usage");

    // The Args class uses a memory pool internally for better performance
    // Let's demonstrate by creating many small allocations

    atom::Args pool_args;

    std::cout << "Adding many small string values to demonstrate memory pool:"
              << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        pool_args.set(key, value);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    std::cout << "  Added 10,000 key-value pairs" << std::endl;
    std::cout << "  Time taken: " << duration.count() << " ms" << std::endl;
    std::cout << "  Final size: " << pool_args.size() << std::endl;

// 16. JSON Serialization (if enabled)
#ifdef ATOM_USE_JSON
    print_header("JSON Serialization");

    atom::Args json_args;
    json_args.set("integer", 42);
    json_args.set("float", 3.14);
    json_args.set("string", std::string("Hello, JSON!"));
    json_args.set("boolean", true);
    json_args.set("array", std::vector<int>{1, 2, 3});

    // Convert to JSON
    nlohmann::json j = json_args.toJson();

    std::cout << "Args converted to JSON:" << std::endl;
    std::cout << "  " << j.dump(2) << std::endl;

    // Create a new Args from JSON
    atom::Args from_json;
    from_json.fromJson(j);

    std::cout << "Args loaded from JSON:" << std::endl;
    std::cout << "  integer: " << from_json.get<int>("integer") << std::endl;
    std::cout << "  float: " << from_json.get<float>("float") << std::endl;
    std::cout << "  string: " << from_json.get<std::string>("string")
              << std::endl;
    std::cout << "  boolean: " << from_json.get<bool>("boolean") << std::endl;
#endif

// 17. Boost Serialization (if enabled)
#ifdef ATOM_USE_BOOST
    print_header("Boost Serialization");

    // This would require including boost serialization headers and setting up
    // archives
    std::cout << "Boost serialization is available." << std::endl;
    std::cout << "Implementation would require boost/archive headers and "
                 "specific archive types."
              << std::endl;
#endif

    std::cout << "\n======================================================="
              << std::endl;
    std::cout << "             ALL EXAMPLES COMPLETED                    "
              << std::endl;
    std::cout << "======================================================="
              << std::endl;

    std::cout << "ArgsView Usage Examples" << std::endl;
    std::cout << "======================" << std::endl;

    // 1. Basic Construction and Access
    print_header("Basic Construction and Access");

    // Create an ArgsView with various types
    atom::ArgsView integers(1, 2, 3, 4, 5);
    atom::ArgsView mixed(42, "hello", 3.14, true);
    atom::ArgsView empty;
    atom::ArgsView persons(Person{"Alice", 30}, Person{"Bob", 25},
                           Person{"Charlie", 35});

    std::cout << "Size of integers ArgsView: " << integers.size() << std::endl;
    std::cout << "Size of mixed ArgsView: " << mixed.size() << std::endl;
    std::cout << "Size of empty ArgsView: " << empty.size() << std::endl;
    std::cout << "Size of persons ArgsView: " << persons.size() << std::endl;

    std::cout << "Is empty ArgsView empty? " << (empty.empty() ? "Yes" : "No")
              << std::endl;
    std::cout << "Is integers ArgsView empty? "
              << (integers.empty() ? "Yes" : "No") << std::endl;

    std::cout << "First element of integers: " << integers.get<0>()
              << std::endl;
    std::cout << "Second element of mixed: " << mixed.get<1>() << std::endl;
    std::cout << "First person: " << persons.get<0>().name << ", age "
              << persons.get<0>().age << std::endl;

    // 2. Construction from Tuples
    print_header("Construction from Tuples");

    std::tuple<int, double, std::string> tuple1(10, 2.5, "tuple");
    atom::ArgsView from_tuple(std::get<0>(tuple1), std::get<1>(tuple1),
                              std::get<2>(tuple1));

    std::cout << "ArgsView from tuple elements: " << from_tuple.get<0>() << ", "
              << from_tuple.get<1>() << ", " << from_tuple.get<2>()
              << std::endl;

    // 3. Construction from Optional Values
    print_header("Construction from Optional Values");

    std::optional<int> opt1 = 42;
    std::optional<std::string> opt2 = "optional";
    std::optional<double> opt3 = 3.14;

    atom::ArgsView<int, std::string, double> from_optionals(
        std::move(opt1), std::move(opt2), std::move(opt3));

    std::cout << "ArgsView from optionals: " << from_optionals.get<0>() << ", "
              << from_optionals.get<1>() << ", " << from_optionals.get<2>()
              << std::endl;

    // 4. ForEach Operation
    print_header("ForEach Operation");

    std::cout << "Integers: ";
    integers.forEach([](const auto& val) { std::cout << val << " "; });
    std::cout << std::endl;

    std::cout << "Persons: ";
    persons.forEach(
        [](const Person& p) { std::cout << p.name << "(" << p.age << ") "; });
    std::cout << std::endl;

    // ForEach using the free function
    std::cout << "Mixed (using free function): ";
    atom::forEach([](const auto& val) { std::cout << val << " "; }, mixed);
    std::cout << std::endl;

    // 5. Transform Operation
    print_header("Transform Operation");

    doubled = integers.transform([](int i) { return i * 2; });
    std::cout << "Doubled integers: ";
    doubled.forEach([](int i) { std::cout << i << " "; });
    std::cout << std::endl;

    auto person_names =
        persons.transform([](const Person& p) { return p.name; });
    std::cout << "Person names: ";
    person_names.forEach(
        [](const std::string& name) { std::cout << name << " "; });
    std::cout << std::endl;

    auto person_summaries = persons.transform([](const Person& p) {
        return p.name + " is " + std::to_string(p.age) + " years old";
    });

    std::cout << "Person summaries: " << std::endl;
    person_summaries.forEach([](const std::string& summary) {
        std::cout << "  - " << summary << std::endl;
    });

    // 6. ToTuple Conversion
    print_header("ToTuple Conversion");

    auto int_tuple = integers.toTuple();
    std::cout << "Integers as tuple: ";
    print_tuple(int_tuple);
    std::cout << std::endl;

    auto mixed_tuple = mixed.toTuple();
    std::cout << "Mixed as tuple: ";
    print_tuple(mixed_tuple);
    std::cout << std::endl;

    // 7. Accumulate Operation
    print_header("Accumulate Operation");

    int sum =
        integers.accumulate([](int acc, int val) { return acc + val; }, 0);
    std::cout << "Sum of integers: " << sum << std::endl;

    std::string concatenated =
        mixed
            .transform([](const auto& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, const char*> ||
                              std::is_same_v<T, std::string>) {
                    return std::string(val);
                } else {
                    return std::to_string(val);
                }
            })
            .accumulate(
                [](std::string acc, std::string val) {
                    return acc.empty() ? val : acc + ", " + val;
                },
                std::string());

    std::cout << "Concatenated mixed values: " << concatenated << std::endl;

    int product =
        integers.accumulate([](int acc, int val) { return acc * val; }, 1);
    std::cout << "Product of integers: " << product << std::endl;

    // 8. Apply Operation
    print_header("Apply Operation");

    auto avg = integers.apply([](int a, int b, int c, int d, int e) {
        return static_cast<double>(a + b + c + d + e) / 5;
    });
    std::cout << "Average of integers: " << avg << std::endl;

    auto oldest_person =
        persons.apply([](const Person& p1, const Person& p2, const Person& p3) {
            const Person* oldest = &p1;
            if (p2.age > oldest->age)
                oldest = &p2;
            if (p3.age > oldest->age)
                oldest = &p3;
            return oldest->name;
        });

    std::cout << "Oldest person: " << oldest_person << std::endl;

    auto sum_first_three =
        integers.apply([](int a, int b, int c, int d, int e) {
            return a + b + c;  // 只使用前三个参数
        });

    std::cout << "Sum of first three integers: " << sum_first_three
              << std::endl;

    // 9. Assignment Operations
    print_header("Assignment Operations");

    atom::ArgsView<int, int, int> three_ints(10, 20, 30);
    std::cout << "Initial three ints: " << three_ints.get<0>() << ", "
              << three_ints.get<1>() << ", " << three_ints.get<2>()
              << std::endl;

    std::tuple<int, int, int> replacement_tuple(100, 200, 300);
    three_ints = replacement_tuple;

    std::cout << "After tuple assignment: " << three_ints.get<0>() << ", "
              << three_ints.get<1>() << ", " << three_ints.get<2>()
              << std::endl;

    atom::ArgsView another_three_ints(1000, 2000, 3000);
    three_ints = another_three_ints;

    std::cout << "After ArgsView assignment: " << three_ints.get<0>() << ", "
              << three_ints.get<1>() << ", " << three_ints.get<2>()
              << std::endl;

    // 10. Filter Operation
    print_header("Filter Operation");

    auto even_integers = integers.filter([](int i) { return i % 2 == 0; });
    std::cout << "Even integers: ";
    even_integers.forEach([](const std::optional<int>& opt) {
        if (opt.has_value())
            std::cout << *opt << " ";
        else
            std::cout << "- ";
    });
    std::cout << std::endl;

    auto adults = persons.filter([](const Person& p) { return p.age >= 30; });
    std::cout << "Adult persons: ";
    adults.forEach([](const std::optional<Person>& opt) {
        if (opt.has_value())
            std::cout << opt->name << "(" << opt->age << ") ";
        else
            std::cout << "- ";
    });
    std::cout << std::endl;

    // 11. Find Operation - 修复这里的bug
    print_header("Find Operation");

    // 修复：find应该返回bool值或std::optional<T>，根据lambda返回值类型自动推导
    auto found_integer = integers.find([](int i) -> bool { return i > 3; });
    std::cout << "First integer > 3: "
              << (found_integer.has_value() ? std::to_string(*found_integer)
                                            : "Not found")
              << std::endl;

    // 修复：lambda返回值类型不应该是std::optional<Person>，应该是bool或Person
    auto found_person = persons.find(
        [](const Person& p) -> bool { return p.name.starts_with("B"); });
    std::cout << "First person with name starting with 'B': "
              << (found_person.has_value() ? found_person->name : "Not found")
              << std::endl;

    // 12. Contains Operation
    print_header("Contains Operation");

    bool contains3 = integers.contains(3);
    bool contains6 = integers.contains(6);
    std::cout << "Integers contains 3: " << (contains3 ? "Yes" : "No")
              << std::endl;
    std::cout << "Integers contains 6: " << (contains6 ? "Yes" : "No")
              << std::endl;

    bool contains_hello = mixed.contains("hello");
    std::cout << "Mixed contains 'hello': " << (contains_hello ? "Yes" : "No")
              << std::endl;

    // 13. Free Function makeArgsView
    print_header("Free Function makeArgsView");

    auto view1 = atom::makeArgsView(10, 20, 30);
    auto view2 = atom::makeArgsView("one", "two", "three");

    std::cout << "view1 size: " << view1.size() << std::endl;
    std::cout << "view2 first element: " << view2.get<0>() << std::endl;

    // 14. Free Function get
    print_header("Free Function get");

    std::cout << "Second element of integers (using free function): "
              << atom::get<1>(integers) << std::endl;

    std::cout << "Third element of mixed (using free function): "
              << atom::get<2>(mixed) << std::endl;

    // 15. Comparison Operations
    print_header("Comparison Operations");

    atom::ArgsView view3(1, 2, 3);
    atom::ArgsView view4(1, 2, 3);
    atom::ArgsView view5(3, 2, 1);

    std::cout << "view3 == view4: " << (view3 == view4 ? "Yes" : "No")
              << std::endl;
    std::cout << "view3 != view5: " << (view3 != view5 ? "Yes" : "No")
              << std::endl;
    std::cout << "view3 < view5: " << (view3 < view5 ? "Yes" : "No")
              << std::endl;
    std::cout << "view3 <= view4: " << (view3 <= view4 ? "Yes" : "No")
              << std::endl;

    return 0;
}