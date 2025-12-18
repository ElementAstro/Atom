/**
 * @file xml_example.cpp
 * @brief Examples for atom::utils XMLReader
 */

#include <iostream>
#include <string>
#include "atom/utils/format/xml.hpp"

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateXMLReading() {
    printSection("1. XML Reading (requires XML file)");

    std::cout << "XMLReader usage example:" << std::endl;
    std::cout << R"(
    // Create reader from file
    XMLReader reader("config.xml");

    // Get element text
    auto text = reader.getElementText("title");
    if (auto* value = std::get_if<std::string>(&text)) {
        std::cout << "Title: " << *value << std::endl;
    }

    // Get attribute value
    auto attr = reader.getAttributeValue("server", "port");
    if (auto* value = std::get_if<std::string>(&attr)) {
        std::cout << "Port: " << *value << std::endl;
    }

    // Get child element names
    auto children = reader.getChildElementNames("settings");
    if (auto* names = std::get_if<std::vector<std::string>>(&children)) {
        for (const auto& name : *names) {
            std::cout << "  Child: " << name << std::endl;
        }
    }
    )" << std::endl;
}

void demonstrateXMLPaths() {
    printSection("2. XML Path Navigation");

    std::cout << "Path-based XML access:" << std::endl;
    std::cout << R"(
    XMLReader reader("data.xml");

    // Navigate using paths
    auto value = reader.getValueByPath("root/config/database/host");
    auto attr = reader.getAttributeValueByPath("root/config/database", "port");

    // Check for child elements
    bool hasChild = reader.hasChildElementByPath("root/config", "database");
    )" << std::endl;
}

void demonstrateAsyncXML() {
    printSection("3. Async XML Operations");

    std::cout << "Asynchronous XML reading:" << std::endl;
    std::cout << R"(
    XMLReader reader("large_data.xml");

    std::vector<std::string> paths = {
        "data/item1/value",
        "data/item2/value",
        "data/item3/value"
    };

    // Async batch retrieval
    auto future = reader.getValuesByPathsAsync(paths);

    // Do other work...

    // Get results
    auto results = future.get();
    for (const auto& result : results) {
        if (auto* value = std::get_if<std::string>(&result)) {
            std::cout << "Value: " << *value << std::endl;
        }
    }
    )" << std::endl;
}

void demonstrateSampleXML() {
    printSection("4. Sample XML Structure");

    std::cout << "Example XML file structure:" << std::endl;
    std::cout << R"(
<?xml version="1.0" encoding="UTF-8"?>
<config>
    <application name="MyApp" version="1.0">
        <title>My Application</title>
        <description>A sample application</description>
    </application>

    <database>
        <host>localhost</host>
        <port>5432</port>
        <name>mydb</name>
        <credentials>
            <username>admin</username>
            <password>secret</password>
        </credentials>
    </database>

    <features>
        <feature enabled="true">logging</feature>
        <feature enabled="false">debug</feature>
        <feature enabled="true">caching</feature>
    </features>
</config>
    )" << std::endl;
}

void demonstrateXMLOperations() {
    printSection("5. Common XML Operations");

    std::cout << "Common operations with XMLReader:" << std::endl;
    std::cout << R"(
    XMLReader reader("config.xml");

    // 1. Get root element names
    auto roots = reader.getRootElementNames();
    std::cout << "Root elements: " << roots.size() << std::endl;

    // 2. Check if element exists
    bool exists = reader.hasChildElement("config", "database");

    // 3. Get element text
    auto title = reader.getElementText("title");

    // 4. Get attribute
    auto version = reader.getAttributeValue("application", "version");

    // 5. Save modified XML
    auto saveResult = reader.saveToFile("output.xml");
    if (auto* success = std::get_if<bool>(&saveResult)) {
        if (*success) {
            std::cout << "Saved successfully" << std::endl;
        }
    }
    )" << std::endl;
}

void demonstrateErrorHandling() {
    printSection("6. Error Handling");

    std::cout << "Handling XML errors:" << std::endl;
    std::cout << R"(
    try {
        XMLReader reader("config.xml");

        auto result = reader.getElementText("nonexistent");

        // Check for error
        if (auto* error = std::get_if<std::string>(&result)) {
            std::cerr << "Error: " << *error << std::endl;
        } else if (auto* value = std::get_if<std::string>(&result)) {
            std::cout << "Value: " << *value << std::endl;
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed to load XML: " << e.what() << std::endl;
    }
    )" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  XMLReader Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    std::cout << "\nNote: These examples show API usage patterns." << std::endl;
    std::cout << "Actual execution requires valid XML files." << std::endl;

    try {
        demonstrateXMLReading();
        demonstrateXMLPaths();
        demonstrateAsyncXML();
        demonstrateSampleXML();
        demonstrateXMLOperations();
        demonstrateErrorHandling();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All XMLReader examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
