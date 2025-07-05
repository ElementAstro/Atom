#include "atom/utils/xml.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Helper function to print results with better formatting
template <typename T>
void printResult(const std::string& description,
                 const atom::utils::Result<T>& result) {
    std::cout << description << ": ";

    if (result.index() == 0) {
        if constexpr (std::is_same_v<T, std::string>) {
            std::cout << "\"" << std::get<0>(result) << "\"" << std::endl;
        } else if constexpr (std::is_same_v<T, bool>) {
            std::cout << (std::get<0>(result) ? "true" : "false") << std::endl;
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            const auto& vec = std::get<0>(result);
            std::cout << "[";
            for (size_t i = 0; i < vec.size(); ++i) {
                std::cout << "\"" << vec[i] << "\"";
                if (i < vec.size() - 1)
                    std::cout << ", ";
            }
            std::cout << "]" << std::endl;
        }
    } else {
        std::cout << "ERROR: " << std::get<1>(result) << std::endl;
    }
}

// Create a sample XML file for demonstration
bool createSampleXML(const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to create sample XML file: " << filePath
                  << std::endl;
        return false;
    }

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<root>\n";
    file << "    <configuration version=\"1.0\" author=\"Example\">\n";
    file << "        <database>\n";
    file << "            <connection type=\"mysql\">\n";
    file << "                <host>localhost</host>\n";
    file << "                <port>3306</port>\n";
    file << "                <username>admin</username>\n";
    file << "                <password>secret</password>\n";
    file << "            </connection>\n";
    file << "            <tables>\n";
    file << "                <table name=\"users\" primary_key=\"id\">\n";
    file << "                    <columns>id, name, email, "
            "created_at</columns>\n";
    file << "                </table>\n";
    file << "                <table name=\"products\" "
            "primary_key=\"product_id\">\n";
    file << "                    <columns>product_id, title, price, "
            "stock</columns>\n";
    file << "                </table>\n";
    file << "            </tables>\n";
    file << "        </database>\n";
    file << "        <logging>\n";
    file << "            <level>debug</level>\n";
    file << "            <file>/var/log/app.log</file>\n";
    file << "            <rotation size=\"10MB\" count=\"5\" />\n";
    file << "        </logging>\n";
    file << "    </configuration>\n";
    file << "    <data>\n";
    file << "        <user id=\"1\">\n";
    file << "            <name>John Doe</name>\n";
    file << "            <email>john@example.com</email>\n";
    file << "            <role>admin</role>\n";
    file << "        </user>\n";
    file << "        <user id=\"2\">\n";
    file << "            <name>Jane Smith</name>\n";
    file << "            <email>jane@example.com</email>\n";
    file << "            <role>user</role>\n";
    file << "        </user>\n";
    file << "    </data>\n";
    file << "</root>\n";

    file.close();
    return true;
}

