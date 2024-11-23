#include "atom/utils/xml.hpp"

#include <iostream>

using namespace atom::utils;

int main() {
    // Create an XMLReader object with the specified file path
    XMLReader reader("example.xml");

    // Get the names of all child elements of the specified parent element
    std::vector<std::string> childNames = reader.getChildElementNames("parent");
    std::cout << "Child elements of 'parent': ";
    for (const auto& name : childNames) {
        std::cout << name << " ";
    }
    std::cout << std::endl;

    // Get the text value of the specified element
    std::string elementText = reader.getElementText("element");
    std::cout << "Text of 'element': " << elementText << std::endl;

    // Get the value of the specified attribute of the specified element
    std::string attributeValue =
        reader.getAttributeValue("element", "attribute");
    std::cout << "Value of 'attribute' in 'element': " << attributeValue
              << std::endl;

    // Get the names of all root elements in the XML file
    std::vector<std::string> rootNames = reader.getRootElementNames();
    std::cout << "Root elements: ";
    for (const auto& name : rootNames) {
        std::cout << name << " ";
    }
    std::cout << std::endl;

    // Check if the specified parent element has a child element with the
    // specified name
    bool hasChild = reader.hasChildElement("parent", "child");
    std::cout << "Parent 'parent' has child 'child': " << std::boolalpha
              << hasChild << std::endl;

    // Get the text value of the specified child element of the specified parent
    // element
    std::string childText = reader.getChildElementText("parent", "child");
    std::cout << "Text of 'child' in 'parent': " << childText << std::endl;

    // Get the value of the specified attribute of the specified child element
    // of the specified parent element
    std::string childAttributeValue =
        reader.getChildElementAttributeValue("parent", "child", "attribute");
    std::cout << "Value of 'attribute' in 'child' of 'parent': "
              << childAttributeValue << std::endl;

    // Get the text value of the element specified by a given path
    std::string valueByPath = reader.getValueByPath("root/parent/child");
    std::cout << "Value by path 'root/parent/child': " << valueByPath
              << std::endl;

    // Get the value of the specified attribute of the element specified by a
    // given path
    std::string attributeValueByPath =
        reader.getAttributeValueByPath("root/parent/child", "attribute");
    std::cout << "Attribute value by path 'root/parent/child': "
              << attributeValueByPath << std::endl;

    // Check if the element specified by a given path has a child element with
    // the specified name
    bool hasChildByPath = reader.hasChildElementByPath("root/parent", "child");
    std::cout << "Element by path 'root/parent' has child 'child': "
              << std::boolalpha << hasChildByPath << std::endl;

    // Get the text value of the child element with the specified name of the
    // element specified by a given path
    std::string childTextByPath =
        reader.getChildElementTextByPath("root/parent", "child");
    std::cout << "Child text by path 'root/parent/child': " << childTextByPath
              << std::endl;

    // Get the value of the specified attribute of the child element with the
    // specified name of the element specified by a given path
    std::string childAttributeValueByPath =
        reader.getChildElementAttributeValueByPath("root/parent", "child",
                                                   "attribute");
    std::cout << "Child attribute value by path 'root/parent/child': "
              << childAttributeValueByPath << std::endl;

    // Save the XML document to the specified file
    bool saved = reader.saveToFile("output.xml");
    std::cout << "XML document saved: " << std::boolalpha << saved << std::endl;

    return 0;
}