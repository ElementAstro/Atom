#include "xml.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

#include <algorithm>
#include <execution>

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/exception/all.hpp>
#include <boost/format.hpp>
#endif

namespace atom::utils {

XMLReader::XMLReader(std::string_view filePath) {
    LOG_F(INFO, "Loading XML file: {}", filePath);
    try {
        tinyxml2::XMLError loadResult =
            doc_.LoadFile(std::string(filePath).c_str());
        if (loadResult != tinyxml2::XML_SUCCESS) {
            std::string errorMsg =
                "Failed to load XML file: " + std::string(filePath) +
                ". TinyXML2 Error: " + doc_.ErrorStr();
            LOG_F(ERROR, "{}", errorMsg);
            THROW_RUNTIME_ERROR(errorMsg);
        }
        LOG_F(INFO, "Successfully loaded XML file: {}", filePath);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during XML file loading: {}", e.what());
        THROW_RUNTIME_ERROR(std::string("XML loading error: ") + e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown exception during XML file loading");
        THROW_RUNTIME_ERROR("Unknown error during XML file loading");
    }
}

auto XMLReader::getChildElementNames(std::string_view parentElementName) const
    -> Result<std::vector<std::string>> {
    LOG_F(INFO, "Getting child element names for parent: {}",
          parentElementName);
    std::vector<std::string> childElementNames;

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        const tinyxml2::XMLElement* parentElement =
            doc_.FirstChildElement(std::string(parentElementName).c_str());

        if (parentElement == nullptr) {
            LOG_F(WARNING, "Parent element '{}' not found.", parentElementName);
            return Result<std::vector<std::string>>{
                "Parent element '" + std::string(parentElementName) +
                "' not found."};
        }

        // Count child elements first to reserve memory
        size_t childCount = 0;
        for (const tinyxml2::XMLElement* element =
                 parentElement->FirstChildElement();
             element != nullptr; element = element->NextSiblingElement()) {
            childCount++;
        }

        childElementNames.reserve(childCount);

        for (const tinyxml2::XMLElement* element =
                 parentElement->FirstChildElement();
             element != nullptr; element = element->NextSiblingElement()) {
            childElementNames.emplace_back(element->Name());
        }

        LOG_F(INFO, "Found {} child elements for parent: {}",
              childElementNames.size(), parentElementName);
        return childElementNames;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getChildElementNames: {}", e.what());
        // Here, T is std::vector<std::string> and error alternative is
        // std::string.
        return Result<std::vector<std::string>>{"Exception: " +
                                                std::string(e.what())};
    }
}

auto XMLReader::getElementText(std::string_view elementName) const
    -> Result<std::string> {
    LOG_F(INFO, "Getting text for element: {}", elementName);

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        const tinyxml2::XMLElement* element =
            doc_.FirstChildElement(std::string(elementName).c_str());

        if (element == nullptr) {
            LOG_F(WARNING, "Element '{}' not found.", elementName);
            return Result<std::string>{
                std::in_place_index<1>,
                "Element '" + std::string(elementName) + "' not found."};
        }

        const char* text = element->GetText();
        if (text == nullptr) {
            LOG_F(WARNING, "Element '{}' contains no text.", elementName);
            return Result<std::string>{
                std::in_place_index<1>,
                "Element '" + std::string(elementName) + "' contains no text."};
        }

        return Result<std::string>{std::in_place_index<0>, std::string(text)};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getElementText: {}", e.what());
        return Result<std::string>{std::in_place_index<1>,
                                   std::string("Exception: ") + e.what()};
    }
}

auto XMLReader::getAttributeValue(std::string_view elementName,
                                  std::string_view attributeName) const
    -> Result<std::string> {
    LOG_F(INFO, "Getting attribute value for element: {}, attribute: {}",
          elementName, attributeName);

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        const tinyxml2::XMLElement* element =
            doc_.FirstChildElement(std::string(elementName).c_str());

        if (element == nullptr) {
            LOG_F(WARNING, "Element '{}' not found.", elementName);
            return Result<std::string>{
                std::in_place_index<1>,
                "Element '" + std::string(elementName) + "' not found."};
        }

        const char* attrValue =
            element->Attribute(std::string(attributeName).c_str());
        if (attrValue == nullptr) {
            LOG_F(WARNING, "Attribute '{}' not found in element '{}'.",
                  attributeName, elementName);
            return Result<std::string>{std::in_place_index<1>,
                                       "Attribute '" +
                                           std::string(attributeName) +
                                           "' not found in element '" +
                                           std::string(elementName) + "'."};
        }

        return Result<std::string>{std::in_place_index<0>,
                                   std::string(attrValue)};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getAttributeValue: {}", e.what());
        return Result<std::string>{std::in_place_index<1>,
                                   std::string("Exception: ") + e.what()};
    }
}

