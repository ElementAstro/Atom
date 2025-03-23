/**
 * Comprehensive examples for atom::meta::TypeMetadata and atom::meta::bindFirst
 *
 * This file demonstrates the complete functionality of both systems:
 * 1. TypeMetadata and TypeRegistry for reflection
 * 2. bindFirst for function binding
 */

#include "atom/meta/bind_first.hpp"
#include "atom/meta/any.hpp"
#include "atom/meta/anymeta.hpp"

#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace atom::meta;
using namespace std::chrono_literals;

// Forward declarations for our example classes
class Person;
class Vehicle;
class DataProcessor;

// Section 1: Simple class with properties and methods for reflection examples
class Person {
private:
    std::string name_;
    int age_;
    std::string address_;
    bool employed_;

public:
    Person() : name_("Unknown"), age_(0), address_(""), employed_(false) {
        std::cout << "Person default constructor called" << std::endl;
    }

    Person(std::string name, int age)
        : name_(std::move(name)), age_(age), address_(""), employed_(false) {
        std::cout << "Person constructor called for " << name_ << std::endl;
    }

    // Getters
    std::string getName() const { return name_; }
    int getAge() const { return age_; }
    std::string getAddress() const { return address_; }
    bool isEmployed() const { return employed_; }

    // Setters
    void setName(const std::string& name) { name_ = name; }
    void setAge(int age) {
        if (age < 0)
            throw std::invalid_argument("Age cannot be negative");
        age_ = age;
    }
    void setAddress(const std::string& address) { address_ = address; }
    void setEmployed(bool employed) { employed_ = employed; }

    // Methods
    void greet() const {
        std::cout << "Hello, my name is " << name_ << " and I am " << age_
                  << " years old." << std::endl;
    }

    void greet(const std::string& greeting) const {
        std::cout << greeting << ", my name is " << name_ << "." << std::endl;
    }

    std::string getDescription() const {
        return "Person: " + name_ + ", Age: " + std::to_string(age_);
    }

    // Method with multiple parameters for testing overloads
    bool updateInfo(const std::string& newName, int newAge) {
        name_ = newName;
        age_ = newAge;
        return true;
    }

    bool updateInfo(const std::string& newName, int newAge,
                    const std::string& newAddress) {
        name_ = newName;
        age_ = newAge;
        address_ = newAddress;
        return true;
    }

    ~Person() {
        std::cout << "Person destructor called for " << name_ << std::endl;
    }
};

// Section 2: Vehicle class for further reflection examples
class Vehicle {
private:
    std::string make_;
    std::string model_;
    int year_;
    double price_;

public:
    Vehicle() : make_("Unknown"), model_("Unknown"), year_(0), price_(0.0) {}

    Vehicle(std::string make, std::string model, int year, double price)
        : make_(std::move(make)),
          model_(std::move(model)),
          year_(year),
          price_(price) {}

    // Getters
    std::string getMake() const { return make_; }
    std::string getModel() const { return model_; }
    int getYear() const { return year_; }
    double getPrice() const { return price_; }

    // Setters
    void setMake(const std::string& make) { make_ = make; }
    void setModel(const std::string& model) { model_ = model; }
    void setYear(int year) { year_ = year; }
    void setPrice(double price) { price_ = price; }

    // Methods
    std::string getDescription() const {
        return make_ + " " + model_ + " (" + std::to_string(year_) + ")";
    }

    double calculateDepreciation(int currentYear) const {
        int age = currentYear - year_;
        if (age <= 0)
            return 0.0;
        return price_ * 0.1 * age;
    }

    void performMaintenance() {
        std::cout << "Performing maintenance on " << make_ << " " << model_
                  << std::endl;
    }

    ~Vehicle() {
        std::cout << "Vehicle destructor called for " << make_ << " " << model_
                  << std::endl;
    }
};

// Section 3: DataProcessor class for testing bind_first functionality
class DataProcessor {
private:
    std::string name_;
    std::shared_ptr<std::mutex> mtx_;  // 使用共享指针管理互斥锁
    int processedItems_ = 0;

public:
    explicit DataProcessor(std::string name)
        : name_(std::move(name)), mtx_(std::make_shared<std::mutex>()) {}

    // 添加复制构造函数，确保共享 mutex
    DataProcessor(const DataProcessor& other)
        : name_(other.name_),
          mtx_(other.mtx_),
          processedItems_(other.processedItems_) {}

