#include "../atom/type/iter.hpp"

#include <iostream>
#include <list>
#include <map>
#include <string>
#include <vector>

// Helper function to print container contents
template<typename Container>
void print_container(const Container& container, const std::string& name) {
    std::cout << name << ": ";
    for (const auto& item : container) {
        std::cout << item << " ";
    }
    std::cout << std::endl;
}

// Helper function to print key-value pairs
template<typename Container>
void print_key_value_container(const Container& container, const std::string& name) {
    std::cout << name << ": ";
    for (const auto& [key, value] : container) {
        std::cout << "[" << key << ": " << value << "] ";
    }
    std::cout << std::endl;
}

// Example 1: PointerIterator
void pointer_iterator_example() {
    std::cout << "\n=== Example 1: PointerIterator ===\n";
    
    // Create a sample container
    std::vector<int> numbers = {10, 20, 30, 40, 50};
    print_container(numbers, "Original vector");
    
    // Create pointer iterators
    auto [begin_ptr, end_ptr] = makePointerRange(numbers.begin(), numbers.end());
    
    // Print addresses of original elements
    std::cout << "Addresses of elements:\n";
    for (auto it = begin_ptr; it != end_ptr; ++it) {
        int* ptr = *it;  // Get a pointer to the element
        std::cout << "Value: " << *ptr << ", Address: " << ptr << std::endl;
    }
    
    // Modify elements via pointers
    std::cout << "\nModifying elements via pointers...\n";
    for (auto it = begin_ptr; it != end_ptr; ++it) {
        int* ptr = *it;
        *ptr *= 2;  // Double each value
    }
    
    print_container(numbers, "Modified vector");
    
    // Example of processContainer function
    std::list<char> chars = {'a', 'b', 'c', 'd', 'e'};
    print_container(chars, "Original list of chars");
    
    std::cout << "Calling processContainer to remove middle elements...\n";
    processContainer(chars);
    print_container(chars, "Resulting list of chars");
}

