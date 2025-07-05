#include "atom/system/wregistry.hpp"

#include <iostream>

int main() {
#ifdef _WIN32
    using namespace atom::system;
    HKEY hRootKey = HKEY_CURRENT_USER;
    std::string_view subKey = "Software\\Example";

    // Get all subkey names under the specified registry key
    std::vector<std::string> subKeys;
    bool success = getRegistrySubKeys(hRootKey, subKey, subKeys);
    std::cout << "Get subkeys success: " << std::boolalpha << success
              << std::endl;
    for (const auto& key : subKeys) {
        std::cout << "Subkey: " << key << std::endl;
    }

    // Get all value names and data under the specified registry key
    std::vector<std::pair<std::string, std::string>> values;
    success = getRegistryValues(hRootKey, subKey, values);
    std::cout << "Get values success: " << std::boolalpha << success
              << std::endl;
    for (const auto& [name, data] : values) {
        std::cout << "Value name: " << name << ", Data: " << data << std::endl;
    }

    // Modify the data of a specified value under the specified registry key
    success = modifyRegistryValue(hRootKey, subKey, "ExampleValue", "NewData");
    std::cout << "Modify value success: " << std::boolalpha << success
              << std::endl;

    // Delete the specified subkey and all its subkeys
    success = deleteRegistrySubKey(hRootKey, "Software\\Example\\SubKey");
    std::cout << "Delete subkey success: " << std::boolalpha << success
              << std::endl;

    // Delete the specified value under the specified registry key
    success = deleteRegistryValue(hRootKey, subKey, "ExampleValue");
    std::cout << "Delete value success: " << std::boolalpha << success
              << std::endl;

    // Recursively enumerate all subkeys and values under the specified registry
    // key
    recursivelyEnumerateRegistrySubKeys(hRootKey, subKey);
    std::cout << "Recursively enumerated subkeys and values" << std::endl;

    // Backup the specified registry key and all its subkeys and values
    success = backupRegistry(hRootKey, subKey, "C:\\backup.reg");
    std::cout << "Backup registry success: " << std::boolalpha << success
              << std::endl;

    // Recursively find subkey names containing the specified string
    findRegistryKey(hRootKey, subKey, "SearchKey");
    std::cout << "Recursively searched for subkeys containing 'SearchKey'"
              << std::endl;

    // Recursively find value names and data containing the specified string
    findRegistryValue(hRootKey, subKey, "SearchValue");
    std::cout << "Recursively searched for values containing 'SearchValue'"
              << std::endl;

    // Export the specified registry key and all its subkeys and values to a REG
    // file
    success = exportRegistry(hRootKey, subKey, "C:\\export.reg");
    std::cout << "Export registry success: " << std::boolalpha << success
              << std::endl;
#endif

    return 0;
}