    // 处理方法
    int process(const std::vector<int>& data) {
        std::lock_guard<std::mutex> lock(*mtx_);
        int sum = 0;
        for (const auto& item : data) {
            sum += item;
            processedItems_++;
            // 模拟处理时间
            std::this_thread::sleep_for(10ms);
        }
        std::cout << name_ << " processed " << data.size()
                  << " items, sum = " << sum << std::endl;
        return sum;
    }

    double processWithFactor(const std::vector<int>& data,
                             double factor) const {
        double result = 0.0;
        for (const auto& item : data) {
            result += item * factor;
            // 模拟处理时间
            std::this_thread::sleep_for(5ms);
        }
        std::cout << name_ << " processed with factor " << factor
                  << ", result = " << result << std::endl;
        return result;
    }

    // 静态方法
    static void printProgress(int current, int total) {
        double percentage = (static_cast<double>(current) / total) * 100.0;
        std::cout << "Progress: " << current << "/" << total << " ("
                  << percentage << "%)" << std::endl;
    }

    // 可能抛出异常的方法
    void riskOperation(bool shouldFail) {
        if (shouldFail) {
            throw std::runtime_error("Operation failed as requested");
        }
        std::cout << "Operation completed successfully" << std::endl;
    }

    // Getter
    int getProcessedItems() const {
        std::lock_guard<std::mutex> lock(*mtx_);
        return processedItems_;
    }

    std::string getName() const { return name_; }
};

