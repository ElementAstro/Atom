/**
 * @file static_vector_examples.cpp
 * @brief Comprehensive examples demonstrating the StaticVector class
 *
 * This file demonstrates all features of the atom::type::StaticVector template
 * class. It covers constructors, element access, modifiers, capacity
 * operations, iterators, and more advanced functionality like SIMD
 * transformations and smart wrappers.
 */

#include "atom/type/static_vector.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <functional>
#include <iostream>
#include <string>

// Define a custom type to demonstrate StaticVector with non-POD types
class Widget {
private:
    int id_;
    std::string name_;

public:
    Widget() : id_(0), name_("Default") {
        std::cout << "Widget default constructed: " << name_ << std::endl;
    }

    Widget(int id, std::string name) : id_(id), name_(std::move(name)) {
        std::cout << "Widget constructed: " << name_ << " (ID: " << id_ << ")"
                  << std::endl;
    }

    Widget(const Widget& other) : id_(other.id_), name_(other.name_) {
        std::cout << "Widget copy constructed: " << name_ << std::endl;
    }

    Widget(Widget&& other) noexcept
        : id_(other.id_), name_(std::move(other.name_)) {
        std::cout << "Widget move constructed: " << name_ << std::endl;
        other.id_ = -1;
    }

    Widget& operator=(const Widget& other) {
        if (this != &other) {
            id_ = other.id_;
            name_ = other.name_;
            std::cout << "Widget copy assigned: " << name_ << std::endl;
        }
        return *this;
    }

    Widget& operator=(Widget&& other) noexcept {
        if (this != &other) {
            id_ = other.id_;
            name_ = std::move(other.name_);
            other.id_ = -1;
            std::cout << "Widget move assigned: " << name_ << std::endl;
        }
        return *this;
    }

    ~Widget() {
        std::cout << "Widget destroyed: " << name_ << " (ID: " << id_ << ")"
                  << std::endl;
    }

    int getId() const { return id_; }
    const std::string& getName() const { return name_; }

    void setName(const std::string& name) { name_ = name; }

    bool operator==(const Widget& other) const {
        return id_ == other.id_ && name_ == other.name_;
    }
};

// Formatter for Widget to use with iostream
std::ostream& operator<<(std::ostream& os, const Widget& widget) {
    return os << "Widget{" << widget.getId() << ", \"" << widget.getName()
              << "\"}";
}

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n================================================"
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "================================================"
              << std::endl;
}

