#include <iostream>
#include <memory>
#include <string>

// Include the component system headers
#include "core/component.hpp"
#include "core/registry.hpp"
#include "lifecycle/lifecycle.hpp"

// using namespace atom::components;

// Simple test component
class TestComponent : public Component {
public:
    explicit TestComponent(const std::string& name) : Component(name) {
        std::cout << "TestComponent created: " << name << std::endl;
    }
    
    ~TestComponent() {
        std::cout << "TestComponent destroyed: " << getName() << std::endl;
    }
    
    bool initialize() override {
        std::cout << "TestComponent initializing: " << getName() << std::endl;
        
        // Add some test variables
        addVariable("test_int", 42);
        addVariable("test_string", std::string("Hello, World!"));
        addVariable("test_double", 3.14159);
        
        return Component::initialize();
    }
    
    bool destroy() override {
        std::cout << "TestComponent destroying: " << getName() << std::endl;
        return Component::destroy();
    }
};

int main() {
    std::cout << "=== Atom Components System Test ===" << std::endl;
    
    try {
        // Test 1: Basic component creation
        std::cout << "\n1. Testing basic component creation..." << std::endl;
        auto component = std::make_shared<TestComponent>("TestComponent1");
        
        // Test 2: Component initialization
        std::cout << "\n2. Testing component initialization..." << std::endl;
        bool initResult = component->initialize();
        std::cout << "Initialization result: " << (initResult ? "SUCCESS" : "FAILED") << std::endl;
        
        // Test 3: Variable management
        std::cout << "\n3. Testing variable management..." << std::endl;
        
        // Get variables
        auto intVar = component->getVariable<int>("test_int");
        auto stringVar = component->getVariable<std::string>("test_string");
        auto doubleVar = component->getVariable<double>("test_double");
        
        std::cout << "test_int: " << intVar->get() << std::endl;
        std::cout << "test_string: " << stringVar->get() << std::endl;
        std::cout << "test_double: " << doubleVar->get() << std::endl;
        
        // Test 4: Registry operations
        std::cout << "\n4. Testing registry operations..." << std::endl;
        auto& registry = Registry::instance();
        
        // Register a component initializer
        registry.addInitializer("TestComponent2", 
            [](Component& comp) {
                std::cout << "Initializing " << comp.getName() << " via registry" << std::endl;
                comp.addVariable("registry_test", std::string("Registry works!"));
            },
            []() {
                std::cout << "Cleanup function called" << std::endl;
            }
        );
        
        // Get the component that was already created and initialized by the registry
        auto registryComponent = registry.getComponent("TestComponent2");
        if (registryComponent) {
            auto registryVar = registryComponent->getVariable<std::string>("registry_test");
            std::cout << "Registry variable: " << registryVar->get() << std::endl;
        } else {
            std::cout << "Failed to get component from registry" << std::endl;
        }
        
        // Test 5: Lifecycle management
        std::cout << "\n5. Testing lifecycle management..." << std::endl;
        auto& lifecycle = atom::components::LifecycleManager::instance();

        bool hookExecuted = false;
        lifecycle.registerHook("TestComponent1", atom::components::LifecyclePhase::PostInitialization,
            [&hookExecuted](Component& comp, atom::components::LifecyclePhase) {
                std::cout << "Lifecycle hook executed for " << comp.getName() << std::endl;
                hookExecuted = true;
            });

        bool phaseResult = lifecycle.executePhase(*component, atom::components::LifecyclePhase::PostInitialization);
        std::cout << "Lifecycle phase execution: " << (phaseResult ? "SUCCESS" : "FAILED") << std::endl;
        std::cout << "Hook executed: " << (hookExecuted ? "YES" : "NO") << std::endl;
        
        // Test 6: Component destruction
        std::cout << "\n6. Testing component destruction..." << std::endl;
        component->destroy();
        registryComponent->destroy();
        
        std::cout << "\n=== All tests completed successfully! ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
