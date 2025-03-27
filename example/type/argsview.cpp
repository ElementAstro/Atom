#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "atom/type/argsview.hpp"

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

// Template to print section headers
void print_header(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
    std::cout << std::string(title.length() + 8, '=') << std::endl;
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

    // 修复：直接使用单层元组而不是嵌套元组
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
    std::optional<double> opt3 = 3.14;  // 修复：初始化opt3

    // 修复：确保Args和OptionalArgs匹配
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

    auto doubled = integers.transform([](int i) { return i * 2; });
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

    // 修复：确保accumulate函数参数正确
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

    // 修复：删除未使用的参数d和e
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

    // 11. Find Operation
    print_header("Find Operation");

    auto found_integer = integers.find([](int i) { return i > 3; });
    std::cout << "First integer > 3: "
              << (found_integer ? std::to_string(*found_integer) : "Not found")
              << std::endl;

    // 修复：返回Person对象而不是bool
    auto found_person = persons.find([](const Person& p) {
        return p.name.starts_with("B") ? std::optional<Person>{p}
                                       : std::optional<Person>{};
    });
    std::cout << "First person with name starting with 'B': "
              << (found_person ? found_person->name : "Not found") << std::endl;

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
    std::cout << "view5 > view3: " << (view5 > view3 ? "Yes" : "No")
              << std::endl;
    std::cout << "view3 >= view4: " << (view3 >= view4 ? "Yes" : "No")
              << std::endl;

    // 16. Utility Functions (sum and concat)
    print_header("Utility Functions (sum and concat)");

    int sum_result = atom::sum(10, 20, 30, 40, 50);
    std::cout << "Sum result: " << sum_result << std::endl;

    std::string concat_result = atom::concat("Hello", " ", "World", "! ", 42);
    std::cout << "Concat result: " << concat_result << std::endl;

    // 17. std::hash Support for ArgsView
    print_header("std::hash Support for ArgsView");

    std::hash<atom::ArgsView<int, int, int>> hasher;
    size_t hash1 = hasher(view3);
    size_t hash2 = hasher(view4);
    size_t hash3 = hasher(view5);

    std::cout << "Hash of view3: " << hash1 << std::endl;
    std::cout << "Hash of view4: " << hash2 << std::endl;
    std::cout << "Hash of view5: " << hash3 << std::endl;
    std::cout << "view3 and view4 have same hash: "
              << (hash1 == hash2 ? "Yes" : "No") << std::endl;
    std::cout << "view3 and view5 have same hash: "
              << (hash1 == hash3 ? "Yes" : "No") << std::endl;

// 18. Debug Print Function (if __DEBUG__ is defined)
#ifdef __DEBUG__
    print_header("Debug Print Function");

    std::cout << "Printing using atom::print: ";
    atom::print(1, 2, 3, "hello", 3.14);
#endif

    std::cout << "\nAll examples completed successfully!" << std::endl;
    return 0;
}