#include "xml.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/exception/all.hpp>
#include <boost/format.hpp>
#endif

namespace atom::utils {

XMLReader::XMLReader(const std::string &filePath) {
    LOG_F(INFO, "Loading XML file: {}", filePath);
    tinyxml2::XMLError loadResult = doc_.LoadFile(filePath.c_str());
    if (loadResult != tinyxml2::XML_SUCCESS) {
        std::string errorMsg = "Failed to load XML file: " + filePath +
                               ". TinyXML2 Error: " + doc_.ErrorStr();
        LOG_F(ERROR, "{}", errorMsg);
        THROW_RUNTIME_ERROR(errorMsg);
    }
    LOG_F(INFO, "Successfully loaded XML file: {}", filePath);
}

auto XMLReader::getChildElementNames(const std::string &parentElementName) const
    -> std::vector<std::string> {
    LOG_F(INFO, "Getting child element names for parent: {}",
          parentElementName);
    std::vector<std::string> childElementNames;
    const tinyxml2::XMLElement *parentElement =
        doc_.FirstChildElement(parentElementName.c_str());
    if (parentElement == nullptr) {
        LOG_F(WARNING, "Parent element '{}' not found.", parentElementName);
        return childElementNames;  // Return empty vector
    }
    for (const tinyxml2::XMLElement *element =
             parentElement->FirstChildElement();
         element != nullptr; element = element->NextSiblingElement()) {
        childElementNames.emplace_back(element->Name());
    }
    LOG_F(INFO, "Found {} child elements for parent: {}",
          childElementNames.size(), parentElementName);
    return childElementNames;
}

auto XMLReader::getElementText(const std::string &elementName) const
    -> std::string {
    LOG_F(INFO, "Getting text for element: {}", elementName);
    const tinyxml2::XMLElement *element =
        doc_.FirstChildElement(elementName.c_str());
    if (element == nullptr) {
        LOG_F(WARNING, "Element '{}' not found.", elementName);
        return "";
    }
    const char *text = element->GetText();
    if (text == nullptr) {
        LOG_F(WARNING, "Element '{}' contains no text.", elementName);
        return "";
    }
    return std::string(text);
}

auto XMLReader::getAttributeValue(const std::string &elementName,
                                  const std::string &attributeName) const
    -> std::string {
    LOG_F(INFO, "Getting attribute value for element: {}, attribute: {}",
          elementName, attributeName);
    const tinyxml2::XMLElement *element =
        doc_.FirstChildElement(elementName.c_str());
    if (element == nullptr) {
        LOG_F(WARNING, "Element '{}' not found.", elementName);
        return "";
    }
    const char *attrValue = element->Attribute(attributeName.c_str());
    if (attrValue == nullptr) {
        LOG_F(WARNING, "Attribute '{}' not found in element '{}'.",
              attributeName, elementName);
        return "";
    }
    return std::string(attrValue);
}

auto XMLReader::getRootElementNames() const -> std::vector<std::string> {
    LOG_F(INFO, "Getting root element names");
    std::vector<std::string> rootElementNames;
    const tinyxml2::XMLElement *rootElement = doc_.RootElement();
    if (rootElement != nullptr) {
        rootElementNames.emplace_back(rootElement->Name());
    } else {
        LOG_F(WARNING, "No root element found in the XML document.");
    }
    LOG_F(INFO, "Found {} root elements", rootElementNames.size());
    return rootElementNames;
}

auto XMLReader::hasChildElement(const std::string &parentElementName,
                                const std::string &childElementName) const
    -> bool {
    LOG_F(INFO, "Checking if parent element: {} has child element: {}",
          parentElementName, childElementName);
    const tinyxml2::XMLElement *parentElement =
        doc_.FirstChildElement(parentElementName.c_str());
    if (parentElement == nullptr) {
        LOG_F(WARNING, "Parent element '{}' not found.", parentElementName);
        return false;
    }
    bool hasChild =
        parentElement->FirstChildElement(childElementName.c_str()) != nullptr;
    LOG_F(INFO, "Parent element '{}' has child element '{}': {}",
          parentElementName, childElementName, hasChild);
    return hasChild;
}

auto XMLReader::getChildElementText(const std::string &parentElementName,
                                    const std::string &childElementName) const
    -> std::string {
    LOG_F(INFO, "Getting text for child element: {} of parent element: {}",
          childElementName, parentElementName);
    const tinyxml2::XMLElement *parentElement =
        doc_.FirstChildElement(parentElementName.c_str());
    if (parentElement == nullptr) {
        LOG_F(WARNING, "Parent element '{}' not found.", parentElementName);
        return "";
    }
    const tinyxml2::XMLElement *childElement =
        parentElement->FirstChildElement(childElementName.c_str());
    if (childElement == nullptr) {
        LOG_F(WARNING, "Child element '{}' not found under parent '{}'.",
              childElementName, parentElementName);
        return "";
    }
    const char *text = childElement->GetText();
    if (text == nullptr) {
        LOG_F(WARNING, "Child element '{}' under parent '{}' contains no text.",
              childElementName, parentElementName);
        return "";
    }
    return std::string(text);
}

auto XMLReader::getChildElementAttributeValue(
    const std::string &parentElementName, const std::string &childElementName,
    const std::string &attributeName) const -> std::string {
    LOG_F(INFO,
          "Getting attribute value for child element: {} of parent element: "
          "{}, attribute: {}",
          childElementName, parentElementName, attributeName);
    const tinyxml2::XMLElement *parentElement =
        doc_.FirstChildElement(parentElementName.c_str());
    if (parentElement == nullptr) {
        LOG_F(WARNING, "Parent element '{}' not found.", parentElementName);
        return "";
    }
    const tinyxml2::XMLElement *childElement =
        parentElement->FirstChildElement(childElementName.c_str());
    if (childElement == nullptr) {
        LOG_F(WARNING, "Child element '{}' not found under parent '{}'.",
              childElementName, parentElementName);
        return "";
    }
    const char *attrValue = childElement->Attribute(attributeName.c_str());
    if (attrValue == nullptr) {
        LOG_F(
            WARNING,
            "Attribute '{}' not found in child element '{}' under parent '{}'.",
            attributeName, childElementName, parentElementName);
        return "";
    }
    return std::string(attrValue);
}

auto XMLReader::getValueByPath(const std::string &path) const -> std::string {
    LOG_F(INFO, "Getting value by path: {}", path);
    tinyxml2::XMLElement *element = getElementByPath(path);
    if (element == nullptr) {
        LOG_F(WARNING, "Element at path '{}' not found.", path);
        return "";
    }
    const char *text = element->GetText();
    if (text == nullptr) {
        LOG_F(WARNING, "Element at path '{}' contains no text.", path);
        return "";
    }
    return std::string(text);
}

auto XMLReader::getAttributeValueByPath(const std::string &path,
                                        const std::string &attributeName) const
    -> std::string {
    LOG_F(INFO, "Getting attribute value by path: {}, attribute: {}", path,
          attributeName);
    tinyxml2::XMLElement *element = getElementByPath(path);
    if (element == nullptr) {
        LOG_F(WARNING, "Element at path '{}' not found.", path);
        return "";
    }
    const char *attrValue = element->Attribute(attributeName.c_str());
    if (attrValue == nullptr) {
        LOG_F(WARNING, "Attribute '{}' not found in element at path '{}'.",
              attributeName, path);
        return "";
    }
    return std::string(attrValue);
}

bool XMLReader::hasChildElementByPath(
    const std::string &path, const std::string &childElementName) const {
    LOG_F(INFO, "Checking if path: {} has child element: {}", path,
          childElementName);
    tinyxml2::XMLElement *element = getElementByPath(path);
    if (element == nullptr) {
        LOG_F(WARNING, "Element at path '{}' not found.", path);
        return false;
    }
    bool hasChild =
        element->FirstChildElement(childElementName.c_str()) != nullptr;
    LOG_F(INFO, "Element at path '{}' has child element '{}': {}", path,
          childElementName, hasChild);
    return hasChild;
}

auto XMLReader::getChildElementTextByPath(
    const std::string &path,
    const std::string &childElementName) const -> std::string {
    LOG_F(INFO, "Getting text for child element: {} by path: {}",
          childElementName, path);
    tinyxml2::XMLElement *element = getElementByPath(path);
    if (element == nullptr) {
        LOG_F(WARNING, "Element at path '{}' not found.", path);
        return "";
    }
    tinyxml2::XMLElement *childElement =
        element->FirstChildElement(childElementName.c_str());
    if (childElement == nullptr) {
        LOG_F(WARNING, "Child element '{}' not found under path '{}'.",
              childElementName, path);
        return "";
    }
    const char *text = childElement->GetText();
    if (text == nullptr) {
        LOG_F(WARNING, "Child element '{}' under path '{}' contains no text.",
              childElementName, path);
        return "";
    }
    return std::string(text);
}

auto XMLReader::getChildElementAttributeValueByPath(
    const std::string &path, const std::string &childElementName,
    const std::string &attributeName) const -> std::string {
    LOG_F(INFO,
          "Getting attribute value for child element: {} by path: {}, "
          "attribute: {}",
          childElementName, path, attributeName);
    tinyxml2::XMLElement *element = getElementByPath(path);
    if (element == nullptr) {
        LOG_F(WARNING, "Element at path '{}' not found.", path);
        return "";
    }
    tinyxml2::XMLElement *childElement =
        element->FirstChildElement(childElementName.c_str());
    if (childElement == nullptr) {
        LOG_F(WARNING, "Child element '{}' not found under path '{}'.",
              childElementName, path);
        return "";
    }
    const char *attrValue = childElement->Attribute(attributeName.c_str());
    if (attrValue == nullptr) {
        LOG_F(WARNING,
              "Attribute '{}' not found in child element '{}' under path '{}'.",
              attributeName, childElementName, path);
        return "";
    }
    return std::string(attrValue);
}

auto XMLReader::saveToFile(const std::string &filePath) const -> bool {
    LOG_F(INFO, "Saving XML to file: {}", filePath);
    tinyxml2::XMLError saveResult = doc_.SaveFile(filePath.c_str());
    if (saveResult != tinyxml2::XML_SUCCESS) {
        std::string errorMsg = "Failed to save XML file: " + filePath +
                               ". TinyXML2 Error: " + doc_.ErrorStr();
        LOG_F(ERROR, "{}", errorMsg);
        // Depending on requirements, you might want to throw an exception here
        // THROW_RUNTIME_ERROR(errorMsg);
        return false;
    }
    LOG_F(INFO, "Successfully saved XML file: {}", filePath);
    return true;
}

tinyxml2::XMLElement *XMLReader::getElementByPath(
    const std::string &path) const {
    LOG_F(INFO, "Getting element by path: {}", path);
    tinyxml2::XMLElement *element = doc_.RootElement();
    size_t pos = 0;
    while (element != nullptr && pos != std::string::npos) {
        size_t newPos = path.find('.', pos);
        std::string elementName = path.substr(pos, newPos - pos);
        element = element->FirstChildElement(elementName.c_str());
        pos = (newPos == std::string::npos) ? newPos : newPos + 1;
    }
    if (element == nullptr) {
        LOG_F(WARNING, "Element at path '{}' not found.", path);
    } else {
        LOG_F(INFO, "Element at path '{}' found: <{}>", path, element->Name());
    }
    return element;
}

}  // namespace atom::utils