/**
 * @file rjson.cpp
 * @brief Demonstrates atom::type's self-built JSON (rjson): JsonValue +
 * parsing.
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/rjson.hpp"

using atom::type::JsonParser;
using atom::type::JsonValue;

int main() {
    const std::string text =
        R"({"name": "atom", "version": 3, "stable": true})";

    std::cout << "=== Parse ===\n";
    JsonValue root = JsonParser::parse(text);
    const auto& obj = root.as_object();
    std::cout << "  name    = " << obj.at("name").as_string() << '\n';
    std::cout << "  version = " << obj.at("version").as_number() << '\n';
    std::cout << "  stable  = " << obj.at("stable").as_bool() << '\n';

    std::cout << "\n=== Build + serialize ===\n";
    atom::type::JsonObject out;
    out["greeting"] = JsonValue(std::string("hi"));
    out["count"] = JsonValue(7);
    JsonValue doc(out);
    std::cout << "  " << doc.to_string() << '\n';
    return 0;
}
