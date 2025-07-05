/**
 * @file template_traits_example.cpp
 * @brief Comprehensive examples of using the Template Traits library
 * @author Example Author
 * @date 2025-03-23
 */

#include <array>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "atom/meta/template_traits.hpp"

// For thread safety example
struct ThreadSafeType {
    struct is_thread_safe : std::true_type {};
};

struct NonThreadSafeType {};

// For template base class detection
template <typename T>
struct TemplateBase {
    using some_param_type = T;
};

class DerivedFromTemplate : public TemplateBase<int> {};
class NotDerived {};

// For variant examples
using VariantType = std::variant<int, double, std::string>;

// Simple utility to print section headers
void printSection(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

// Utility to print type names
template <typename T>
void printTypeName() {
    std::cout << "Type name: "
              << atom::meta::DemangleHelper::demangle(typeid(T).name())
              << std::endl;
}

// Transform types
template <typename T>
struct AddPointer {
    using type = T*;
};

// Multiple template base checks
template <typename T>
struct AnotherBase {};

// Filter types
template <typename T>
struct IsIntegral {
    static constexpr bool value = std::is_integral_v<T>;
};

int main() {
    std::cout << "TEMPLATE TRAITS COMPREHENSIVE EXAMPLES\n";
    std::cout << "======================================\n";

    //--------------------------------------------------------------------------
    // 1. Type identity and structured binding
    //--------------------------------------------------------------------------
    /*
    TODO: Fix this section to use the new identity structure
    printSection("Type Identity and Structured Binding");

    using SimpleIdentity = atom::meta::identity<int>;
    using ValueIdentity = atom::meta::identity<std::string, 42, "hello">;

    std::cout << "SimpleIdentity has_value: " << SimpleIdentity::has_value
              << std::endl;
    std::cout << "ValueIdentity has_value: " << ValueIdentity::has_value
              << std::endl;
    std::cout << "ValueIdentity value: " << ValueIdentity::value << std::endl;
    std::cout << "ValueIdentity value_at<1>(): " << ValueIdentity::value_at<1>()
              << std::endl;

    // Structured binding example
    auto [type, value1, value2] = ValueIdentity{};
    std::cout << "Decomposed ValueIdentity values: " << value1 << ", " << value2
              << std::endl;
    */

    //--------------------------------------------------------------------------
    // 2. Type list operations
    //--------------------------------------------------------------------------
    printSection("Type List Operations");

    using MyTypeList = atom::meta::type_list<int, double, std::string, float>;

    std::cout << "Type list size: " << MyTypeList::size << std::endl;

    // Access type at index
    using ThirdType = MyTypeList::at<2>;
    std::cout << "Third type: "
              << atom::meta::DemangleHelper::demangle(typeid(ThirdType).name())
              << std::endl;

    // Append and prepend
    using ExtendedList = MyTypeList::append<char, long>;
    using PrependedList = MyTypeList::prepend<bool, char*>;

    std::cout << "Extended list size: " << ExtendedList::size << std::endl;
    std::cout << "Prepended list size: " << PrependedList::size << std::endl;

    using PointerList = MyTypeList::transform<AddPointer>;
    std::cout << "First type in transformed list: "
              << atom::meta::DemangleHelper::demangle(
                     typeid(PointerList::at<0>).name())
              << std::endl;

    using IntegralTypes = MyTypeList::filter<IsIntegral>;
    std::cout << "Integral types count: " << IntegralTypes::size << std::endl;

    //--------------------------------------------------------------------------
    // 3. Template detection and traits
    //--------------------------------------------------------------------------
    printSection("Template Detection and Traits");

    // Check if a type is a template instantiation
    std::cout << "std::vector<int> is a template: "
              << atom::meta::is_template_v<std::vector<int>> << std::endl;
    std::cout << "int is a template: "
              << atom::meta::is_template_v<int> << std::endl;

    // Template traits
    using VectorType = std::vector<double>;
    std::cout << "Template name: "
              << atom::meta::template_traits<VectorType>::template_name
              << std::endl;
    std::cout << "Template arity: "
              << atom::meta::template_traits<VectorType>::arity << std::endl;

    // Check for specific argument type
    std::cout << "std::vector<double> has double as argument: "
              << atom::meta::template_traits<VectorType>::has_arg<
                     double> << std::endl;
    std::cout
        << "std::vector<double> has int as argument: "
        << atom::meta::template_traits<VectorType>::has_arg<int> << std::endl;

    // Check if a type is a specialization
    std::cout << "std::vector<int> is a specialization of std::vector: "
              << atom::meta::is_specialization_of_v<
                     std::vector, std::vector<int>> << std::endl;
    std::cout << "std::list<int> is a specialization of std::vector: "
              << atom::meta::is_specialization_of_v<
                     std::vector, std::list<int>> << std::endl;

    // Nth template argument
    using TupleType = std::tuple<int, double, std::string>;
    using SecondArgType = atom::meta::template_arg_t<1, TupleType>;
    std::cout
        << "Second template argument of std::tuple<int, double, std::string>: "
        << atom::meta::DemangleHelper::demangle(typeid(SecondArgType).name())
        << std::endl;

    //--------------------------------------------------------------------------
    // 4. Inheritance and derived type traits
    //--------------------------------------------------------------------------
    printSection("Inheritance and Derived Type Traits");

    // Check inheritance relationships
    std::cout
        << "std::vector<int> is derived from std::vector<int>: "
        << std::is_base_of_v<std::vector<int>, std::vector<int>> << std::endl;

    // Multiple inheritance checks
    class Base1 {};
    class Base2 {};
    class DerivedFromBoth : public Base1, public Base2 {};
    class DerivedFromFirst : public Base1 {};

    std::cout << "DerivedFromBoth is derived from all specified bases: "
              << atom::meta::is_derived_from_all_v<DerivedFromBoth, Base1,
                                                   Base2> << std::endl;
    std::cout << "DerivedFromFirst is derived from all specified bases: "
              << atom::meta::is_derived_from_all_v<DerivedFromFirst, Base1,
                                                   Base2> << std::endl;

    // Check if derived from any base
    std::cout << "DerivedFromFirst is derived from any specified base: "
              << atom::meta::is_derived_from_any_v<DerivedFromFirst, Base1,
                                                   Base2> << std::endl;
    std::cout
        << "int is derived from any specified base: "
        << atom::meta::is_derived_from_any_v<int, Base1, Base2> << std::endl;

    //--------------------------------------------------------------------------
    // 5. Template-of-templates detection
    //--------------------------------------------------------------------------
    printSection("Template-of-Templates Detection");

    // Check if type is a partial specialization of a template
    std::cout << "std::map<int, std::string> is a partial specialization of "
                 "std::map: "
              << atom::meta::is_partial_specialization_of_v<
                     std::map<int, std::string>, std::map> << std::endl;

    // Alias template detection
    using IntVector = std::vector<int>;
    std::cout << "IntVector is likely an alias template: "
              << atom::meta::is_alias_template<IntVector>::likely_alias
              << std::endl;

    // Class template concept
    std::cout << "std::vector<int> satisfies ClassTemplate concept: "
              << atom::meta::ClassTemplate<std::vector<int>> << std::endl;

    //--------------------------------------------------------------------------
    // 6. Type sequence and parameter pack utilities
    //--------------------------------------------------------------------------
    printSection("Type Sequence and Parameter Pack Utilities");

    // Count occurrences of a type
    constexpr size_t intCount =
        atom::meta::count_occurrences_v<int, double, int, char, int, float>;
    std::cout << "Number of occurrences of int: " << intCount << std::endl;

    // Find first index of a type
    constexpr size_t firstIntIndex =
        atom::meta::find_first_index_v<int, char, double, int, float, int>;
    std::cout << "First index of int: " << firstIntIndex << std::endl;

    // Find all indices of a type
    constexpr auto allIntIndices =
        atom::meta::find_all_indices<int, char, int, double, int, float>::value;
    std::cout << "All indices of int:";
    for (auto idx : allIntIndices) {
        std::cout << " " << idx;
    }
    std::cout << std::endl;

    //--------------------------------------------------------------------------
    // 7. Type extraction and manipulation utilities
    //--------------------------------------------------------------------------
    printSection("Type Extraction and Manipulation");

    // Reference wrapper extraction
    using RefWrapperType = std::reference_wrapper<int>;
    using ExtractedRefType =
        atom::meta::extract_reference_wrapper_type_t<RefWrapperType>;
    std::cout << "Type extracted from std::reference_wrapper<int>: "
              << atom::meta::DemangleHelper::demangle(
                     typeid(ExtractedRefType).name())
              << std::endl;

    // Pointer extraction
    using SmartPtrType = std::shared_ptr<double>;
    using ExtractedPtrType = atom::meta::extract_pointer_type_t<SmartPtrType>;
    std::cout << "Type extracted from std::shared_ptr<double>: "
              << atom::meta::DemangleHelper::demangle(
                     typeid(ExtractedPtrType).name())
              << std::endl;

    // Function traits
    auto lambda = [](int a, double b) -> std::string {
        return std::to_string(a + b);
    };
    using LambdaReturnType =
        atom::meta::extract_function_return_type_t<decltype(lambda)>;
    std::cout << "Lambda return type: "
              << atom::meta::DemangleHelper::demangle(
                     typeid(LambdaReturnType).name())
              << std::endl;

    using LambdaParameterTypes =
        atom::meta::extract_function_parameters_t<decltype(lambda)>;
    std::cout
        << "Lambda first parameter type: "
        << atom::meta::DemangleHelper::demangle(
               typeid(std::tuple_element_t<0, LambdaParameterTypes>).name())
        << std::endl;

    //--------------------------------------------------------------------------
    // 8. Tuple and structured binding support detection
    //--------------------------------------------------------------------------
    printSection("Tuple and Structured Binding Support");

    // Check if type is tuple-like
    std::cout << "std::tuple<int, double> is tuple-like: "
              << atom::meta::TupleLike<std::tuple<int, double>> << std::endl;
    std::cout
        << "std::pair<int, std::string> is tuple-like: "
        << atom::meta::TupleLike<std::pair<int, std::string>> << std::endl;
    std::cout << "std::array<int, 5> is tuple-like: "
              << atom::meta::TupleLike<std::array<int, 5>> << std::endl;
    std::cout << "int is tuple-like: "
              << atom::meta::TupleLike<int> << std::endl;

    //--------------------------------------------------------------------------
    // 9. Advanced type constraint detection
    //--------------------------------------------------------------------------
    /*
     printSection("Advanced Type Constraints");

    // Test copyability
    std::cout << "int has nothrow copyability: "
              << atom::meta::has_copyability<int>(
                     atom::meta::constraint_level::nothrow)
              << std::endl;

    // Test relocatability
    std::cout << "std::string has nothrow relocatability: "
              << atom::meta::has_relocatability<std::string>(
                     atom::meta::constraint_level::nothrow)
              << std::endl;

    // Test destructibility
    std::cout << "std::unique_ptr<int> has trivial destructibility: "
              << atom::meta::has_destructibility<std::unique_ptr<int>>(
                     atom::meta::constraint_level::trivial)
              << std::endl;

    // Test concepts
    std::cout << "int satisfies Copyable concept: "
              << atom::meta::Copyable<int> << std::endl;
    std::cout << "int satisfies TriviallyCopyable concept: "
              << atom::meta::TriviallyCopyable<int> << std::endl;
    std::cout << "std::vector<int> satisfies NothrowRelocatable concept: "
              << atom::meta::NothrowRelocatable<std::vector<int>> << std::endl;
    */

    //--------------------------------------------------------------------------
    // 10. Template base class detection
    //--------------------------------------------------------------------------
    printSection("Template Base Class Detection");

    // Check if a class is derived from a template
    std::cout << "DerivedFromTemplate is derived from TemplateBase: "
              << atom::meta::is_base_of_template_v<
                     TemplateBase, DerivedFromTemplate> << std::endl;
    std::cout << "NotDerived is derived from TemplateBase: "
              << atom::meta::is_base_of_template_v<TemplateBase,
                                                   NotDerived> << std::endl;

    class DerivedFromMultiple : public TemplateBase<int>,
                                public AnotherBase<double> {};

    std::cout
        << "DerivedFromMultiple is derived from any template: "
        << atom::meta::is_base_of_any_template_v<
               DerivedFromMultiple, TemplateBase, AnotherBase> << std::endl;

    //--------------------------------------------------------------------------
    // 11. Thread safety, variants, and containers
    //--------------------------------------------------------------------------
    printSection("Thread Safety, Variants, and Containers");

    // Thread safety
    std::cout << "ThreadSafeType satisfies ThreadSafe concept: "
              << atom::meta::ThreadSafe<ThreadSafeType> << std::endl;
    std::cout << "NonThreadSafeType satisfies ThreadSafe concept: "
              << atom::meta::ThreadSafe<NonThreadSafeType> << std::endl;

    // Variant traits
    std::cout << "VariantType is a variant: "
              << atom::meta::variant_traits<VariantType>::is_variant
              << std::endl;
    std::cout
        << "VariantType contains int: "
        << atom::meta::variant_traits<VariantType>::contains<int> << std::endl;
    std::cout
        << "VariantType contains bool: "
        << atom::meta::variant_traits<VariantType>::contains<bool> << std::endl;
    std::cout << "VariantType size: "
              << atom::meta::variant_traits<VariantType>::size << std::endl;

    // Container traits
    std::cout << "std::vector<int> is a container: "
              << atom::meta::container_traits<std::vector<int>>::is_container
              << std::endl;
    std::cout
        << "std::vector<int> is a sequence container: "
        << atom::meta::container_traits<std::vector<int>>::is_sequence_container
        << std::endl;
    std::cout << "std::map<int, double> is an associative container: "
              << atom::meta::container_traits<
                     std::map<int, double>>::is_associative_container
              << std::endl;
    std::cout
        << "std::array<int, 10> is fixed size: "
        << atom::meta::container_traits<std::array<int, 10>>::is_fixed_size
        << std::endl;

    //--------------------------------------------------------------------------
    // 12. Error reporting and static diagnostics
    //--------------------------------------------------------------------------
    printSection("Error Reporting and Static Diagnostics");

    // Static check example
    constexpr bool check_result = atom::meta::static_check<true>::value;
    std::cout << "Static check result: " << check_result << std::endl;

    // Type name for diagnostics
    std::cout << "Type name for std::vector<int>: "
              << atom::meta::DemangleHelper::demangle(
                     typeid(std::vector<int>).name())
              << std::endl;

    // Demonstrate static_error (commented to avoid compilation error)
    // constexpr const char error_msg[] = "This is a deliberate error";
    // using Error = atom::meta::static_error<error_msg>;

    //--------------------------------------------------------------------------
    // 13. Advanced combinations and practical applications
    //--------------------------------------------------------------------------
    printSection("Advanced Combinations and Practical Applications");

    // Example 1: Template introspection utility
    auto showTemplateInfo = [](auto&& x) {
        using T = std::decay_t<decltype(x)>;

        std::cout << "Template introspection for: "
                  << atom::meta::DemangleHelper::demangle(typeid(T).name())
                  << std::endl;

        if constexpr (atom::meta::is_template_v<T>) {
            std::cout << "  - Is a template: Yes" << std::endl;
            std::cout << "  - Template name: "
                      << atom::meta::template_traits<T>::template_name
                      << std::endl;
            std::cout << "  - Arity: " << atom::meta::template_traits<T>::arity
                      << std::endl;
            std::cout << "  - Arguments: ";

            auto argNames = atom::meta::template_traits<T>::arg_names;
            for (size_t i = 0; i < argNames.size(); ++i) {
                std::cout << (i > 0 ? ", " : "") << argNames[i];
            }
            std::cout << std::endl;
        } else {
            std::cout << "  - Is a template: No" << std::endl;
        }

        // Check some common properties
        std::cout << "  - Is copyable: "
                  << atom::meta::Copyable<T> << std::endl;
        std::cout << "  - Is trivially copyable: "
                  << atom::meta::TriviallyCopyable<T> << std::endl;

        if constexpr (atom::meta::container_traits<T>::is_container) {
            std::cout << "  - Is a container: Yes" << std::endl;
            std::cout << "  - Is a sequence container: "
                      << atom::meta::container_traits<T>::is_sequence_container
                      << std::endl;
            std::cout
                << "  - Is an associative container: "
                << atom::meta::container_traits<T>::is_associative_container
                << std::endl;
        } else {
            std::cout << "  - Is a container: No" << std::endl;
        }

        if constexpr (atom::meta::variant_traits<T>::is_variant) {
            std::cout << "  - Is a variant: Yes" << std::endl;
            std::cout << "  - Variant size: "
                      << atom::meta::variant_traits<T>::size << std::endl;
        } else {
            std::cout << "  - Is a variant: No" << std::endl;
        }

        if constexpr (atom::meta::TupleLike<T>) {
            std::cout << "  - Is tuple-like: Yes" << std::endl;
            std::cout << "  - Tuple size: "
                      << std::tuple_size_v<T> << std::endl;
        } else {
            std::cout << "  - Is tuple-like: No" << std::endl;
        }
    };

    // Use the introspection utility
    std::cout << "\nIntrospection Examples:\n" << std::endl;

    std::vector<int> vec{1, 2, 3};
    std::tuple<int, double, std::string> tup{1, 2.5, "hello"};
    VariantType var{42};

    showTemplateInfo(vec);
    std::cout << std::endl;
    showTemplateInfo(tup);
    std::cout << std::endl;
    showTemplateInfo(var);

    return 0;
}
