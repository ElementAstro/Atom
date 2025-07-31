#include <iostream>
#include "atom/type/qvariant.hpp"

int main() {
    std::cout << "Testing basic VariantWrapper functionality..." << std::endl;

    try {
        // Test basic VariantWrapper
        atom::type::VariantWrapper<int, std::string> variant;
        std::cout << "Created empty variant" << std::endl;

        variant = 42;
        std::cout << "Assigned int: " << variant.get<int>() << std::endl;

        variant = std::string("hello");
        std::cout << "Assigned string: " << variant.get<std::string>() << std::endl;

        // Test toString
        std::string str = variant.toString();
        std::cout << "toString result: " << str << std::endl;

        // Test stream operator
        std::cout << "Stream operator: " << variant << std::endl;

        std::cout << "Basic VariantWrapper test completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