// Section 4: Register Person class with TypeRegistry
void registerPersonType() {
    TypeMetadata personMetadata;

    // Register constructors
    personMetadata.addConstructor(
        "Person", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.empty()) {
                return BoxedValue(Person{});
            } else if (args.size() == 2) {
                auto namePtr = args[0].tryCast<std::string>();
                auto agePtr = args[1].tryCast<int>();
                if (namePtr && agePtr) {
                    return BoxedValue(Person(*namePtr, *agePtr));
                }
            }
            THROW_INVALID_ARGUMENT("Invalid arguments for Person constructor");
            return BoxedValue{};
        });

    // Register methods
    personMetadata.addMethod(
        "greet", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.size() >= 1) {
                auto personPtr = args[0].tryCast<Person>();
                if (personPtr) {
                    if (args.size() == 1) {
                        personPtr->greet();
                    } else if (args.size() == 2) {
                        auto greetingPtr = args[1].tryCast<std::string>();
                        if (greetingPtr) {
                            personPtr->greet(*greetingPtr);
                        }
                    }
                }
            }
            return BoxedValue{};  // void return
        });

    personMetadata.addMethod(
        "getDescription", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.size() >= 1) {
                auto personPtr = args[0].tryCast<Person>();
                if (personPtr) {
                    return BoxedValue(personPtr->getDescription());
                }
            }
            return BoxedValue{};
        });

    personMetadata.addMethod(
        "updateInfo", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.size() >= 3) {
                auto personPtr = args[0].tryCast<Person>();
                auto namePtr = args[1].tryCast<std::string>();
                auto agePtr = args[2].tryCast<int>();

                if (personPtr && namePtr && agePtr) {
                    if (args.size() == 3) {
                        return BoxedValue(
                            personPtr->updateInfo(*namePtr, *agePtr));
                    } else if (args.size() == 4) {
                        auto addressPtr = args[3].tryCast<std::string>();
                        if (addressPtr) {
                            return BoxedValue(personPtr->updateInfo(
                                *namePtr, *agePtr, *addressPtr));
                        }
                    }
                }
            }
            return BoxedValue(false);
        });

    // Register properties with getters and setters
    personMetadata.addProperty(
        "name",
        [](const BoxedValue& obj) -> BoxedValue {
            if (auto personPtr = obj.tryCast<Person>()) {
                return BoxedValue(personPtr->getName());
            }
            return BoxedValue{};
        },
        [](BoxedValue& obj, const BoxedValue& value) {
            if (auto personPtr = obj.tryCast<Person>()) {
                if (auto namePtr = value.tryCast<std::string>()) {
                    personPtr->setName(*namePtr);
                }
            }
        },
        BoxedValue(std::string("Unknown")), "Person's full name");

    personMetadata.addProperty(
        "age",
        [](const BoxedValue& obj) -> BoxedValue {
            if (auto personPtr = obj.tryCast<Person>()) {
                return BoxedValue(personPtr->getAge());
            }
            return BoxedValue{};
        },
        [](BoxedValue& obj, const BoxedValue& value) {
            if (auto personPtr = obj.tryCast<Person>()) {
                if (auto agePtr = value.tryCast<int>()) {
                    personPtr->setAge(*agePtr);
                }
            }
        },
        BoxedValue(0), "Person's age in years");

    personMetadata.addProperty(
        "address",
        [](const BoxedValue& obj) -> BoxedValue {
            if (auto personPtr = obj.tryCast<Person>()) {
                return BoxedValue(personPtr->getAddress());
            }
            return BoxedValue{};
        },
        [](BoxedValue& obj, const BoxedValue& value) {
            if (auto personPtr = obj.tryCast<Person>()) {
                if (auto addressPtr = value.tryCast<std::string>()) {
                    personPtr->setAddress(*addressPtr);
                }
            }
        },
        BoxedValue(std::string("")), "Person's residential address");

    personMetadata.addProperty(
        "employed",
        [](const BoxedValue& obj) -> BoxedValue {
            if (auto personPtr = obj.tryCast<Person>()) {
                return BoxedValue(personPtr->isEmployed());
            }
            return BoxedValue{};
        },
        [](BoxedValue& obj, const BoxedValue& value) {
            if (auto personPtr = obj.tryCast<Person>()) {
                if (auto employedPtr = value.tryCast<bool>()) {
                    personPtr->setEmployed(*employedPtr);
                }
            }
        },
        BoxedValue(false), "Person's employment status");

    // Register events
    personMetadata.addEvent("onNameChanged",
                            "Triggered when a person's name changes");
    personMetadata.addEvent("onAgeChanged",
                            "Triggered when a person's age changes");
    personMetadata.addEvent("onAddressChanged",
                            "Triggered when a person's address changes");
    personMetadata.addEvent("onEmploymentChanged",
                            "Triggered when employment status changes");

    // Add event listeners
    personMetadata.addEventListener(
        "onNameChanged",
        [](BoxedValue& obj, const std::vector<BoxedValue>& args) {
            if (auto personPtr = obj.tryCast<Person>()) {
                if (!args.empty()) {
                    if (auto newNamePtr = args[0].tryCast<std::string>()) {
                        std::cout << "Event: Name changed to " << *newNamePtr
                                  << std::endl;
                    }
                }
            }
        },
        10  // Higher priority
    );

    personMetadata.addEventListener(
        "onAgeChanged",
        [](BoxedValue& obj, const std::vector<BoxedValue>& args) {
            if (auto personPtr = obj.tryCast<Person>()) {
                if (!args.empty()) {
                    if (auto newAgePtr = args[0].tryCast<int>()) {
                        std::cout << "Event: Age changed to " << *newAgePtr
                                  << std::endl;
                    }
                }
            }
        },
        5  // Medium priority
    );

    // Register with global registry
    TypeRegistry::instance().registerType("Person", std::move(personMetadata));
}

