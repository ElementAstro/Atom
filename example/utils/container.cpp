/**
 * @file container_example.cpp
 * @brief Comprehensive examples demonstrating all container utility functions
 *
 * This example demonstrates all functions available in atom::utils::container.hpp:
 * - Set operations (subset, intersection, union, difference)
 * - Container transformations
 * - Duplicate handling
 * - Container flattening
 * - Container combining operations (zip, cartesian product)
 * - Filtering and partitioning
 * - Element finding
 * - String literal conversion
 */

 #include "atom/utils/container.hpp"
 #include <iomanip>
 #include <iostream>
 #include <list>
 #include <map>
 #include <set>
 #include <string>
 #include <vector>
 
 // Helper function to print section headers
 void printSection(const std::string& title) {
     std::cout << "\n===============================================" << std::endl;
     std::cout << "  " << title << std::endl;
     std::cout << "===============================================" << std::endl;
 }
 
 // Helper function to print containers
 template<typename Container>
 void printContainer(const std::string& label, const Container& container) {
     std::cout << std::left << std::setw(25) << label << ": [";
     bool first = true;
     for (const auto& elem : container) {
         if (!first) std::cout << ", ";
         std::cout << elem;
         first = false;
     }
     std::cout << "]" << std::endl;
 }
 
 // Helper function to print pairs
 template<typename T1, typename T2>
 void printPairs(const std::string& label, const std::vector<std::pair<T1, T2>>& pairs) {
     std::cout << std::left << std::setw(25) << label << ": [";
     bool first = true;
     for (const auto& [first_elem, second_elem] : pairs) {
         if (!first) std::cout << ", ";
         std::cout << "(" << first_elem << ", " << second_elem << ")";
         first = false;
     }
     std::cout << "]" << std::endl;
 }
 
 // Helper function to print maps
 template<typename K, typename V>
 void printMap(const std::string& label, const std::map<K, V>& map) {
     std::cout << std::left << std::setw(25) << label << ": {";
     bool first = true;
     for (const auto& [key, value] : map) {
         if (!first) std::cout << ", ";
         std::cout << key << ": " << value;
         first = false;
     }
     std::cout << "}" << std::endl;
 }
 
 // Sample class to demonstrate member function handling
 class Person {
 public:
     Person(std::string name, int age, std::string city)
         : name_(std::move(name)), age_(age), city_(std::move(city)) {}
     
     std::string getName() const { return name_; }
     int getAge() const { return age_; }
     std::string getCity() const { return city_; }
     
     // For printing Person objects
     friend std::ostream& operator<<(std::ostream& os, const Person& person) {
         os << person.name_ << "(" << person.age_ << ")";
         return os;
     }
     
     // For making Person objects hashable
     bool operator==(const Person& other) const {
         return name_ == other.name_ && age_ == other.age_ && city_ == other.city_;
     }
     
 private:
     std::string name_;
     int age_;
     std::string city_;
 };
 
 // Make Person hashable for std::unordered_set
 namespace std {
     template<>
     struct hash<Person> {
         size_t operator()(const Person& p) const {
             return hash<string>()(p.getName()) ^ hash<int>()(p.getAge()) ^ hash<string>()(p.getCity());
         }
     };
 }
 
 int main() {
     try {
         std::cout << "Container Utilities Demonstration" << std::endl;
 
         // ===================================================
         // Example 1: Basic Container Operations and Subset Checking
         // ===================================================
         printSection("1. Basic Container Operations and Subset Checking");
         
         // Create different container types
         std::vector<int> vec1 = {1, 2, 3, 4, 5};
         std::list<int> list1 = {2, 3, 4};
         std::set<int> set1 = {3, 4, 5, 6, 7};
         
         printContainer("Vector", vec1);
         printContainer("List", list1);
         printContainer("Set", set1);
         
         // Test contains function
         std::cout << "\nContains function demonstration:" << std::endl;
         std::cout << "Vector contains 3: " << (atom::utils::contains(vec1, 3) ? "Yes" : "No") << std::endl;
         std::cout << "Vector contains 8: " << (atom::utils::contains(vec1, 8) ? "Yes" : "No") << std::endl;
         
         // Test conversion to unordered_set
         std::cout << "\nToUnorderedSet demonstration:" << std::endl;
         auto vec1AsSet = atom::utils::toUnorderedSet(vec1);
         std::cout << "Vector as unordered_set - size: " << vec1AsSet.size() << std::endl;
         std::cout << "Checking membership in unordered_set:" << std::endl;
         std::cout << "Contains 3: " << (vec1AsSet.contains(3) ? "Yes" : "No") << std::endl;
         std::cout << "Contains 8: " << (vec1AsSet.contains(8) ? "Yes" : "No") << std::endl;
         
         // Test subset operations with different algorithms
         std::cout << "\nSubset checking demonstration:" << std::endl;
         std::cout << "Is list a subset of vector (isSubset): " 
                   << (atom::utils::isSubset(list1, vec1) ? "Yes" : "No") << std::endl;
         std::cout << "Is list a subset of vector (linearSearch): " 
                   << (atom::utils::isSubsetLinearSearch(list1, vec1) ? "Yes" : "No") << std::endl;
         std::cout << "Is list a subset of vector (hashSet): " 
                   << (atom::utils::isSubsetWithHashSet(list1, vec1) ? "Yes" : "No") << std::endl;
         
         // Test negative subset case
         std::list<int> list2 = {2, 3, 8};
         printContainer("List 2", list2);
         std::cout << "Is list2 a subset of vector: " 
                   << (atom::utils::isSubset(list2, vec1) ? "Yes" : "No") << std::endl;
         
         // ===================================================
         // Example 2: Set Operations
         // ===================================================
         printSection("2. Set Operations");
         
         // Create test containers
         std::vector<int> setA = {1, 2, 3, 4, 5};
         std::list<int> setB = {4, 5, 6, 7};
         
         printContainer("Set A", setA);
         printContainer("Set B", setB);
         
         // Test intersection
         auto intersect = atom::utils::intersection(setA, setB);
         printContainer("Intersection (A ∩ B)", intersect);
         
         // Test union
         auto unionSet = atom::utils::unionSet(setA, setB);
         printContainer("Union (A ∪ B)", unionSet);
         
         // Test difference
         auto diff1 = atom::utils::difference(setA, setB);
         printContainer("Difference (A - B)", diff1);
         
         auto diff2 = atom::utils::difference(setB, setA);
         printContainer("Difference (B - A)", diff2);
         
         // Test symmetric difference
         auto symDiff = atom::utils::symmetricDifference(setA, setB);
         printContainer("Symmetric Difference", symDiff);
         
         // Test container equality
         std::vector<int> vecEqual1 = {1, 2, 3};
         std::list<int> listEqual1 = {1, 2, 3};
         std::set<int> setEqual1 = {3, 2, 1}; // Different order but same elements
         
         printContainer("Vector for equality", vecEqual1);
         printContainer("List for equality", listEqual1);
         printContainer("Set for equality", setEqual1);
         
         std::cout << "\nEquality checking demonstration:" << std::endl;
         std::cout << "Vector equals List: " 
                   << (atom::utils::isEqual(vecEqual1, listEqual1) ? "Yes" : "No") << std::endl;
         std::cout << "Vector equals Set: " 
                   << (atom::utils::isEqual(vecEqual1, setEqual1) ? "Yes" : "No") << std::endl;
         
         // ===================================================
         // Example 3: Container Transformations
         // ===================================================
         printSection("3. Container Transformations");
         
         // Create a vector of Person objects
         std::vector<Person> people = {
             Person("Alice", 30, "New York"),
             Person("Bob", 25, "Chicago"),
             Person("Charlie", 35, "Los Angeles"),
             Person("David", 28, "Boston")
         };
         
         std::cout << "People collection:" << std::endl;
         for (const auto& person : people) {
             std::cout << "  " << person.getName() << ", Age: " << person.getAge() 
                       << ", City: " << person.getCity() << std::endl;
         }
         
         // Transform container using member functions
         std::cout << "\nTransforming containers using member functions:" << std::endl;
         
         auto names = atom::utils::transformToVector(people, &Person::getName);
         printContainer("Names", names);
         
         auto ages = atom::utils::transformToVector(people, &Person::getAge);
         printContainer("Ages", ages);
         
         auto cities = atom::utils::transformToVector(people, &Person::getCity);
         printContainer("Cities", cities);
         
         // Test applyAndStore (alternative transformation function)
         std::cout << "\nUsing applyAndStore function:" << std::endl;
         auto namesByApply = atom::utils::applyAndStore(people, &Person::getName);
         printContainer("Names by applyAndStore", namesByApply);
         
         // ===================================================
         // Example 4: Handling Duplicates
         // ===================================================
         printSection("4. Handling Duplicates");
         
         // Create containers with duplicates
         std::vector<int> duplicateInts = {1, 2, 2, 3, 4, 4, 5, 5, 5};
         std::vector<std::string> duplicateStrings = {"apple", "banana", "apple", "cherry", "banana", "date"};
         std::map<std::string, int> duplicateMap = {{"a", 1}, {"b", 2}, {"a", 3}, {"c", 4}};
         
         printContainer("Duplicate Integers", duplicateInts);
         printContainer("Duplicate Strings", duplicateStrings);
         std::cout << "Duplicate Map entries: ";
         for (const auto& [key, value] : duplicateMap) {
             std::cout << key << ":" << value << " ";
         }
         std::cout << std::endl;
         
         // Remove duplicates
         auto uniqueInts = atom::utils::unique(duplicateInts);
         auto uniqueStrings = atom::utils::unique(duplicateStrings);
         auto uniqueMap = atom::utils::unique(duplicateMap);
         
         printContainer("Unique Integers", uniqueInts);
         printContainer("Unique Strings", uniqueStrings);
         std::cout << "Unique Map entries: ";
         for (const auto& [key, value] : uniqueMap) {
             std::cout << key << ":" << value << " ";
         }
         std::cout << std::endl;
         
         // ===================================================
         // Example 5: Container Flattening
         // ===================================================
         printSection("5. Container Flattening");
         
         // Create nested containers
         std::vector<std::vector<int>> nestedInts = {
             {1, 2, 3},
             {4, 5},
             {6, 7, 8, 9}
         };
         
         std::cout << "Nested integers:" << std::endl;
         for (const auto& innerVec : nestedInts) {
             printContainer("  Inner vector", innerVec);
         }
         
         // Flatten the nested containers
         auto flattenedInts = atom::utils::flatten(nestedInts);
         printContainer("Flattened integers", flattenedInts);
         
         // More complex example - nested lists
         std::vector<std::list<std::string>> nestedLists = {
             {"red", "green", "blue"},
             {"apple", "banana"},
             {"one", "two", "three"}
         };
         
         std::cout << "\nNested lists:" << std::endl;
         for (const auto& innerList : nestedLists) {
             printContainer("  Inner list", innerList);
         }
         
         // Flatten the nested lists
         auto flattenedStrings = atom::utils::flatten(nestedLists);
         printContainer("Flattened strings", flattenedStrings);
         
         // ===================================================
         // Example 6: Container Combining Operations
         // ===================================================
         printSection("6. Container Combining Operations");
         
         // Create containers to combine
         std::vector<char> letters = {'A', 'B', 'C'};
         std::list<int> numbers = {1, 2, 3, 4, 5};  // Longer than letters
         
         printContainer("Letters", letters);
         printContainer("Numbers", numbers);
         
         // Zip containers
         std::cout << "\nZip operation (combines corresponding elements):" << std::endl;
         auto zipped = atom::utils::zip(letters, numbers);
         printPairs("Zipped pairs", zipped);
         std::cout << "Note: Zip stops at the end of the shortest container" << std::endl;
         
         // Cartesian product
         std::cout << "\nCartesian product (all possible combinations):" << std::endl;
         auto product = atom::utils::cartesianProduct(letters, std::vector<int>{1, 2});
         printPairs("Cartesian product", product);
         
         // ===================================================
         // Example 7: Filtering and Partitioning
         // ===================================================
         printSection("7. Filtering and Partitioning");
         
         // Create a container to filter
         std::vector<int> mixedNumbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
         printContainer("Mixed Numbers", mixedNumbers);
         
         // Define predicates
         auto isEven = [](int n) { return n % 2 == 0; };
         auto isGreaterThan5 = [](int n) { return n > 5; };
         
         // Filter elements
         std::cout << "\nFiltering demonstration:" << std::endl;
         auto evenNumbers = atom::utils::filter(mixedNumbers, isEven);
         printContainer("Even numbers", evenNumbers);
         
         auto largeNumbers = atom::utils::filter(mixedNumbers, isGreaterThan5);
         printContainer("Numbers > 5", largeNumbers);
         
         // Partition elements
         std::cout << "\nPartitioning demonstration:" << std::endl;
         auto [even, odd] = atom::utils::partition(mixedNumbers, isEven);
         printContainer("Even partition", even);
         printContainer("Odd partition", odd);
         
         auto [large, small] = atom::utils::partition(mixedNumbers, isGreaterThan5);
         printContainer("Large partition (>5)", large);
         printContainer("Small partition (≤5)", small);
         
         // ===================================================
         // Example 8: Finding Elements
         // ===================================================
         printSection("8. Finding Elements");
         
         std::vector<Person> employees = {
             Person("John", 42, "Seattle"),
             Person("Sarah", 38, "Portland"),
             Person("Michael", 29, "San Francisco"),
             Person("Emma", 45, "Seattle")
         };
         
         std::cout << "Employee collection:" << std::endl;
         for (const auto& employee : employees) {
             std::cout << "  " << employee.getName() << ", Age: " << employee.getAge() 
                       << ", City: " << employee.getCity() << std::endl;
         }
         
         // Find first element that satisfies predicate
         std::cout << "\nFinding elements demonstration:" << std::endl;
         
         auto youngEmployee = atom::utils::findIf(employees, [](const Person& p) { 
             return p.getAge() < 30; 
         });
         
         if (youngEmployee) {
             std::cout << "Found young employee: " << youngEmployee->getName() 
                       << ", Age: " << youngEmployee->getAge() << std::endl;
         } else {
             std::cout << "No young employee found" << std::endl;
         }
         
         auto seattleEmployee = atom::utils::findIf(employees, [](const Person& p) { 
             return p.getCity() == "Seattle"; 
         });
         
         if (seattleEmployee) {
             std::cout << "Found Seattle employee: " << seattleEmployee->getName() 
                       << ", Age: " << seattleEmployee->getAge() << std::endl;
         } else {
             std::cout << "No Seattle employee found" << std::endl;
         }
         
         auto oldEmployee = atom::utils::findIf(employees, [](const Person& p) { 
             return p.getAge() > 50; 
         });
         
         if (oldEmployee) {
             std::cout << "Found employee over 50: " << oldEmployee->getName() 
                       << ", Age: " << oldEmployee->getAge() << std::endl;
         } else {
             std::cout << "No employee over 50 found" << std::endl;
         }
         
         // ===================================================
         // Example 9: String Literal to Vector
         // ===================================================
         printSection("9. String Literal to Vector");
         
         // Use the custom string literal operator
         auto fruits = "apple, banana, cherry, date"_vec;
         printContainer("Fruits from string literal", fruits);
         
         auto colors = "red,green,blue,yellow"_vec;
         printContainer("Colors from string literal", colors);
         
         auto mixedSpacing = "  item1  ,item2,   item3,item4  "_vec;
         printContainer("Mixed spacing from string literal", mixedSpacing);
 
         std::cout << "\nAll examples completed successfully!" << std::endl;
 
     } catch (const std::exception& e) {
         std::cerr << "ERROR: " << e.what() << std::endl;
         return 1;
     }
 
     return 0;
 }