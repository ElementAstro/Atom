/*
 * advanced_bindings_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Advanced Bindings Example
Demonstrates class binding, operator overloading, exception translation,
and advanced binding features with the component system.

**************************************************/

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "atom/components/advanced_bindings.hpp"
#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"

using namespace atom::components;

/**
 * @brief Math utility class for binding demonstration
 */
class MathUtils {
public:
    MathUtils() : precision_(6) {
        std::cout << "MathUtils created with precision: " << precision_
                  << std::endl;
    }

    explicit MathUtils(int precision) : precision_(precision) {
        std::cout << "MathUtils created with custom precision: " << precision_
                  << std::endl;
    }

    ~MathUtils() { std::cout << "MathUtils destroyed" << std::endl; }

    // Basic operations
    double add(double a, double b) const { return a + b; }

    double multiply(double a, double b) const { return a * b; }

    double power(double base, double exponent) const {
        return std::pow(base, exponent);
    }

    double sqrt(double value) const {
        if (value < 0) {
            throw std::invalid_argument(
                "Cannot calculate square root of negative number");
        }
        return std::sqrt(value);
    }

    // Vector operations
    std::vector<double> addVectors(const std::vector<double>& a,
                                   const std::vector<double>& b) const {
        if (a.size() != b.size()) {
            throw std::invalid_argument("Vector sizes must match");
        }

        std::vector<double> result;
        result.reserve(a.size());

        for (size_t i = 0; i < a.size(); ++i) {
            result.push_back(a[i] + b[i]);
        }

        return result;
    }

    double dotProduct(const std::vector<double>& a,
                      const std::vector<double>& b) const {
        if (a.size() != b.size()) {
            throw std::invalid_argument(
                "Vector sizes must match for dot product");
        }

        double result = 0.0;
        for (size_t i = 0; i < a.size(); ++i) {
            result += a[i] * b[i];
        }

        return result;
    }

    // Properties
    int getPrecision() const { return precision_; }
    void setPrecision(int precision) {
        if (precision < 0 || precision > 15) {
            throw std::out_of_range("Precision must be between 0 and 15");
        }
        precision_ = precision;
    }

    // Static methods
    static double pi() { return 3.14159265358979323846; }
    static double e() { return 2.71828182845904523536; }

    // Operator overloading
    MathUtils operator+(const MathUtils& other) const {
        return MathUtils(precision_ + other.precision_);
    }

    bool operator==(const MathUtils& other) const {
        return precision_ == other.precision_;
    }

    // String representation
    std::string toString() const {
        return "MathUtils(precision=" + std::to_string(precision_) + ")";
    }

private:
    int precision_;
};

/**
 * @brief 3D Vector class for advanced binding features
 */
class Vector3D {
public:
    Vector3D() : x_(0), y_(0), z_(0) {}
    Vector3D(double x, double y, double z) : x_(x), y_(y), z_(z) {}

    // Getters and setters
    double getX() const { return x_; }
    double getY() const { return y_; }
    double getZ() const { return z_; }

    void setX(double x) { x_ = x; }
    void setY(double y) { y_ = y; }
    void setZ(double z) { z_ = z; }

    // Vector operations
    Vector3D add(const Vector3D& other) const {
        return Vector3D(x_ + other.x_, y_ + other.y_, z_ + other.z_);
    }

    Vector3D subtract(const Vector3D& other) const {
        return Vector3D(x_ - other.x_, y_ - other.y_, z_ - other.z_);
    }

    Vector3D multiply(double scalar) const {
        return Vector3D(x_ * scalar, y_ * scalar, z_ * scalar);
    }

    double dot(const Vector3D& other) const {
        return x_ * other.x_ + y_ * other.y_ + z_ * other.z_;
    }

    Vector3D cross(const Vector3D& other) const {
        return Vector3D(y_ * other.z_ - z_ * other.y_,
                        z_ * other.x_ - x_ * other.z_,
                        x_ * other.y_ - y_ * other.x_);
    }

    double magnitude() const { return std::sqrt(x_ * x_ + y_ * y_ + z_ * z_); }

    Vector3D normalize() const {
        double mag = magnitude();
        if (mag == 0) {
            throw std::runtime_error("Cannot normalize zero vector");
        }
        return Vector3D(x_ / mag, y_ / mag, z_ / mag);
    }

    // Operator overloading
    Vector3D operator+(const Vector3D& other) const { return add(other); }

    Vector3D operator-(const Vector3D& other) const { return subtract(other); }

    Vector3D operator*(double scalar) const { return multiply(scalar); }