// Section 5: Register Vehicle class with TypeRegistry
void registerVehicleType() {
    TypeMetadata vehicleMetadata;

    // Register constructors
    vehicleMetadata.addConstructor(
        "Vehicle", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.empty()) {
                return BoxedValue(Vehicle{});
            } else if (args.size() == 4) {
                auto makePtr = args[0].tryCast<std::string>();
                auto modelPtr = args[1].tryCast<std::string>();
                auto yearPtr = args[2].tryCast<int>();
                auto pricePtr = args[3].tryCast<double>();

                if (makePtr && modelPtr && yearPtr && pricePtr) {
                    return BoxedValue(
                        Vehicle(*makePtr, *modelPtr, *yearPtr, *pricePtr));
                }
            }
            THROW_INVALID_ARGUMENT("Invalid arguments for Vehicle constructor");
            return BoxedValue{};
        });

    // Register methods
    vehicleMetadata.addMethod(
        "getDescription", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.size() >= 1) {
                auto vehiclePtr = args[0].tryCast<Vehicle>();
                if (vehiclePtr) {
                    return BoxedValue(vehiclePtr->getDescription());
                }
            }
            return BoxedValue{};
        });

    vehicleMetadata.addMethod(
        "calculateDepreciation",
        [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.size() >= 2) {
                auto vehiclePtr = args[0].tryCast<Vehicle>();
                auto yearPtr = args[1].tryCast<int>();

                if (vehiclePtr && yearPtr) {
                    return BoxedValue(
                        vehiclePtr->calculateDepreciation(*yearPtr));
                }
            }
            return BoxedValue{};
        });

    vehicleMetadata.addMethod(
        "performMaintenance", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.size() >= 1) {
                auto vehiclePtr = args[0].tryCast<Vehicle>();
                if (vehiclePtr) {
                    vehiclePtr->performMaintenance();
                }
            }
            return BoxedValue{};
        });

    // Register properties
    vehicleMetadata.addProperty(
        "make",
        [](const BoxedValue& obj) -> BoxedValue {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                return BoxedValue(vehiclePtr->getMake());
            }
            return BoxedValue{};
        },
        [](BoxedValue& obj, const BoxedValue& value) {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                if (auto makePtr = value.tryCast<std::string>()) {
                    vehiclePtr->setMake(*makePtr);
                }
            }
        },
        BoxedValue(std::string("Unknown")), "Vehicle manufacturer");

    vehicleMetadata.addProperty(
        "model",
        [](const BoxedValue& obj) -> BoxedValue {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                return BoxedValue(vehiclePtr->getModel());
            }
            return BoxedValue{};
        },
        [](BoxedValue& obj, const BoxedValue& value) {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                if (auto modelPtr = value.tryCast<std::string>()) {
                    vehiclePtr->setModel(*modelPtr);
                }
            }
        },
        BoxedValue(std::string("Unknown")), "Vehicle model name");

    vehicleMetadata.addProperty(
        "year",
        [](const BoxedValue& obj) -> BoxedValue {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                return BoxedValue(vehiclePtr->getYear());
            }
            return BoxedValue{};
        },
        [](BoxedValue& obj, const BoxedValue& value) {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                if (auto yearPtr = value.tryCast<int>()) {
                    vehiclePtr->setYear(*yearPtr);
                }
            }
        },
        BoxedValue(0), "Vehicle manufacturing year");

    vehicleMetadata.addProperty(
        "price",
        [](const BoxedValue& obj) -> BoxedValue {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                return BoxedValue(vehiclePtr->getPrice());
            }
            return BoxedValue{};
        },
        [](BoxedValue& obj, const BoxedValue& value) {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                if (auto pricePtr = value.tryCast<double>()) {
                    vehiclePtr->setPrice(*pricePtr);
                }
            }
        },
        BoxedValue(0.0), "Vehicle price in dollars");

    // Register events
    vehicleMetadata.addEvent("onPriceChanged",
                             "Triggered when the vehicle's price changes");
    vehicleMetadata.addEvent("onMaintenancePerformed",
                             "Triggered when maintenance is performed");

    vehicleMetadata.addEventListener(
        "onPriceChanged",
        [](BoxedValue& obj, const std::vector<BoxedValue>& args) {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                if (!args.empty()) {
                    if (auto oldPricePtr = args[0].tryCast<double>()) {
                        double newPrice = vehiclePtr->getPrice();
                        std::cout << "Event: Price changed from $"
                                  << *oldPricePtr << " to $" << newPrice
                                  << std::endl;
                    }
                }
            }
        });

    // Register with global registry
    TypeRegistry::instance().registerType("Vehicle",
                                          std::move(vehicleMetadata));
}

