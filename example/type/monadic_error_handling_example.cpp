#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Include type utilities for advanced patterns
#include "atom/type/argsview.hpp"
#include "atom/type/concurrent_vector.hpp"
#include "atom/type/expected.hpp"
#include "atom/type/indestructible.hpp"
#include "atom/type/optional.hpp"
#include "atom/type/trackable.hpp"

// Helper function to print section headersvoid print_header(const std::string&
// title) {
std::cout << "\n=== " << title << " ===" << std::endl;
std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// Pattern 1: Monadic Error Handling Chainclass ValidationError {
public:
std::string field;
std::string message;

ValidationError(std::string f, std::string m)
    : field(std::move(f)), message(std::move(m)) {}

friend std::ostream& operator<<(std::ostream& os, const ValidationError& e) {
    return os << e.field << ": " << e.message;
}
}
;

struct UserData {
    std::string name;
    std::string email;
    int age;

    friend std::ostream& operator<<(std::ostream& os, const UserData& u) {
        return os << "User{name='" << u.name << "', email='" << u.email
                  << "', age=" << u.age << "}";
    }
};

// Validation functions that return expectedatom::type::expected<std::string,
// ValidationError> validateName(
    const std::string& name) {
        if (name.empty()) {
            return atom::type::Error<ValidationError>(
                ValidationError("name", "Name cannot be empty"));
        }
        if (name.length() < 2) {
            return atom::type::Error<ValidationError>(
                ValidationError("name", "Name too short"));
        }
        return name;
    }

    atom::type::expected<std::string, ValidationError> validateEmail(
        const std::string& email) {
        if (email.find('@') == std::string::npos) {
            return atom::type::Error<ValidationError>(
                ValidationError("email", "Invalid email format"));
        }
        return email;
    }

    atom::type::expected<int, ValidationError> validateAge(int age) {
        if (age < 0) {
            return atom::type::Error<ValidationError>(
                ValidationError("age", "Age cannot be negative"));
        }
        if (age > 150) {
            return atom::type::Error<ValidationError>(
                ValidationError("age", "Age too high"));
        }
        return age;
    }

    // Monadic validation chainatom::type::expected<UserData, ValidationError>
    // createUser(
    const std::string& name, const std::string& email, int age) {
        return validateName(name).and_then([&](const std::string& valid_name) {
            return validateEmail(email).and_then(
                [&](const std::string& valid_email) {
                    return validateAge(age).map([&](int valid_age) {
                        return UserData{valid_name, valid_email, valid_age};
                    });
                });
        });
    }

    // Pattern 2: Resource Management with Trackable and Indestructibleclass
    // ResourceManager {
private:
    atom::type::Indestructible<atom::type::concurrent_vector<std::string>>
        resources_;

