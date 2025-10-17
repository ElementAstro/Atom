#include "atom/meta/anymeta.hpp"

#include <chrono>
#include <string>
#include <vector>

using namespace atom::meta;
using namespace std::chrono_literals;

// Sample classes for demonstration
class Person {
private:
    std::string name_;
    int age_;
    std::string address_;

public:
    Person() : name_("Unknown"), age_(0), address_("Nowhere") {}

    Person(std::string name, int age)
        : name_(std::move(name)), age_(age), address_("Default Address") {}

    Person(std::string name, int age, std::string address)
        : name_(std::move(name)),
          age_(std::move(age)),
          address_(std::move(address)) {}

    // Getters and setters
    std::string getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    int getAge() const { return age_; }
    void setAge(int age) { age_ = age; }

    std::string getAddress() const { return address_; }
    void setAddress(const std::string& address) { address_ = address; }

    // Methods
    std::string toString() const {
        return "Person{name='" + name_ + "', age=" + std::to_string(age_) +
               ", address='" + address_ + "'}";
    }

    void celebrateBirthday() {
        age_++;
        std::cout << name_ << " is now " << age_ << " years old!" << std::endl;
    }

    bool isAdult() const { return age_ >= 18; }
};

class Vehicle {
private:
    std::string make_;
    std::string model_;
    int year_;
    double mileage_;

public:
    Vehicle() : make_("Unknown"), model_("Unknown"), year_(0), mileage_(0.0) {}

    Vehicle(std::string make, std::string model, int year)
        : make_(std::move(make)),
          model_(std::move(model)),
          year_(year),
          mileage_(0.0) {}

    // Getters and setters
    std::string getMake() const { return make_; }
    void setMake(const std::string& make) { make_ = make; }

    std::string getModel() const { return model_; }
    void setModel(const std::string& model) { model_ = model; }

    int getYear() const { return year_; }
    void setYear(int year) { year_ = year; }

    double getMileage() const { return mileage_; }
    void setMileage(double mileage) { mileage_ = mileage; }

    // Methods
    std::string toString() const {
        return "Vehicle{make='" + make_ + "', model='" + model_ +
               "', year=" + std::to_string(year_) +
               ", mileage=" + std::to_string(mileage_) + "}";
    }

    void drive(double distance) {
        mileage_ += distance;
        std::cout << "Drove " << distance
                  << " miles. Total mileage: " << mileage_ << std::endl;
    }

    bool isAntique() const { return year_ < 1980; }
};

// Helper to print events for demonstration
class EventLogger {
public:
    static void logEvent(const std::string& event,
                         const std::string& objectType, const std::string& id) {
        std::cout << "[EVENT] " << event << " on " << objectType
                  << " (ID: " << id << ") at " << getCurrentTimestamp()
                  << std::endl;
    }

    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        char buf[100] = {0};
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&now_c));
        return std::string(buf);
    }
};

