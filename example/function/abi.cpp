#include "atom/function/abi.hpp"

#include <iostream>
#include <vector>

using namespace atom::meta;

int main() {
    // Demangle a type name
    std::string demangledType = DemangleHelper::demangleType<int>();
    std::cout << "Demangled type: " << demangledType << std::endl;

    // Demangle an instance type
    int instance = 42;
    std::string demangledInstanceType = DemangleHelper::demangleType(instance);
    std::cout << "Demangled instance type: " << demangledInstanceType
              << std::endl;

    // Demangle a mangled name with optional source location
    std::string mangledName = typeid(int).name();
    std::string demangledName =
        DemangleHelper::demangle(mangledName, std::source_location::current());
    std::cout << "Demangled name with location: " << demangledName << std::endl;

    // Demangle multiple mangled names
    std::vector<std::string_view> mangledNames = {typeid(int).name(),
                                                  typeid(double).name()};
    std::vector<std::string> demangledNames =
        DemangleHelper::demangleMany(mangledNames);
    std::cout << "Demangled names: ";
    for (const auto& name : demangledNames) {
        std::cout << name << " ";
    }
    std::cout << std::endl;

#if ENABLE_DEBUG
    // Visualize a demangled type name
    std::string visualizedType = DemangleHelper::visualize(demangledType);
    std::cout << "Visualized type: " << visualizedType << std::endl;
#endif

    return 0;
}