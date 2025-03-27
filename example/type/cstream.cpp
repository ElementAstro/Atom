#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "atom/type/cstream.hpp"

// Helper function to print a section header
void printHeader(const std::string& title) {
    std::cout << "\n==============================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "===============================================" << std::endl;
}

// Helper function to print a vector
template <typename T>
void printVector(const std::vector<T>& vec, const std::string& label) {
    std::cout << label << ": [";
    bool first = true;
    for (const auto& item : vec) {
        if (!first)
            std::cout << ", ";
        std::cout << item;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// Helper function to print a list
template <typename T>
void printList(const std::list<T>& lst, const std::string& label) {
    std::cout << label << ": [";
    bool first = true;
    for (const auto& item : lst) {
        if (!first)
            std::cout << ", ";
        std::cout << item;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// Person class for demonstrating complex object handling
class Person {
private:
    std::string name;
    int age;
    std::string department;

public:
    Person() : name(""), age(0), department("") {}

    Person(std::string name, int age, std::string department)
        : name(std::move(name)), age(age), department(std::move(department)) {}

    // Getters
    const std::string& getName() const { return name; }
    int getAge() const { return age; }
    const std::string& getDepartment() const { return department; }

    // For sorting and comparisons
    bool operator==(const Person& other) const {
        return name == other.name && age == other.age &&
               department == other.department;
    }

    bool operator<(const Person& other) const {
        if (name != other.name)
            return name < other.name;
        if (age != other.age)
            return age < other.age;
        return department < other.department;
    }

    // For printing
    friend std::ostream& operator<<(std::ostream& os, const Person& person) {
        os << "Person{name='" << person.name << "', age=" << person.age
           << ", department='" << person.department << "'}";
        return os;
    }
};

int main() {
    std::cout << "======================================================="
              << std::endl;
    std::cout << "     COMPREHENSIVE CSTREAM USAGE EXAMPLE     " << std::endl;
    std::cout << "======================================================="
              << std::endl;

    // ============================================================
    // 1. Basic Usage with Different Container Types
    // ============================================================
    printHeader("1. BASIC USAGE WITH DIFFERENT CONTAINER TYPES");

    // Using cstream with std::vector
    std::vector<int> numbers = {5, 2, 8, 1, 7, 3, 9, 4, 6, 10};
    std::cout << "Original vector:" << std::endl;
    printVector(numbers, "numbers");

    // Getting a reference to use with cstream
    auto numbersStream = atom::type::makeStream(numbers);
    std::cout << "\nUsing cstream with std::vector:" << std::endl;
    std::cout << "Size: " << numbersStream.size() << std::endl;

    // Using cstream with std::list
    std::list<std::string> words = {"apple", "banana", "cherry", "date",
                                    "elderberry"};
    std::cout << "\nOriginal list:" << std::endl;
    printList(words, "words");

    auto wordsStream = atom::type::makeStream(words);
    std::cout << "\nUsing cstream with std::list:" << std::endl;
    std::cout << "Size: " << wordsStream.size() << std::endl;

    // Using makeStreamCopy to operate on a copy
    auto numbersCopyStream = atom::type::makeStreamCopy(numbers);
    std::cout << "\nUsing makeStreamCopy to create a copy:" << std::endl;
    numbersCopyStream.sorted();
    printVector(numbersCopyStream.getRef(), "Sorted copy");
    printVector(numbers, "Original (unchanged)");

    // Creating stream from rvalue
    auto rvalueStream =
        atom::type::makeStream(std::vector<int>{100, 200, 300, 400, 500});
    std::cout << "\nStream from rvalue:" << std::endl;
    printVector(rvalueStream.getRef(), "rvalueStream");

    // Using cpstream with C-style array
    int cArray[] = {11, 22, 33, 44, 55};
    auto cArrayStream = atom::type::cpstream<int>(cArray, 5);
    std::cout << "\nStream from C-style array:" << std::endl;
    printVector(cArrayStream.getRef(), "cArrayStream");

    // ============================================================
    // 2. Sorting and Transformation Operations
    // ============================================================
    printHeader("2. SORTING AND TRANSFORMATION OPERATIONS");

    // Sort the vector
    auto sortedNumbers = atom::type::makeStream(numbers).sorted();
    std::cout << "Sorted in ascending order:" << std::endl;
    printVector(sortedNumbers.getRef(), "sortedNumbers");

    // Sort with custom comparator (descending order)
    auto descendingNumbers =
        atom::type::makeStream(numbers).sorted(std::greater<int>());
    std::cout << "\nSorted in descending order:" << std::endl;
    printVector(descendingNumbers.getRef(), "descendingNumbers");

    // Transform to string
    auto stringNumbers = numbersStream.transform<std::vector<std::string>>(
        [](int n) { return "Num" + std::to_string(n); });
    std::cout << "\nTransformed to strings:" << std::endl;
    printVector(stringNumbers.getRef(), "stringNumbers");

    // Transform to double (multiply by 1.5)
    auto doubledNumbers = numbersStream.transform<std::vector<double>>(
        [](int n) { return n * 1.5; });
    std::cout << "\nTransformed to doubles (multiplied by 1.5):" << std::endl;
    printVector(doubledNumbers.getRef(), "doubledNumbers");

    // ============================================================
    // 3. Filtering and Removing Elements
    // ============================================================
    printHeader("3. FILTERING AND REMOVING ELEMENTS");

    // Filter even numbers
    auto originalForFilter = std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto filteredEven =
        atom::type::makeStream(originalForFilter).filter([](int n) {
            return n % 2 == 0;
        });
    std::cout << "Filtered even numbers:" << std::endl;
    printVector(filteredEven.getRef(), "filteredEven");

    // Using cpFilter to create a copy and filter
    auto copyFiltered =
        atom::type::makeStream(numbers).cpFilter([](int n) { return n > 5; });
    std::cout << "\nCopy-filtered numbers > 5:" << std::endl;
    printVector(copyFiltered.getRef(), "copyFiltered");
    printVector(numbers, "Original (unchanged)");

    // Remove elements
    auto removeResult =
        atom::type::makeStream(originalForFilter).remove([](int n) {
            return n % 3 == 0;
        });
    std::cout << "\nRemoved numbers divisible by 3:" << std::endl;
    printVector(removeResult.getRef(), "removeResult");

    // ============================================================
    // 4. Aggregation Operations
    // ============================================================
    printHeader("4. AGGREGATION OPERATIONS");

    // Calculate sum
    int sum = numbersStream.accumulate(0);
    std::cout << "Sum of all numbers: " << sum << std::endl;

    // Calculate product
    int product = numbersStream.accumulate(1, std::multiplies<int>());
    std::cout << "Product of all numbers: " << product << std::endl;

    // Find minimum and maximum
    int minValue = numbersStream.min();
    int maxValue = numbersStream.max();
    std::cout << "Minimum value: " << minValue << std::endl;
    std::cout << "Maximum value: " << maxValue << std::endl;

    // Calculate mean
    double mean = numbersStream.mean();
    std::cout << "Mean value: " << std::fixed << std::setprecision(2) << mean
              << std::endl;

    // ============================================================
    // 5. Checking Operations (all, any, none)
    // ============================================================
    printHeader("5. CHECKING OPERATIONS");

    // Check all
    bool allPositive = numbersStream.all([](int n) { return n > 0; });
    std::cout << "All numbers are positive: " << (allPositive ? "Yes" : "No")
              << std::endl;

    bool allEven = numbersStream.all([](int n) { return n % 2 == 0; });
    std::cout << "All numbers are even: " << (allEven ? "Yes" : "No")
              << std::endl;

    // Check any
    bool anyGreaterThan8 = numbersStream.any([](int n) { return n > 8; });
    std::cout << "Any number greater than 8: "
              << (anyGreaterThan8 ? "Yes" : "No") << std::endl;

    bool anyNegative = numbersStream.any([](int n) { return n < 0; });
    std::cout << "Any negative numbers: " << (anyNegative ? "Yes" : "No")
              << std::endl;

    // Check none
    bool noneNegative = numbersStream.none([](int n) { return n < 0; });
    std::cout << "None of the numbers are negative: "
              << (noneNegative ? "Yes" : "No") << std::endl;

    bool noneGreaterThan10 = numbersStream.none([](int n) { return n > 10; });
    std::cout << "None of the numbers are greater than 10: "
              << (noneGreaterThan10 ? "Yes" : "No") << std::endl;

    // ============================================================
    // 6. First Element and Contains Operations
    // ============================================================
    printHeader("6. FIRST ELEMENT AND CONTAINS OPERATIONS");

    // Get the first element
    auto firstElement = numbersStream.first();
    std::cout << "First element: "
              << (firstElement ? std::to_string(*firstElement) : "None")
              << std::endl;

    // Get the first element matching a predicate
    auto firstEven = numbersStream.first([](int n) { return n % 2 == 0; });
    std::cout << "First even number: "
              << (firstEven ? std::to_string(*firstEven) : "None") << std::endl;

    auto firstNegative = numbersStream.first([](int n) { return n < 0; });
    std::cout << "First negative number: "
              << (firstNegative ? std::to_string(*firstNegative) : "None")
              << std::endl;

    // Check if contains
    bool contains7 = numbersStream.contains(7);
    std::cout << "Contains 7: " << (contains7 ? "Yes" : "No") << std::endl;

    bool contains100 = numbersStream.contains(100);
    std::cout << "Contains 100: " << (contains100 ? "Yes" : "No") << std::endl;

    // ============================================================
    // 7. Counting Operations
    // ============================================================
    printHeader("7. COUNTING OPERATIONS");

    // Count occurrences
    auto duplicateNumbers = std::vector<int>{1, 2, 3, 3, 4, 4, 4, 5, 5, 1};
    auto dupStream = atom::type::makeStream(duplicateNumbers);

    std::cout << "Duplicate numbers vector: ";
    printVector(duplicateNumbers, "duplicateNumbers");

    size_t count1 = dupStream.count(1);
    std::cout << "Count of 1: " << count1 << std::endl;

    size_t count3 = dupStream.count(3);
    std::cout << "Count of 3: " << count3 << std::endl;

    // Count matching predicate
    size_t countEven = dupStream.count([](int n) { return n % 2 == 0; });
    std::cout << "Count of even numbers: " << countEven << std::endl;

    size_t countGreaterThan3 = dupStream.count([](int n) { return n > 3; });
    std::cout << "Count of numbers greater than 3: " << countGreaterThan3
              << std::endl;

    // ============================================================
    // 8. ForEach Operation
    // ============================================================
    printHeader("8. FOREACH OPERATION");

    // Modify elements in-place
    auto forEachNumbers = std::vector<int>{1, 2, 3, 4, 5};
    std::cout << "Original numbers: ";
    printVector(forEachNumbers, "forEachNumbers");

    atom::type::makeStream(forEachNumbers).forEach([](int& n) { n *= 2; });

    std::cout << "After forEach (doubled): ";
    printVector(forEachNumbers, "forEachNumbers");

    // Print each element
    std::cout << "\nPrinting each element:" << std::endl;
    atom::type::makeStream(forEachNumbers).forEach([](int n) {
        std::cout << "  Element: " << n << std::endl;
    });

    // ============================================================
    // 9. Map and FlatMap Operations
    // ============================================================
    printHeader("9. MAP AND FLATMAP OPERATIONS");

    // Map operation
    auto mappedNumbers =
        atom::type::makeStream(numbers).map([](int n) { return n * n; });
    std::cout << "Mapped numbers (squared): ";
    printVector(mappedNumbers.getRef(), "mappedNumbers");

    // FlatMap operation
    auto flatMapInput = std::vector<int>{1, 2, 3};
    auto flatMapped = atom::type::makeStream(flatMapInput)
                          .flatMap([](int n) -> std::vector<int> {
                              return {n, n * 10, n * 100};
                          });

    std::cout << "\nFlatMap input: ";
    printVector(flatMapInput, "flatMapInput");
    std::cout << "FlatMapped output: ";
    printVector(flatMapped.getRef(), "flatMapped");

    // ============================================================
    // 10. Distinct and Reverse Operations
    // ============================================================
    printHeader("10. DISTINCT AND REVERSE OPERATIONS");

    // Distinct operation
    auto duplicatesForDistinct = std::vector<int>{1, 2, 3, 3, 4, 4, 4, 5, 5, 1};
    std::cout << "Original with duplicates: ";
    printVector(duplicatesForDistinct, "duplicatesForDistinct");

    auto distinctNumbers =
        atom::type::makeStream(duplicatesForDistinct).distinct();
    std::cout << "After distinct: ";
    printVector(distinctNumbers.getRef(), "distinctNumbers");

    // Reverse operation
    auto numbersToReverse = std::vector<int>{1, 2, 3, 4, 5};
    std::cout << "\nOriginal numbers: ";
    printVector(numbersToReverse, "numbersToReverse");

    auto reversedNumbers = atom::type::makeStream(numbersToReverse).reverse();
    std::cout << "After reverse: ";
    printVector(reversedNumbers.getRef(), "reversedNumbers");

    // ============================================================
    // 11. Chain Operations (Method Chaining)
    // ============================================================
    printHeader("11. CHAIN OPERATIONS (METHOD CHAINING)");

    auto initialData = std::vector<int>{9, 2, 8, 1, 7, 3, 9, 4, 6, 10, 5, 8};
    std::cout << "Initial data: ";
    printVector(initialData, "initialData");

    // Complex chaining example
    auto result =
        atom::type::makeStream(initialData)
            .distinct()                                // Remove duplicates
            .filter([](int n) { return n % 2 == 0; })  // Keep only even numbers
            .sorted()                                  // Sort ascending
            .map([](int n) { return n * 2; });         // Double each value

    std::cout << "After chaining operations: ";
    printVector(result.getRef(), "result");

    // Another chaining example
    auto chainResult =
        atom::type::makeStream(initialData)
            .filter([](int n) { return n > 5; })  // Keep numbers > 5
            .sorted(std::greater<int>())          // Sort descending
            .map([](int n) { return n - 1; })     // Subtract 1
            .distinct();                          // Remove duplicates

    std::cout << "Another chaining example: ";
    printVector(chainResult.getRef(), "chainResult");

    // ============================================================
    // 12. Working with Complex Objects
    // ============================================================
    printHeader("12. WORKING WITH COMPLEX OBJECTS");

    // Create a vector of Person objects
    std::vector<Person> people = {
        Person("Alice", 30, "Engineering"),   Person("Bob", 25, "Marketing"),
        Person("Charlie", 35, "Engineering"), Person("Diana", 28, "Finance"),
        Person("Eva", 32, "Marketing"),       Person("Frank", 40, "Finance"),
        Person("Grace", 27, "Engineering")};

    // Print initial people
    std::cout << "People:" << std::endl;
    for (const auto& person : people) {
        std::cout << "  " << person << std::endl;
    }

    // Filter by department
    auto engineers = atom::type::makeStream(people).cpFilter(
        [](const Person& p) { return p.getDepartment() == "Engineering"; });

    std::cout << "\nEngineers:" << std::endl;
    for (const auto& person : engineers.getRef()) {
        std::cout << "  " << person << std::endl;
    }

    // Sort by age
    auto peopleByAge = atom::type::makeStream(people).sorted(
        [](const Person& a, const Person& b) {
            return a.getAge() < b.getAge();
        });

    std::cout << "\nPeople sorted by age:" << std::endl;
    for (const auto& person : peopleByAge.getRef()) {
        std::cout << "  " << person << std::endl;
    }

    // Map to names
    auto names =
        atom::type::makeStream(people).transform<std::vector<std::string>>(
            [](const Person& p) { return p.getName(); });

    std::cout << "\nExtracted names: ";
    printVector(names.getRef(), "names");

    // Calculate average age
    double avgAge = atom::type::makeStream(people)
                        .transform<std::vector<int>>(
                            [](const Person& p) { return p.getAge(); })
                        .mean();

    std::cout << "\nAverage age: " << std::fixed << std::setprecision(1)
              << avgAge << std::endl;

    // Count people by department
    std::cout << "\nCount by department:" << std::endl;
    std::cout << "  Engineering: "
              << atom::type::makeStream(people).count([](const Person& p) {
                     return p.getDepartment() == "Engineering";
                 })
              << std::endl;

    std::cout << "  Marketing: "
              << atom::type::makeStream(people).count([](const Person& p) {
                     return p.getDepartment() == "Marketing";
                 })
              << std::endl;

    std::cout << "  Finance: "
              << atom::type::makeStream(people).count([](const Person& p) {
                     return p.getDepartment() == "Finance";
                 })
              << std::endl;

    // ============================================================
    // 13. Moving Results and Getting Copies
    // ============================================================
    printHeader("13. MOVING RESULTS AND GETTING COPIES");

    // Making a copy of the stream
    auto originalVec = std::vector<int>{1, 2, 3, 4, 5};
    auto stream = atom::type::makeStream(originalVec);

    // Creating a copy of the stream
    auto copyStream = stream.copy();

    // Modify the copy
    copyStream.getRef().push_back(6);
    copyStream.forEach([](int& n) { n *= 2; });

    std::cout << "Original vector: ";
    printVector(originalVec, "originalVec");

    std::cout << "Modified copy: ";
    printVector(copyStream.getRef(), "copyStream.getRef()");

    // Moving the result out
    auto movedResult = atom::type::makeStream(std::vector<int>{10, 20, 30})
                           .filter([](int n) { return n > 15; })
                           .getMove();

    std::cout << "\nMoved result: ";
    printVector(movedResult, "movedResult");

    // Getting a copy
    auto originalForCopy = std::vector<int>{100, 200, 300};
    auto copiedData = atom::type::makeStream(originalForCopy)
                          .map([](int n) { return n + 1; })
                          .get();

    std::cout << "\nOriginal for copy: ";
    printVector(originalForCopy, "originalForCopy");

    std::cout << "Copied data: ";
    printVector(copiedData, "copiedData");

    // ============================================================
    // 14. Utility Functions (Pair functions)
    // ============================================================
    printHeader("14. UTILITY FUNCTIONS");

    // Working with pairs
    std::vector<std::pair<std::string, int>> nameAgePairs = {
        {"Alice", 30}, {"Bob", 25}, {"Charlie", 35}, {"Diana", 28}};

    std::cout << "Name-age pairs:" << std::endl;
    for (const auto& pair : nameAgePairs) {
        std::cout << "  " << pair.first << ": " << pair.second << std::endl;
    }

    // Extract names using Pair::first
    auto extractedNames = atom::type::makeStream(nameAgePairs)
                              .transform<std::vector<std::string>>(
                                  atom::type::Pair<std::string, int>::first);

    std::cout << "\nExtracted names using Pair::first: ";
    printVector(extractedNames.getRef(), "extractedNames");

    // Extract ages using Pair::second
    auto extractedAges = atom::type::makeStream(nameAgePairs)
                             .transform<std::vector<int>>(
                                 atom::type::Pair<std::string, int>::second);

    std::cout << "Extracted ages using Pair::second: ";
    printVector(extractedAges.getRef(), "extractedAges");

    // ============================================================
    // 15. Identity Function
    // ============================================================
    printHeader("15. IDENTITY FUNCTION");

    // Using identity function
    auto identityResult =
        atom::type::makeStream(numbers).transform<std::vector<int>>(
            atom::type::identity<int>());

    std::cout << "Original numbers: ";
    printVector(numbers, "numbers");

    std::cout << "After identity transform: ";
    printVector(identityResult.getRef(), "identityResult");

    // ============================================================
    // 16. Container Accumulate
    // ============================================================
    printHeader("16. CONTAINER ACCUMULATE");

    // Using ContainerAccumulate to join vectors
    std::vector<int> v1 = {1, 2, 3};
    std::vector<int> v2 = {4, 5, 6};
    std::vector<int> v3 = {7, 8, 9};

    std::vector<std::vector<int>> vectorOfVectors = {v1, v2, v3};

    std::cout << "Vectors to accumulate:" << std::endl;
    for (size_t i = 0; i < vectorOfVectors.size(); ++i) {
        std::cout << "  v" << (i + 1) << ": ";
        printVector(vectorOfVectors[i], "");
    }

    // Accumulate vectors into one
    std::vector<int> accumulated;
    for (const auto& vec : vectorOfVectors) {
        atom::type::ContainerAccumulate<std::vector<int>>()(accumulated, vec);
    }

    std::cout << "\nAccumulated result: ";
    printVector(accumulated, "accumulated");

    std::cout << "\n======================================================="
              << std::endl;
    std::cout << "     CSTREAM EXAMPLE COMPLETED SUCCESSFULLY     "
              << std::endl;
    std::cout << "======================================================="
              << std::endl;

    return 0;
}