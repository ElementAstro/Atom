/*
 * xml.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-10-27

Description: A XML reader class using tinyxml2.

**************************************************/

#ifndef ATOM_UTILS_XML_HPP
#define ATOM_UTILS_XML_HPP

#if __has_include(<tinyxml2.h>)
#include <tinyxml2.h>
#elif __has_include(<tinyxml2/tinyxml2.h>)
#include <tinyxml2/tinyxml2.h>
#endif

#include <future>
#include <mutex>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace atom::utils {

// Concept to ensure string-like types
template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

// Result type that can hold a value or an error
template <typename T>
using Result = std::variant<T, std::string>;

/**
 * @brief A class for reading and manipulating data from an XML file.
 */
class XMLReader {
public:
    /**
     * @brief Constructs an XMLReader object with the specified file path.
     *
     * @param filePath The path to the XML file to read.
     * @throws std::runtime_error if the file cannot be loaded.
     */
    explicit XMLReader(std::string_view filePath);

    /**
     * @brief Returns the names of all child elements of the specified parent
     * element.
     *
     * @param parentElementName The name of the parent element.
     * @return A vector containing the names of all child elements.
     */
    auto getChildElementNames(std::string_view parentElementName) const
        -> Result<std::vector<std::string>>;

    /**
     * @brief Returns the text value of the specified element.
     *
     * @param elementName The name of the element.
     * @return The text value of the element or an error message.
     */
    auto getElementText(std::string_view elementName) const
        -> Result<std::string>;

    /**
     * @brief Returns the value of the specified attribute of the specified
     * element.
     *
     * @param elementName The name of the element.
     * @param attributeName The name of the attribute.
     * @return The value of the attribute or an error message.
     */
    auto getAttributeValue(std::string_view elementName,
                           std::string_view attributeName) const
        -> Result<std::string>;

    /**
     * @brief Returns the names of all root elements in the XML file.
     *
     * @return A vector containing the names of all root elements.
     */
    auto getRootElementNames() const -> std::vector<std::string>;

    /**
     * @brief Checks if the specified parent element has a child element with
     * the specified name.
     *
     * @param parentElementName The name of the parent element.
     * @param childElementName The name of the child element.
     * @return true if the child element exists, false otherwise.
     */
    auto hasChildElement(std::string_view parentElementName,
                         std::string_view childElementName) const -> bool;

    /**
     * @brief Returns the text value of the specified child element of the
     * specified parent element.
     *
     * @param parentElementName The name of the parent element.
     * @param childElementName The name of the child element.
     * @return The text value of the child element or an error message.
     */
    auto getChildElementText(std::string_view parentElementName,
                             std::string_view childElementName) const
        -> Result<std::string>;

    /**
     * @brief Returns the value of the specified attribute of the specified
     * child element of the specified parent element.
     *
     * @param parentElementName The name of the parent element.
     * @param childElementName The name of the child element.
     * @param attributeName The name of the attribute.
     * @return The value of the attribute or an error message.
     */
    auto getChildElementAttributeValue(
        std::string_view parentElementName, std::string_view childElementName,
        std::string_view attributeName) const -> Result<std::string>;

    /**
     * @brief Returns the text value of the element specified by a given path.
     *
     * @param path The path to the element.
     * @return The text value of the element or an error message.
     */
    auto getValueByPath(std::string_view path) const -> Result<std::string>;

    /**
     * @brief Returns the value of the specified attribute of the element
     * specified by a given path.
     *
     * @param path The path to the element.
     * @param attributeName The name of the attribute.
     * @return The value of the attribute or an error message.
     */
    auto getAttributeValueByPath(std::string_view path,
                                 std::string_view attributeName) const
        -> Result<std::string>;

    /**
     * @brief Checks if the element specified by a given path has a child
     * element with the specified name.
     *
     * @param path The path to the parent element.
     * @param childElementName The name of the child element.
     * @return true if the child element exists, false otherwise.
     */
    auto hasChildElementByPath(std::string_view path,
                               std::string_view childElementName) const -> bool;

    /**
     * @brief Returns the text value of the child element with the specified
     * name of the element specified by a given path.
     *
     * @param path The path to the parent element.
     * @param childElementName The name of the child element.
     * @return The text value of the child element or an error message.
     */
    auto getChildElementTextByPath(std::string_view path,
                                   std::string_view childElementName) const
        -> Result<std::string>;

    /**
     * @brief Returns the value of the specified attribute of the child element
     * with the specified name of the element specified by a given path.
     *
     * @param path The path to the parent element.
     * @param childElementName The name of the child element.
     * @param attributeName The name of the attribute.
     * @return The value of the attribute or an error message.
     */
    auto getChildElementAttributeValueByPath(
        std::string_view path, std::string_view childElementName,
        std::string_view attributeName) const -> Result<std::string>;

    /**
     * @brief Saves the XML document to the specified file.
     *
     * @param filePath The path to save the XML document to.
     * @return Result indicating success or an error message.
     */
    auto saveToFile(std::string_view filePath) const -> Result<bool>;

    /**
     * @brief Asynchronously retrieves multiple values by paths.
     *
     * @param paths The vector of paths to retrieve values for.
     * @return A future containing a vector of results.
     */
    auto getValuesByPathsAsync(const std::vector<std::string>& paths) const
        -> std::future<std::vector<Result<std::string>>>;

private:
    mutable tinyxml2::XMLDocument doc_;
    mutable std::mutex mutex_;  // For thread safety

    /**
     * @brief Returns a pointer to the element specified by a given path.
     *
     * @param path The path to the element.
     * @return A pointer to the element or nullptr if not found.
     */
    auto getElementByPath(std::string_view path) const -> tinyxml2::XMLElement*;

    /**
     * @brief Validates that the XML path is well-formed.
     *
     * @param path The path to validate.
     * @return true if the path is valid, false otherwise.
     */
    static auto isValidPath(std::string_view path) -> bool;
};
}  // namespace atom::utils

#endif
