#pragma once
#include "xml_document.hpp"  // Assumed to provide Node, Document, StringLike, Numeric concepts

#include <functional>  // For std::invoke
#include <ranges>      // For std::ranges::range
#include <string>
#include <string_view>
#include <type_traits>  // For std::is_invocable_v, std::decay_t
#include <utility>      // For std::forward, std::move

namespace atom::extra::pugixml {

// Attribute helper for fluent construction
struct AttributePair {
    std::string name;
    std::string value;

    template <StringLike NameType, StringLike ValueType>
    AttributePair(NameType&& n, ValueType&& v)
        : name(std::forward<NameType>(n)), value(std::forward<ValueType>(v)) {}

    template <StringLike NameType, Numeric ValueType>
    AttributePair(NameType&& n, ValueType v)
        : name(std::forward<NameType>(n)), value(std::to_string(v)) {}
};

// Helper function for creating attributes
template <typename NameType, typename ValueType>
[[nodiscard]] constexpr AttributePair attr(NameType&& name, ValueType&& value) {
    return AttributePair{std::forward<NameType>(name),
                         std::forward<ValueType>(value)};
}

// Forward declaration for NodeBuilder to be used in the concept
class NodeBuilder;

// Concept to check if a type is a callable configurator for NodeBuilder
template <typename F>
concept NodeBuilderConfigurator =
    std::is_invocable_v<std::decay_t<F>, NodeBuilder&>;

// Builder class for fluent XML construction
class NodeBuilder {
private:
    Node node_;

public:
    explicit NodeBuilder(Node node) : node_(node) {}

    // Fluent attribute setting
    template <typename... Pairs>
    NodeBuilder& attributes(Pairs&&... pairs) {
        (node_.set_attribute(pairs.name, pairs.value), ...);
        return *this;
    }

    // Single attribute
    template <StringLike NameType, typename ValueType>
    NodeBuilder& attribute(NameType&& name, ValueType&& value) {
        node_.set_attribute(std::forward<NameType>(name),
                            std::forward<ValueType>(value));
        return *this;
    }

    // Text content
    template <typename T>
    NodeBuilder& text(T&& value) {
        node_.set_text(std::forward<T>(value));
        return *this;
    }

    // Child elements with fluent interface
    template <typename F>
        requires NodeBuilderConfigurator<F>
    NodeBuilder& child(std::string_view name, F&& configurator) {
        auto child_node = node_.append_child(name);
        NodeBuilder builder(child_node);
        std::invoke(std::forward<F>(configurator), builder);
        return *this;
    }

    // Simple child with text
    template <typename T>
        requires(!NodeBuilderConfigurator<T>)
    NodeBuilder& child(std::string_view name, T&& text_value) {
        auto child_node = node_.append_child(name);
        child_node.set_text(std::forward<T>(text_value));
        return *this;
    }

    // Multiple children from container
    template <std::ranges::range Container, typename Transformer>
    NodeBuilder& children(std::string_view element_name,
                          const Container& container, Transformer&& transform) {
        for (const auto& item : container) {
            auto child_node = node_.append_child(element_name);
            NodeBuilder builder(child_node);
            // Assuming transform takes (NodeBuilder&, const auto& item)
            std::invoke(std::forward<Transformer>(transform), builder, item);
        }
        return *this;
    }

    // Conditional building
    template <typename F>
        requires NodeBuilderConfigurator<
            F>  // Assuming configurator takes NodeBuilder&
    NodeBuilder& if_condition(bool condition, F&& configurator) {
        if (condition) {
            std::invoke(std::forward<F>(configurator), *this);
        }
        return *this;
    }

    // Get the built node
    [[nodiscard]] Node build() const noexcept { return node_; }
    [[nodiscard]] Node get() const noexcept { return node_; }

    // Implicit conversion to Node
    operator Node() const noexcept { return node_; }
};

// Document builder for complete document construction
class DocumentBuilder {
private:
    Document doc_;

public:
    DocumentBuilder() = default;

    // Start with XML declaration
    DocumentBuilder& declaration(std::string_view version = "1.0",
                                 std::string_view encoding = "UTF-8",
                                 std::string_view standalone = "") {
        doc_ = Document::create_empty(version, encoding, standalone);
        return *this;
    }

    // Create root element with fluent configuration
    template <typename F>
        requires NodeBuilderConfigurator<F>
    DocumentBuilder& root(std::string_view name, F&& configurator) {
        auto root_node = doc_.create_root(name);
        NodeBuilder builder(root_node);
        std::invoke(std::forward<F>(configurator), builder);
        return *this;
    }

    // Simple root with text
    template <typename T>
        requires(!NodeBuilderConfigurator<T>)
    DocumentBuilder& root(std::string_view name, T&& text_value) {
        auto root_node = doc_.create_root(name);
        root_node.set_text(std::forward<T>(text_value));
        return *this;
    }

    // Build the document
    [[nodiscard]] Document build() { return std::move(doc_); }
    [[nodiscard]] Document get() { return std::move(doc_); }
};

// Factory functions for easier usage
[[nodiscard]] inline DocumentBuilder document() { return DocumentBuilder{}; }

[[nodiscard]] inline NodeBuilder element(Node node) {
    return NodeBuilder{node};
}

// Structured XML building with initializer lists
namespace literals {

// User-defined literal for XML strings (for future constexpr parsing)
[[nodiscard]] inline std::string operator""_xml(const char* str, size_t len) {
    return std::string{str, len};
}

}  // namespace literals

}  // namespace atom::extra::pugixml