auto XMLReader::getRootElementNames() const -> std::vector<std::string> {
    LOG_F(INFO, "Getting root element names");
    std::vector<std::string> rootElementNames;

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        const tinyxml2::XMLElement* rootElement = doc_.RootElement();

        if (rootElement != nullptr) {
            rootElementNames.emplace_back(rootElement->Name());
        } else {
            LOG_F(WARNING, "No root element found in the XML document.");
        }

        LOG_F(INFO, "Found {} root elements", rootElementNames.size());
        return rootElementNames;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getRootElementNames: {}", e.what());
        return {};  // Return empty vector on error
    }
}

auto XMLReader::hasChildElement(std::string_view parentElementName,
                                std::string_view childElementName) const
    -> bool {
    LOG_F(INFO, "Checking if parent element: {} has child element: {}",
          parentElementName, childElementName);

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        const tinyxml2::XMLElement* parentElement =
            doc_.FirstChildElement(std::string(parentElementName).c_str());

        if (parentElement == nullptr) {
            LOG_F(WARNING, "Parent element '{}' not found.", parentElementName);
            return false;
        }

        bool hasChild = parentElement->FirstChildElement(
                            std::string(childElementName).c_str()) != nullptr;

        LOG_F(INFO, "Parent element '{}' has child element '{}': {}",
              parentElementName, childElementName, hasChild);
        return hasChild;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in hasChildElement: {}", e.what());
        return false;
    }
}

auto XMLReader::getChildElementText(std::string_view parentElementName,
                                    std::string_view childElementName) const
    -> Result<std::string> {
    LOG_F(INFO, "Getting text for child element: {} of parent element: {}",
          childElementName, parentElementName);

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        const tinyxml2::XMLElement* parentElement =
            doc_.FirstChildElement(std::string(parentElementName).c_str());

        if (parentElement == nullptr) {
            LOG_F(WARNING, "Parent element '{}' not found.", parentElementName);
            return Result<std::string>{std::in_place_index<1>,
                                       "Parent element '" +
                                           std::string(parentElementName) +
                                           "' not found."};
        }

        const tinyxml2::XMLElement* childElement =
            parentElement->FirstChildElement(
                std::string(childElementName).c_str());

        if (childElement == nullptr) {
            LOG_F(WARNING, "Child element '{}' not found under parent '{}'.",
                  childElementName, parentElementName);
            return Result<std::string>{
                std::in_place_index<1>,
                "Child element '" + std::string(childElementName) +
                    "' not found under parent '" +
                    std::string(parentElementName) + "'."};
        }

        const char* text = childElement->GetText();
        if (text == nullptr) {
            LOG_F(WARNING,
                  "Child element '{}' under parent '{}' contains no text.",
                  childElementName, parentElementName);
            return Result<std::string>{
                std::in_place_index<1>,
                "Child element '" + std::string(childElementName) +
                    "' under parent '" + std::string(parentElementName) +
                    "' contains no text."};
        }

        return Result<std::string>{std::in_place_index<0>, std::string(text)};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getChildElementText: {}", e.what());
        return Result<std::string>{std::in_place_index<1>,
                                   std::string("Exception: ") + e.what()};
    }
}

auto XMLReader::getChildElementAttributeValue(
    std::string_view parentElementName, std::string_view childElementName,
    std::string_view attributeName) const -> Result<std::string> {
    LOG_F(INFO,
          "Getting attribute value for child element: {} of parent element: "
          "{}, attribute: {}",
          childElementName, parentElementName, attributeName);

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        const tinyxml2::XMLElement* parentElement =
            doc_.FirstChildElement(std::string(parentElementName).c_str());

        if (parentElement == nullptr) {
            LOG_F(WARNING, "Parent element '{}' not found.", parentElementName);
            return Result<std::string>{std::in_place_index<1>,
                                       "Parent element '" +
                                           std::string(parentElementName) +
                                           "' not found."};
        }

        const tinyxml2::XMLElement* childElement =
            parentElement->FirstChildElement(
                std::string(childElementName).c_str());

        if (childElement == nullptr) {
            LOG_F(WARNING, "Child element '{}' not found under parent '{}'.",
                  childElementName, parentElementName);
            return Result<std::string>{
                std::in_place_index<1>,
                "Child element '" + std::string(childElementName) +
                    "' not found under parent '" +
                    std::string(parentElementName) + "'."};
        }

        const char* attrValue =
            childElement->Attribute(std::string(attributeName).c_str());
        if (attrValue == nullptr) {
            LOG_F(WARNING,
                  "Attribute '{}' not found in child element '{}' under parent "
                  "'{}'.",
                  attributeName, childElementName, parentElementName);
            return Result<std::string>{
                std::in_place_index<1>,
                "Attribute '" + std::string(attributeName) +
                    "' not found in child element '" +
                    std::string(childElementName) + "' under parent '" +
                    std::string(parentElementName) + "'."};
        }

        return Result<std::string>{std::in_place_index<0>,
                                   std::string(attrValue)};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getChildElementAttributeValue: {}",
              e.what());
        return Result<std::string>{std::in_place_index<1>,
                                   std::string("Exception: ") + e.what()};
    }
}

