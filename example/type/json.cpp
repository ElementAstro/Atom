/**
 * @file json.cpp
 * @brief Demonstrates the vendored nlohmann::json that atom/type re-exports.
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "atom/type/json.hpp"

using json = nlohmann::json;

int main() {
    std::cout << "=== Build ===\n";
    json doc;
    doc["name"] = "atom";
    doc["version"] = 3;
    doc["tags"] = {"cpp", "astronomy"};
    std::cout << "  " << doc.dump() << '\n';

    std::cout << "\n=== Parse + access ===\n";
    json parsed = json::parse(R"({"x": 10, "y": 20})");
    std::cout << "  x + y = "
              << (parsed["x"].get<int>() + parsed["y"].get<int>()) << '\n';
    return 0;
}
