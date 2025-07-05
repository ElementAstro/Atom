/**
 * @file small_list_example.cpp
 * @brief Comprehensive examples demonstrating the SmallList class
 *
 * This file showcases all features of the SmallList template class including
 * constructors, element access, modifiers, iterators, and more.
 */

#include "atom/type/small_list.hpp"
#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <list>
#include <numeric>
#include <string>
#include <vector>

using namespace atom::type;

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper function to display SmallList contents
template <typename T>
void printList(const SmallList<T>& list, const std::string& name) {
    std::cout << name << " (size=" << list.size() << "): [";
    bool first = true;
    for (const auto& item : list) {
        if (!first) {
            std::cout << ", ";
        }
        std::cout << item;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// Helper function to measure execution time
template <typename Func>
double measureTime(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count();
}

// Custom class for testing
class Person {
public:
    Person() : name_("Unnamed"), age_(0) {}

    Person(std::string name, int age) : name_(std::move(name)), age_(age) {}

    std::string getName() const { return name_; }
    int getAge() const { return age_; }

    void setName(const std::string& name) { name_ = name; }
    void setAge(int age) { age_ = age; }

    bool operator==(const Person& other) const {
        return name_ == other.name_ && age_ == other.age_;
    }

    bool operator!=(const Person& other) const { return !(*this == other); }

    bool operator<(const Person& other) const {
        return std::tie(name_, age_) < std::tie(other.name_, other.age_);
    }

    friend std::ostream& operator<<(std::ostream& os, const Person& person) {
        return os << "{" << person.name_ << ", " << person.age_ << "}";
    }

private:
    std::string name_;
    int age_;
};

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  SmallList Class Demonstration" << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        // Example 1: Constructors and Basic Operations
        printSection("1. Constructors and Basic Operations");

        // Default constructor
        printSubsection("Default Constructor");
        SmallList<int> empty_list;
        printList(empty_list, "empty_list");
        std::cout << "Is empty: " << (empty_list.empty() ? "true" : "false")
                  << std::endl;

        // Initializer list constructor
        printSubsection("Initializer List Constructor");
        SmallList<int> int_list = {1, 2, 3, 4, 5};
        printList(int_list, "int_list");
        std::cout << "Size: " << int_list.size() << std::endl;
        std::cout << "Is empty: " << (int_list.empty() ? "true" : "false")
                  << std::endl;

        // Copy constructor
        printSubsection("Copy Constructor");
        SmallList<int> copy_list(int_list);
        printList(copy_list, "copy_list");

        // Move constructor
        printSubsection("Move Constructor");
        SmallList<int> move_source = {10, 20, 30, 40, 50};
        SmallList<int> move_list(std::move(move_source));
        printList(move_list, "move_list");
        printList(move_source, "move_source after move");  // Should be empty

        // Example 2: Element Access
        printSection("2. Element Access");

        SmallList<int> access_list = {100, 200, 300, 400, 500};

        // front() and back()
        printSubsection("front() and back()");
        std::cout << "Front element: " << access_list.front() << std::endl;
        std::cout << "Back element: " << access_list.back() << std::endl;

        // Try to access elements in an empty list
        printSubsection("Element Access on Empty List");
        SmallList<int> empty_access_list;
        try {
            int front_val = empty_access_list.front();
            std::cout << "This should not print: " << front_val << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // tryFront() and tryBack()
        printSubsection("tryFront() and tryBack()");
        auto optional_front = access_list.tryFront();
        if (optional_front) {
            std::cout << "tryFront value: " << optional_front->get()
                      << std::endl;
        } else {
            std::cout << "Front element not available" << std::endl;
        }

        auto optional_back = access_list.tryBack();
        if (optional_back) {
            std::cout << "tryBack value: " << optional_back->get() << std::endl;
        } else {
            std::cout << "Back element not available" << std::endl;
        }

        // Try safe access on empty list
        auto empty_front = empty_access_list.tryFront();
        std::cout << "tryFront on empty list has value: "
                  << (empty_front.has_value() ? "true" : "false") << std::endl;

        // Example 3: Modifiers
        printSection("3. Modifiers");

        // pushBack
        printSubsection("pushBack()");
        SmallList<std::string> string_list;
        string_list.pushBack("apple");
        string_list.pushBack("banana");
        string_list.pushBack("cherry");
        printList(string_list, "After pushBack");

        // pushFront
        printSubsection("pushFront()");
        string_list.pushFront("orange");
        string_list.pushFront("grape");
        printList(string_list, "After pushFront");

        // popBack
        printSubsection("popBack()");
        string_list.popBack();
        printList(string_list, "After popBack");

        // popFront
        printSubsection("popFront()");
        string_list.popFront();
        printList(string_list, "After popFront");

        // insert
        printSubsection("insert()");
        auto it = string_list.begin();
        std::advance(it, 1);  // Move to the second element
        string_list.insert(it, "kiwi");
        printList(string_list, "After insert at position 1");

        // insert at beginning
        string_list.insert(string_list.begin(), "pineapple");
        printList(string_list, "After insert at beginning");

        // insert at end
        string_list.insert(string_list.end(), "mango");
        printList(string_list, "After insert at end");

        // erase
        printSubsection("erase()");
        it = string_list.begin();
        std::advance(it, 2);
        it = string_list.erase(it);
        printList(string_list, "After erase at position 2");
        std::cout << "Iterator after erase points to: " << *it << std::endl;

        // clear
        printSubsection("clear()");
        SmallList<std::string> clear_list = {"one", "two", "three"};
        std::cout << "Before clear, size: " << clear_list.size() << std::endl;
        clear_list.clear();
        std::cout << "After clear, size: " << clear_list.size() << std::endl;
        std::cout << "Is empty: " << (clear_list.empty() ? "true" : "false")
                  << std::endl;

        // Example 4: Iterators
        printSection("4. Iterators");

        SmallList<int> iter_list = {10, 20, 30, 40, 50};

        // begin() and end()
        printSubsection("begin() and end()");
        std::cout << "Elements using begin/end: ";
        for (auto it = iter_list.begin(); it != iter_list.end(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // Range-based for loop
        printSubsection("Range-based for loop");
        std::cout << "Elements using range-based for: ";
        for (const auto& val : iter_list) {
            std::cout << val << " ";
        }
        std::cout << std::endl;

        // cbegin() and cend()
        printSubsection("cbegin() and cend()");
        std::cout << "Elements using cbegin/cend: ";
        for (auto it = iter_list.cbegin(); it != iter_list.cend(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // Reverse iterators
        printSubsection("Reverse Iterators");
        std::cout << "Elements using rbegin/rend: ";
        for (auto it = iter_list.rbegin(); it != iter_list.rend(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // Const reverse iterators
        printSubsection("Const Reverse Iterators");
        std::cout << "Elements using crbegin/crend: ";
        for (auto it = iter_list.crbegin(); it != iter_list.crend(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // Iterator arithmetic and comparison
        printSubsection("Iterator Operations");
        auto first = iter_list.begin();
        auto second = iter_list.begin();
        std::advance(second, 2);

        std::cout << "First iterator points to: " << *first << std::endl;
        std::cout << "Second iterator points to: " << *second << std::endl;
        std::cout << "first == second: " << (first == second ? "true" : "false")
                  << std::endl;
        std::cout << "first != second: " << (first != second ? "true" : "false")
                  << std::endl;

        // Move iterators
        printSubsection("Moving Iterators");
        auto movable = iter_list.begin();
        std::cout << "Initial: " << *movable << std::endl;

        ++movable;
        std::cout << "After ++: " << *movable << std::endl;

        movable++;
        std::cout << "After ++: " << *movable << std::endl;

        --movable;
        std::cout << "After --: " << *movable << std::endl;

        movable--;
        std::cout << "After --: " << *movable << std::endl;

        // Accessing through iterators
        printSubsection("Accessing Members Through Iterators");
        SmallList<Person> person_list;
        person_list.pushBack(Person("Alice", 30));
        person_list.pushBack(Person("Bob", 25));
        person_list.pushBack(Person("Charlie", 35));

        std::cout << "Person names: ";
        for (auto it = person_list.begin(); it != person_list.end(); ++it) {
            std::cout << it->getName() << " ";
        }
        std::cout << std::endl;

        // Example 5: List Operations
        printSection("5. List Operations");

        // remove
        printSubsection("remove()");
        SmallList<int> remove_list = {1, 2, 3, 2, 5, 2, 7};
        printList(remove_list, "Before remove");
        size_t removed = remove_list.remove(2);
        printList(remove_list, "After removing value 2");
        std::cout << "Number of elements removed: " << removed << std::endl;

        // removeIf
        printSubsection("removeIf()");
        SmallList<int> remove_if_list = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        printList(remove_if_list, "Before removeIf");
        removed = remove_if_list.removeIf([](int n) { return n % 2 == 0; });
        printList(remove_if_list, "After removing even numbers");
        std::cout << "Number of elements removed: " << removed << std::endl;

        // unique
        printSubsection("unique()");
        SmallList<int> unique_list = {1, 1, 2, 2, 2, 3, 3, 1, 1, 4, 5, 5};
        printList(unique_list, "Before unique");
        size_t uniqued = unique_list.unique();
        printList(unique_list, "After unique");
        std::cout << "Number of duplicates removed: " << uniqued << std::endl;

        // sort
        printSubsection("sort()");
        SmallList<int> sort_list = {5, 3, 9, 1, 7, 2, 8, 4, 6};
        printList(sort_list, "Before sort");
        sort_list.sort();
        printList(sort_list, "After sort");

        // Custom sort
        printSubsection("sort() with custom comparator");
        SmallList<Person> sort_person_list;
        sort_person_list.pushBack(Person("Dave", 40));
        sort_person_list.pushBack(Person("Alice", 30));
        sort_person_list.pushBack(Person("Charlie", 35));
        sort_person_list.pushBack(Person("Bob", 25));

        std::cout << "Before sort:" << std::endl;
        for (const auto& person : sort_person_list) {
            std::cout << "  " << person << std::endl;
        }

        // Sort by age
        sort_person_list.sort([](const Person& a, const Person& b) {
            return a.getAge() < b.getAge();
        });

        std::cout << "After sort by age:" << std::endl;
        for (const auto& person : sort_person_list) {
            std::cout << "  " << person << std::endl;
        }

        // isSorted
        printSubsection("isSorted()");
        std::cout << "Is sort_list sorted? "
                  << (sort_list.isSorted() ? "Yes" : "No") << std::endl;

        SmallList<int> unsorted_list = {1, 3, 2, 5, 4};
        std::cout << "Is unsorted_list sorted? "
                  << (unsorted_list.isSorted() ? "Yes" : "No") << std::endl;

        // merge
        printSubsection("merge()");
        SmallList<int> merge_list1 = {1, 3, 5, 7, 9};
        SmallList<int> merge_list2 = {2, 4, 6, 8, 10};

        printList(merge_list1, "merge_list1 before merge");
        printList(merge_list2, "merge_list2 before merge");

        merge_list1.merge(merge_list2);

        printList(merge_list1, "merge_list1 after merge");
        printList(merge_list2, "merge_list2 after merge");  // Should be empty

        // Try to merge unsorted lists
        printSubsection("merge() with unsorted lists");
        SmallList<int> unsorted_merge1 = {5, 3, 1};
        SmallList<int> unsorted_merge2 = {6, 4, 2};

        try {
            unsorted_merge1.merge(unsorted_merge2);
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // reverse
        printSubsection("reverse()");
        SmallList<int> reverse_list = {1, 2, 3, 4, 5};
        printList(reverse_list, "Before reverse");
        reverse_list.reverse();
        printList(reverse_list, "After reverse");

        // splice
        printSubsection("splice()");
        SmallList<std::string> splice_dest = {"one", "two", "five"};
        SmallList<std::string> splice_src = {"three", "four"};

        printList(splice_dest, "splice_dest before splice");
        printList(splice_src, "splice_src before splice");

        auto splice_pos = splice_dest.begin();
        std::advance(splice_pos, 2);  // Position before "five"

        splice_dest.splice(splice_pos, splice_src);

        printList(splice_dest, "splice_dest after splice");
        printList(splice_src, "splice_src after splice");  // Should be empty

        // Example 6: Resize Operations
        printSection("6. Resize Operations");

        // resize (smaller)
        printSubsection("resize() - shrink");
        SmallList<int> resize_list = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        printList(resize_list, "Before resize to 5");
        resize_list.resize(5);
        printList(resize_list, "After resize to 5");

        // resize (larger)
        printSubsection("resize() - grow with default values");
        printList(resize_list, "Before resize to 8");
        resize_list.resize(8);
        printList(resize_list, "After resize to 8");

        // resize with value
        printSubsection("resize() - grow with specific value");
        SmallList<int> resize_value_list = {1, 2, 3};
        printList(resize_value_list, "Before resize to 6 with value 42");
        resize_value_list.resize(6, 42);
        printList(resize_value_list, "After resize to 6 with value 42");

        // Example 7: Emplace Operations
        printSection("7. Emplace Operations");

        // emplaceBack
        printSubsection("emplaceBack()");
        SmallList<Person> emplace_list;
        emplace_list.emplaceBack("Emily", 28);
        emplace_list.emplaceBack("Frank", 32);

        std::cout << "After emplaceBack:" << std::endl;
        for (const auto& person : emplace_list) {
            std::cout << "  " << person << std::endl;
        }

        // emplaceFront
        printSubsection("emplaceFront()");
        emplace_list.emplaceFront("Diana", 23);
        emplace_list.emplaceFront("George", 45);

        std::cout << "After emplaceFront:" << std::endl;
        for (const auto& person : emplace_list) {
            std::cout << "  " << person << std::endl;
        }

        // emplace
        printSubsection("emplace()");
        auto emplace_it = emplace_list.begin();
        std::advance(emplace_it, 2);
        emplace_list.emplace(emplace_it, "Hannah", 31);

        std::cout << "After emplace at position 2:" << std::endl;
        for (const auto& person : emplace_list) {
            std::cout << "  " << person << std::endl;
        }

        // Example 8: Comparison Operations
        printSection("8. Comparison Operations");

        SmallList<int> compare_list1 = {1, 2, 3, 4, 5};
        SmallList<int> compare_list2 = {1, 2, 3, 4, 5};
        SmallList<int> compare_list3 = {1, 2, 3, 4, 6};
        SmallList<int> compare_list4 = {1, 2, 3};

        std::cout << "compare_list1 == compare_list2: "
                  << (compare_list1 == compare_list2 ? "true" : "false")
                  << std::endl;
        std::cout << "compare_list1 != compare_list3: "
                  << (compare_list1 != compare_list3 ? "true" : "false")
                  << std::endl;
        std::cout << "compare_list1 < compare_list3: "
                  << (compare_list1 < compare_list3 ? "true" : "false")
                  << std::endl;
        std::cout << "compare_list3 > compare_list1: "
                  << (compare_list3 > compare_list1 ? "true" : "false")
                  << std::endl;
        std::cout << "compare_list1 <= compare_list2: "
                  << (compare_list1 <= compare_list2 ? "true" : "false")
                  << std::endl;
        std::cout << "compare_list4 <= compare_list1: "
                  << (compare_list4 <= compare_list1 ? "true" : "false")
                  << std::endl;
        std::cout << "compare_list1 >= compare_list4: "
                  << (compare_list1 >= compare_list4 ? "true" : "false")
                  << std::endl;

        // Example 9: Swap Operations
        printSection("9. Swap Operations");

        SmallList<int> swap_list1 = {1, 2, 3};
        SmallList<int> swap_list2 = {4, 5, 6, 7};

        printList(swap_list1, "swap_list1 before swap");
        printList(swap_list2, "swap_list2 before swap");

        // Member swap
        printSubsection("Member swap()");
        swap_list1.swap(swap_list2);

        printList(swap_list1, "swap_list1 after member swap");
        printList(swap_list2, "swap_list2 after member swap");

        // Non-member swap
        printSubsection("Non-member swap()");
        swap(swap_list1, swap_list2);

        printList(swap_list1, "swap_list1 after non-member swap");
        printList(swap_list2, "swap_list2 after non-member swap");

        // Example 10: Performance Comparison
        printSection("10. Performance Comparison");

        const int num_elements = 10000;
        const int num_operations = 1000;

        // Initialize data
        std::vector<int> data;
        data.reserve(num_elements);
        for (int i = 0; i < num_elements; ++i) {
            data.push_back(i);
        }

        // Test SmallList
        printSubsection("SmallList Performance");

        // Insertion
        double smalllist_insert_time = measureTime([&]() {
            SmallList<int> test_list;
            for (int i = 0; i < num_elements; ++i) {
                test_list.pushBack(i);
            }
        });
        std::cout << "SmallList insertion time: " << smalllist_insert_time
                  << " µs" << std::endl;

        // Create list for further testing
        SmallList<int> perf_smalllist;
        for (int i = 0; i < num_elements; ++i) {
            perf_smalllist.pushBack(i);
        }

        // Random access
        double smalllist_access_time = measureTime([&]() {
            int sum = 0;
            auto it = perf_smalllist.begin();
            for (int i = 0; i < num_operations; ++i) {
                std::advance(it, num_elements / num_operations);
                if (it == perf_smalllist.end())
                    it = perf_smalllist.begin();
                sum += *it;
            }
        });
        std::cout << "SmallList random access time: " << smalllist_access_time
                  << " µs" << std::endl;

        // Sorting
        double smalllist_sort_time = measureTime([&]() {
            SmallList<int> sort_test;
            // Add elements in reverse order
            for (int i = num_elements - 1; i >= 0; --i) {
                sort_test.pushBack(i);
            }
            sort_test.sort();
        });
        std::cout << "SmallList sort time: " << smalllist_sort_time << " µs"
                  << std::endl;

        // Test STL std::list for comparison
        printSubsection("std::list Performance");

        // Insertion
        double stdlist_insert_time = measureTime([&]() {
            std::list<int> test_list;
            for (int i = 0; i < num_elements; ++i) {
                test_list.push_back(i);
            }
        });
        std::cout << "std::list insertion time: " << stdlist_insert_time
                  << " µs" << std::endl;

        // Create list for further testing
        std::list<int> perf_stdlist;
        for (int i = 0; i < num_elements; ++i) {
            perf_stdlist.push_back(i);
        }

        // Random access
        double stdlist_access_time = measureTime([&]() {
            int sum = 0;
            auto it = perf_stdlist.begin();
            for (int i = 0; i < num_operations; ++i) {
                std::advance(it, num_elements / num_operations);
                if (it == perf_stdlist.end())
                    it = perf_stdlist.begin();
                sum += *it;
            }
        });
        std::cout << "std::list random access time: " << stdlist_access_time
                  << " µs" << std::endl;

        // Sorting
        double stdlist_sort_time = measureTime([&]() {
            std::list<int> sort_test;
            // Add elements in reverse order
            for (int i = num_elements - 1; i >= 0; --i) {
                sort_test.push_back(i);
            }
            sort_test.sort();
        });
        std::cout << "std::list sort time: " << stdlist_sort_time << " µs"
                  << std::endl;

        // Comparison
        printSubsection("Performance Comparison");
        std::cout << "SmallList vs std::list insertion ratio: "
                  << (smalllist_insert_time / stdlist_insert_time) << std::endl;
        std::cout << "SmallList vs std::list access ratio: "
                  << (smalllist_access_time / stdlist_access_time) << std::endl;
        std::cout << "SmallList vs std::list sort ratio: "
                  << (smalllist_sort_time / stdlist_sort_time) << std::endl;

        // Example 11: Edge Cases and Error Handling
        printSection("11. Edge Cases and Error Handling");

        // Operations on empty lists
        printSubsection("Operations on Empty Lists");
        SmallList<int> empty_list_ops;

        try {
            std::cout << "Trying to access front() of empty list..."
                      << std::endl;
            int val = empty_list_ops.front();
            std::cout << "This should not print: " << val << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Exception caught: " << e.what() << std::endl;
        }

        try {
            std::cout << "Trying to access back() of empty list..."
                      << std::endl;
            int val = empty_list_ops.back();
            std::cout << "This should not print: " << val << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Exception caught: " << e.what() << std::endl;
        }

        try {
            std::cout << "Trying to call popBack() on empty list..."
                      << std::endl;
            empty_list_ops.popBack();
            std::cout << "This should not print!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Exception caught: " << e.what() << std::endl;
        }

        // Invalid iterator operations
        printSubsection("Invalid Iterator Operations");
        SmallList<int> iter_ops_list = {1, 2, 3};

        try {
            std::cout << "Trying to dereference end iterator..." << std::endl;
            int val = *(iter_ops_list.end());
            std::cout << "This should not print: " << val << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Exception caught: " << e.what() << std::endl;
        }

        try {
            std::cout << "Trying to decrement begin iterator..." << std::endl;
            auto it = iter_ops_list.begin();
            --it;
            std::cout << "This should not print!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Exception caught: " << e.what() << std::endl;
        }

        // Self-assignment and self-swap
        printSubsection("Self-operations");
        SmallList<int> self_ops_list = {1, 2, 3};
        printList(self_ops_list, "Before self-assignment");

        self_ops_list = self_ops_list;
        printList(self_ops_list, "After self-assignment");

        self_ops_list.swap(self_ops_list);
        printList(self_ops_list, "After self-swap");

        // Example 12: Additional Operations
        printSection("12. Additional Operations");

        // Using with standard algorithms
        printSubsection("Using with Standard Algorithms");
        SmallList<int> algo_list = {9, 1, 8, 2, 7, 3, 6, 4, 5};

        // Find
        auto find_it = std::find(algo_list.begin(), algo_list.end(), 7);
        if (find_it != algo_list.end()) {
            std::cout << "Found value 7 in the list" << std::endl;
        }

        // Count
        int count = std::count_if(algo_list.begin(), algo_list.end(),
                                  [](int n) { return n > 5; });
        std::cout << "Number of elements > 5: " << count << std::endl;

        // Transform in-place
        std::transform(algo_list.begin(), algo_list.end(), algo_list.begin(),
                       [](int n) { return n * 2; });
        printList(algo_list, "After doubling all elements");

        // Accumulate
        int sum = std::accumulate(algo_list.begin(), algo_list.end(), 0);
        std::cout << "Sum of elements: " << sum << std::endl;

        // Complex example with strings
        printSubsection("Complex String Operations");
        SmallList<std::string> words = {"apple", "banana", "cherry", "date",
                                        "elderberry"};

        // Find longest word
        auto longest =
            std::max_element(words.begin(), words.end(),
                             [](const std::string& a, const std::string& b) {
                                 return a.length() < b.length();
                             });
        std::cout << "Longest word: " << *longest << std::endl;

        // Count total characters
        int total_chars = std::accumulate(
            words.begin(), words.end(), 0,
            [](int sum, const std::string& s) { return sum + s.length(); });
        std::cout << "Total characters: " << total_chars << std::endl;

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