    bool operator==(const Vector3D& other) const {
        const double epsilon = 1e-9;
        return std::abs(x_ - other.x_) < epsilon &&
               std::abs(y_ - other.y_) < epsilon &&
               std::abs(z_ - other.z_) < epsilon;
    }

    // String representation
    std::string toString() const {
        return "Vector3D(" + std::to_string(x_) + ", " + std::to_string(y_) +
               ", " + std::to_string(z_) + ")";
    }

private:
    double x_, y_, z_;
};

/**
 * @brief Component that demonstrates advanced bindings
 */
class AdvancedBindingsComponent : public Component {
public:
    explicit AdvancedBindingsComponent(const std::string& name)
        : Component(name) {
        std::cout << "AdvancedBindingsComponent '" << name << "' created"
                  << std::endl;

        // Initialize with some data
        mathUtils_ = std::make_shared<MathUtils>(8);
        position_ = std::make_shared<Vector3D>(1.0, 2.0, 3.0);
        velocity_ = std::make_shared<Vector3D>(0.1, 0.2, 0.3);

        setupBindings();
    }

private:
    std::shared_ptr<MathUtils> mathUtils_;
    std::shared_ptr<Vector3D> position_;
    std::shared_ptr<Vector3D> velocity_;

    void setupBindings() {
        // Note: AdvancedBinder class is not implemented in the current codebase
        // Commenting out the binding code to allow compilation
        /*
        // Bind MathUtils class
        auto& binder = AdvancedBinder::instance();

        // Register MathUtils class
        binder.registerClass<MathUtils>("MathUtils")
            .constructor<>()
            .constructor<int>()
            .method("add", &MathUtils::add)
            .method("multiply", &MathUtils::multiply)
            .method("power", &MathUtils::power)
            .method("sqrt", &MathUtils::sqrt)
            .method("addVectors", &MathUtils::addVectors)
            .method("dotProduct", &MathUtils::dotProduct)
            .property("precision", &MathUtils::getPrecision,
                      &MathUtils::setPrecision)
            .staticMethod("pi", &MathUtils::pi)
            .staticMethod("e", &MathUtils::e)
            .method("toString", &MathUtils::toString);

        // Register Vector3D class
        binder.registerClass<Vector3D>("Vector3D")
            .constructor<>()
            .constructor<double, double, double>()
            .property("x", &Vector3D::getX, &Vector3D::setX)
            .property("y", &Vector3D::getY, &Vector3D::setY)
            .property("z", &Vector3D::getZ, &Vector3D::setZ)
            .method("add", &Vector3D::add)
            .method("subtract", &Vector3D::subtract)
            .method("multiply", &Vector3D::multiply)
            .method("dot", &Vector3D::dot)
            .method("cross", &Vector3D::cross)
            .method("magnitude", &Vector3D::magnitude)
            .method("normalize", &Vector3D::normalize)
            .method("toString", &Vector3D::toString);
        */

        // Register component commands that use bound classes
        def("getMathUtils",
            [this]() -> std::shared_ptr<MathUtils> { return mathUtils_; });

        def("getPosition",
            [this]() -> std::shared_ptr<Vector3D> { return position_; });

        def("getVelocity",
            [this]() -> std::shared_ptr<Vector3D> { return velocity_; });

        def("updatePosition", [this](double deltaTime) {
            auto displacement = velocity_->multiply(deltaTime);
            position_ =
                std::make_shared<Vector3D>(position_->getX() + displacement.getX(),
                                         position_->getY() + displacement.getY(),
                                         position_->getZ() + displacement.getZ());
            std::cout << "  [" << getName()
                      << "] Position updated to: " << position_->toString()
                      << std::endl;
        });

        def("calculateDistance",
            [this](double x, double y, double z) -> double {
                Vector3D target(x, y, z);
                Vector3D diff = position_->subtract(target);
                return diff.magnitude();
            });

        def("performMathOperation",
            [this](const std::string& operation, double a, double b) -> double {
                if (operation == "add") {
                    return mathUtils_->add(a, b);
                } else if (operation == "multiply") {
                    return mathUtils_->multiply(a, b);
                } else if (operation == "power") {
                    return mathUtils_->power(a, b);
                } else {
                    throw std::invalid_argument("Unknown operation: " +
                                                operation);
                }
            });
    }
};

