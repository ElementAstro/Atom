#pragma once

// Main include header for the modern XML library
#include "xml_builder.hpp"
#include "xml_document.hpp"
#include "xml_node_wrapper.hpp"
#include "xml_query.hpp"

namespace atom::extra::pugixml {

// Version information
namespace version {
constexpr int major = 1;
constexpr int minor = 0;
constexpr int patch = 0;
constexpr std::string_view string = "1.0.0";
}  // namespace version

// Convenience aliases
using XmlDocument = Document;
using XmlNode = Node;
using XmlAttribute = Attribute;
using XmlBuilder = NodeBuilder;
using XmlDocumentBuilder = DocumentBuilder;

}  // namespace atom::extra::pugixml