public:
    void addResource(const std::string& resource) {
        resources_.get().push_back(resource);
        std::cout << "Added resource: " << resource << std::endl;
    }

    size_t getResourceCount() const { return resources_.get().size(); }

    void listResources() const {
        std::cout << "Current resources:" << std::endl;
        auto& vec = resources_.get();
        for (size_t i = 0; i < vec.size(); ++i) {
            auto item = vec.at(i);
            if (item) {
                std::cout << "  " << i << ": " << *item << std::endl;
            }
        }
    }
    }
    ;

    // Pattern 3: Functional Programming with ArgsViewtemplate <typename...
    // Args>
    auto compose_operations(Args... args) {
        auto view = atom::makeArgsView(args...);

        return view
            .transform([](auto value) {
                if constexpr (std::is_arithmetic_v<decltype(value)>) {
                    return value * 2;
                } else {
                    return value;
                }
            })
            .accumulate(
                [](auto acc, auto val) {
                    if constexpr (std::is_arithmetic_v<decltype(acc)> &&
                                  std::is_arithmetic_v<decltype(val)>) {
                        return acc + val;
                    } else {
                        return acc;
                    }
                },
                0);
    }

    // Pattern 4: Optional Chaining for Safe Navigationstruct Address {
    atom::type::optional<std::string> street;
    atom::type::optional<std::string> city;
    atom::type::optional<std::string> country;

    Address() = default;
    Address(const std::string& s, const std::string& c, const std::string& co)
        : street(s), city(c), country(co) {}
    }
    ;

    struct Person {
        std::string name;
        atom::type::optional<Address> address;

        Person(std::string n) : name(std::move(n)) {}
        Person(std::string n, Address a)
            : name(std::move(n)), address(std::move(a)) {}
    };

    atom::type::optional<std::string> getPersonCountry(const Person& person) {
        return person.address.and_then(
            [](const Address& addr) { return addr.country; });
    }

    atom::type::optional<std::string> getFullAddress(const Person& person) {
        if (!person.address) {
            return atom::type::nullopt;
        }

        const auto& addr = *person.address;
        std::string result;

        if (addr.street) {
            result += *addr.street;
        }
        if (addr.city) {
            if (!result.empty())
                result += ", ";
            result += *addr.city;
        }
        if (addr.country) {
            if (!result.empty())
                result += ", ";
            result += *addr.country;
        }

        return result.empty() ? atom::type::nullopt
                              : atom::type::make_optional(result);
    }

    int main() {
        std::cout << "Advanced Type System Patterns" << std::endl;
        std::cout << "=============================" << std::endl;

        // Pattern 1: Monadic Error Handling
        print_header("Monadic Error Handling Chain");

        std::vector<std::tuple<std::string, std::string, int>> test_users = {
            {"Alice Johnson", "alice@example.com", 28},
            {"", "bob@example.com", 35},         // Invalid name
            {"Charlie", "invalid-email", 42},    // Invalid email
            {"Diana", "diana@example.com", -5},  // Invalid age
            {"Eve", "eve@example.com", 30}       // Valid
        };

        for (const auto& [name, email, age] : test_users) {
            std::cout << "\nValidating: name='" << name << "', email='" << email
                      << "', age=" << age << std::endl;

            auto result = createUser(name, email, age);
            if (result) {
                std::cout << "✓ Valid user created: " << *result << std::endl;
            } else {
                std::cout << "✗ Validation failed: " << result.error().error()
                          << std::endl;
            }
        }

        // Pattern 2: Resource Management
        print_header("Resource Management with Indestructible");

        ResourceManager manager;
        manager.addResource("Database Connection");
        manager.addResource("File Handle");
        manager.addResource("Network Socket");

        std::cout << "Total resources: " << manager.getResourceCount()
                  << std::endl;
        manager.listResources();

        // Pattern 3: Functional Composition
        print_header("Functional Programming with ArgsView");

        auto result1 = compose_operations(1, 2, 3, 4, 5);
        std::cout << "Compose operations on (1,2,3,4,5): " << result1
                  << std::endl;

        auto result2 = compose_operations(10, 20, 30);
        std::cout << "Compose operations on (10,20,30): " << result2
                  << std::endl;

        // Demonstrate ArgsView functional operations
        auto numbers = atom::makeArgsView(1, 2, 3, 4, 5);

        auto doubled = numbers.transform([](int x) { return x * 2; });
        std::cout << "Doubled numbers: ";
        doubled.forEach([](int x) { std::cout << x << " "; });
        std::cout << std::endl;

        auto sum =
            numbers.accumulate([](int acc, int val) { return acc + val; }, 0);
        std::cout << "Sum of numbers: " << sum << std::endl;

        auto even_count = numbers.accumulate(
            [](int acc, int val) { return acc + (val % 2 == 0 ? 1 : 0); }, 0);
        std::cout << "Count of even numbers: " << even_count << std::endl;

        // Pattern 4: Optional Chaining
        print_header("Optional Chaining for Safe Navigation");

        // Create people with different address completeness
        Person person1("John Doe");
        Person person2("Jane Smith", Address("123 Main St", "Boston", "USA"));
        Person person3("Bob Wilson", Address("", "London", "UK"));

        std::vector<Person> people = {person1, person2, person3};

        for (const auto& person : people) {
            std::cout << "\nPerson: " << person.name << std::endl;

            auto country = getPersonCountry(person);
            if (country) {
                std::cout << "  Country: " << *country << std::endl;
            } else {
                std::cout << "  Country: Not available" << std::endl;
            }

            auto full_address = getFullAddress(person);
            if (full_address) {
                std::cout << "  Full address: " << *full_address << std::endl;
            } else {
                std::cout << "  Full address: Not available" << std::endl;
            }
        }

        // Pattern 5: Concurrent Processing with Type Safety
        print_header("Concurrent Processing with Type Safety");

        atom::type::concurrent_vector<std::string> concurrent_data;

        // Add data concurrently (simulated)
        std::vector<std::string> data_to_add = {"Item 1", "Item 2", "Item 3",
                                                "Item 4", "Item 5"};

        for (const auto& item : data_to_add) {
            concurrent_data.push_back(item);
        }

        std::cout << "Added " << concurrent_data.size() << " items concurrently"
                  << std::endl;

        // Process data safely
        std::cout << "Processing concurrent data:" << std::endl;
        for (size_t i = 0; i < concurrent_data.size(); ++i) {
            auto item = concurrent_data.at(i);
            if (item) {
                std::cout << "  [" << i << "]: " << *item << std::endl;
            }
        }

        std::cout << "\nAll advanced pattern examples completed successfully!"
                  << std::endl;
        return 0;
    }