void demonstrateBasicBindings() {
    std::cout << "\n=== Basic Bindings Demo ===" << std::endl;

    auto& registry = Registry::instance();
    // auto& binder = AdvancedBinder::instance(); // AdvancedBinder not implemented

    std::cout << "\n1. Creating component with bound classes..." << std::endl;
    auto component =
        registry.createComponent<AdvancedBindingsComponent>("BindingsDemo");

    std::cout << "\n2. Testing bound class access..." << std::endl;

    // Get bound objects from component
    try {
        auto mathUtils = std::any_cast<std::shared_ptr<MathUtils>>(
            component->runCommand("getMathUtils", {}));

        if (mathUtils) {
            std::cout << "MathUtils object: " << mathUtils->toString()
                      << std::endl;
            std::cout << "Pi constant: " << MathUtils::pi() << std::endl;
            std::cout << "E constant: " << MathUtils::e() << std::endl;

            // Test math operations
            double result1 = mathUtils->add(5.5, 3.2);
            std::cout << "5.5 + 3.2 = " << result1 << std::endl;

            double result2 = mathUtils->power(2.0, 8.0);
            std::cout << "2^8 = " << result2 << std::endl;

            // Test vector operations
            std::vector<double> vec1 = {1.0, 2.0, 3.0};
            std::vector<double> vec2 = {4.0, 5.0, 6.0};
            auto vecSum = mathUtils->addVectors(vec1, vec2);

            std::cout << "Vector addition result: [";
            for (size_t i = 0; i < vecSum.size(); ++i) {
                std::cout << vecSum[i];
                if (i < vecSum.size() - 1)
                    std::cout << ", ";
            }
            std::cout << "]" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Error accessing MathUtils: " << e.what() << std::endl;
    }
}

void demonstrateVectorBindings() {
    std::cout << "\n=== Vector Bindings Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("BindingsDemo");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n3. Testing Vector3D bindings..." << std::endl;

    try {
        auto position = std::any_cast<std::shared_ptr<Vector3D>>(
            component->runCommand("getPosition", {}));
        auto velocity = std::any_cast<std::shared_ptr<Vector3D>>(
            component->runCommand("getVelocity", {}));

        if (position && velocity) {
            std::cout << "Initial position: " << position->toString()
                      << std::endl;
            std::cout << "Velocity: " << velocity->toString() << std::endl;

            // Test vector operations
            auto sum = position->add(*velocity);
            std::cout << "Position + Velocity: " << sum.toString() << std::endl;

            auto cross = position->cross(*velocity);
            std::cout << "Position × Velocity: " << cross.toString()
                      << std::endl;

            double dot = position->dot(*velocity);
            std::cout << "Position · Velocity: " << dot << std::endl;

            double magnitude = position->magnitude();
            std::cout << "Position magnitude: " << magnitude << std::endl;

            // Test normalization
            auto normalized = position->normalize();
            std::cout << "Normalized position: " << normalized.toString()
                      << std::endl;
            std::cout << "Normalized magnitude: " << normalized.magnitude()
                      << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Error accessing Vector3D: " << e.what() << std::endl;
    }
}

void demonstrateExceptionHandling() {
    std::cout << "\n=== Exception Handling Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("BindingsDemo");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n4. Testing exception translation..." << std::endl;

    // Test math exceptions
    std::cout << "\n--- Math Exceptions ---" << std::endl;
    try {
        auto mathUtils = std::any_cast<std::shared_ptr<MathUtils>>(
            component->runCommand("getMathUtils", {}));

        if (mathUtils) {
            // Test square root of negative number
            std::cout << "Testing sqrt(-1)..." << std::endl;
            double result = mathUtils->sqrt(-1.0);
            std::cout << "Unexpected success: " << result << std::endl;
        }
    } catch (const std::invalid_argument& e) {
        std::cout << "Expected invalid_argument: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Other exception: " << e.what() << std::endl;
    }

    // Test precision range exception
    try {
        auto mathUtils = std::any_cast<std::shared_ptr<MathUtils>>(
            component->runCommand("getMathUtils", {}));

        if (mathUtils) {
            std::cout << "Testing invalid precision (20)..." << std::endl;
            mathUtils->setPrecision(20);
            std::cout << "Unexpected success" << std::endl;
        }
    } catch (const std::out_of_range& e) {
        std::cout << "Expected out_of_range: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Other exception: " << e.what() << std::endl;
    }

    // Test vector exceptions
    std::cout << "\n--- Vector Exceptions ---" << std::endl;
    try {
        Vector3D zeroVector(0, 0, 0);
        std::cout << "Testing normalize of zero vector..." << std::endl;
        auto normalized = zeroVector.normalize();
        std::cout << "Unexpected success: " << normalized.toString()
                  << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "Expected runtime_error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Other exception: " << e.what() << std::endl;
    }

    // Test component command exceptions
    std::cout << "\n--- Component Command Exceptions ---" << std::endl;
    try {
        std::cout << "Testing unknown math operation..." << std::endl;
        std::vector<std::any> args = {std::string("unknown"), std::string("5"), std::string("3")};
        auto result = component->runCommand("performMathOperation", args);
        std::cout << "Unexpected success" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }
}

void demonstrateOperatorOverloading() {
    std::cout << "\n=== Operator Overloading Demo ===" << std::endl;

    std::cout << "\n5. Testing operator overloading..." << std::endl;

    // Test Vector3D operators
    std::cout << "\n--- Vector3D Operators ---" << std::endl;
    Vector3D v1(1.0, 2.0, 3.0);
    Vector3D v2(4.0, 5.0, 6.0);

    std::cout << "v1: " << v1.toString() << std::endl;
    std::cout << "v2: " << v2.toString() << std::endl;

    auto sum = v1 + v2;
    std::cout << "v1 + v2: " << sum.toString() << std::endl;

    auto diff = v1 - v2;
    std::cout << "v1 - v2: " << diff.toString() << std::endl;

    auto scaled = v1 * 2.5;
    std::cout << "v1 * 2.5: " << scaled.toString() << std::endl;

    bool equal = v1 == v2;
    std::cout << "v1 == v2: " << (equal ? "true" : "false") << std::endl;

    Vector3D v3(1.0, 2.0, 3.0);
    bool equal2 = v1 == v3;
    std::cout << "v1 == v3: " << (equal2 ? "true" : "false") << std::endl;

    // Test MathUtils operators
    std::cout << "\n--- MathUtils Operators ---" << std::endl;
    MathUtils m1(5);
    MathUtils m2(3);

    std::cout << "m1: " << m1.toString() << std::endl;
    std::cout << "m2: " << m2.toString() << std::endl;

    auto m3 = m1 + m2;
    std::cout << "m1 + m2: " << m3.toString() << std::endl;

    bool mathEqual = m1 == m2;
    std::cout << "m1 == m2: " << (mathEqual ? "true" : "false") << std::endl;
}

void demonstrateAdvancedFeatures() {
    std::cout << "\n=== Advanced Features Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("BindingsDemo");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n6. Testing advanced binding features..." << std::endl;

    // Test component integration
    std::cout << "\n--- Component Integration ---" << std::endl;

    // Update position using bound objects
    std::cout << "Updating position with deltaTime = 0.5..." << std::endl;
    std::vector<std::any> updateArgs = {std::string("0.5")};
    component->runCommand("updatePosition", updateArgs);

    // Calculate distance to a target
    std::vector<std::any> distanceArgs = {std::string("5.0"), std::string("5.0"), std::string("5.0")};
    double distance = std::stod(std::any_cast<std::string>(
        component->runCommand("calculateDistance", distanceArgs)));
    std::cout << "Distance to (5, 5, 5): " << distance << std::endl;

    // Test math operations through component
    std::cout << "\n--- Math Operations Through Component ---" << std::endl;

    std::vector<std::any> addArgs = {"add", "10.5", "7.3"};
    auto addResult = component->runCommand("performMathOperation", addArgs);
    double addValue = std::any_cast<double>(addResult);
    std::cout << "10.5 + 7.3 = " << addValue << std::endl;

    std::vector<std::any> multiplyArgs = {"multiply", "4.2", "3.1"};
    auto multiplyResult = component->runCommand("performMathOperation", multiplyArgs);
    double multiplyValue = std::any_cast<double>(multiplyResult);
    std::cout << "4.2 * 3.1 = " << multiplyValue << std::endl;

    std::vector<std::any> powerArgs = {"power", "3.0", "4.0"};
    auto powerResult = component->runCommand("performMathOperation", powerArgs);
    double powerValue = std::any_cast<double>(powerResult);
    std::cout << "3^4 = " << powerValue << std::endl;
}

void demonstrateBindingStatistics() {
    std::cout << "\n=== Binding Statistics Demo ===" << std::endl;

    // auto& binder = AdvancedBinder::instance(); // AdvancedBinder not implemented

    std::cout << "\n7. Binding system statistics..." << std::endl;
    std::cout << "AdvancedBinder not implemented in this version" << std::endl;
    std::cout << "Statistics functionality would be available with AdvancedBinder" << std::endl;
}

int main() {
    std::cout << "=== Atom Component Advanced Bindings Examples ==="
              << std::endl;

    try {
        demonstrateBasicBindings();
        demonstrateVectorBindings();
        demonstrateExceptionHandling();
        demonstrateOperatorOverloading();
        demonstrateAdvancedFeatures();
        demonstrateBindingStatistics();

        std::cout << "\n=== All Advanced Bindings Examples Completed "
                     "Successfully! ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in advanced bindings examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
