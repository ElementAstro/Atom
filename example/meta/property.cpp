#include "atom/meta/property.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

// Custom class to demonstrate Property with user-defined types
class Person {
private:
    std::string name_;
    int age_;

public:
    Person() : name_("Unknown"), age_(0) {}
    Person(std::string name, int age) : name_(std::move(name)), age_(age) {}

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    int getAge() const { return age_; }
    void setAge(int age) { age_ = age; }

    // For stream output
    friend std::ostream& operator<<(std::ostream& os, const Person& person) {
        os << "Person{name='" << person.name_ << "', age=" << person.age_
           << "}";
        return os;
    }

    // For comparison operators
    bool operator==(const Person& other) const {
        return name_ == other.name_ && age_ == other.age_;
    }

    auto operator<=>(const Person& other) const {
        if (auto cmp = name_ <=> other.name_; cmp != 0)
            return cmp;
        return age_ <=> other.age_;
    }

    // For arithmetic operators
    Person operator+(const Person& other) const {
        return Person(name_ + " " + other.name_, age_ + other.age_);
    }

    Person operator-(const Person& other) const {
        return Person(name_, age_ - other.age_);
    }

    Person operator*(const Person& other) const {
        return Person(name_, age_ * other.age_);
    }

    Person operator/(const Person& other) const {
        return Person(name_, age_ / other.age_);
    }

    Person operator%(const Person& other) const {
        return Person(name_, age_ % other.age_);
    }
};

// Class demonstrating the property macros
class UserProfile {
public:
    UserProfile(const std::string& username, int level, bool premium)
        : username_(username), level_(level), premium_(premium) {}

    DEFINE_RW_PROPERTY(std::string, username)
    DEFINE_RO_PROPERTY(int, level)
    DEFINE_WO_PROPERTY(bool, premium)
};

