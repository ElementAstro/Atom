#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "atom/type/expected.hpp"

// Helper function to print section headers
void printHeader(const std::string& title) {
    std::cout << "\n=================================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==================================================\n"
              << std::endl;
}

// Example class to demonstrate expected with custom types
class User {
private:
    int id;
    std::string name;
    std::string email;

public:
    User() : id(0), name(""), email("") {}

    User(int id, std::string name, std::string email)
        : id(id), name(std::move(name)), email(std::move(email)) {}

    int getId() const { return id; }
    const std::string& getName() const { return name; }
    const std::string& getEmail() const { return email; }

    // For demonstration of equality comparisons
    bool operator==(const User& other) const {
        return id == other.id && name == other.name && email == other.email;
    }

    // For demonstration of streaming
    friend std::ostream& operator<<(std::ostream& os, const User& user) {
        os << "User{id=" << user.id << ", name=\"" << user.name
           << "\", email=\"" << user.email << "\"}";
        return os;
    }
};

// Custom error type for demonstration
struct DatabaseError {
    int code;
    std::string message;

    DatabaseError(int code, std::string message)
        : code(code), message(std::move(message)) {}

    bool operator==(const DatabaseError& other) const {
        return code == other.code && message == other.message;
    }

    friend std::ostream& operator<<(std::ostream& os,
                                    const DatabaseError& error) {
        os << "DatabaseError{code=" << error.code << ", message=\""
           << error.message << "\"}";
        return os;
    }
};

// Simulated database functions that return expected
atom::type::expected<User> findUserById(int id) {
    // Simulate database lookup
    if (id <= 0) {
        return atom::type::Error<std::string>("Invalid user ID");
    }

    if (id > 1000) {
        return atom::type::Error<std::string>("User not found");
    }

    // Simulate found user
    return User(id, "Test User " + std::to_string(id),
                "user" + std::to_string(id) + "@example.com");
}

atom::type::expected<std::vector<User>, DatabaseError> getAllUsers() {
    // Simulate database connection error
    bool connectionError = false;

    if (connectionError) {
        return atom::type::Error<DatabaseError>(
            DatabaseError(1001, "Database connection failed"));
    }

    // Return successful result
    std::vector<User> users;
    for (int i = 1; i <= 5; i++) {
        users.emplace_back(i, "User " + std::to_string(i),
                           "user" + std::to_string(i) + "@example.com");
    }

    return users;
}

// Function returning expected<void> for operations that don't return a value
atom::type::expected<void, DatabaseError> updateUserEmail(
    int userId, const std::string& newEmail) {
    if (userId <= 0) {
        return atom::type::Error<DatabaseError>(
            DatabaseError(1002, "Invalid user ID"));
    }

    if (newEmail.find('@') == std::string::npos) {
        return atom::type::Error<DatabaseError>(
            DatabaseError(1003, "Invalid email format"));
    }

    // Simulate successful update
    return {};  // Success case for void expected
}