auto XMLReader::getValueByPath(std::string_view path) const
    -> Result<std::string> {
    LOG_F(INFO, "Getting value by path: {}", path);

    if (!isValidPath(path)) {
        LOG_F(WARNING, "Invalid path format: {}", path);
        return Result<std::string>{std::in_place_index<1>,
                                   "Invalid path format: " + std::string(path)};
    }

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        tinyxml2::XMLElement* element = getElementByPath(path);

        if (element == nullptr) {
            LOG_F(WARNING, "Element at path '{}' not found.", path);
            return Result<std::string>{
                std::in_place_index<1>,
                "Element at path '" + std::string(path) + "' not found."};
        }

        const char* text = element->GetText();
        if (text == nullptr) {
            LOG_F(WARNING, "Element at path '{}' contains no text.", path);
            return Result<std::string>{std::in_place_index<1>,
                                       "Element at path '" + std::string(path) +
                                           "' contains no text."};
        }

        return Result<std::string>{std::in_place_index<0>, std::string(text)};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getValueByPath: {}", e.what());
        return Result<std::string>{std::in_place_index<1>,
                                   std::string("Exception: ") + e.what()};
    }
}

auto XMLReader::getAttributeValueByPath(std::string_view path,
                                        std::string_view attributeName) const
    -> Result<std::string> {
    LOG_F(INFO, "Getting attribute value by path: {}, attribute: {}", path,
          attributeName);

    if (!isValidPath(path)) {
        LOG_F(WARNING, "Invalid path format: {}", path);
        return Result<std::string>{std::in_place_index<1>,
                                   "Invalid path format: " + std::string(path)};
    }

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        tinyxml2::XMLElement* element = getElementByPath(path);

        if (element == nullptr) {
            LOG_F(WARNING, "Element at path '{}' not found.", path);
            return Result<std::string>{
                std::in_place_index<1>,
                "Element at path '" + std::string(path) + "' not found."};
        }

        const char* attrValue =
            element->Attribute(std::string(attributeName).c_str());
        if (attrValue == nullptr) {
            LOG_F(WARNING, "Attribute '{}' not found in element at path '{}'.",
                  attributeName, path);
            return Result<std::string>{std::in_place_index<1>,
                                       "Attribute '" +
                                           std::string(attributeName) +
                                           "' not found in element at path '" +
                                           std::string(path) + "'."};
        }

        return Result<std::string>{std::in_place_index<0>,
                                   std::string(attrValue)};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getAttributeValueByPath: {}", e.what());
        return Result<std::string>{std::in_place_index<1>,
                                   std::string("Exception: ") + e.what()};
    }
}

bool XMLReader::hasChildElementByPath(std::string_view path,
                                      std::string_view childElementName) const {
    LOG_F(INFO, "Checking if path: {} has child element: {}", path,
          childElementName);

    if (!isValidPath(path)) {
        LOG_F(WARNING, "Invalid path format: {}", path);
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        tinyxml2::XMLElement* element = getElementByPath(path);

        if (element == nullptr) {
            LOG_F(WARNING, "Element at path '{}' not found.", path);
            return false;
        }

        bool hasChild = element->FirstChildElement(
                            std::string(childElementName).c_str()) != nullptr;

        LOG_F(INFO, "Element at path '{}' has child element '{}': {}", path,
              childElementName, hasChild);
        return hasChild;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in hasChildElementByPath: {}", e.what());
        return false;
    }
}

