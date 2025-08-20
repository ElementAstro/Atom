#include "xml.hpp"

#include "atom/error/exception.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <execution>

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/exception/all.hpp>
#include <boost/format.hpp>
#endif

namespace atom::utils {

XMLReader::XMLReader(std::string_view filePath) {
    spdlog::info("Loading XML file: {}", filePath);
    try {
        auto loadResult = doc_.LoadFile(std::string(filePath).c_str());
        if (loadResult != tinyxml2::XML_SUCCESS) {
            auto errorMsg =
                std::format("Failed to load XML file: {}. TinyXML2 Error: {}",
                            filePath, doc_.ErrorStr());
            spdlog::error("{}", errorMsg);
            THROW_RUNTIME_ERROR(errorMsg);
        }
        spdlog::info("Successfully loaded XML file: {}", filePath);
    } catch (const std::exception& e) {
        spdlog::error("Exception during XML file loading: {}", e.what());
        THROW_RUNTIME_ERROR(std::format("XML loading error: {}", e.what()));
    } catch (...) {
        spdlog::error("Unknown exception during XML file loading");
        THROW_RUNTIME_ERROR("Unknown error during XML file loading");
    }
}

auto XMLReader::getChildElementNames(std::string_view parentElementName) const
    -> Result<std::vector<std::string>> {
    spdlog::debug("Getting child element names for parent: {}",
                  parentElementName);

    try {
        std::lock_guard lock(mutex_);
        auto parentElement =
            doc_.FirstChildElement(std::string(parentElementName).c_str());

        if (!parentElement) {
            auto errorMsg =
                std::format("Parent element '{}' not found", parentElementName);
            spdlog::warn("{}", errorMsg);
            return Result<std::vector<std::string>>{errorMsg};
        }

        std::vector<std::string> childElementNames;

        size_t childCount = 0;
        for (auto element = parentElement->FirstChildElement(); element;
             element = element->NextSiblingElement()) {
            ++childCount;
        }
        childElementNames.reserve(childCount);

        for (auto element = parentElement->FirstChildElement(); element;
             element = element->NextSiblingElement()) {
            childElementNames.emplace_back(element->Name());
        }

        spdlog::debug("Found {} child elements for parent: {}",
                      childElementNames.size(), parentElementName);
        return childElementNames;
    } catch (const std::exception& e) {
        auto errorMsg = std::format("Exception: {}", e.what());
        spdlog::error("Exception in getChildElementNames: {}", e.what());
        return Result<std::vector<std::string>>{errorMsg};
    }
}

auto XMLReader::getElementText(std::string_view elementName) const
    -> Result<std::string> {
    spdlog::debug("Getting text for element: {}", elementName);

    try {
        std::lock_guard lock(mutex_);
        auto element = doc_.FirstChildElement(std::string(elementName).c_str());

        if (!element) {
            auto errorMsg = std::format("Element '{}' not found", elementName);
            spdlog::warn("{}", errorMsg);
            return Result<std::string>{std::in_place_index<1>, errorMsg};
        }

        auto text = element->GetText();
        if (!text) {
            auto errorMsg =
                std::format("Element '{}' contains no text", elementName);
            spdlog::warn("{}", errorMsg);
            return Result<std::string>{std::in_place_index<1>, errorMsg};
        }

        return Result<std::string>{std::in_place_index<0>, std::string(text)};
    } catch (const std::exception& e) {
        auto errorMsg = std::format("Exception: {}", e.what());
        spdlog::error("Exception in getElementText: {}", e.what());
        return Result<std::string>{std::in_place_index<1>, errorMsg};
    }
}

auto XMLReader::getAttributeValue(std::string_view elementName,
                                  std::string_view attributeName) const
    -> Result<std::string> {
    spdlog::debug("Getting attribute value for element: {}, attribute: {}",
                  elementName, attributeName);

    try {
        std::lock_guard lock(mutex_);
        auto element = doc_.FirstChildElement(std::string(elementName).c_str());

        if (!element) {
            auto errorMsg = std::format("Element '{}' not found", elementName);
            spdlog::warn("{}", errorMsg);
            return Result<std::string>{std::in_place_index<1>, errorMsg};
        }

        auto attrValue = element->Attribute(std::string(attributeName).c_str());
        if (!attrValue) {
            auto errorMsg =
                std::format("Attribute '{}' not found in element '{}'",
                            attributeName, elementName);
            spdlog::warn("{}", errorMsg);
            return Result<std::string>{std::in_place_index<1>, errorMsg};
        }

        return Result<std::string>{std::in_place_index<0>,
                                   std::string(attrValue)};
    } catch (const std::exception& e) {
        auto errorMsg = std::format("Exception: {}", e.what());
        spdlog::error("Exception in getAttributeValue: {}", e.what());
        return Result<std::string>{std::in_place_index<1>, errorMsg};
    }
}

auto XMLReader::getRootElementNames() const -> std::vector<std::string> {
    spdlog::debug("Getting root element names");

    try {
        std::lock_guard lock(mutex_);
        auto rootElement = doc_.RootElement();

        std::vector<std::string> rootElementNames;
        if (rootElement) {
            rootElementNames.emplace_back(rootElement->Name());
        } else {
            spdlog::warn("No root element found in the XML document");
        }

        spdlog::debug("Found {} root elements", rootElementNames.size());
        return rootElementNames;
    } catch (const std::exception& e) {
        spdlog::error("Exception in getRootElementNames: {}", e.what());
        return {};
    }
}

auto XMLReader::hasChildElement(std::string_view parentElementName,
                                std::string_view childElementName) const
    -> bool {
    spdlog::debug("Checking if parent element: {} has child element: {}",
                  parentElementName, childElementName);

    try {
        std::lock_guard lock(mutex_);
        auto parentElement =
            doc_.FirstChildElement(std::string(parentElementName).c_str());

        if (!parentElement) {
            spdlog::warn("Parent element '{}' not found", parentElementName);
            return false;
        }

        bool hasChild = parentElement->FirstChildElement(
                            std::string(childElementName).c_str()) != nullptr;

        spdlog::debug("Parent element '{}' has child element '{}': {}",
                      parentElementName, childElementName, hasChild);
        return hasChild;
    } catch (const std::exception& e) {
        spdlog::error("Exception in hasChildElement: {}", e.what());
        return false;
    }
}

auto XMLReader::getValueByPath(std::string_view path) const
    -> Result<std::string> {
    spdlog::debug("Getting value by path: {}", path);

    if (!isValidPath(path)) {
        auto errorMsg = std::format("Invalid path format: {}", path);
        spdlog::warn("{}", errorMsg);
        return Result<std::string>{std::in_place_index<1>, errorMsg};
    }

    try {
        std::lock_guard lock(mutex_);
        auto element = getElementByPath(path);

        if (!element) {
            auto errorMsg = std::format("Element at path '{}' not found", path);
            spdlog::warn("{}", errorMsg);
            return Result<std::string>{std::in_place_index<1>, errorMsg};
        }

        auto text = element->GetText();
        if (!text) {
            auto errorMsg =
                std::format("Element at path '{}' contains no text", path);
            spdlog::warn("{}", errorMsg);
            return Result<std::string>{std::in_place_index<1>, errorMsg};
        }

        return Result<std::string>{std::in_place_index<0>, std::string(text)};
    } catch (const std::exception& e) {
        auto errorMsg = std::format("Exception: {}", e.what());
        spdlog::error("Exception in getValueByPath: {}", e.what());
        return Result<std::string>{std::in_place_index<1>, errorMsg};
    }
}

auto XMLReader::getAttributeValueByPath(std::string_view path,
                                        std::string_view attributeName) const
    -> Result<std::string> {
    spdlog::debug("Getting attribute value by path: {}, attribute: {}", path,
                  attributeName);

    if (!isValidPath(path)) {
        auto errorMsg = std::format("Invalid path format: {}", path);
        spdlog::warn("{}", errorMsg);
        return Result<std::string>{std::in_place_index<1>, errorMsg};
    }

    try {
        std::lock_guard lock(mutex_);
        auto element = getElementByPath(path);

        if (!element) {
            auto errorMsg = std::format("Element at path '{}' not found", path);
            spdlog::warn("{}", errorMsg);
            return Result<std::string>{std::in_place_index<1>, errorMsg};
        }

        auto attrValue = element->Attribute(std::string(attributeName).c_str());
        if (!attrValue) {
            auto errorMsg =
                std::format("Attribute '{}' not found in element at path '{}'",
                            attributeName, path);
            spdlog::warn("{}", errorMsg);
            return Result<std::string>{std::in_place_index<1>, errorMsg};
        }

        return Result<std::string>{std::in_place_index<0>,
                                   std::string(attrValue)};
    } catch (const std::exception& e) {
        auto errorMsg = std::format("Exception: {}", e.what());
        spdlog::error("Exception in getAttributeValueByPath: {}", e.what());
        return Result<std::string>{std::in_place_index<1>, errorMsg};
    }
}

auto XMLReader::hasChildElementByPath(std::string_view path,
                                      std::string_view childElementName) const
    -> bool {
    spdlog::debug("Checking if path: {} has child element: {}", path,
                  childElementName);

    if (!isValidPath(path)) {
        spdlog::warn("Invalid path format: {}", path);
        return false;
    }

    try {
        std::lock_guard lock(mutex_);
        auto element = getElementByPath(path);

        if (!element) {
            spdlog::warn("Element at path '{}' not found", path);
            return false;
        }

        bool hasChild = element->FirstChildElement(
                            std::string(childElementName).c_str()) != nullptr;

        spdlog::debug("Element at path '{}' has child element '{}': {}", path,
                      childElementName, hasChild);
        return hasChild;
    } catch (const std::exception& e) {
        spdlog::error("Exception in hasChildElementByPath: {}", e.what());
        return false;
    }
}

auto XMLReader::saveToFile(std::string_view filePath) const -> Result<bool> {
    spdlog::info("Saving XML to file: {}", filePath);

    try {
        std::lock_guard lock(mutex_);
        auto saveResult = doc_.SaveFile(std::string(filePath).c_str());

        if (saveResult != tinyxml2::XML_SUCCESS) {
            auto errorMsg =
                std::format("Failed to save XML file: {}. TinyXML2 Error: {}",
                            filePath, doc_.ErrorStr());
            spdlog::error("{}", errorMsg);
            return Result<bool>{std::in_place_index<1>, errorMsg};
        }

        spdlog::info("Successfully saved XML file: {}", filePath);
        return Result<bool>{std::in_place_index<0>, true};
    } catch (const std::exception& e) {
        auto errorMsg = std::format("Exception: {}", e.what());
        spdlog::error("Exception in saveToFile: {}", e.what());
        return Result<bool>{std::in_place_index<1>, errorMsg};
    }
}

auto XMLReader::getValuesByPathsAsync(const std::vector<std::string>& paths)
    const -> std::future<std::vector<Result<std::string>>> {
    return std::async(std::launch::async, [this, paths]() {
        std::vector<Result<std::string>> results(paths.size());

        if (paths.size() > 4) {
            std::transform(std::execution::par_unseq, paths.begin(),
                           paths.end(), results.begin(),
                           [this](const std::string& path) {
                               return this->getValueByPath(path);
                           });
        } else {
            for (size_t i = 0; i < paths.size(); ++i) {
                results[i] = this->getValueByPath(paths[i]);
            }
        }
        return results;
    });
}

auto XMLReader::getElementByPath(std::string_view path) const
    -> tinyxml2::XMLElement* {
    // Implementation for path-based element retrieval
    // This would need to be implemented based on your path format
    return nullptr;  // Placeholder
}

auto XMLReader::isValidPath(std::string_view path) -> bool {
    // Implementation for path validation
    // This would need to be implemented based on your path format
    return !path.empty();  // Placeholder
}

}  // namespace atom::utils