// Section 6: Main function with comprehensive examples
int main() {
    std::cout << "===================================================="
              << std::endl;
    std::cout << "  TypeMetadata and bindFirst Comprehensive Examples  "
              << std::endl;
    std::cout << "===================================================="
              << std::endl
              << std::endl;

    // PART 1: TypeMetadata and TypeRegistry Examples
    std::cout << "PART 1: TypeMetadata and TypeRegistry Examples" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;

    // Register our types
    registerPersonType();
    registerVehicleType();

    std::cout << "\n1.1: Creating objects using reflection" << std::endl;
    std::cout << "--------------------------------" << std::endl;

    // Create Person instance using the factory function
    auto johnBoxed =
        createInstance("Person", {var(std::string("John")), var(30)});

    // Call methods on the boxed person
    callMethod(johnBoxed, "greet", {johnBoxed});
    callMethod(johnBoxed, "greet",
               {johnBoxed, var(std::string("Good morning"))});

    // Get and set properties
    auto nameValue = getProperty(johnBoxed, "name");
    std::cout << "Original name: " << nameValue.debugString() << std::endl;

    setProperty(johnBoxed, "name", var(std::string("John Smith")));
    setProperty(johnBoxed, "age", var(31));
    setProperty(johnBoxed, "address", var(std::string("123 Main St")));

    // Fire events
    fireEvent(johnBoxed, "onNameChanged", {var(std::string("John Smith"))});
    fireEvent(johnBoxed, "onAgeChanged", {var(31)});

    // Call method with description
    auto description = callMethod(johnBoxed, "getDescription", {johnBoxed});
    std::cout << "Description: " << description.debugString() << std::endl;

    std::cout << "\n1.2: Working with Vehicle class reflection" << std::endl;
    std::cout << "--------------------------------------" << std::endl;

    // Create Vehicle instance
    auto carBoxed = createInstance(
        "Vehicle", {var(std::string("Toyota")), var(std::string("Camry")),
                    var(2022), var(25000.0)});

    // Get properties
    auto makeValue = getProperty(carBoxed, "make");
    auto modelValue = getProperty(carBoxed, "model");
    auto yearValue = getProperty(carBoxed, "year");
    auto priceValue = getProperty(carBoxed, "price");

    std::cout << "Vehicle: " << makeValue.debugString() << " "
              << modelValue.debugString()
              << ", Year: " << yearValue.debugString() << ", Price: $"
              << priceValue.debugString() << std::endl;

    // Call method to calculate depreciation
    auto depreciation =
        callMethod(carBoxed, "calculateDepreciation", {carBoxed, var(2025)});
    std::cout << "Depreciation by 2025: $" << depreciation.debugString()
              << std::endl;

    // Set property and fire event
    auto oldPrice = getProperty(carBoxed, "price");
    setProperty(carBoxed, "price", var(23000.0));
    fireEvent(carBoxed, "onPriceChanged", {oldPrice});

    // Call maintenance method
    callMethod(carBoxed, "performMaintenance", {carBoxed});
    fireEvent(carBoxed, "onMaintenancePerformed", {});

    std::cout << "\n1.3: Advanced TypeMetadata operations" << std::endl;
    std::cout << "----------------------------------" << std::endl;

    // Method overloading with updateInfo
    auto updateResult1 =
        callMethod(johnBoxed, "updateInfo",
                   {johnBoxed, var(std::string("Johnny")), var(32)});
    std::cout << "Update result 1: " << updateResult1.debugString()
              << std::endl;

    auto updateResult2 = callMethod(johnBoxed, "updateInfo",
                                    {johnBoxed, var(std::string("Johnny B.")),
                                     var(33), var(std::string("456 Oak Dr"))});
    std::cout << "Update result 2: " << updateResult2.debugString()
              << std::endl;

    // Check if person's properties were updated
    auto newName = getProperty(johnBoxed, "name");
    auto newAge = getProperty(johnBoxed, "age");
    auto newAddress = getProperty(johnBoxed, "address");

    std::cout << "Updated Person - Name: " << newName.debugString()
              << ", Age: " << newAge.debugString()
              << ", Address: " << newAddress.debugString() << std::endl;

    // PART 2: bindFirst Examples
    std::cout << "\nPART 2: bindFirst Examples" << std::endl;
    std::cout << "------------------------" << std::endl;

    std::cout << "\n2.1: Basic bindFirst with member functions" << std::endl;
    std::cout << "---------------------------------------" << std::endl;

    // Create a DataProcessor instance
    DataProcessor processor("MainProcessor");

    // Create test data
    std::vector<int> testData = {1, 2, 3, 4, 5};

    // 使用引用包装器绑定成员函数
    auto boundProcess = bindFirst(&DataProcessor::process, std::ref(processor));
    auto result = boundProcess(testData);
    std::cout << "boundProcess result: " << result << std::endl;

    // 绑定 const 成员函数
    auto boundProcessWithFactor =
        bindFirst(&DataProcessor::processWithFactor, std::ref(processor));
    auto factorResult = boundProcessWithFactor(testData, 2.5);
    std::cout << "boundProcessWithFactor result: " << factorResult << std::endl;

    std::cout << "\n2.2: Binding to references and pointers" << std::endl;
    std::cout << "-------------------------------------" << std::endl;

    // Bind to reference
    auto& processorRef = processor;
    auto boundProcessRef =
        bindFirst(&DataProcessor::process, std::ref(processorRef));
    result = boundProcessRef(testData);
    std::cout << "boundProcessRef result: " << result << std::endl;

    // Bind to pointer
    DataProcessor* processorPtr = &processor;
    auto boundProcessPtr = bindFirst(&DataProcessor::process, processorPtr);
    result = boundProcessPtr(testData);
    std::cout << "boundProcessPtr result: " << result << std::endl;

    std::cout << "\n2.3: Binding static functions" << std::endl;
    std::cout << "----------------------------" << std::endl;

    // Bind static method
    auto boundPrintProgress = bindStatic(&DataProcessor::printProgress);
    boundPrintProgress(50, 100);

    // 修复：为 lambda 绑定创建一个函数指针类型
    using GlobalFuncType = int (*)(int, int);
    GlobalFuncType globalFuncPtr = [](int a, int b) -> int { return a + b; };
    auto boundGlobalFunc = bindStatic(globalFuncPtr);
    std::cout << "boundGlobalFunc(5, 10) = " << boundGlobalFunc(5, 10)
              << std::endl;

    std::cout << "\n2.4: Binding to class members" << std::endl;
    std::cout << "----------------------------" << std::endl;

    // Create a Person for member binding
    Person alice("Alice", 25);

    // Bind to data member accessor function
    auto nameGetter = bindFirst(&Person::getName, alice);
    auto ageGetter = bindFirst(&Person::getAge, alice);

    std::cout << "Person name via bound getter: " << nameGetter() << std::endl;
    std::cout << "Person age via bound getter: " << ageGetter() << std::endl;

    std::cout << "\n2.5: Exception handling with bindFirst" << std::endl;
    std::cout << "----------------------------------" << std::endl;

    // Create binding with exception handling
    auto safeSetAge = bindFirstWithExceptionHandling(
        &Person::setAge, std::ref(alice), "Failed to set person age");

    try {
        safeSetAge(30);
        std::cout << "Age successfully set to: " << alice.getAge() << std::endl;

        safeSetAge(-10);  // This should throw an exception
    } catch (const BindingException& e) {
        std::cout << "Caught BindingException: " << e.what() << std::endl;
    }

    std::cout << "\n2.6: Thread-safe binding" << std::endl;
    std::cout << "----------------------" << std::endl;

    // Create shared_ptr for thread-safe binding
    auto sharedProcessor =
        std::make_shared<DataProcessor>("ThreadSafeProcessor");

    // Create thread-safe binding
    auto threadSafeProcess =
        bindFirstThreadSafe(&DataProcessor::process, sharedProcessor);

    // Launch multiple threads to call the bound function
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) {
        threads.emplace_back([&threadSafeProcess, i]() {
            std::vector<int> data = {i + 1, i + 2, i + 3, i + 4};
            std::cout << "Thread " << i
                      << " result: " << threadSafeProcess(data) << std::endl;
        });
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Total processed items: "
              << sharedProcessor->getProcessedItems() << std::endl;

    std::cout << "\n2.7: Asynchronous execution with asyncBindFirst"
              << std::endl;
    std::cout << "--------------------------------------------" << std::endl;

    // Create large dataset
    std::vector<int> largeData;
    for (int i = 0; i < 50; i++) {
        largeData.push_back(i);
    }

    // 使用引用包装器绑定
    auto boundLargeProcess =
        bindFirst(&DataProcessor::process, std::ref(processor));

    std::cout << "Starting async processing..." << std::endl;
    auto futureResult = asyncBindFirst(boundLargeProcess, largeData);

    std::cout << "Doing other work while processing..." << std::endl;
    for (int i = 0; i < 3; i++) {
        std::cout << "Main thread working..." << std::endl;
        std::this_thread::sleep_for(100ms);
    }

    // Wait for result
    int asyncResult = futureResult.get();
    std::cout << "Async processing completed with result: " << asyncResult
              << std::endl;

    std::cout << "\n2.8: Function object binding" << std::endl;
    std::cout << "---------------------------" << std::endl;

    // Lambda binding
    auto processor2 = DataProcessor("SecondProcessor");
    auto lambda = [](DataProcessor& proc, const std::vector<int>& data) {
        return proc.process(data) * 2;
    };

    // 使用引用包装器绑定
    auto boundLambda = bindFirst(lambda, std::ref(processor2));
    result = boundLambda(testData);
    std::cout << "Lambda binding result: " << result << std::endl;

    // Function object with operator()
    struct Multiplier {
        int factor;

        Multiplier(int f) : factor(f) {}

        int operator()(int value) const { return value * factor; }

        // 绑定到成员函数
        int process(DataProcessor& proc, const std::vector<int>& data) {
            return proc.process(data) * factor;
        }

        // 绑定到静态函数
        static void printMessage(const std::string& msg) {
            std::cout << "Message: " << msg << std::endl;
        }

        // 绑定到 lambda
        static void printLambdaMessage(const std::function<void()>& lambda) {
            lambda();
        }

        // 绑定到函数指针
        static void printFunctionPointerMessage(int (*func)(int, int), int a,
                                                int b) {
            std::cout << "Function pointer result: " << func(a, b) << std::endl;
        }

        // 绑定到成员函数指针
        static void printMemberFunctionPointerMessage(Multiplier* obj,
                                                      int value) {
            std::cout << "Member function pointer result: "
                      << obj->operator()(value) << std::endl;
        }

        // 绑定到 const 成员函数
        static void printConstMemberFunctionPointerMessage(
            const Multiplier* obj, int value) {
            std::cout << "Const member function pointer result: "
                      << obj->operator()(value) << std::endl;
        }

        // 绑定到引用
        static void printReferenceMessage(const Multiplier& obj, int value) {
            std::cout << "Reference result: " << obj.operator()(value)
                      << std::endl;
        }

        // 绑定到 const 引用
        static void printConstReferenceMessage(const Multiplier& obj,
                                               int value) {
            std::cout << "Const reference result: " << obj.operator()(value)
                      << std::endl;
        }

        // 绑定到指针
        static void printPointerMessage(Multiplier* obj, int value) {
            std::cout << "Pointer result: " << obj->operator()(value)
                      << std::endl;
        }

        // 绑定到 const 指针
        static void printConstPointerMessage(const Multiplier* obj, int value) {
            std::cout << "Const pointer result: " << obj->operator()(value)
                      << std::endl;
        }

        // 绑定到 std::function
        static void printStdFunctionMessage(const std::function<void()>& func) {
            func();
        }

        // 绑定到 std::shared_ptr
        static void printSharedPtrMessage(
            const std::shared_ptr<Multiplier>& obj, int value) {
            std::cout << "Shared pointer result: " << obj->operator()(value)
                      << std::endl;
        }

        // 绑定到 std::weak_ptr
        static void printWeakPtrMessage(const std::weak_ptr<Multiplier>& obj,
                                        int value) {
            if (auto sharedObj = obj.lock()) {
                std::cout << "Weak pointer result: "
                          << sharedObj->operator()(value) << std::endl;
            } else {
                std::cout << "Weak pointer expired" << std::endl;
            }
        }

        // 绑定到 std::unique_ptr
        static void printUniquePtrMessage(
            const std::unique_ptr<Multiplier>& obj, int value) {
            std::cout << "Unique pointer result: " << obj->operator()(value)
                      << std::endl;
        }

        // 绑定到 std::shared_ptr 和 std::weak_ptr
        static void printSharedWeakPtrMessage(
            const std::shared_ptr<Multiplier>& sharedObj,
            const std::weak_ptr<Multiplier>& weakObj, int value) {
            if (auto lockedObj = weakObj.lock()) {
                std::cout << "Shared and weak pointer result: "
                          << lockedObj->operator()(value) << std::endl;
            } else {
                std::cout << "Weak pointer expired" << std::endl;
            }
        }

        // 绑定到 std::unique_ptr 和 std::weak_ptr
        static void printUniqueWeakPtrMessage(
            const std::unique_ptr<Multiplier>& uniqueObj,
            const std::weak_ptr<Multiplier>& weakObj, int value) {
            if (auto lockedObj = weakObj.lock()) {
                std::cout << "Unique and weak pointer result: "
                          << lockedObj->operator()(value) << std::endl;
            } else {
                std::cout << "Weak pointer expired" << std::endl;
            }
        }

        // 绑定到 std::shared_ptr 和 std::unique_ptr
        static void printSharedUniquePtrMessage(
            const std::shared_ptr<Multiplier>& sharedObj,
            const std::unique_ptr<Multiplier>& uniqueObj, int value) {
            std::cout << "Shared and unique pointer result: "
                      << sharedObj->operator()(value) << std::endl;
        }
    };

}