// Example 2: EarlyIncIterator
void early_inc_iterator_example() {
    std::cout << "\n=== Example 2: EarlyIncIterator ===\n";
    
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    print_container(numbers, "Original vector");
    
    // Create early increment iterators
    auto begin_early = makeEarlyIncIterator(numbers.begin());
    auto end_early = makeEarlyIncIterator(numbers.end());
    
    std::cout << "Using EarlyIncIterator to traverse the vector:\n";
    for (auto it = begin_early; it != end_early; ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
    
    // Demonstrate the early increment behavior
    std::cout << "\nDemonstrating early increment behavior:\n";
    auto it = makeEarlyIncIterator(numbers.begin());
    std::cout << "Initial value: " << *it << std::endl;
    
    // Post increment returns iterator before increment
    auto copy = it++;
    std::cout << "After post-increment, original iterator: " << *it << std::endl;
    std::cout << "Returned copy: " << *copy << std::endl;
    
    // Pre increment returns reference to incremented iterator
    auto& ref = ++it;
    std::cout << "After pre-increment: " << *it << std::endl;
    std::cout << "Returned reference: " << *ref << " (should be the same)" << std::endl;
}

// Example 3: TransformIterator
void transform_iterator_example() {
    std::cout << "\n=== Example 3: TransformIterator ===\n";
    
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    print_container(numbers, "Original vector");
    
    // Square function
    auto square = [](int n) { return n * n; };
    
    // Create transform iterators that will square each element
    auto begin_transform = makeTransformIterator(numbers.begin(), square);
    auto end_transform = makeTransformIterator(numbers.end(), square);
    
    std::cout << "Squared values using TransformIterator: ";
    for (auto it = begin_transform; it != end_transform; ++it) {
        std::cout << *it << " ";  // Will print squared values
    }
    std::cout << std::endl;
    
    // Transform strings to their lengths
    std::vector<std::string> strings = {"hello", "world", "custom", "iterators", "example"};
    print_container(strings, "Original strings");
    
    auto string_length = [](const std::string& s) { return s.length(); };
    auto begin_length = makeTransformIterator(strings.begin(), string_length);
    auto end_length = makeTransformIterator(strings.end(), string_length);
    
    std::cout << "String lengths using TransformIterator: ";
    for (auto it = begin_length; it != end_length; ++it) {
        std::cout << *it << " ";  // Will print string lengths
    }
    std::cout << std::endl;
    
    // Using transform iterator with structured bindings
    std::map<std::string, int> scores = {
        {"Alice", 95},
        {"Bob", 87},
        {"Charlie", 92},
        {"David", 78},
        {"Eve", 89}
    };
    print_key_value_container(scores, "Original scores");
    
    // Transform to formatted strings
    auto format_score = [](const std::pair<const std::string, int>& p) -> std::string {
        return p.first + ": " + std::to_string(p.second) + " points";
    };
    
    auto begin_format = makeTransformIterator(scores.begin(), format_score);
    auto end_format = makeTransformIterator(scores.end(), format_score);
    
    std::cout << "Formatted scores using TransformIterator:\n";
    for (auto it = begin_format; it != end_format; ++it) {
        std::cout << "  " << *it << std::endl;
    }
}

// Example 4: FilterIterator
void filter_iterator_example() {
    std::cout << "\n=== Example 4: FilterIterator ===\n";
    
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    print_container(numbers, "Original vector");
    
    // Filter for even numbers
    auto is_even = [](int n) { return n % 2 == 0; };
    auto begin_even = makeFilterIterator(numbers.begin(), numbers.end(), is_even);
    auto end_even = makeFilterIterator(numbers.end(), numbers.end(), is_even);
    
    std::cout << "Even numbers using FilterIterator: ";
    for (auto it = begin_even; it != end_even; ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
    
    // Filter for numbers greater than 5
    auto greater_than_5 = [](int n) { return n > 5; };
    auto begin_gt5 = makeFilterIterator(numbers.begin(), numbers.end(), greater_than_5);
    auto end_gt5 = makeFilterIterator(numbers.end(), numbers.end(), greater_than_5);
    
    std::cout << "Numbers > 5 using FilterIterator: ";
    for (auto it = begin_gt5; it != end_gt5; ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
    
    // Filter strings by length
    std::vector<std::string> strings = {"hi", "hello", "a", "world", "cpp", "custom", "iterators"};
    print_container(strings, "Original strings");
    
    auto length_greater_than_3 = [](const std::string& s) { return s.length() > 3; };
    auto begin_str = makeFilterIterator(strings.begin(), strings.end(), length_greater_than_3);
    auto end_str = makeFilterIterator(strings.end(), strings.end(), length_greater_than_3);
    
    std::cout << "Strings longer than 3 characters using FilterIterator: ";
    for (auto it = begin_str; it != end_str; ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
    
    // Filter on a map - only show scores above 90
    std::map<std::string, int> scores = {
        {"Alice", 95},
        {"Bob", 87},
        {"Charlie", 92},
        {"David", 78},
        {"Eve", 89}
    };
    
    auto high_score = [](const std::pair<const std::string, int>& p) { return p.second >= 90; };
    auto begin_high = makeFilterIterator(scores.begin(), scores.end(), high_score);
    auto end_high = makeFilterIterator(scores.end(), scores.end(), high_score);
    
    std::cout << "High scorers (>= 90) using FilterIterator: ";
    for (auto it = begin_high; it != end_high; ++it) {
        std::cout << it->first << "(" << it->second << ") ";
    }
    std::cout << std::endl;
}

// Example 5: ReverseIterator
void reverse_iterator_example() {
    std::cout << "\n=== Example 5: ReverseIterator ===\n";
    
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    print_container(numbers, "Original vector");
    
    // Create reverse iterators
    ReverseIterator<std::vector<int>::iterator> rbegin(numbers.end());
    ReverseIterator<std::vector<int>::iterator> rend(numbers.begin());
    
    std::cout << "Vector traversed in reverse using ReverseIterator: ";
    for (auto it = rbegin; it != rend; ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
    
    // Compare with STL reverse iterator
    std::cout << "Vector traversed with STL reverse_iterator: ";
    for (auto it = numbers.rbegin(); it != numbers.rend(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
    
    // Modify elements using the custom reverse iterator
    std::cout << "Modifying elements using ReverseIterator...\n";
    for (auto it = rbegin; it != rend; ++it) {
        *it += 10;
    }
    print_container(numbers, "Modified vector");
    
    // Get underlying iterator using base()
    std::cout << "Using base() to get the original iterator:\n";
    auto rev_it = rbegin;
    ++rev_it;  // Move to the second element from the end
    auto base_it = rev_it.base();  // Get the forward iterator
    
    std::cout << "Reverse iterator points to: " << *rev_it << std::endl;
    std::cout << "Base iterator points to: " << *(base_it - 1) << std::endl;
}

// Example 6: ZipIterator
void zip_iterator_example() {
    std::cout << "\n=== Example 6: ZipIterator ===\n";
    
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::vector<std::string> names = {"one", "two", "three", "four", "five"};
    std::vector<char> letters = {'a', 'b', 'c', 'd', 'e'};
    
    print_container(numbers, "Numbers");
    print_container(names, "Names");
    print_container(letters, "Letters");
    
    // Create zip iterators for two containers
    auto begin_zip2 = makeZipIterator(numbers.begin(), names.begin());
    auto end_zip2 = makeZipIterator(numbers.end(), names.end());
    
    std::cout << "\nZipping numbers and names:\n";
    for (auto it = begin_zip2; it != end_zip2; ++it) {
        auto [num, name] = *it;  // Unpack the tuple
        std::cout << num << ": " << name << std::endl;
    }
    
    // Create zip iterators for three containers
    auto begin_zip3 = makeZipIterator(numbers.begin(), names.begin(), letters.begin());
    auto end_zip3 = makeZipIterator(numbers.end(), names.end(), letters.end());
    
    std::cout << "\nZipping numbers, names, and letters:\n";
    for (auto it = begin_zip3; it != end_zip3; ++it) {
        auto [num, name, letter] = *it;  // Unpack the tuple
        std::cout << num << ": " << name << " (" << letter << ")" << std::endl;
    }
    
    // Use zip iterator to modify elements
    std::vector<int> vec1 = {1, 2, 3, 4};
    std::vector<int> vec2 = {10, 20, 30, 40};
    
    std::cout << "\nBefore modification:\n";
    print_container(vec1, "Vector 1");
    print_container(vec2, "Vector 2");
    
    auto begin_mod = makeZipIterator(vec1.begin(), vec2.begin());
    auto end_mod = makeZipIterator(vec1.end(), vec2.end());
    
    // Sum corresponding elements from vec2 into vec1
    for (auto it = begin_mod; it != end_mod; ++it) {
        const auto& [v1, v2] = *it;  // Now correctly bound with const reference
                                     // This is just to demonstrate the concept
    }
    
    // The correct way to modify elements is to manually unpack and modify
    for (size_t i = 0; i < vec1.size(); ++i) {
        vec1[i] += vec2[i];
    }
    
    std::cout << "\nAfter modification (vec1 += vec2):\n";
    print_container(vec1, "Vector 1");
    print_container(vec2, "Vector 2");
}

// Example 7: Combining different iterators
void combined_iterators_example() {
    std::cout << "\n=== Example 7: Combining Different Iterators ===\n";
    
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    print_container(numbers, "Original vector");
    
    // 1. Filter for even numbers, then transform to squares
    auto is_even = [](int n) { return n % 2 == 0; };
    auto square = [](int n) { return n * n; };
    
    auto begin_filter = makeFilterIterator(numbers.begin(), numbers.end(), is_even);
    auto end_filter = makeFilterIterator(numbers.end(), numbers.end(), is_even);
    
    auto begin_combined = makeTransformIterator(begin_filter, square);
    auto end_combined = makeTransformIterator(end_filter, square);
    
    std::cout << "Squares of even numbers: ";
    for (auto it = begin_combined; it != end_combined; ++it) {
        std::cout << *it << " ";  // Should print 4, 16, 36, 64, 100
    }
    std::cout << std::endl;
    
    // 2. Create pointers to the elements, then filter by value
    std::cout << "\nPointing to elements greater than 5:\n";
    
    auto [begin_ptr, end_ptr] = makePointerRange(numbers.begin(), numbers.end());
    
    auto value_gt_5 = [](int* ptr) { return *ptr > 5; };
    auto begin_ptr_filter = makeFilterIterator(begin_ptr, end_ptr, value_gt_5);
    auto end_ptr_filter = makeFilterIterator(end_ptr, end_ptr, value_gt_5);
    
    for (auto it = begin_ptr_filter; it != end_ptr_filter; ++it) {
        int* ptr = *it;
        std::cout << "Value: " << *ptr << ", Address: " << ptr << std::endl;
    }
    
    // 3. Combine transform and zip
    std::vector<std::string> names = {"Alice", "Bob", "Charlie", "David", "Eve"};
    std::vector<int> ages = {25, 30, 35, 40, 45};
    
    auto name_to_length = [](const std::string& s) { return s.length(); };
    auto begin_name_len = makeTransformIterator(names.begin(), name_to_length);
    auto end_name_len = makeTransformIterator(names.end(), name_to_length);
    
    auto begin_combined_zip = makeZipIterator(begin_name_len, ages.begin());
    auto end_combined_zip = makeZipIterator(end_name_len, ages.end());
    
    std::cout << "\nName lengths paired with ages:\n";
    for (auto it = begin_combined_zip; it != end_combined_zip; ++it) {
        auto [length, age] = *it;
        std::cout << "Name length: " << length << ", Age: " << age << std::endl;
    }
}

int main() {
    std::cout << "===== Custom Iterator Examples =====\n";
    
    pointer_iterator_example();
    early_inc_iterator_example();
    transform_iterator_example();
    filter_iterator_example();
    reverse_iterator_example();
    zip_iterator_example();
    combined_iterators_example();
    
    return 0;
}