// Helper function to print a StaticVector's content
template <typename T, std::size_t Capacity, std::size_t Alignment>
void printVector(const atom::type::StaticVector<T, Capacity, Alignment>& vec,
                 const std::string& label = "Vector contents") {
    std::cout << label << " (size=" << vec.size()
              << ", capacity=" << vec.capacity() << "):" << std::endl;

    if (vec.empty()) {
        std::cout << "  <empty>" << std::endl;
        return;
    }

    for (std::size_t i = 0; i < vec.size(); ++i) {
        std::cout << "  [" << i << "]: " << vec[i] << std::endl;
    }
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  StaticVector Class Demonstration" << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        // Example 1: Constructors
        printSection("1. Constructors");

        // Default constructor
        std::cout << "Default constructor:" << std::endl;
        atom::type::StaticVector<int, 10> vec1;
        printVector(vec1, "Default constructed vector");

        // Size constructor
        std::cout << "\nSize constructor (value-initialized elements):"
                  << std::endl;
        atom::type::StaticVector<int, 10> vec2(5);
        printVector(vec2, "Size constructor vector");

        // Size and value constructor
        std::cout << "\nSize and value constructor:" << std::endl;
        atom::type::StaticVector<int, 10> vec3(3, 42);
        printVector(vec3, "Size and value vector");

        // Initializer list constructor
        std::cout << "\nInitializer list constructor:" << std::endl;
        atom::type::StaticVector<int, 10> vec4{1, 2, 3, 4, 5};
        printVector(vec4, "Initializer list vector");

        // Range constructor
        std::cout << "\nRange constructor:" << std::endl;
        std::array<double, 4> arr{1.1, 2.2, 3.3, 4.4};
        atom::type::StaticVector<double, 10> vec5(arr.begin(), arr.end());
        printVector(vec5, "Range constructed vector");

        // Copy constructor
        std::cout << "\nCopy constructor:" << std::endl;
        atom::type::StaticVector<int, 10> vec6(vec4);
        printVector(vec6, "Copy constructed vector");

        // Move constructor
        std::cout << "\nMove constructor:" << std::endl;
        atom::type::StaticVector<int, 10> vec7(std::move(vec6));
        printVector(vec7, "Move constructed vector");
        printVector(vec6, "Original vector after move");  // Should be empty

        // Constructor error handling
        std::cout << "\nConstructor error handling:" << std::endl;
        try {
            // This should throw since capacity is only 5
            atom::type::StaticVector<int, 5> vec_overflow{1, 2, 3, 4, 5, 6};
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // Custom alignment
        std::cout << "\nCustom alignment constructor:" << std::endl;
        atom::type::StaticVector<float, 8, 32> vec8{
            1.0f, 2.0f, 3.0f};  // 32-byte aligned for SIMD
        printVector(vec8, "Custom aligned vector");

        // Example 2: Assignment Operations
        printSection("2. Assignment Operations");

        // Copy assignment
        std::cout << "Copy assignment:" << std::endl;
        atom::type::StaticVector<int, 10> vec9;
        vec9 = vec4;
        printVector(vec9, "After copy assignment");

        // Move assignment
        std::cout << "\nMove assignment:" << std::endl;
        atom::type::StaticVector<int, 10> vec10;
        vec10 = std::move(vec9);
        printVector(vec10, "After move assignment");
        printVector(vec9, "Original vector after move assignment");

        // Initializer list assignment
        std::cout << "\nInitializer list assignment:" << std::endl;
        vec9 = {10, 20, 30, 40};
        printVector(vec9, "After initializer list assignment");

        // Assign method with count and value
        std::cout << "\nassign() method with count and value:" << std::endl;
        vec9.assign(3, 99);
        printVector(vec9, "After assign(3, 99)");

        // Assign method with range
        std::cout << "\nassign() method with range:" << std::endl;
        std::vector<int> std_vec{5, 6, 7, 8, 9};
        vec9.assign(std_vec.begin(), std_vec.end());
        printVector(vec9, "After assign(range)");

        // Assign method with container
        std::cout << "\nassign() method with container:" << std::endl;
        vec9.assign(std_vec);
        printVector(vec9, "After assign(container)");

        // Example 3: Element Access
        printSection("3. Element Access");

        atom::type::StaticVector<int, 10> vec11{10, 20, 30, 40, 50};

        // Subscript operator
        std::cout << "Subscript operator access:" << std::endl;
        std::cout << "vec11[0] = " << vec11[0] << std::endl;
        std::cout << "vec11[2] = " << vec11[2] << std::endl;

        // at() method with bounds checking
        std::cout << "\nat() method with bounds checking:" << std::endl;
        try {
            std::cout << "vec11.at(1) = " << vec11.at(1) << std::endl;
            std::cout << "Attempting out-of-bounds access with at()..."
                      << std::endl;
            std::cout << vec11.at(10) << std::endl;  // Should throw
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // front() and back()
        std::cout << "\nfront() and back() methods:" << std::endl;
        std::cout << "vec11.front() = " << vec11.front() << std::endl;
        std::cout << "vec11.back() = " << vec11.back() << std::endl;

        // Exception handling for front() and back() on empty vector
        std::cout << "\nException handling for front() and back():"
                  << std::endl;
        atom::type::StaticVector<int, 5> empty_vec;
        try {
            std::cout << "Attempting front() on empty vector..." << std::endl;
            std::cout << empty_vec.front() << std::endl;  // Should throw
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        try {
            std::cout << "Attempting back() on empty vector..." << std::endl;
            std::cout << empty_vec.back() << std::endl;  // Should throw
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // data() pointer access
        std::cout << "\ndata() pointer access:" << std::endl;
        const int* data_ptr = vec11.data();
        std::cout << "First three elements via data pointer: " << data_ptr[0]
                  << ", " << data_ptr[1] << ", " << data_ptr[2] << std::endl;

        // as_span() view
        std::cout << "\nas_span() method:" << std::endl;
        std::span<int> vec_span = vec11.as_span();
        std::cout << "First three elements via span: " << vec_span[0] << ", "
                  << vec_span[1] << ", " << vec_span[2] << std::endl;
        std::cout << "Span size: " << vec_span.size() << std::endl;

        // Example 4: Modifiers
        printSection("4. Modifiers");

        // push_back
        std::cout << "push_back methods:" << std::endl;
        atom::type::StaticVector<std::string, 5> str_vec;

        str_vec.pushBack("Hello");
        str_vec.pushBack("World");
        std::string str = "C++";
        str_vec.pushBack(str);
        str_vec.pushBack(std::string("StaticVector"));

        printVector(str_vec, "After push_back operations");

        // push_back overflow handling
        std::cout << "\npush_back overflow handling:" << std::endl;
        try {
            std::cout << "Trying to add beyond capacity..." << std::endl;
            str_vec.pushBack("Overflow");    // This is fine
            str_vec.pushBack("Will Throw");  // This should throw
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // emplace_back
        std::cout << "\nemplace_back method:" << std::endl;
        atom::type::StaticVector<Widget, 5> widget_vec;

        std::cout << "Adding widgets with emplace_back:" << std::endl;
        widget_vec.emplaceBack(1, "First");
        widget_vec.emplaceBack(2, "Second");

        std::cout << "\nWidget vector contents:" << std::endl;
        for (const auto& widget : widget_vec) {
            std::cout << "  " << widget << std::endl;
        }

        // pop_back
        std::cout << "\npop_back method:" << std::endl;
        std::cout << "Before pop_back: size = " << widget_vec.size()
                  << std::endl;
        widget_vec.popBack();
        std::cout << "After pop_back: size = " << widget_vec.size()
                  << std::endl;

        // pop_back on empty vector
        std::cout << "\npop_back on empty vector:" << std::endl;
        atom::type::StaticVector<int, 5> empty_for_pop;
        try {
            empty_for_pop.popBack();  // Should throw
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // insert - single element
        std::cout << "\ninsert method - single element:" << std::endl;
        atom::type::StaticVector<int, 10> insert_vec{10, 20, 30, 40, 50};
        printVector(insert_vec, "Before insert");

        auto insert_it = insert_vec.insert(insert_vec.begin() + 2, 25);
        std::cout << "Inserted value: " << *insert_it << " at position "
                  << std::distance(insert_vec.begin(), insert_it) << std::endl;
        printVector(insert_vec, "After insert");

        // insert - fill
        std::cout << "\ninsert method - fill:" << std::endl;
        insert_it = insert_vec.insert(insert_vec.begin() + 4, 2, 35);
        std::cout << "Inserted at position: "
                  << std::distance(insert_vec.begin(), insert_it) << std::endl;
        printVector(insert_vec, "After fill insert");

        // insert - range
        std::cout << "\ninsert method - range:" << std::endl;
        std::array<int, 3> insert_arr{60, 70, 80};
        insert_it = insert_vec.insert(insert_vec.end(), insert_arr.begin(),
                                      insert_arr.end());
        std::cout << "First inserted value from range: " << *insert_it
                  << std::endl;
        printVector(insert_vec, "After range insert");

        // emplace
        std::cout << "\nemplace method:" << std::endl;
        Widget& emplaced =
            *widget_vec.emplace(widget_vec.begin(), 3, "Emplaced");
        std::cout << "Emplaced widget: " << emplaced << std::endl;

        for (const auto& widget : widget_vec) {
            std::cout << "  " << widget << std::endl;
        }

        // erase - single element
        std::cout << "\nerase method - single element:" << std::endl;
        atom::type::StaticVector<int, 10> erase_vec{1, 2, 3, 4, 5, 6, 7, 8};
        printVector(erase_vec, "Before erase");

        auto erase_it = erase_vec.erase(erase_vec.begin() + 3);
        std::cout << "Element after erased element: " << *erase_it << std::endl;
        printVector(erase_vec, "After erase");

        // erase - range
        std::cout << "\nerase method - range:" << std::endl;
        erase_it =
            erase_vec.erase(erase_vec.begin() + 1, erase_vec.begin() + 4);
        std::cout << "Element after erased range: " << *erase_it << std::endl;
        printVector(erase_vec, "After range erase");

        // clear
        std::cout << "\nclear method:" << std::endl;
        std::cout << "Before clear: size = " << erase_vec.size() << std::endl;
        erase_vec.clear();
        std::cout << "After clear: size = " << erase_vec.size() << std::endl;

        // resize
        std::cout << "\nresize method:" << std::endl;
        atom::type::StaticVector<int, 10> resize_vec{1, 2, 3};
        printVector(resize_vec, "Before resize");

        // Resize to larger size
        resize_vec.resize(5);
        printVector(resize_vec, "After resize(5)");

        // Resize with value
        resize_vec.resize(8, 42);
        printVector(resize_vec, "After resize(8, 42)");

        // Resize to smaller size
        resize_vec.resize(4);
        printVector(resize_vec, "After resize(4)");

        // Resize beyond capacity
        try {
            resize_vec.resize(11);  // Should throw
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // swap
        std::cout << "\nswap method:" << std::endl;
        atom::type::StaticVector<int, 10> swap_vec1{1, 2, 3};
        atom::type::StaticVector<int, 10> swap_vec2{4, 5, 6, 7};

        std::cout << "Before swap:" << std::endl;
        printVector(swap_vec1, "swap_vec1");
        printVector(swap_vec2, "swap_vec2");

        swap_vec1.swap(swap_vec2);

        std::cout << "\nAfter swap:" << std::endl;
        printVector(swap_vec1, "swap_vec1");
        printVector(swap_vec2, "swap_vec2");

        // Global swap
        std::cout << "\nGlobal swap function:" << std::endl;
        atom::type::swap(swap_vec1, swap_vec2);

        std::cout << "After global swap:" << std::endl;
        printVector(swap_vec1, "swap_vec1");
        printVector(swap_vec2, "swap_vec2");

        // Example 5: Capacity and Size
        printSection("5. Capacity and Size");

        atom::type::StaticVector<double, 15> cap_vec{1.0, 2.0, 3.0};

        // empty
        std::cout << "empty() method:" << std::endl;
        std::cout << "cap_vec is " << (cap_vec.empty() ? "empty" : "not empty")
                  << std::endl;
        std::cout << "empty_vec is "
                  << (empty_vec.empty() ? "empty" : "not empty") << std::endl;

        // size
        std::cout << "\nsize() method:" << std::endl;
        std::cout << "cap_vec size: " << cap_vec.size() << std::endl;

        // max_size and capacity
        std::cout << "\nmax_size() and capacity() methods:" << std::endl;
        std::cout << "cap_vec capacity: " << cap_vec.capacity() << std::endl;
        std::cout << "cap_vec max_size: " << cap_vec.max_size() << std::endl;

        // reserve
        std::cout << "\nreserve() method:" << std::endl;
        std::cout << "Before reserve: size = " << cap_vec.size()
                  << ", capacity = " << cap_vec.capacity() << std::endl;

        // This is a no-op for StaticVector but shouldn't throw
        cap_vec.reserve(10);
        std::cout << "After reserve(10): size = " << cap_vec.size()
                  << ", capacity = " << cap_vec.capacity() << std::endl;

        // Reserve beyond capacity should throw
        try {
            cap_vec.reserve(20);  // Should throw
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // shrink_to_fit
        std::cout << "\nshrink_to_fit() method (no-op for StaticVector):"
                  << std::endl;
        std::cout << "Before shrink_to_fit: size = " << cap_vec.size()
                  << ", capacity = " << cap_vec.capacity() << std::endl;
        cap_vec.shrink_to_fit();  // No-op for StaticVector
        std::cout << "After shrink_to_fit: size = " << cap_vec.size()
                  << ", capacity = " << cap_vec.capacity() << std::endl;

        // Example 6: Iterator Operations
        printSection("6. Iterator Operations");

        atom::type::StaticVector<int, 10> iter_vec{10, 20, 30, 40, 50};

        // begin/end
        std::cout << "begin()/end() iteration:" << std::endl;
        std::cout << "Vector elements: ";
        for (auto it = iter_vec.begin(); it != iter_vec.end(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // cbegin/cend
        std::cout << "\ncbegin()/cend() const iteration:" << std::endl;
        std::cout << "Vector elements: ";
        for (auto it = iter_vec.cbegin(); it != iter_vec.cend(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // rbegin/rend
        std::cout << "\nrbegin()/rend() reverse iteration:" << std::endl;
        std::cout << "Vector elements in reverse: ";
        for (auto it = iter_vec.rbegin(); it != iter_vec.rend(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // crbegin/crend
        std::cout << "\ncrbegin()/crend() const reverse iteration:"
                  << std::endl;
        std::cout << "Vector elements in reverse: ";
        for (auto it = iter_vec.crbegin(); it != iter_vec.crend(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // Range-based for loop
        std::cout << "\nRange-based for loop:" << std::endl;
        std::cout << "Vector elements: ";
        for (const auto& val : iter_vec) {
            std::cout << val << " ";
        }
        std::cout << std::endl;

        // Iterator modifications
        std::cout << "\nModifying elements through iterator:" << std::endl;
        for (auto it = iter_vec.begin(); it != iter_vec.end(); ++it) {
            *it *= 2;
        }
        std::cout << "Vector elements after multiplication: ";
        for (const auto& val : iter_vec) {
            std::cout << val << " ";
        }
        std::cout << std::endl;

        // Example 7: Comparison Operations
        printSection("7. Comparison Operations");

        atom::type::StaticVector<int, 10> comp_vec1{1, 2, 3, 4};
        atom::type::StaticVector<int, 10> comp_vec2{1, 2, 3, 4};
        atom::type::StaticVector<int, 10> comp_vec3{1, 2, 3};
        atom::type::StaticVector<int, 10> comp_vec4{1, 2, 4, 5};

        // Equality operator
        std::cout << "Equality comparison:" << std::endl;
        std::cout << "comp_vec1 == comp_vec2: "
                  << (comp_vec1 == comp_vec2 ? "true" : "false") << std::endl;
        std::cout << "comp_vec1 == comp_vec3: "
                  << (comp_vec1 == comp_vec3 ? "true" : "false") << std::endl;

        // Three-way comparison
        std::cout << "\nThree-way comparison:" << std::endl;

        auto compare = [](const auto& a, const auto& b) -> std::string {
            auto result = a <=> b;
            if (result < 0)
                return "less";
            if (result > 0)
                return "greater";
            return "equal";
        };

        std::cout << "comp_vec1 <=> comp_vec2: "
                  << compare(comp_vec1, comp_vec2) << std::endl;
        std::cout << "comp_vec1 <=> comp_vec3: "
                  << compare(comp_vec1, comp_vec3) << std::endl;
        std::cout << "comp_vec1 <=> comp_vec4: "
                  << compare(comp_vec1, comp_vec4) << std::endl;
        std::cout << "comp_vec3 <=> comp_vec4: "
                  << compare(comp_vec3, comp_vec4) << std::endl;

        // Example 8: Advanced Features
        printSection("8. Advanced Features");

        // transform_elements (SIMD optimized for arithmetic types)
        std::cout << "transform_elements method:" << std::endl;
        atom::type::StaticVector<float, 16, 16> simd_vec{
            1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};

        printVector(simd_vec, "Before transformation");
        simd_vec.transform_elements([](float x) { return x * x + 1.0f; });
        printVector(simd_vec, "After transformation (x^2 + 1)");

        // parallel_for_each
        std::cout << "\nparallel_for_each method:" << std::endl;
        atom::type::StaticVector<double, 1024> parallel_vec(10, 1.0);

        parallel_vec.parallel_for_each(
            [](double& x) { x = std::sqrt(x) * 10.0; });

        std::cout << "First 5 elements after parallel processing:" << std::endl;
        for (int i = 0; i < 5 && i < parallel_vec.size(); ++i) {
            std::cout << "  [" << i << "]: " << parallel_vec[i] << std::endl;
        }

        // safeAddElements
        std::cout << "\nsafeAddElements method:" << std::endl;
        atom::type::StaticVector<int, 10> safe_vec{1, 2, 3};

        std::array<int, 4> add_elements{4, 5, 6, 7};
        std::span<const int> elements_span(add_elements);

        bool success = safe_vec.safeAddElements(elements_span);
        std::cout << "Safe add successful: " << (success ? "Yes" : "No")
                  << std::endl;
        printVector(safe_vec, "After safe add");

        // Try to add too many elements
        std::array<int, 10> too_many{10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
        success = safe_vec.safeAddElements(std::span<const int>(too_many));
        std::cout << "Safe add with too many elements successful: "
                  << (success ? "Yes" : "No") << std::endl;

        // Global safeAddElements function
        std::cout << "\nGlobal safeAddElements function:" << std::endl;
        atom::type::StaticVector<int, 10> global_safe_vec{1, 2};

        std::array<int, 3> more_elements{3, 4, 5};
        success = atom::type::safeAddElements(
            global_safe_vec, std::span<const int>(more_elements));
        std::cout << "Global safe add successful: " << (success ? "Yes" : "No")
                  << std::endl;
        printVector(global_safe_vec, "After global safe add");

        // makeStaticVector
        std::cout << "\nmakeStaticVector function:" << std::endl;
        std::vector<double> std_double_vec{1.1, 2.2, 3.3, 4.4, 5.5};

        auto made_vec =
            atom::type::makeStaticVector<double, 10>(std_double_vec);
        printVector(made_vec, "Vector created from std::vector");

        // simdTransform
        std::cout << "\nsimdTransform function:" << std::endl;
        atom::type::StaticVector<float, 8> simd_src1{1.0f, 2.0f, 3.0f, 4.0f};
        atom::type::StaticVector<float, 8> simd_src2{5.0f, 6.0f, 7.0f, 8.0f};
        atom::type::StaticVector<float, 8> simd_dest;

        success =
            atom::type::simdTransform(simd_src1, simd_src2, simd_dest,
                                      [](float a, float b) { return a + b; });

        std::cout << "SIMD transform successful: " << (success ? "Yes" : "No")
                  << std::endl;
        printVector(simd_dest, "Result of SIMD transform (addition)");

        // Example 9: SmartStaticVector
        printSection("9. SmartStaticVector");

        // Creation and basic usage
        std::cout << "SmartStaticVector basic usage:" << std::endl;
        atom::type::SmartStaticVector<int, 10> smart_vec;

        // Access the underlying vector
        smart_vec->pushBack(10);
        smart_vec->pushBack(20);
        smart_vec->pushBack(30);

        std::cout << "SmartStaticVector contents:" << std::endl;
        for (int i = 0; i < smart_vec->size(); ++i) {
            std::cout << "  [" << i << "]: " << smart_vec->at(i) << std::endl;
        }

        // Check if shared
        std::cout << "\nSharing behavior:" << std::endl;
        std::cout << "Is initial vector shared? "
                  << (smart_vec.isShared() ? "Yes" : "No") << std::endl;

        // Create a copy that shares the data
        auto shared_smart_vec = smart_vec;
        std::cout << "Is vector shared after copy? "
                  << (smart_vec.isShared() ? "Yes" : "No") << std::endl;

        // Modify the copy
        std::cout << "\nModifying through copy:" << std::endl;
        shared_smart_vec->pushBack(40);

        std::cout << "Original vector size: " << smart_vec->size() << std::endl;
        std::cout << "Copy vector size: " << shared_smart_vec->size()
                  << std::endl;

        // Make unique and modify
        std::cout << "\nmakeUnique behavior:" << std::endl;
        smart_vec.makeUnique();
        std::cout << "Is vector shared after makeUnique? "
                  << (smart_vec.isShared() ? "Yes" : "No") << std::endl;

        smart_vec->pushBack(50);
        std::cout << "Original vector size after makeUnique and modification: "
                  << smart_vec->size() << std::endl;
        std::cout << "Copy vector size after original was modified: "
                  << shared_smart_vec->size() << std::endl;

        // Example 10: Edge Cases and Error Handling
        printSection("10. Edge Cases and Error Handling");

        // Special case: Zero-size vector operations
        std::cout << "Empty vector operations:" << std::endl;
        atom::type::StaticVector<int, 10> zero_vec;

        std::cout << "Empty vector size: " << zero_vec.size() << std::endl;
        std::cout << "Empty vector capacity: " << zero_vec.capacity()
                  << std::endl;
        std::cout << "Empty vector is empty: "
                  << (zero_vec.empty() ? "Yes" : "No") << std::endl;

        // Out-of-range error handling
        std::cout << "\nOut-of-range error handling:" << std::endl;
        atom::type::StaticVector<int, 5> range_vec{1, 2, 3};

        try {
            std::cout << "Attempting to insert beyond capacity..." << std::endl;
            range_vec.insert(range_vec.begin(), 10, 0);  // Should throw
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // Invalid iterator error handling
        std::cout << "\nInvalid iterator error handling:" << std::endl;
        try {
            std::cout << "Attempting to erase with invalid iterator..."
                      << std::endl;
            range_vec.erase(range_vec.end() + 1);  // Should throw
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // Invalid range error handling
        std::cout << "\nInvalid range error handling:" << std::endl;
        try {
            std::cout << "Attempting to erase with invalid range..."
                      << std::endl;
            range_vec.erase(range_vec.begin() + 1,
                            range_vec.begin());  // Should throw
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // Example 11: Performance Comparison
        printSection("11. Performance Comparison");

        constexpr size_t NUM_ELEMENTS = 1000000;
        constexpr size_t STATIC_CAPACITY = 1000000;

        // Compare StaticVector vs std::vector for push_back operations
        std::cout << "Performance comparison: push_back operations"
                  << std::endl;

        // Test StaticVector
        auto start_time = std::chrono::high_resolution_clock::now();

        atom::type::StaticVector<int, STATIC_CAPACITY> perf_static_vec;
        for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
            perf_static_vec.pushBack(static_cast<int>(i));
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto static_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                  start_time)
                .count();

        // Test std::vector
        start_time = std::chrono::high_resolution_clock::now();

        std::vector<int> perf_std_vec;
        perf_std_vec.reserve(
            NUM_ELEMENTS);  // Fair comparison with pre-allocated memory
        for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
            perf_std_vec.push_back(static_cast<int>(i));
        }

        end_time = std::chrono::high_resolution_clock::now();
        auto std_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_time - start_time)
                            .count();

        std::cout << "StaticVector push_back time: " << static_time << " ms"
                  << std::endl;
        std::cout << "std::vector push_back time: " << std_time << " ms"
                  << std::endl;

        // Example 12: Working with Complex Types
        printSection("12. Working with Complex Types");

        // Complex numbers
        std::cout << "Complex numbers in StaticVector:" << std::endl;
        atom::type::StaticVector<std::complex<double>, 10> complex_vec;

        complex_vec.pushBack({1.0, 2.0});
        complex_vec.pushBack({3.0, 4.0});
        complex_vec.pushBack({5.0, 6.0});

        std::cout << "Complex vector contents:" << std::endl;
        for (const auto& c : complex_vec) {
            std::cout << "  " << c.real() << " + " << c.imag() << "i"
                      << std::endl;
        }

        // Pair type
        std::cout << "\nPairs in StaticVector:" << std::endl;
        atom::type::StaticVector<std::pair<int, std::string>, 5> pair_vec;

        pair_vec.pushBack({1, "one"});
        pair_vec.pushBack({2, "two"});
        pair_vec.emplaceBack(3, "three");

        std::cout << "Pair vector contents:" << std::endl;
        for (const auto& [num, str] : pair_vec) {
            std::cout << "  " << num << " -> " << str << std::endl;
        }

        // StaticVector of StaticVector (nested)
        std::cout << "\nNested StaticVector:" << std::endl;
        atom::type::StaticVector<atom::type::StaticVector<int, 5>, 3>
            nested_vec;

        nested_vec.pushBack(atom::type::StaticVector<int, 5>{1, 2, 3});
        nested_vec.pushBack(atom::type::StaticVector<int, 5>{4, 5});
        nested_vec.pushBack(atom::type::StaticVector<int, 5>{6, 7, 8, 9});

        std::cout << "Nested vector contents:" << std::endl;
        for (size_t i = 0; i < nested_vec.size(); ++i) {
            std::cout << "  Row " << i << ": ";
            for (const auto& val : nested_vec[i]) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