// Function to print a property's value
template <typename T>
void printProperty(const std::string& name, const Property<T>& prop) {
    try {
        std::cout << name << " = " << static_cast<T>(prop) << std::endl;
    } catch (const std::exception& e) {
        std::cout << name << " error: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=============================================\n";
    std::cout << "Property Template Usage Examples\n";
    std::cout << "=============================================\n\n";

    // 1. Basic Property Creation and Usage
    std::cout << "1. BASIC PROPERTY CREATION AND USAGE\n";
    std::cout << "-------------------------------------------\n";

    // Property with default value
    Property<int> intProperty(42);
    std::cout << "intProperty = " << static_cast<int>(intProperty) << std::endl;

    // Property with custom getter
    int backingValue = 100;
    Property<int> getterProperty([&backingValue]() { return backingValue; });
    std::cout << "getterProperty = " << static_cast<int>(getterProperty)
              << std::endl;

    // Property with getter and setter
    Property<std::string> stringProperty(
        []() { return "Hello, World!"; },
        [](const std::string& value) {
            std::cout << "Setting value to: " << value << std::endl;
        });
    std::cout << "stringProperty = " << static_cast<std::string>(stringProperty)
              << std::endl;
    stringProperty = "New Value";

    // Empty property - will throw when accessed
    Property<double> emptyProperty;
    try {
        double value = static_cast<double>(emptyProperty);
        std::cout << "emptyProperty = " << value << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
    }

    std::cout << std::endl;

    // 2. Property Attributes
    std::cout << "2. PROPERTY ATTRIBUTES\n";
    std::cout << "-------------------------------------------\n";

    // Make a property read-only
    double tempValue = 98.6;
    Property<double> temperatureProperty(
        [&tempValue]() { return tempValue; },
        [&tempValue](double value) { tempValue = value; });

    std::cout << "Before making read-only: "
              << static_cast<double>(temperatureProperty) << std::endl;
    temperatureProperty = 99.2;
    std::cout << "After setting value: "
              << static_cast<double>(temperatureProperty) << std::endl;

    temperatureProperty.makeReadonly();
    std::cout << "After making read-only, can still read: "
              << static_cast<double>(temperatureProperty) << std::endl;
    temperatureProperty = 100.0;  // This won't change the value anymore
    std::cout << "After attempting to change read-only: "
              << static_cast<double>(temperatureProperty) << std::endl;

    // Make a property write-only
    Property<std::string> passwordProperty(
        []() { return "********"; },
        []([[maybe_unused]] const std::string& value) {
            std::cout << "Password set to encrypted value" << std::endl;
        });

    std::cout << "Before making write-only: "
              << static_cast<std::string>(passwordProperty) << std::endl;
    passwordProperty.makeWriteonly();
    try {
        std::string password = static_cast<std::string>(passwordProperty);
        std::cout << "Password (should not see): " << password << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
    }
    passwordProperty = "new_secure_password";  // Still works

    // Clear a property
    Property<int> clearableProperty(123);
    std::cout << "Before clearing: " << static_cast<int>(clearableProperty)
              << std::endl;
    clearableProperty.clear();
    try {
        int value = static_cast<int>(clearableProperty);
        std::cout << "Value after clear (should not see): " << value
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error after clear: " << e.what() << std::endl;
    }

    std::cout << std::endl;

    // 3. Change Notification
    std::cout << "3. CHANGE NOTIFICATION\n";
    std::cout << "-------------------------------------------\n";

    Property<int> observableProperty(0);
    observableProperty.setOnChange([](const int& newValue) {
        std::cout << "Change detected! New value: " << newValue << std::endl;
    });

    std::cout << "Setting value to trigger onChange callback...\n";
    observableProperty = 42;
    observableProperty = 100;

    // Manual notification
    std::cout << "Manual notification...\n";
    observableProperty.notifyChange(999);

    std::cout << std::endl;

    // 4. Comparison and Arithmetic Operators
    std::cout << "4. COMPARISON AND ARITHMETIC OPERATORS\n";
    std::cout << "-------------------------------------------\n";

    Property<int> a(5);
    Property<int> b(10);

    std::cout << "a = " << static_cast<int>(a)
              << ", b = " << static_cast<int>(b) << std::endl;

    // Comparison operators
    std::cout << "a == 5: " << (a == 5) << std::endl;
    std::cout << "a != 5: " << (a != 5) << std::endl;
    std::cout << "a < 10: " << (a < 10) << std::endl;
    std::cout << "b > 5: " << (b > 5) << std::endl;

    // Arithmetic operators
    a += 7;
    std::cout << "a += 7: " << static_cast<int>(a) << std::endl;

    b -= 3;
    std::cout << "b -= 3: " << static_cast<int>(b) << std::endl;

    a *= 2;
    std::cout << "a *= 2: " << static_cast<int>(a) << std::endl;

    b /= 2;
    std::cout << "b /= 2: " << static_cast<int>(b) << std::endl;

    a %= 5;
    std::cout << "a %= 5: " << static_cast<int>(a) << std::endl;

    std::cout << std::endl;

    // 5. Asynchronous Operations
    std::cout << "5. ASYNCHRONOUS OPERATIONS\n";
    std::cout << "-------------------------------------------\n";

    Property<int> asyncProperty(0);

    // Async get
    std::cout << "Starting async get...\n";
    auto futureGet = asyncProperty.asyncGet();
    std::cout << "Doing other work while waiting for value...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "Async get result: " << futureGet.get() << std::endl;

    // Async set
    std::cout << "Starting async set...\n";
    auto futureSet = asyncProperty.asyncSet(42);
    std::cout << "Doing other work while setting value...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    futureSet.wait();  // Wait for completion
    std::cout << "After async set, property = "
              << static_cast<int>(asyncProperty) << std::endl;

    std::cout << std::endl;

    // 6. Property Caching
    std::cout << "6. PROPERTY CACHING\n";
    std::cout << "-------------------------------------------\n";

    Property<std::string> cachedProperty("Initial Value");

    // Cache different values with different keys
    cachedProperty.cacheValue("default", "Default Value");
    cachedProperty.cacheValue("alternative", "Alternative Value");
    cachedProperty.cacheValue("backup", "Backup Value");

    // Retrieve cached values
    auto defaultValue = cachedProperty.getCachedValue("default");
    auto alternativeValue = cachedProperty.getCachedValue("alternative");
    auto nonExistentValue = cachedProperty.getCachedValue("nonexistent");

    std::cout << "Cached default value: "
              << (defaultValue ? *defaultValue : "Not found") << std::endl;
    std::cout << "Cached alternative value: "
              << (alternativeValue ? *alternativeValue : "Not found")
              << std::endl;
    std::cout << "Cached nonexistent value: "
              << (nonExistentValue ? *nonExistentValue : "Not found")
              << std::endl;

    // Clear cache
    cachedProperty.clearCache();
    auto clearedValue = cachedProperty.getCachedValue("default");
    std::cout << "After clearing cache, default value: "
              << (clearedValue ? *clearedValue : "Not found") << std::endl;

    std::cout << std::endl;

    // 7. Custom Types
    std::cout << "7. CUSTOM TYPES\n";
    std::cout << "-------------------------------------------\n";

    Person john("John Doe", 30);
    Person jane("Jane Smith", 25);

    Property<Person> personProperty(john);
    std::cout << "Initial person: " << static_cast<Person>(personProperty)
              << std::endl;

    personProperty = jane;
    std::cout << "After assignment: " << static_cast<Person>(personProperty)
              << std::endl;

    // Operators with custom types
    std::cout << "personProperty == jane: " << (personProperty == jane)
              << std::endl;

    personProperty += Person("Jr.", 5);
    std::cout << "After += operation: " << static_cast<Person>(personProperty)
              << std::endl;

    std::cout << std::endl;

    // 8. Property Macros
    std::cout << "8. PROPERTY MACROS\n";
    std::cout << "-------------------------------------------\n";

    UserProfile user("johndoe", 10, true);

    // Read-write property
    std::cout << "username (RW): " << static_cast<std::string>(user.username)
              << std::endl;
    user.username = "janedoe";
    std::cout << "username after change: "
              << static_cast<std::string>(user.username) << std::endl;

    // Read-only property
    std::cout << "level (RO): " << static_cast<int>(user.level) << std::endl;
    try {
        user.level = 20;  // This should not change the value
        std::cout << "level after attempted change: "
                  << static_cast<int>(user.level) << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error when trying to set read-only property: " << e.what()
                  << std::endl;
    }

    // Write-only property
    user.premium = false;  // This should work
    try {
        bool isPremium = static_cast<bool>(user.premium);  // This should fail
        std::cout << "premium (should not see): " << std::boolalpha << isPremium
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error when reading write-only property: "
                  << e.what() << std::endl;
    }

    std::cout << std::endl;

    // 9. Stream Output
    std::cout << "9. STREAM OUTPUT\n";
    std::cout << "-------------------------------------------\n";

    Property<int> streamInt(42);
    Property<std::string> streamString("Hello, Stream!");
    Property<Person> streamPerson(Person("Stream Person", 50));

    std::cout << "Direct stream output for int: " << streamInt << std::endl;
    std::cout << "Direct stream output for string: " << streamString
              << std::endl;
    std::cout << "Direct stream output for custom class: " << streamPerson
              << std::endl;

    std::cout << std::endl;

    // 10. Practical Example: Temperature Conversion
    std::cout << "10. PRACTICAL EXAMPLE: TEMPERATURE CONVERSION\n";
    std::cout << "-------------------------------------------\n";

    double celsiusValue = 25.0;

    // Create a Celsius property
    Property<double> celsius(
        [&celsiusValue]() { return celsiusValue; },
        [&celsiusValue](double value) { celsiusValue = value; });

    // Create a Fahrenheit property that converts from/to Celsius
    Property<double> fahrenheit(
        [&celsius]() {
            return static_cast<double>(celsius) * 9.0 / 5.0 + 32.0;
        },
        [&celsius](double value) { celsius = (value - 32.0) * 5.0 / 9.0; });

    std::cout << "Initial temperature: " << celsius << "°C = " << fahrenheit
              << "°F" << std::endl;

    // Change Celsius, observe Fahrenheit
    celsius = 30.0;
    std::cout << "After changing Celsius: " << celsius << "°C = " << fahrenheit
              << "°F" << std::endl;

    // Change Fahrenheit, observe Celsius
    fahrenheit = 32.0;
    std::cout << "After changing Fahrenheit: " << celsius
              << "°C = " << fahrenheit << "°F" << std::endl;

    return 0;
}