int main() {
    // Section 0: Setup and file creation
    std::cout << "===== XMLReader Example =====" << std::endl << std::endl;

    // Create a sample XML file for our demonstration
    std::string sampleFile = "example.xml";
    if (!createSampleXML(sampleFile)) {
        return 1;
    }

    std::cout << "Created sample XML file: " << sampleFile << std::endl
              << std::endl;

    try {
        // Section 1: Basic XML Reading
        std::cout << "===== Section 1: Basic XML Reading =====" << std::endl;

        // Create an XMLReader instance
        atom::utils::XMLReader reader(sampleFile);

        // Get root element names
        std::cout << "Root elements: ";
        std::vector<std::string> rootElements = reader.getRootElementNames();
        for (size_t i = 0; i < rootElements.size(); ++i) {
            std::cout << "\"" << rootElements[i] << "\"";
            if (i < rootElements.size() - 1)
                std::cout << ", ";
        }
        std::cout << std::endl << std::endl;

        // Section 2: Element and Attribute Access
        std::cout << "===== Section 2: Element and Attribute Access ====="
                  << std::endl;

        // Get child element names
        auto childElements = reader.getChildElementNames("root");
        printResult("Child elements of 'root'", childElements);

        // Get element text
        auto configVersion =
            reader.getAttributeValue("configuration", "version");
        printResult("Configuration version", configVersion);

        // Get attribute value
        auto configAuthor = reader.getAttributeValue("configuration", "author");
        printResult("Configuration author", configAuthor);

        // Check if an element has a child element
        bool hasLogging = reader.hasChildElement("configuration", "logging");
        std::cout << "Has logging element: " << (hasLogging ? "true" : "false")
                  << std::endl;

        // Get child element text
        auto logLevel = reader.getChildElementText("logging", "level");
        printResult("Logging level", logLevel);

        // Get child element attribute value
        auto rotationSize =
            reader.getChildElementAttributeValue("logging", "rotation", "size");
        printResult("Log rotation size", rotationSize);

        std::cout << std::endl;

        // Section 3: Path-based Access
        std::cout << "===== Section 3: Path-based Access =====" << std::endl;

        // Get value by path
        auto hostValue = reader.getValueByPath(
            "root/configuration/database/connection/host");
        printResult("Database host", hostValue);

        // Get attribute value by path
        auto tableNameAttr = reader.getAttributeValueByPath(
            "root/configuration/database/tables/table", "name");
        printResult("First table name", tableNameAttr);

        // Check if a path has a child element
        bool hasColumns = reader.hasChildElementByPath(
            "root/configuration/database/tables/table", "columns");
        std::cout << "Table has columns element: "
                  << (hasColumns ? "true" : "false") << std::endl;

        // Get child element text by path
        auto columnsText = reader.getChildElementTextByPath(
            "root/configuration/database/tables/table", "columns");
        printResult("Table columns", columnsText);

        // Get child element attribute value by path
        auto primaryKey = reader.getChildElementAttributeValueByPath(
            "root/configuration/database/tables", "table", "primary_key");
        printResult("Table primary key", primaryKey);

        std::cout << std::endl;

        // Section 4: Handling Non-existent Elements
        std::cout << "===== Section 4: Handling Non-existent Elements ====="
                  << std::endl;

        // Try to access a non-existent element
        auto nonExistentElement = reader.getElementText("non_existent");
        printResult("Non-existent element", nonExistentElement);

        // Try to access a non-existent attribute
        auto nonExistentAttr =
            reader.getAttributeValue("configuration", "non_existent");
        printResult("Non-existent attribute", nonExistentAttr);

        // Try to access a non-existent path
        auto nonExistentPath = reader.getValueByPath("root/invalid/path");
        printResult("Non-existent path", nonExistentPath);

        std::cout << std::endl;

        // Section 5: Saving XML
        std::cout << "===== Section 5: Saving XML =====" << std::endl;

        // Save the XML to a new file
        std::string newFile = "example_copy.xml";
        auto saveResult = reader.saveToFile(newFile);

        if (std::holds_alternative<bool>(saveResult) &&
            std::get<0>(saveResult)) {
            std::cout << "Successfully saved XML to: " << newFile << std::endl;

            // Verify the new file exists
            bool fileExists = std::filesystem::exists(newFile);
            std::cout << "File exists: " << (fileExists ? "true" : "false")
                      << std::endl;

            // Clean up the copied file
            std::filesystem::remove(newFile);
        } else {
            std::cout << "Failed to save XML: " << std::get<1>(saveResult)
                      << std::endl;
        }

        std::cout << std::endl;

        // Section 6: Asynchronous Operations
        std::cout << "===== Section 6: Asynchronous Operations ====="
                  << std::endl;

        // Create a list of paths to retrieve values for
        std::vector<std::string> paths = {
            "root/configuration/database/connection/host",
            "root/configuration/database/connection/port",
            "root/configuration/database/connection/username",
            "root/configuration/database/connection/password",
            "root/configuration/logging/level",
            "root/configuration/logging/file"};

        std::cout << "Retrieving values asynchronously for " << paths.size()
                  << " paths..." << std::endl;

        // Start async operation
        auto futureResults = reader.getValuesByPathsAsync(paths);

        // Simulate doing other work while waiting for results
        std::cout << "Performing other tasks while waiting for results..."
                  << std::endl;
        for (int i = 0; i < 3; ++i) {
            std::cout << "Working... " << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::cout << "still working..." << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::cout << " done!" << std::endl;
        }

        // Get the async results
        auto asyncResults = futureResults.get();

        std::cout << "\nAsync results:" << std::endl;
        for (size_t i = 0; i < paths.size(); ++i) {
            std::cout << "  " << std::left << std::setw(45) << paths[i] << ": ";

            if (asyncResults[i].index() == 0) {
                std::cout << "\"" << std::get<0>(asyncResults[i]) << "\""
                          << std::endl;
            } else {
                std::cout << "ERROR: " << std::get<1>(asyncResults[i])
                          << std::endl;
            }
        }

        std::cout << std::endl;

        // Section 7: Error Handling - Invalid XML File
        std::cout << "===== Section 7: Error Handling =====" << std::endl;

        std::cout << "Attempting to load a non-existent file:" << std::endl;
        try {
            atom::utils::XMLReader invalidReader("non_existent_file.xml");
        } catch (const std::exception& e) {
            std::cout << "Caught exception: " << e.what() << std::endl;
        }

        // Create an invalid XML file for testing
        std::string invalidFile = "invalid.xml";
        {
            std::ofstream file(invalidFile);
            file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            file << "<root>\n";
            file << "    <unclosed_element>\n";  // Missing closing tag
            file << "</root>\n";
            file.close();
        }

        std::cout << "\nAttempting to work with an invalid XML file:"
                  << std::endl;
        try {
            atom::utils::XMLReader invalidReader(invalidFile);
            auto result = invalidReader.getChildElementNames("root");
            printResult("Child elements of 'root' in invalid XML", result);
        } catch (const std::exception& e) {
            std::cout << "Caught exception: " << e.what() << std::endl;
        }

        // Clean up the invalid file
        std::filesystem::remove(invalidFile);

        std::cout << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception caught in main: " << e.what() << std::endl;
    }

    // Clean up the sample file
    std::filesystem::remove(sampleFile);

    std::cout << "Example completed successfully!" << std::endl;
    return 0;
}
