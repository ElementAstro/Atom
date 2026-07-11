/**
 * @file ryaml.cpp
 * @brief Demonstrates atom::type's self-built YAML (ryaml): YamlValue +
 * parsing.
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/ryaml.hpp"

using atom::type::YamlParser;
using atom::type::YamlValue;

int main() {
    const std::string text = "name: atom\nversion: 3\nstable: true\n";

    std::cout << "=== Parse ===\n";
    YamlValue root = YamlParser::parse(text);
    const auto& obj = root.as_object();
    std::cout << "  name    = " << obj.at("name").as_string() << '\n';
    std::cout << "  version = " << obj.at("version").as_number() << '\n';
    std::cout << "  stable  = " << obj.at("stable").as_bool() << '\n';
    return 0;
}
