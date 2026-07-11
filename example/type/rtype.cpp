/**
 * @file rtype.cpp
 * @brief Demonstrates atom::type reflection (rtype): describe a struct with
 *        Reflectable + make_field, then serialize to/from the self-built JSON.
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "atom/type/rtype.hpp"

using atom::type::make_field;
using atom::type::Reflectable;

struct Person {
    std::string name;
    int age = 0;
};

int main() {
    // Build via CTAD (the deduction guide recovers Person from the fields).
    auto schema = Reflectable(
        make_field<Person>("name", "The person's name", &Person::name),
        make_field<Person>("age", "The person's age", &Person::age));

    std::cout << "=== to_json ===\n";
    Person p{"Ada", 36};
    auto json = schema.to_json(p);
    std::cout << "  name = " << json["name"].as_string() << '\n';
    std::cout << "  age  = " << json["age"].as_number() << '\n';

    std::cout << "\n=== from_json (round-trip) ===\n";
    Person back = schema.from_json(json);
    std::cout << "  name = " << back.name << ", age = " << back.age << '\n';
    return 0;
}
