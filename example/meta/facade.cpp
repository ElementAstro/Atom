/**
 * @file facade.cpp
 * @brief Comprehensive example demonstrating the Atom Meta facade system
 *
 * This example shows how to:
 * - Create custom facades with constraints and capabilities
 * - Use the facade builder pattern to compose functionality
 * - Work with type-erased proxies for dynamic dispatch
 * - Implement custom conventions and reflections
 * - Demonstrate advanced type erasure patterns
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <any>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Atom Meta facade headers
#include "atom/meta/facade.hpp"

using namespace atom::meta;

// Example classes for demonstrationclass Calculator {
public:
int add(int a, int b) const { return a + b; }
double multiply(double x, double y) const { return x * y; }
std::string describe() const { return "Simple Calculator"; }

void print(std::ostream& os) const { os << "Calculator: " << describe(); }
}
;

class StringProcessor {
public:
    std::string process(const std::string& input) const {
        return "Processed: " + input;
    }

    std::string describe() const { return "String Processor"; }

    void print(std::ostream& os) const {
        os << "StringProcessor: " << describe();
    }
};

// Custom dispatch types for our facadestruct printable_dispatch {
using dispatch_type = void(std::ostream&) const;
}
;

struct describable_dispatch {
    using dispatch_type = std::string() const;
};

struct calculable_dispatch {
    using dispatch_type = std::any(const std::string&,
                                   const std::vector<std::any>&) const;
};

/**
 * @brief Demonstrates basic facade creation and usage
 */
void basicFacadeExample() {
    std::cout << "\n=== Basic Facade Example ===\n";

    try {
        // Create a simple facade with printable capability
        using simple_facade =
            default_builder::add_convention<printable_dispatch,
                                            void(std::ostream&) const>::build;

        // Create proxies for different types
        proxy<simple_facade> calc_proxy(Calculator{});
        proxy<simple_facade> str_proxy(StringProcessor{});

        std::cout << "Created proxies with simple facade\n";

        // Use the proxies through the facade interface
        std::cout << "Calculator proxy: ";
        calc_proxy.call<printable_dispatch>(std::cout);
        std::cout << "\n";

        std::cout << "StringProcessor proxy: ";
        str_proxy.call<printable_dispatch>(std::cout);
        std::cout << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic facade example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates advanced facade building with multiple conventions
 */
void advancedFacadeExample() {
    std::cout << "\n=== Advanced Facade Example ===\n";

    try {
        // Create a more complex facade with multiple capabilities
        using advanced_facade =
            default_builder::add_convention<printable_dispatch,
                                            void(std::ostream&) const>::
                add_convention<describable_dispatch,
                               std::string() const>::restrict_layout<128>::
                    support_copy<constraint_level::nothrow>::build;

        // Create proxies
        proxy<advanced_facade> calc_proxy(Calculator{});
        proxy<advanced_facade> str_proxy(StringProcessor{});

        std::cout << "Created proxies with advanced facade\n";

        // Test printable capability
        std::cout << "Printable capability:\n";
        std::cout << "  Calculator: ";
        calc_proxy.call<printable_dispatch>(std::cout);
        std::cout << "\n";

        std::cout << "  StringProcessor: ";
        str_proxy.call<printable_dispatch>(std::cout);
        std::cout << "\n";

        // Test describable capability
        std::cout << "Describable capability:\n";
        auto calc_desc = calc_proxy.call<describable_dispatch, std::string>();
        auto str_desc = str_proxy.call<describable_dispatch, std::string>();

        std::cout << "  Calculator description: " << calc_desc << "\n";
        std::cout << "  StringProcessor description: " << str_desc << "\n";

        // Test copy capability
        std::cout << "Copy capability:\n";
        auto calc_copy = calc_proxy;
        std::cout << "  Copied calculator proxy: ";
        calc_copy.call<printable_dispatch>(std::cout);
        std::cout << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in advanced facade example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates facade constraints and their effects
 */
void constraintsExample() {
    std::cout << "\n=== Facade Constraints Example ===\n";

    try {
        // Create facades with different constraints
        using small_facade = default_builder::restrict_layout<64>::build;
        using large_facade = default_builder::restrict_layout<512>::build;
        using noncopyable_facade =
            default_builder::support_copy<constraint_level::none>::build;

        std::cout << "Facade constraints:\n";
        std::cout << "  Small facade max size: "
                  << small_facade::constraints.max_size << " bytes\n";
        std::cout << "  Large facade max size: "
                  << large_facade::constraints.max_size << " bytes\n";
        std::cout << "  Noncopyable facade copyability: "
                  << (noncopyable_facade::constraints.copyability ==
                              constraint_level::none
                          ? "none"
                          : "allowed")
                  << "\n";

        // Demonstrate size constraints
        proxy<small_facade> small_proxy(42);  // int fits in small facade
        std::cout << "Created small proxy with int\n";

        proxy<large_facade> large_proxy(std::string(
            "This is a longer string that might not fit in small facade"));
        std::cout << "Created large proxy with string\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in constraints example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates facade composition and skill system
 */
void compositionExample() {
    std::cout << "\n=== Facade Composition Example ===\n";

    try {
        // Create a composed facade by combining multiple capabilities
        using base_facade =
            default_builder::add_convention<printable_dispatch,
                                            void(std::ostream&) const>::build;

        using extended_facade =
            default_builder::add_facade<base_facade>::add_convention<
                describable_dispatch, std::string() const>::build;

        std::cout << "Created composed facade with inherited capabilities\n";

        proxy<extended_facade> proxy(Calculator{});

        // Use inherited capability
        std::cout << "Using inherited printable capability: ";
        proxy.call<printable_dispatch>(std::cout);
        std::cout << "\n";

        // Use new capability
        auto desc = proxy.call<describable_dispatch, std::string>();
        std::cout << "Using new describable capability: " << desc << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in composition example: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating all facade capabilities
 */
int main() {
    std::cout << "================================================\n";
    std::cout << "  Atom Meta Facade System Examples\n";
    std::cout << "================================================\n";

    try {
        basicFacadeExample();
        advancedFacadeExample();
        constraintsExample();
        compositionExample();

        std::cout << "\n=== All Facade Examples Completed Successfully ===\n";
        std::cout << "The facade system provides:\n";
        std::cout << "  ✓ Type erasure with configurable constraints\n";
        std::cout << "  ✓ Dynamic dispatch through conventions\n";
        std::cout << "  ✓ Composable facade building\n";
        std::cout << "  ✓ Memory layout control\n";
        std::cout << "  ✓ Copy/move semantics configuration\n";
        std::cout << "  ✓ Thread safety options\n";

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