// Function to demonstrate file operations with expected
atom::type::expected<std::string> readFileContents(
    const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return atom::type::Error<std::string>("Failed to open file: " +
                                              filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    if (file.bad()) {
        return atom::type::Error<std::string>("Error reading file: " +
                                              filename);
    }

    return buffer.str();
}

// Function to demonstrate math operations with expected
atom::type::expected<double> divideNumbers(double a, double b) {
    if (b == 0.0) {
        return atom::type::Error<std::string>("Division by zero");
    }

    return a / b;
}

// Function to demonstrate complex processing with expected
atom::type::expected<std::vector<int>> parseNumberList(
    const std::string& input) {
    std::vector<int> numbers;
    std::stringstream ss(input);
    std::string item;

    while (std::getline(ss, item, ',')) {
        try {
            numbers.push_back(std::stoi(item));
        } catch (const std::exception&) {
            return atom::type::Error<std::string>("Failed to parse '" + item +
                                                  "' as an integer");
        }
    }

    if (numbers.empty()) {
        return atom::type::Error<std::string>(
            "No numbers found in input string");
    }

    return numbers;
}

int main() {
    std::cout << "==================================================="
              << std::endl;
    std::cout << "     COMPREHENSIVE EXPECTED USAGE EXAMPLES" << std::endl;
    std::cout << "===================================================\n"
              << std::endl;

    // ============================================================
    // 1. Basic Construction and Value Access
    // ============================================================
    printHeader("1. BASIC CONSTRUCTION AND VALUE ACCESS");

    // Create expected with a value
    atom::type::expected<int> success_value = 42;
    std::cout << "Creating expected<int> with value 42:" << std::endl;

    if (success_value.has_value()) {
        std::cout << "  Has value: " << success_value.value() << std::endl;
    } else {
        std::cout << "  Has error: " << success_value.error().error()
                  << std::endl;
    }

    // Create expected with an error using Error constructor
    atom::type::expected<int> error_value =
        atom::type::Error<std::string>("Something went wrong");
    std::cout << "\nCreating expected<int> with error:" << std::endl;

    if (error_value.has_value()) {
        std::cout << "  Has value: " << error_value.value() << std::endl;
    } else {
        std::cout << "  Has error: " << error_value.error().error()
                  << std::endl;
    }

    // Using boolean conversion
    std::cout << "\nBoolean conversion:" << std::endl;
    std::cout << "  success_value is "
              << (bool(success_value) ? "valid" : "invalid") << std::endl;
    std::cout << "  error_value is "
              << (bool(error_value) ? "valid" : "invalid") << std::endl;

    // Demonstrating exception during value access
    std::cout << "\nValue access with error handling:" << std::endl;
    try {
        int value = error_value.value();
        std::cout << "  Value: " << value << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Caught exception: " << e.what() << std::endl;
    }

    // Demonstrating exception during error access
    try {
        auto err = success_value.error();
        std::cout << "  Error: " << err.error() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Caught exception: " << e.what() << std::endl;
    }

    // ============================================================
    // 2. Working with make_expected and make_unexpected
    // ============================================================
    printHeader("2. WORKING WITH make_expected AND make_unexpected");

    // Using make_expected helper function
    auto exp1 = atom::type::make_expected(100);
    std::cout << "Using make_expected(100):" << std::endl;
    std::cout << "  Has value: " << exp1.has_value() << std::endl;
    std::cout << "  Value: " << exp1.value() << std::endl;

    // Using make_unexpected with std::string
    auto unexp1 = atom::type::make_unexpected("Error message");
    auto exp2 = atom::type::expected<double>(unexp1);
    std::cout << "\nUsing make_unexpected with string:" << std::endl;
    std::cout << "  Has error: " << (!exp2.has_value()) << std::endl;
    std::cout << "  Error: " << exp2.error().error() << std::endl;

    // Using make_unexpected with custom error type
    auto db_err = DatabaseError(500, "Server error");
    auto unexp2 = atom::type::make_unexpected(db_err);
    auto exp3 = atom::type::expected<User, DatabaseError>(unexp2);
    std::cout << "\nUsing make_unexpected with custom error type:" << std::endl;
    std::cout << "  Error code: " << exp3.error().error().code << std::endl;
    std::cout << "  Error message: " << exp3.error().error().message
              << std::endl;

    // Using direct unexpected constructor
    atom::type::unexpected<int> unexp3(404);
    atom::type::expected<std::string, int> exp4(unexp3);
    std::cout << "\nUsing direct unexpected constructor:" << std::endl;
    std::cout << "  Error: " << exp4.error().error() << std::endl;

    // ============================================================
    // 3. Expected with void value type
    // ============================================================
    printHeader("3. EXPECTED WITH VOID VALUE TYPE");

    // Create a void expected (success case)
    atom::type::expected<void> void_success;
    std::cout << "Void expected (success case):" << std::endl;
    std::cout << "  Has value: " << void_success.has_value() << std::endl;
    std::cout << "  Boolean conversion: "
              << (bool(void_success) ? "true" : "false") << std::endl;

    // Try to access the value (should do nothing for void)
    try {
        void_success.value();
        std::cout << "  Accessed value successfully (no-op for void)"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Exception: " << e.what() << std::endl;
    }

    // Create a void expected with error
    atom::type::expected<void> void_error =
        atom::type::Error<std::string>("Operation failed");
    std::cout << "\nVoid expected (error case):" << std::endl;
    std::cout << "  Has value: " << void_error.has_value() << std::endl;
    std::cout << "  Boolean conversion: "
              << (bool(void_error) ? "true" : "false") << std::endl;
    std::cout << "  Error: " << void_error.error().error() << std::endl;

    // Demonstrate void expected from a function
    auto update_result = updateUserEmail(1, "new@example.com");
    std::cout << "\nVoid expected from function:" << std::endl;
    if (update_result.has_value()) {
        std::cout << "  User email updated successfully" << std::endl;
    } else {
        std::cout << "  Update failed: "
                  << update_result.error().error().message << std::endl;
    }

    auto invalid_update = updateUserEmail(0, "invalid-email");
    std::cout << "\nVoid expected with error from function:" << std::endl;
    if (invalid_update.has_value()) {
        std::cout << "  User email updated successfully" << std::endl;
    } else {
        std::cout << "  Update failed: [" << invalid_update.error().error().code
                  << "] " << invalid_update.error().error().message
                  << std::endl;
    }

    // ============================================================
    // 4. Custom Types with Expected
    // ============================================================
    printHeader("4. CUSTOM TYPES WITH EXPECTED");

    // Working with custom User type
    auto user_result = findUserById(42);
    std::cout << "Finding user by ID 42:" << std::endl;

    if (user_result.has_value()) {
        const User& user = user_result.value();
        std::cout << "  Found user: " << user << std::endl;
        std::cout << "  ID: " << user.getId() << std::endl;
        std::cout << "  Name: " << user.getName() << std::endl;
        std::cout << "  Email: " << user.getEmail() << std::endl;
    } else {
        std::cout << "  Error: " << user_result.error().error() << std::endl;
    }

    // Error case with invalid ID
    auto invalid_user = findUserById(-1);
    std::cout << "\nFinding user by invalid ID (-1):" << std::endl;
    if (invalid_user.has_value()) {
        std::cout << "  Found user: " << invalid_user.value() << std::endl;
    } else {
        std::cout << "  Error: " << invalid_user.error().error() << std::endl;
    }

    // Collection of custom types
    auto users_result = getAllUsers();
    std::cout << "\nGetting all users:" << std::endl;

    if (users_result.has_value()) {
        const auto& users = users_result.value();
        std::cout << "  Found " << users.size() << " users:" << std::endl;
        for (const auto& user : users) {
            std::cout << "  - " << user << std::endl;
        }
    } else {
        const auto& error = users_result.error().error();
        std::cout << "  Database error [" << error.code
                  << "]: " << error.message << std::endl;
    }

    // ============================================================
    // 5. Monadic Operations: and_then
    // ============================================================
    printHeader("5. MONADIC OPERATIONS: and_then");

    // Basic and_then example with success path
    auto int_result = atom::type::make_expected(10);
    auto doubled = int_result.and_then(
        [](int value) -> atom::type::expected<int> { return value * 2; });

    std::cout << "and_then with success path:" << std::endl;
    std::cout << "  Original value: " << int_result.value() << std::endl;
    std::cout << "  After and_then: " << doubled.value() << std::endl;

    // and_then with error path (propagation)
    auto error_int = atom::type::expected<int>(
        atom::type::Error<std::string>("Initial error"));
    auto after_and_then =
        error_int.and_then([](int value) -> atom::type::expected<std::string> {
            return "Processed: " + std::to_string(value);
        });

    std::cout << "\nand_then with error propagation:" << std::endl;
    std::cout << "  Has error: " << (!after_and_then.has_value()) << std::endl;
    std::cout << "  Error: " << after_and_then.error().error() << std::endl;

    // Chaining multiple and_then operations
    auto chain_start = atom::type::make_expected(5);
    auto final_result =
        chain_start
            .and_then([](int value) -> atom::type::expected<double> {
                return value * 2.5;
            })
            .and_then([](double value) -> atom::type::expected<std::string> {
                return "Result: " + std::to_string(value);
            });

    std::cout << "\nChaining multiple and_then operations:" << std::endl;
    std::cout << "  Final result: " << final_result.value() << std::endl;

    // Using and_then with void expected
    auto void_op = atom::type::expected<void>();
    auto void_chain = void_op.and_then([]() -> atom::type::expected<int> {
        return 42;  // return something after void operation succeeds
    });

    std::cout << "\nand_then with void expected:" << std::endl;
    std::cout << "  Result after void operation: " << void_chain.value()
              << std::endl;

    // Real-world example: chain of operations
    auto user_chain = findUserById(1).and_then(
        [](const User& user) -> atom::type::expected<std::string> {
            return "Processed user: " + user.getName();
        });

    std::cout << "\nReal-world and_then example:" << std::endl;
    std::cout << "  Result: " << user_chain.value() << std::endl;

    // ============================================================
    // 6. Mapping Operations: map
    // ============================================================
    printHeader("6. MAPPING OPERATIONS: map");

    // Basic map with success path
    auto map_start = atom::type::make_expected(100);
    auto map_result = map_start.map([](int value) {
        return value / 10.0;  // map from int to double
    });

    std::cout << "Basic map operation:" << std::endl;
    std::cout << "  Original value (int): " << map_start.value() << std::endl;
    std::cout << "  Mapped value (double): " << map_result.value() << std::endl;

    // Map with error propagation
    auto error_start = atom::type::expected<int>(
        atom::type::Error<std::string>("Map error test"));
    auto error_map = error_start.map([](int value) {
        return std::to_string(value);  // never executed due to error
    });

    std::cout << "\nMap with error propagation:" << std::endl;
    std::cout << "  Has error: " << (!error_map.has_value()) << std::endl;
    std::cout << "  Error: " << error_map.error().error() << std::endl;

    // Mapping to a different type
    auto user_map = findUserById(2).map([](const User& user) {
        return user.getEmail();  // map from User to string (email)
    });

    std::cout << "\nMapping from User to email string:" << std::endl;
    std::cout << "  Result: " << user_map.value() << std::endl;

    // Chaining map operations
    auto chain_map = atom::type::make_expected(25)
                         .map([](int value) {
                             return std::sqrt(value);  // map to double
                         })
                         .map([](double value) {
                             return "Square root: " +
                                    std::to_string(value);  // map to string
                         });

    std::cout << "\nChaining map operations:" << std::endl;
    std::cout << "  Final result: " << chain_map.value() << std::endl;

    // Practical example: parsing and processing
    auto parse_result = parseNumberList("10,20,30,40,50");
    auto sum_result = parse_result.map([](const std::vector<int>& numbers) {
        return std::accumulate(numbers.begin(), numbers.end(), 0);
    });

    std::cout << "\nParsing and summing numbers:" << std::endl;
    std::cout << "  Sum: " << sum_result.value() << std::endl;

    // Error case in parsing
    auto parse_error = parseNumberList("10,twenty,30");
    auto sum_error = parse_error.map([](const std::vector<int>& numbers) {
        return std::accumulate(numbers.begin(), numbers.end(), 0);
    });

    std::cout << "\nError in parsing:" << std::endl;
    std::cout << "  Error: " << sum_error.error().error() << std::endl;

    // ============================================================
    // 7. Error Transformation
    // ============================================================
    printHeader("7. ERROR TRANSFORMATION");

    // Basic error transformation
    auto basic_error = atom::type::make_unexpected<std::string>("Basic error");
    auto transformed_error =
        atom::type::expected<int>(basic_error)
            .transform_error([](const std::string& err) {
                return atom::type::Error<std::string>("Transformed: " + err);
            });

    std::cout << "Basic error transformation:" << std::endl;
    std::cout << "  Original error: " << basic_error.error() << std::endl;
    std::cout << "  Transformed error: " << transformed_error.error().error()
              << std::endl;

    // Transforming to a different error type
    auto string_error = atom::type::make_unexpected<std::string>("Code 404");
    auto code_error =
        atom::type::expected<int>(string_error)
            .transform_error([](const std::string& err) {
                return atom::type::Error<std::string>("HTTP " + err);
            });

    std::cout << "\nTransforming to a different error type:" << std::endl;
    std::cout << "  Original error: " << string_error.error() << std::endl;
    std::cout << "  Transformed error: " << code_error.error().error()
              << std::endl;

    // Transforming complex error types
    auto db_error_val = DatabaseError(1001, "Database connection failed");
    auto db_error_exp = atom::type::make_unexpected(db_error_val);

    auto simplified_error =
        atom::type::expected<User, DatabaseError>(db_error_exp)
            .transform_error([](const DatabaseError& err) {
                return atom::type::Error<std::string>(
                    "DB-" + std::to_string(err.code) + ": " + err.message);
            });

    std::cout << "\nTransforming complex error type:" << std::endl;
    std::cout << "  Original error: [" << db_error_val.code << "] "
              << db_error_val.message << std::endl;
    std::cout << "  Simplified error: " << simplified_error.error().error()
              << std::endl;

    // No transformation for success case
    auto success_case = atom::type::make_expected(123);
    auto after_transform =
        success_case.transform_error([](const std::string& err) {
            return atom::type::Error<std::string>("This won't be called: " +
                                                  err);
        });

    std::cout << "\nNo transformation for success case:" << std::endl;
    std::cout << "  Original value: " << success_case.value() << std::endl;
    std::cout << "  Value after transform_error: " << after_transform.value()
              << std::endl;

    // ============================================================
    // 8. Combining and Chaining Different Operations
    // ============================================================
    printHeader("8. COMBINING AND CHAINING DIFFERENT OPERATIONS");

    // Combining map and transform_error
    auto combined_ops =
        atom::type::expected<int>(
            atom::type::Error<std::string>("Initial error"))
            .map([](int value) {
                return value * 2;  // Never called due to error
            })
            .transform_error([](const std::string& err) {
                return atom::type::Error<std::string>("Error occurred: " + err);
            });

    std::cout << "Combining map and transform_error:" << std::endl;
    std::cout << "  Final error: " << combined_ops.error().error() << std::endl;

    // Complex chaining with different operations
    auto complex_chain =
        findUserById(3)
            .map([](const User& user) {
                return user.getName() + " (" + user.getEmail() + ")";
            })
            .and_then([](const std::string& userInfo)
                          -> atom::type::expected<std::vector<std::string>> {
                return std::vector<std::string>{userInfo, "Additional info"};
            })
            .map([](const std::vector<std::string>& items) {
                return "Processed: " + items[0];
            });

    std::cout << "\nComplex chaining of operations:" << std::endl;
    std::cout << "  Final result: " << complex_chain.value() << std::endl;

    // Real-world example: file processing with error handling
    auto file_process =
        readFileContents("nonexistent.txt")
            .map([](const std::string& content) {
                return "File size: " + std::to_string(content.size());
            })
            .transform_error([](const std::string& err) {
                return atom::type::Error<std::string>("File error: " + err);
            });

    std::cout << "\nFile processing with error handling:" << std::endl;
    if (file_process.has_value()) {
        std::cout << "  " << file_process.value() << std::endl;
    } else {
        std::cout << "  " << file_process.error().error() << std::endl;
    }

    // Math operations with validation
    auto calculation =
        divideNumbers(10.0, 2.0)
            .and_then([](double result) -> atom::type::expected<double> {
                if (result < 1.0) {
                    return atom::type::Error<std::string>("Result too small");
                }
                return result * 100;
            })
            .map([](double value) {
                return "Calculation result: " + std::to_string(value);
            });

    std::cout << "\nMath operations with validation:" << std::endl;
    std::cout << "  " << calculation.value() << std::endl;

    // Division by zero error handling
    auto division_error =
        divideNumbers(5.0, 0.0)
            .map([](double result) {
                return result * 2;  // Never called
            })
            .transform_error([](const std::string& err) {
                return atom::type::Error<std::string>("Math error: " + err);
            });

    std::cout << "\nDivision by zero error handling:" << std::endl;
    std::cout << "  " << division_error.error().error() << std::endl;

    // ============================================================
    // 9. Equality Comparisons
    // ============================================================
    printHeader("9. EQUALITY COMPARISONS");

    // Compare two expected values (both containing values)
    auto expect1 = atom::type::make_expected(42);
    auto expect2 = atom::type::make_expected(42);
    auto expect3 = atom::type::make_expected(43);

    std::cout << "Comparing expected values:" << std::endl;
    // 修复：使用值比较而不是整个对象比较
    std::cout << "  expect1 == expect2: "
              << ((expect1.has_value() && expect2.has_value() &&
                   expect1.value() == expect2.value())
                      ? "true"
                      : "false")
              << std::endl;
    std::cout << "  expect1 != expect3: "
              << ((expect1.has_value() && expect3.has_value() &&
                   expect1.value() != expect3.value())
                      ? "true"
                      : "false")
              << std::endl;

    // Compare two expected errors
    auto err1 = atom::type::make_unexpected<std::string>("Error message");
    auto err2 = atom::type::make_unexpected<std::string>("Error message");
    auto err3 = atom::type::make_unexpected<std::string>("Different error");

    auto expect_err1 = atom::type::expected<int>(err1);
    auto expect_err2 = atom::type::expected<int>(err2);
    auto expect_err3 = atom::type::expected<int>(err3);

    std::cout << "\nComparing expected errors:" << std::endl;
    // 修复：使用错误值比较而不是整个对象比较
    std::cout << "  expect_err1 == expect_err2: "
              << ((!expect_err1.has_value() && !expect_err2.has_value() &&
                   expect_err1.error().error() == expect_err2.error().error())
                      ? "true"
                      : "false")
              << std::endl;
    std::cout << "  expect_err1 != expect_err3: "
              << ((!expect_err1.has_value() && !expect_err3.has_value() &&
                   expect_err1.error().error() != expect_err3.error().error())
                      ? "true"
                      : "false")
              << std::endl;

    // Compare a value and an error (always not equal)
    std::cout << "\nComparing value with error:" << std::endl;
    std::cout << "  expect1 has value and expect_err1 has error: "
              << (expect1.has_value() && !expect_err1.has_value() ? "true"
                                                                  : "false")
              << std::endl;

    // Compare void expected
    auto void_exp1 = atom::type::expected<void>();
    auto void_exp2 = atom::type::expected<void>();
    auto void_err = atom::type::expected<void>(
        atom::type::Error<std::string>("Void error"));

    std::cout << "\nComparing void expected:" << std::endl;
    std::cout << "  void_exp1 and void_exp2 both have values: "
              << (void_exp1.has_value() && void_exp2.has_value() ? "true"
                                                                 : "false")
              << std::endl;
    std::cout << "  void_exp1 has value but void_err has error: "
              << (void_exp1.has_value() && !void_err.has_value() ? "true"
                                                                 : "false")
              << std::endl;

    // Compare with custom types
    auto user1 =
        atom::type::make_expected(User(1, "Same User", "same@example.com"));
    auto user2 =
        atom::type::make_expected(User(1, "Same User", "same@example.com"));
    auto user3 = atom::type::make_expected(
        User(2, "Different User", "diff@example.com"));

    std::cout << "\nComparing with custom types:" << std::endl;
    // 修复：手动比较值而不是使用==运算符
    bool users_equal = user1.has_value() && user2.has_value() &&
                       user1.value() == user2.value();
    bool users_different = user1.has_value() && user3.has_value() &&
                           !(user1.value() == user3.value());

    std::cout << "  user1 == user2: " << (users_equal ? "true" : "false")
              << std::endl;
    std::cout << "  user1 != user3: " << (users_different ? "true" : "false")
              << std::endl;

    std::cout << "\n==================================================="
              << std::endl;
    std::cout << "     EXPECTED EXAMPLES COMPLETED SUCCESSFULLY     "
              << std::endl;
    std::cout << "===================================================="
              << std::endl;

    return 0;
}