// Register Person type with the TypeRegistry
void registerPersonType() {
    TypeMetadata personMetadata;

    // Register constructors
    personMetadata.addConstructor(
        "Person", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.empty()) {
                return BoxedValue(Person{});  // Default constructor
            }
            THROW_NOT_FOUND("Invalid constructor arguments for Person");
        });

    personMetadata.addConstructor(
        "Person", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.size() == 2) {
                auto namePtr = args[0].tryCast<std::string>();
                auto agePtr = args[1].tryCast<int>();
                if (namePtr && agePtr) {
                    return BoxedValue(Person{*namePtr, *agePtr});
                }
            }
            THROW_NOT_FOUND("Invalid constructor arguments for Person");
        });

    personMetadata.addConstructor(
        "Person", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.size() == 3) {
                auto namePtr = args[0].tryCast<std::string>();
                auto agePtr = args[1].tryCast<int>();
                auto addressPtr = args[2].tryCast<std::string>();
                if (namePtr && agePtr && addressPtr) {
                    return BoxedValue(Person{*namePtr, *agePtr, *addressPtr});
                }
            }
            THROW_NOT_FOUND("Invalid constructor arguments for Person");
        });

    // Register methods
    personMetadata.addMethod(
        "toString", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (!args.empty()) {
                if (auto personPtr = args[0].tryCast<Person>()) {
                    return BoxedValue(personPtr->toString());
                }
            }
            THROW_NOT_FOUND("Invalid arguments for toString method");
        });

    personMetadata.addMethod(
        "celebrateBirthday", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (!args.empty()) {
                if (auto personPtr = args[0].tryCast<Person>()) {
                    personPtr->celebrateBirthday();
                    return BoxedValue();  // Return void
                }
            }
            THROW_NOT_FOUND("Invalid arguments for celebrateBirthday method");
        });

    personMetadata.addMethod(
        "isAdult", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (!args.empty()) {
                if (auto personPtr = args[0].tryCast<Person>()) {
                    return BoxedValue(personPtr->isAdult());
                }
            }
            THROW_NOT_FOUND("Invalid arguments for isAdult method");
        });

    // Register properties
    personMetadata.addProperty(
        "name",
        [](const BoxedValue& obj) -> BoxedValue {  // Getter
            if (auto personPtr = obj.tryCast<Person>()) {
                return BoxedValue(personPtr->getName());
            }
            THROW_NOT_FOUND("Invalid object for name getter");
        },
        [](BoxedValue& obj, const BoxedValue& value) {  // Setter
            if (auto personPtr = obj.tryCast<Person>()) {
                if (auto namePtr = value.tryCast<std::string>()) {
                    personPtr->setName(*namePtr);
                    return;
                }
            }
            THROW_NOT_FOUND("Invalid object or value for name setter");
        },
        BoxedValue(std::string("Unknown")),  // Default value
        "Person's name"                      // Description
    );

    personMetadata.addProperty(
        "age",
        [](const BoxedValue& obj) -> BoxedValue {  // Getter
            if (auto personPtr = obj.tryCast<Person>()) {
                return BoxedValue(personPtr->getAge());
            }
            THROW_NOT_FOUND("Invalid object for age getter");
        },
        [](BoxedValue& obj, const BoxedValue& value) {  // Setter
            if (auto personPtr = obj.tryCast<Person>()) {
                if (auto agePtr = value.tryCast<int>()) {
                    personPtr->setAge(*agePtr);
                    return;
                }
            }
            THROW_NOT_FOUND("Invalid object or value for age setter");
        },
        BoxedValue(0),           // Default value
        "Person's age in years"  // Description
    );

    personMetadata.addProperty(
        "address",
        [](const BoxedValue& obj) -> BoxedValue {  // Getter
            if (auto personPtr = obj.tryCast<Person>()) {
                return BoxedValue(personPtr->getAddress());
            }
            THROW_NOT_FOUND("Invalid object for address getter");
        },
        [](BoxedValue& obj, const BoxedValue& value) {  // Setter
            if (auto personPtr = obj.tryCast<Person>()) {
                if (auto addrPtr = value.tryCast<std::string>()) {
                    personPtr->setAddress(*addrPtr);
                    return;
                }
            }
            THROW_NOT_FOUND("Invalid object or value for address setter");
        },
        BoxedValue(std::string("Nowhere")),  // Default value
        "Person's residential address"       // Description
    );

    // Register events
    personMetadata.addEvent("onCreate", "Triggered when a person is created");
    personMetadata.addEvent("onUpdate",
                            "Triggered when a person's data is updated");
    personMetadata.addEvent("onDelete", "Triggered when a person is deleted");
    personMetadata.addEvent("onBirthday",
                            "Triggered when a person celebrates a birthday");

    // Add event listeners
    personMetadata.addEventListener(
        "onCreate",
        [](BoxedValue& obj, const std::vector<BoxedValue>& args) {
            if (auto personPtr = obj.tryCast<Person>()) {
                std::string id = !args.empty() && args[0].tryCast<std::string>()
                                     ? *args[0].tryCast<std::string>()
                                     : "unknown";
                EventLogger::logEvent("Created", "Person", id);
            }
        },
        10);  // High priority

    personMetadata.addEventListener(
        "onUpdate", [](BoxedValue& obj, const std::vector<BoxedValue>& args) {
            if (auto personPtr = obj.tryCast<Person>()) {
                std::string id = !args.empty() && args[0].tryCast<std::string>()
                                     ? *args[0].tryCast<std::string>()
                                     : "unknown";
                std::string field =
                    args.size() > 1 && args[1].tryCast<std::string>()
                        ? *args[1].tryCast<std::string>()
                        : "unknown";
                EventLogger::logEvent("Updated " + field, "Person", id);
            }
        });

    personMetadata.addEventListener(
        "onBirthday", [](BoxedValue& obj,
                         [[maybe_unused]] const std::vector<BoxedValue>& args) {
            if (auto personPtr = obj.tryCast<Person>()) {
                std::string name = personPtr->getName();
                int age = personPtr->getAge();
                std::cout << "🎂 Happy Birthday to " << name << "! Now " << age
                          << " years old!" << std::endl;
            }
        });

    // Register type in the global registry
    TypeRegistry::instance().registerType("Person", std::move(personMetadata));
}

