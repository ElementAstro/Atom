#include "atom/utils/anyutils.hpp"

#include <iostream>
#include <tuple>
#include <unordered_map>
#include <vector>

int main() {
    // Example usage of toString with a built-in type
    int intValue = 42;
    std::string intStr = toString(intValue);
    std::cout << "Integer to string: " << intStr << std::endl;

    // Example usage of toString with a container
    std::vector<int> intVector = {1, 2, 3, 4, 5};
    std::string vectorStr = toString(intVector);
    std::cout << "Vector to string: " << vectorStr << std::endl;

    // Example usage of toString with a map
    std::unordered_map<std::string, int> intMap = {
        {"one", 1}, {"two", 2}, {"three", 3}};
    std::string mapStr = toString(intMap);
    std::cout << "Map to string: " << mapStr << std::endl;

    // Example usage of toString with a pair
    std::pair<std::string, int> strIntPair = {"example", 123};
    std::string pairStr = toString(strIntPair);
    std::cout << "Pair to string: " << pairStr << std::endl;

    // Example usage of toJson with a built-in type
    std::string intJson = toJson(intValue);
    std::cout << "Integer to JSON: " << intJson << std::endl;

    // Example usage of toJson with a container
    std::string vectorJson = toJson(intVector);
    std::cout << "Vector to JSON: " << vectorJson << std::endl;

    // Example usage of toJson with a map
    std::string mapJson = toJson(intMap);
    std::cout << "Map to JSON: " << mapJson << std::endl;

    // Example usage of toJson with a pair
    std::string pairJson = toJson(strIntPair);
    std::cout << "Pair to JSON: " << pairJson << std::endl;

    // Example usage of toXml with a built-in type
    std::string intXml = toXml(intValue, "intValue");
    std::cout << "Integer to XML: " << intXml << std::endl;

    // Example usage of toXml with a container
    std::string vectorXml = toXml(intVector, "intVector");
    std::cout << "Vector to XML: " << vectorXml << std::endl;

    // Example usage of toXml with a map
    std::string mapXml = toXml(intMap, "intMap");
    std::cout << "Map to XML: " << mapXml << std::endl;

    // Example usage of toXml with a pair
    std::string pairXml = toXml(strIntPair, "strIntPair");
    std::cout << "Pair to XML: " << pairXml << std::endl;

    // Example usage of toYaml with a built-in type
    std::string intYaml = toYaml(intValue, "intValue");
    std::cout << "Integer to YAML: " << intYaml << std::endl;

    // Example usage of toYaml with a container
    std::string vectorYaml = toYaml(intVector, "intVector");
    std::cout << "Vector to YAML: " << vectorYaml << std::endl;

    // Example usage of toYaml with a map
    std::string mapYaml = toYaml(intMap, "intMap");
    std::cout << "Map to YAML: " << mapYaml << std::endl;

    // Example usage of toYaml with a pair
    std::string pairYaml = toYaml(strIntPair, "strIntPair");
    std::cout << "Pair to YAML: " << pairYaml << std::endl;

    // Example usage of toYaml with a tuple
    std::tuple<int, std::string, double> tupleValue = {1, "example", 3.14};
    std::string tupleYaml = toYaml(tupleValue, "tupleValue");
    std::cout << "Tuple to YAML: " << tupleYaml << std::endl;

    // Example usage of toToml with a built-in type
    std::string intToml = toToml(intValue, "intValue");
    std::cout << "Integer to TOML: " << intToml << std::endl;

    // Example usage of toToml with a container
    std::string vectorToml = toToml(intVector, "intVector");
    std::cout << "Vector to TOML: " << vectorToml << std::endl;

    // Example usage of toToml with a map
    std::string mapToml = toToml(intMap, "intMap");
    std::cout << "Map to TOML: " << mapToml << std::endl;

    // Example usage of toToml with a pair
    std::string pairToml = toToml(strIntPair, "strIntPair");
    std::cout << "Pair to TOML: " << pairToml << std::endl;

    // Example usage of toToml with a tuple
    std::string tupleToml = toToml(tupleValue, "tupleValue");
    std::cout << "Tuple to TOML: " << tupleToml << std::endl;

    return 0;
}