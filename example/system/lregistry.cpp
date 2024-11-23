#include "atom/system/lregistry.hpp"

#include <iostream>

using namespace atom::system;

int main() {
    // Create a Registry object
    Registry registry;

    // Load registry data from a file
    registry.loadRegistryFromFile();
    std::cout << "Registry data loaded from file" << std::endl;

    // Create a new key in the registry
    registry.createKey("Software\\MyApp");
    std::cout << "Created key: Software\\MyApp" << std::endl;

    // Set a value for a key in the registry
    registry.setValue("Software\\MyApp", "Version", "1.0.0");
    std::cout << "Set value: Version = 1.0.0 for key: Software\\MyApp"
              << std::endl;

    // Get the value associated with a key and value name from the registry
    std::string version = registry.getValue("Software\\MyApp", "Version");
    std::cout << "Retrieved value: Version = " << version << std::endl;

    // Check if a key exists in the registry
    bool keyExists = registry.keyExists("Software\\MyApp");
    std::cout << "Key exists: " << std::boolalpha << keyExists << std::endl;

    // Check if a value exists for a key in the registry
    bool valueExists = registry.valueExists("Software\\MyApp", "Version");
    std::cout << "Value exists: " << std::boolalpha << valueExists << std::endl;

    // Retrieve all value names for a given key from the registry
    auto valueNames = registry.getValueNames("Software\\MyApp");
    std::cout << "Value names for key Software\\MyApp:" << std::endl;
    for (const auto& name : valueNames) {
        std::cout << name << std::endl;
    }

    // Delete a value from a key in the registry
    registry.deleteValue("Software\\MyApp", "Version");
    std::cout << "Deleted value: Version from key: Software\\MyApp"
              << std::endl;

    // Delete a key from the registry
    registry.deleteKey("Software\\MyApp");
    std::cout << "Deleted key: Software\\MyApp" << std::endl;

    // Backup the registry data
    registry.backupRegistryData();
    std::cout << "Registry data backed up" << std::endl;

    // Restore the registry data from a backup file
    registry.restoreRegistryData("backup_file.reg");
    std::cout << "Registry data restored from backup file" << std::endl;

    return 0;
}