// Register Vehicle type with the TypeRegistry
void registerVehicleType() {
    TypeMetadata vehicleMetadata;

    // Register constructors
    vehicleMetadata.addConstructor(
        "Vehicle", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.empty()) {
                return BoxedValue(Vehicle{});  // Default constructor
            }
            THROW_NOT_FOUND("Invalid constructor arguments for Vehicle");
        });

    vehicleMetadata.addConstructor(
        "Vehicle", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.size() == 3) {
                auto makePtr = args[0].tryCast<std::string>();
                auto modelPtr = args[1].tryCast<std::string>();
                auto yearPtr = args[2].tryCast<int>();
                if (makePtr && modelPtr && yearPtr) {
                    return BoxedValue(Vehicle{*makePtr, *modelPtr, *yearPtr});
                }
            }
            THROW_NOT_FOUND("Invalid constructor arguments for Vehicle");
        });

    // Register methods
    vehicleMetadata.addMethod(
        "toString", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (!args.empty()) {
                if (auto vehiclePtr = args[0].tryCast<Vehicle>()) {
                    return BoxedValue(vehiclePtr->toString());
                }
            }
            THROW_NOT_FOUND("Invalid arguments for toString method");
        });

    vehicleMetadata.addMethod(
        "drive", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (args.size() >= 2) {
                auto vehiclePtr = args[0].tryCast<Vehicle>();
                auto distancePtr = args[1].tryCast<double>();
                if (vehiclePtr && distancePtr) {
                    vehiclePtr->drive(*distancePtr);
                    return BoxedValue();  // Return void
                }
            }
            THROW_NOT_FOUND("Invalid arguments for drive method");
        });

    vehicleMetadata.addMethod(
        "isAntique", [](std::vector<BoxedValue> args) -> BoxedValue {
            if (!args.empty()) {
                if (auto vehiclePtr = args[0].tryCast<Vehicle>()) {
                    return BoxedValue(vehiclePtr->isAntique());
                }
            }
            THROW_NOT_FOUND("Invalid arguments for isAntique method");
        });

    // Register properties
    vehicleMetadata.addProperty(
        "make",
        [](const BoxedValue& obj) -> BoxedValue {  // Getter
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                return BoxedValue(vehiclePtr->getMake());
            }
            THROW_NOT_FOUND("Invalid object for make getter");
        },
        [](BoxedValue& obj, const BoxedValue& value) {  // Setter
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                if (auto makePtr = value.tryCast<std::string>()) {
                    vehiclePtr->setMake(*makePtr);
                    return;
                }
            }
            THROW_NOT_FOUND("Invalid object or value for make setter");
        },
        BoxedValue(std::string("Unknown")),  // Default value
        "Vehicle manufacturer name"          // Description
    );

    vehicleMetadata.addProperty(
        "model",
        [](const BoxedValue& obj) -> BoxedValue {  // Getter
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                return BoxedValue(vehiclePtr->getModel());
            }
            THROW_NOT_FOUND("Invalid object for model getter");
        },
        [](BoxedValue& obj, const BoxedValue& value) {  // Setter
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                if (auto modelPtr = value.tryCast<std::string>()) {
                    vehiclePtr->setModel(*modelPtr);
                    return;
                }
            }
            THROW_NOT_FOUND("Invalid object or value for model setter");
        },
        BoxedValue(std::string("Unknown")),  // Default value
        "Vehicle model name"                 // Description
    );

    vehicleMetadata.addProperty(
        "year",
        [](const BoxedValue& obj) -> BoxedValue {  // Getter
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                return BoxedValue(vehiclePtr->getYear());
            }
            THROW_NOT_FOUND("Invalid object for year getter");
        },
        [](BoxedValue& obj, const BoxedValue& value) {  // Setter
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                if (auto yearPtr = value.tryCast<int>()) {
                    vehiclePtr->setYear(*yearPtr);
                    return;
                }
            }
            THROW_NOT_FOUND("Invalid object or value for year setter");
        },
        BoxedValue(0),                // Default value
        "Vehicle manufacturing year"  // Description
    );

    vehicleMetadata.addProperty(
        "mileage",
        [](const BoxedValue& obj) -> BoxedValue {  // Getter
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                return BoxedValue(vehiclePtr->getMileage());
            }
            THROW_NOT_FOUND("Invalid object for mileage getter");
        },
        [](BoxedValue& obj, const BoxedValue& value) {  // Setter
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                if (auto mileagePtr = value.tryCast<double>()) {
                    vehiclePtr->setMileage(*mileagePtr);
                    return;
                }
            }
            THROW_NOT_FOUND("Invalid object or value for mileage setter");
        },
        BoxedValue(0.0),                     // Default value
        "Vehicle odometer reading in miles"  // Description
    );

    // Register events
    vehicleMetadata.addEvent("onCreate", "Triggered when a vehicle is created");
    vehicleMetadata.addEvent("onDrive", "Triggered when a vehicle is driven");
    vehicleMetadata.addEvent("onMaintenance",
                             "Triggered when a vehicle receives maintenance");

    // Add event listeners
    vehicleMetadata.addEventListener(
        "onCreate", [](BoxedValue& obj, const std::vector<BoxedValue>& args) {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                std::string id = !args.empty() && args[0].tryCast<std::string>()
                                     ? *args[0].tryCast<std::string>()
                                     : "unknown";
                std::cout << "[Vehicle Created] " << vehiclePtr->getMake()
                          << " " << vehiclePtr->getModel() << " ("
                          << vehiclePtr->getYear() << ")" << std::endl;
                EventLogger::logEvent("Created", "Vehicle", id);
            }
        });

    vehicleMetadata.addEventListener(
        "onDrive", [](BoxedValue& obj, const std::vector<BoxedValue>& args) {
            if (auto vehiclePtr = obj.tryCast<Vehicle>()) {
                double distance = args.size() > 0 && args[0].tryCast<double>()
                                      ? *args[0].tryCast<double>()
                                      : 0.0;
                std::cout << "[Vehicle Driven] " << vehiclePtr->getMake() << " "
                          << vehiclePtr->getModel() << " driven " << distance
                          << " miles" << std::endl;
            }
        });

    // Register type in the global registry
    TypeRegistry::instance().registerType("Vehicle",
                                          std::move(vehicleMetadata));
}