auto XMLReader::getChildElementTextByPath(
    std::string_view path,
    std::string_view childElementName) const -> Result<std::string> {
    LOG_F(INFO, "Getting text for child element: {} by path: {}",
          childElementName, path);

    if (!isValidPath(path)) {
        LOG_F(WARNING, "Invalid path format: {}", path);
        return Result<std::string>{std::in_place_index<1>,
                                   "Invalid path format: " + std::string(path)};
    }

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        tinyxml2::XMLElement* element = getElementByPath(path);

        if (element == nullptr) {
            LOG_F(WARNING, "Element at path '{}' not found.", path);
            return Result<std::string>{
                std::in_place_index<1>,
                "Element at path '" + std::string(path) + "' not found."};
        }

        tinyxml2::XMLElement* childElement =
            element->FirstChildElement(std::string(childElementName).c_str());

        if (childElement == nullptr) {
            LOG_F(WARNING, "Child element '{}' not found under path '{}'.",
                  childElementName, path);
            return Result<std::string>{
                std::in_place_index<1>,
                "Child element '" + std::string(childElementName) +
                    "' not found under path '" + std::string(path) + "'."};
        }

        const char* text = childElement->GetText();
        if (text == nullptr) {
            LOG_F(WARNING,
                  "Child element '{}' under path '{}' contains no text.",
                  childElementName, path);
            return Result<std::string>{
                std::in_place_index<1>,
                "Child element '" + std::string(childElementName) +
                    "' under path '" + std::string(path) +
                    "' contains no text."};
        }

        return Result<std::string>{std::in_place_index<0>, std::string(text)};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getChildElementTextByPath: {}", e.what());
        return Result<std::string>{std::in_place_index<1>,
                                   std::string("Exception: ") + e.what()};
    }
}

auto XMLReader::getChildElementAttributeValueByPath(
    std::string_view path, std::string_view childElementName,
    std::string_view attributeName) const -> Result<std::string> {
    LOG_F(INFO,
          "Getting attribute value for child element: {} by path: {}, "
          "attribute: {}",
          childElementName, path, attributeName);

    if (!isValidPath(path)) {
        LOG_F(WARNING, "Invalid path format: {}", path);
        return Result<std::string>{std::in_place_index<1>,
                                   "Invalid path format: " + std::string(path)};
    }

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        tinyxml2::XMLElement* element = getElementByPath(path);

        if (element == nullptr) {
            LOG_F(WARNING, "Element at path '{}' not found.", path);
            return Result<std::string>{
                std::in_place_index<1>,
                "Element at path '" + std::string(path) + "' not found."};
        }

        tinyxml2::XMLElement* childElement =
            element->FirstChildElement(std::string(childElementName).c_str());

        if (childElement == nullptr) {
            LOG_F(WARNING, "Child element '{}' not found under path '{}'.",
                  childElementName, path);
            return Result<std::string>{
                std::in_place_index<1>,
                "Child element '" + std::string(childElementName) +
                    "' not found under path '" + std::string(path) + "'."};
        }

        const char* attrValue =
            childElement->Attribute(std::string(attributeName).c_str());
        if (attrValue == nullptr) {
            LOG_F(WARNING,
                  "Attribute '{}' not found in child element '{}' under path "
                  "'{}'.",
                  attributeName, childElementName, path);
            return Result<std::string>{
                std::in_place_index<1>,
                "Attribute '" + std::string(attributeName) +
                    "' not found in child element '" +
                    std::string(childElementName) + "' under path '" +
                    std::string(path) + "'."};
        }

        return Result<std::string>{std::in_place_index<0>,
                                   std::string(attrValue)};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getChildElementAttributeValueByPath: {}",
              e.what());
        return Result<std::string>{std::in_place_index<1>,
                                   std::string("Exception: ") + e.what()};
    }
}

auto XMLReader::saveToFile(std::string_view filePath) const -> Result<bool> {
    LOG_F(INFO, "Saving XML to file: {}", filePath);

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        tinyxml2::XMLError saveResult =
            doc_.SaveFile(std::string(filePath).c_str());

        if (saveResult != tinyxml2::XML_SUCCESS) {
            std::string errorMsg =
                "Failed to save XML file: " + std::string(filePath) +
                ". TinyXML2 Error: " + doc_.ErrorStr();
            LOG_F(ERROR, "{}", errorMsg);
            return Result<bool>{std::in_place_index<1>, errorMsg};
        }

        LOG_F(INFO, "Successfully saved XML file: {}", filePath);
        return Result<bool>{std::in_place_index<0>, true};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in saveToFile: {}", e.what());
        return Result<bool>{std::in_place_index<1>,
                            std::string("Exception: ") + e.what()};
    }
}

auto XMLReader::getValuesByPathsAsync(const std::vector<std::string>& paths)
    const -> std::future<std::vector<Result<std::string>>> {
    return std::async(std::launch::async, [this, paths]() {
        std::vector<Result<std::string>> results(paths.size());

        // Use parallel execution if there are enough paths to process
        if (paths.size() > 4) {
            std::transform(std::execution::par_unseq, paths.begin(),
                           paths.end(), results.begin(),
                           [this](const std::string& path) {
                               return this->getValueByPath(path);
                           });
        } else {
            // Serial execution for small number of paths
            for (size_t i = 0; i < paths.size(); ++i) {
                results[i] = this->getValueByPath(paths[i]);
            }
        }
        return results;
    });
}

}  // namespace atom::utils