// Template based registration example using TypeRegistrar
template <>
class TypeRegistrar<std::string> {
public:
    static void registerType() {
        TypeMetadata stringMetadata;

        // Constructor for strings
        stringMetadata.addConstructor(
            "String", [](std::vector<BoxedValue> args) -> BoxedValue {
                if (args.empty()) {
                    return BoxedValue(std::string{});  // Empty string
                } else if (args.size() == 1) {
                    // Try to convert argument to string
                    if (auto strPtr = args[0].tryCast<std::string>()) {
                        return BoxedValue(std::string{*strPtr});
                    } else if (auto intPtr = args[0].tryCast<int>()) {
                        return BoxedValue(std::to_string(*intPtr));
                    } else if (auto doublePtr = args[0].tryCast<double>()) {
                        return BoxedValue(std::to_string(*doublePtr));
                    }
                }
                THROW_NOT_FOUND("Invalid constructor arguments for String");
            });

        // Methods
        stringMetadata.addMethod(
            "length", [](std::vector<BoxedValue> args) -> BoxedValue {
                if (!args.empty()) {
                    if (auto strPtr = args[0].tryCast<std::string>()) {
                        return BoxedValue(static_cast<int>(strPtr->length()));
                    }
                }
                THROW_NOT_FOUND("Invalid arguments for length method");
            });

        stringMetadata.addMethod(
            "toUpperCase", [](std::vector<BoxedValue> args) -> BoxedValue {
                if (!args.empty()) {
                    if (auto strPtr = args[0].tryCast<std::string>()) {
                        std::string result = *strPtr;
                        for (auto& c : result) {
                            c = std::toupper(c);
                        }
                        return BoxedValue(result);
                    }
                }
                THROW_NOT_FOUND("Invalid arguments for toUpperCase method");
            });

        stringMetadata.addMethod(
            "toLowerCase", [](std::vector<BoxedValue> args) -> BoxedValue {
                if (!args.empty()) {
                    if (auto strPtr = args[0].tryCast<std::string>()) {
                        std::string result = *strPtr;
                        for (auto& c : result) {
                            c = std::tolower(c);
                        }
                        return BoxedValue(result);
                    }
                }
                THROW_NOT_FOUND("Invalid arguments for toLowerCase method");
            });

        // Multiple overloads of the same method
        stringMetadata.addMethod(
            "substring", [](std::vector<BoxedValue> args) -> BoxedValue {
                // substring(start)
                if (args.size() == 2) {
                    auto strPtr = args[0].tryCast<std::string>();
                    auto startPtr = args[1].tryCast<int>();
                    if (strPtr && startPtr) {
                        try {
                            return BoxedValue(strPtr->substr(*startPtr));
                        } catch (const std::out_of_range& e) {
                            THROW_OUT_OF_RANGE("Substring index out of range");
                        }
                    }
                }
                THROW_NOT_FOUND(
                    "Invalid arguments for substring(start) method");
            });

        stringMetadata.addMethod(
            "substring", [](std::vector<BoxedValue> args) -> BoxedValue {
                // substring(start, length)
                if (args.size() == 3) {
                    auto strPtr = args[0].tryCast<std::string>();
                    auto startPtr = args[1].tryCast<int>();
                    auto lengthPtr = args[2].tryCast<int>();
                    if (strPtr && startPtr && lengthPtr) {
                        try {
                            return BoxedValue(
                                strPtr->substr(*startPtr, *lengthPtr));
                        } catch (const std::out_of_range& e) {
                            THROW_OUT_OF_RANGE("Substring index out of range");
                        }
                    }
                }
                THROW_NOT_FOUND(
                    "Invalid arguments for substring(start, length) method");
            });

        // Events
        stringMetadata.addEvent("onChange",
                                "Triggered when a string value changes");
        stringMetadata.addEventListener(
            "onChange",
            [](BoxedValue& obj, const std::vector<BoxedValue>& args) {
                if (auto strPtr = obj.tryCast<std::string>()) {
                    std::string oldValue =
                        args.size() > 0 && args[0].tryCast<std::string>()
                            ? *args[0].tryCast<std::string>()
                            : "";
                    std::cout << "[String Changed] From: '" << oldValue
                              << "' To: '" << *strPtr << "'" << std::endl;
                }
            });

        // Register type in the global registry
        TypeRegistry::instance().registerType("String",
                                              std::move(stringMetadata));
    }
};

int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "    AnyMeta Comprehensive Examples    " << std::endl;
    std::cout << "=========================================" << std::endl;

    //===========================================
    // 1. Type Registration
    //===========================================
    std::cout << "\n[1. Type Registration]\n" << std::endl;

    // Register our test types
    registerPersonType();
    registerVehicleType();
    TypeRegistrar<std::string>::registerType();

    std::cout << "Registered types: Person, Vehicle, String" << std::endl;

    //===========================================
    // 2. Object Creation
    //===========================================
    std::cout << "\n[2. Object Creation]\n" << std::endl;

    // Create Person objects using different constructors
    BoxedValue person1;
    BoxedValue person2;
    BoxedValue person3;

    try {
        // Default constructor
        person1 = createInstance("Person", {});
        std::cout << "Created person1 with default constructor" << std::endl;

        // Constructor with name and age
        person2 = createInstance(
            "Person", {BoxedValue(std::string("Alice")), BoxedValue(25)});
        std::cout << "Created person2 (Alice, 25)" << std::endl;

        // Constructor with name, age, and address
        person3 = createInstance(
            "Person", {BoxedValue(std::string("Bob")), BoxedValue(30),
                       BoxedValue(std::string("123 Main St"))});
        std::cout << "Created person3 (Bob, 30, 123 Main St)" << std::endl;

        // Fire creation events
        fireEvent(person1, "onCreate", {BoxedValue(std::string("person1"))});
        fireEvent(person2, "onCreate", {BoxedValue(std::string("person2"))});
        fireEvent(person3, "onCreate", {BoxedValue(std::string("person3"))});
    } catch (const std::exception& e) {
        std::cerr << "Error creating Person: " << e.what() << std::endl;
    }

    // Create Vehicle objects
    BoxedValue car;
    BoxedValue oldCar;

    try {
        car = createInstance(
            "Vehicle", {BoxedValue(std::string("Toyota")),
                        BoxedValue(std::string("Camry")), BoxedValue(2023)});
        std::cout << "Created Toyota Camry (2023)" << std::endl;

        oldCar = createInstance(
            "Vehicle", {BoxedValue(std::string("Ford")),
                        BoxedValue(std::string("Model T")), BoxedValue(1920)});
        std::cout << "Created Ford Model T (1920)" << std::endl;

        // Fire creation events
        fireEvent(car, "onCreate", {BoxedValue(std::string("car"))});
        fireEvent(oldCar, "onCreate", {BoxedValue(std::string("oldCar"))});
    } catch (const std::exception& e) {
        std::cerr << "Error creating Vehicle: " << e.what() << std::endl;
    }

    // Create String objects
    BoxedValue str1;
    BoxedValue str2;

    try {
        str1 =
            createInstance("String", {BoxedValue(std::string("Hello World"))});
        std::cout << "Created string: " << *str1.tryCast<std::string>()
                  << std::endl;

        str2 = createInstance("String", {BoxedValue(42)});
        std::cout << "Created string from int: " << *str2.tryCast<std::string>()
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error creating String: " << e.what() << std::endl;
    }

    //===========================================
    // 3. Method Calls
    //===========================================
    std::cout << "\n[3. Method Calls]\n" << std::endl;

    // Call toString() on Person objects
    try {
        BoxedValue result = callMethod(person2, "toString", {person2});
        if (auto strResult = result.tryCast<std::string>()) {
            std::cout << "person2.toString(): " << *strResult << std::endl;
        }

        result = callMethod(person3, "toString", {person3});
        if (auto strResult = result.tryCast<std::string>()) {
            std::cout << "person3.toString(): " << *strResult << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error calling toString: " << e.what() << std::endl;
    }

    // Call celebrateBirthday() on a Person
    try {
        BoxedValue result = callMethod(person2, "celebrateBirthday", {person2});
        // Fire birthday event
        fireEvent(person2, "onBirthday", {});

        // Get the age after birthday
        result = callMethod(person2, "isAdult", {person2});
        if (auto boolResult = result.tryCast<bool>()) {
            std::cout << "Is person2 an adult? " << (*boolResult ? "Yes" : "No")
                      << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error calling celebrateBirthday: " << e.what()
                  << std::endl;
    }

    // Call drive() on Vehicle objects
    try {
        callMethod(car, "drive", {car, BoxedValue(100.5)});
        callMethod(car, "drive", {car, BoxedValue(50.3)});

        // Fire onDrive event
        fireEvent(car, "onDrive", {BoxedValue(150.8)});

        // Check if oldCar is an antique
        BoxedValue result = callMethod(oldCar, "isAntique", {oldCar});
        if (auto boolResult = result.tryCast<bool>()) {
            std::cout << "Is the Ford Model T an antique? "
                      << (*boolResult ? "Yes" : "No") << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error calling vehicle methods: " << e.what() << std::endl;
    }
    // Call string methods
    try {
        BoxedValue result = callMethod(str1, "length", {str1});
        if (auto intResult = result.tryCast<int>()) {
            std::cout << "Length of str1: " << *intResult << std::endl;
        }

        result = callMethod(str1, "toUpperCase", {str1});
        if (auto strResult = result.tryCast<std::string>()) {
            std::cout << "Uppercase str1: " << *strResult << std::endl;
        }

        result = callMethod(str1, "substring", {str1, BoxedValue(6)});
        if (auto strResult = result.tryCast<std::string>()) {
            std::cout << "Substring of str1 from index 6: " << *strResult
                      << std::endl;
        }

        result =
            callMethod(str1, "substring", {str1, BoxedValue(0), BoxedValue(5)});
        if (auto strResult = result.tryCast<std::string>()) {
            std::cout << "Substring of str1 from index 0 with length 5: "
                      << *strResult << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error calling string methods: " << e.what() << std::endl;
    }
    //===========================================
    // 4. Property Access
    //===========================================
    std::cout << "\n[4. Property Access]\n" << std::endl;
    // Access and modify properties of Person
    try {
        // Access properties
        BoxedValue name = getProperty(person2, "name");
        BoxedValue age = getProperty(person2, "age");
        BoxedValue address = getProperty(person2, "address");

        std::cout << "person2 - Name: " << *name.tryCast<std::string>()
                  << ", Age: " << *age.tryCast<int>()
                  << ", Address: " << *address.tryCast<std::string>()
                  << std::endl;

        // Modify properties
        setProperty(person2, "name", BoxedValue(std::string("Alice Johnson")));
        setProperty(person2, "age", BoxedValue(26));
        setProperty(person2, "address", BoxedValue(std::string("456 Elm St")));

        // Access modified properties
        name = getProperty(person2, "name");
        age = getProperty(person2, "age");
        address = getProperty(person2, "address");

        std::cout << "Modified person2 - Name: " << *name.tryCast<std::string>()
                  << ", Age: " << *age.tryCast<int>()
                  << ", Address: " << *address.tryCast<std::string>()
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error accessing/modifying properties: " << e.what()
                  << std::endl;
    }
    // Access and modify properties of Vehicle
    try {
        // Access properties
        BoxedValue make = getProperty(car, "make");
        BoxedValue model = getProperty(car, "model");
        BoxedValue year = getProperty(car, "year");
        BoxedValue mileage = getProperty(car, "mileage");

        std::cout << "car - Make: " << *make.tryCast<std::string>()
                  << ", Model: " << *model.tryCast<std::string>()
                  << ", Year: " << *year.tryCast<int>()
                  << ", Mileage: " << *mileage.tryCast<double>() << std::endl;

        // Modify properties
        setProperty(car, "make", BoxedValue(std::string("Honda")));
        setProperty(car, "model", BoxedValue(std::string("Civic")));
        setProperty(car, "year", BoxedValue(2024));
        setProperty(car, "mileage", BoxedValue(1500.5));

        // Access modified properties
        make = getProperty(car, "make");
        model = getProperty(car, "model");
        year = getProperty(car, "year");
        mileage = getProperty(car, "mileage");

        std::cout << "Modified car - Make: " << *make.tryCast<std::string>()
                  << ", Model: " << *model.tryCast<std::string>()
                  << ", Year: " << *year.tryCast<int>()
                  << ", Mileage: " << *mileage.tryCast<double>() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error accessing/modifying properties: " << e.what()
                  << std::endl;
    }
    // Access and modify properties of String
    try {
        // Access properties
        BoxedValue length = getProperty(str1, "length");
        std::cout << "str1 - Length: " << *length.tryCast<int>() << std::endl;

        // Modify property (not directly supported, but we can create a new
        // string)
        setProperty(str1, "value", BoxedValue(std::string("New Value")));

        // Access modified property
        BoxedValue newValue = getProperty(str1, "value");
        std::cout << "Modified str1 - Value: "
                  << *newValue.tryCast<std::string>() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error accessing/modifying properties: " << e.what()
                  << std::endl;
    }
    //===========================================
    // 5. Event Handling
    //===========================================
    std::cout << "\n[5. Event Handling]\n" << std::endl;
    // Fire events for Person
    try {
        fireEvent(person2, "onUpdate",
                  {BoxedValue(std::string("person2")),
                   BoxedValue(std::string("name"))});
        fireEvent(person2, "onUpdate",
                  {BoxedValue(std::string("person2")),
                   BoxedValue(std::string("age"))});
        fireEvent(person2, "onUpdate",
                  {BoxedValue(std::string("person2")),
                   BoxedValue(std::string("address"))});
    } catch (const std::exception& e) {
        std::cerr << "Error firing events: " << e.what() << std::endl;
    }
    // Fire events for Vehicle
    try {
        fireEvent(car, "onMaintenance", {BoxedValue(std::string("car"))});
        fireEvent(car, "onDrive", {BoxedValue(100.5)});
    } catch (const std::exception& e) {
        std::cerr << "Error firing events: " << e.what() << std::endl;
    }
    // Fire events for String
    try {
        fireEvent(str1, "onChange", {BoxedValue(std::string("old value"))});
    } catch (const std::exception& e) {
        std::cerr << "Error firing events: " << e.what() << std::endl;
    }
    //===========================================
    // 6. Cleanup
    //===========================================
    std::cout << "\n[6. Cleanup]\n" << std::endl;
    // Cleanup is handled by the TypeRegistry destructor
    // and the BoxedValue destructor
    std::cout << "Cleanup complete." << std::endl;
    std::cout << "=========================================" << std::endl;
    return 0;
}
