#pragma once
#include <algorithm>
#include <concepts>
#include <functional>
#include <iterator>
#include <optional>
#include <pugixml.hpp>
#include <string_view>
#include <type_traits>
#include <vector>

namespace atom::extra::pugixml {

// Forward declarations
class Document;
class Node;
class Attribute;
class NodeIterator;
class AttributeIterator;

// Concepts for type safety
template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// Exception types
class XmlException : public std::runtime_error {
public:
    explicit XmlException(const std::string& message)
        : std::runtime_error(message) {}
};

class ParseException : public XmlException {
public:
    explicit ParseException(const std::string& message)
        : XmlException("Parse error: " + message) {}
};

// String literal wrapper for compile-time strings
template <size_t N>
struct CompileTimeString {
    consteval CompileTimeString(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }

    constexpr std::string_view view() const noexcept {
        return std::string_view{value, N - 1};
    }

    char value[N];
};

// Attribute wrapper class
class Attribute {
private:
    pugi::xml_attribute attr_;

public:
    explicit Attribute(pugi::xml_attribute attr) noexcept : attr_(attr) {}

    // Properties
    [[nodiscard]] constexpr bool empty() const noexcept {
        return attr_.empty();
    }
    [[nodiscard]] bool valid() const noexcept { return !attr_.empty(); }

    // Name and value access
    [[nodiscard]] std::string_view name() const noexcept {
        return attr_.name();
    }

    [[nodiscard]] std::string_view value() const noexcept {
        return attr_.value();
    }

    // Type-safe value conversion
    template <Numeric T>
    [[nodiscard]] std::optional<T> as() const noexcept {
        if (empty())
            return std::nullopt;

        if constexpr (std::integral<T>) {
            if constexpr (std::same_as<T, bool>) {
                return attr_.as_bool();
            } else {
                return static_cast<T>(attr_.as_llong());
            }
        } else {
            return static_cast<T>(attr_.as_double());
        }
    }

    // String conversion
    [[nodiscard]] std::string as_string() const { return attr_.as_string(); }

    // Fluent API for setting values
    template <StringLike T>
    Attribute& set_value(T&& value) {
        attr_.set_value(std::string_view(value).data());
        return *this;
    }

    template <Numeric T>
    Attribute& set_value(T value) {
        if constexpr (std::integral<T>) {
            attr_.set_value(static_cast<long long>(value));
        } else {
            attr_.set_value(static_cast<double>(value));
        }
        return *this;
    }

    // Comparison operators
    [[nodiscard]] bool operator==(const Attribute& other) const noexcept {
        return attr_ == other.attr_;
    }

    // Conversion to pugi type
    [[nodiscard]] pugi::xml_attribute native() const noexcept { return attr_; }
};

// Node iterator for range support
class NodeIterator {
private:
    pugi::xml_node_iterator iter_;

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = Node;
    using difference_type = std::ptrdiff_t;
    using pointer = Node*;
    using reference = Node&;

    explicit NodeIterator(pugi::xml_node_iterator iter) : iter_(iter) {}

    Node operator*() const;
    NodeIterator& operator++() {
        ++iter_;
        return *this;
    }
    NodeIterator operator++(int) {
        auto temp = *this;
        ++iter_;
        return temp;
    }

    bool operator==(const NodeIterator& other) const noexcept {
        return iter_ == other.iter_;
    }
};

// Attribute iterator
class AttributeIterator {
private:
    pugi::xml_attribute_iterator iter_;

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = Attribute;
    using difference_type = std::ptrdiff_t;
    using pointer = Attribute*;
    using reference = Attribute&;

    explicit AttributeIterator(pugi::xml_attribute_iterator iter)
        : iter_(iter) {}

    Attribute operator*() const { return Attribute(*iter_); }
    AttributeIterator& operator++() {
        ++iter_;
        return *this;
    }
    AttributeIterator operator++(int) {
        auto temp = *this;
        ++iter_;
        return temp;
    }

    bool operator==(const AttributeIterator& other) const noexcept {
        return iter_ == other.iter_;
    }
};

// Range adapters
class NodeRange {
private:
    pugi::xml_node node_;

public:
    explicit NodeRange(pugi::xml_node node) : node_(node) {}

    NodeIterator begin() const { return NodeIterator(node_.begin()); }
    NodeIterator end() const { return NodeIterator(node_.end()); }
};

class AttributeRange {
private:
    pugi::xml_node node_;

public:
    explicit AttributeRange(pugi::xml_node node) : node_(node) {}

    AttributeIterator begin() const {
        return AttributeIterator(node_.attributes_begin());
    }
    AttributeIterator end() const {
        return AttributeIterator(node_.attributes_end());
    }
};

// Main Node wrapper class
class Node {
private:
    pugi::xml_node node_;

public:
    explicit Node(pugi::xml_node node = {}) noexcept : node_(node) {}

    // Properties
    [[nodiscard]] constexpr bool empty() const noexcept {
        return node_.empty();
    }
    [[nodiscard]] bool valid() const noexcept { return !node_.empty(); }

    // Basic information
    [[nodiscard]] std::string_view name() const noexcept {
        return node_.name();
    }

    [[nodiscard]] std::string_view value() const noexcept {
        return node_.value();
    }

    [[nodiscard]] pugi::xml_node_type type() const noexcept {
        return node_.type();
    }

    // Text content with type conversion
    [[nodiscard]] std::string text() const { return node_.child_value(); }

    template <Numeric T>
    [[nodiscard]] std::optional<T> text_as() const noexcept {
        if (empty())
            return std::nullopt;

        auto text_node = node_.text();
        if (text_node.empty())
            return std::nullopt;

        if constexpr (std::integral<T>) {
            if constexpr (std::same_as<T, bool>) {
                return text_node.as_bool();
            } else {
                return static_cast<T>(text_node.as_llong());
            }
        } else {
            return static_cast<T>(text_node.as_double());
        }
    }

    // Fluent API for setting text
    template <StringLike T>
    Node& set_text(T&& value) {
        node_.text().set(std::string_view(value).data());
        return *this;
    }

    template <Numeric T>
    Node& set_text(T value) {
        if constexpr (std::integral<T>) {
            node_.text().set(static_cast<long long>(value));
        } else {
            node_.text().set(static_cast<double>(value));
        }
        return *this;
    }

    // Attribute access
    [[nodiscard]] std::optional<Attribute> attribute(
        std::string_view name) const noexcept {
        auto attr = node_.attribute(name.data());
        if (attr.empty())
            return std::nullopt;
        return Attribute(attr);
    }

    // Fluent API for setting attributes
    template <StringLike NameType, StringLike ValueType>
    Node& set_attribute(NameType&& name, ValueType&& value) {
        node_.attribute(std::string_view(name).data())
            .set_value(std::string_view(value).data());
        return *this;
    }

    template <StringLike NameType, Numeric ValueType>
    Node& set_attribute(NameType&& name, ValueType value) {
        auto attr = node_.attribute(std::string_view(name).data());
        if constexpr (std::integral<ValueType>) {
            attr.set_value(static_cast<long long>(value));
        } else {
            attr.set_value(static_cast<double>(value));
        }
        return *this;
    }

    // Range support for child nodes
    [[nodiscard]] NodeRange children() const noexcept {
        return NodeRange(node_);
    }

    // Range support for attributes
    [[nodiscard]] AttributeRange attributes() const noexcept {
        return AttributeRange(node_);
    }

    // Child node access
    [[nodiscard]] std::optional<Node> child(
        std::string_view name) const noexcept {
        auto child = node_.child(name.data());
        if (child.empty())
            return std::nullopt;
        return Node(child);
    }

    [[nodiscard]] std::optional<Node> first_child() const noexcept {
        auto child = node_.first_child();
        if (child.empty())
            return std::nullopt;
        return Node(child);
    }

    [[nodiscard]] std::optional<Node> last_child() const noexcept {
        auto child = node_.last_child();
        if (child.empty())
            return std::nullopt;
        return Node(child);
    }

    // Navigation
    [[nodiscard]] std::optional<Node> next_sibling() const noexcept {
        auto sibling = node_.next_sibling();
        if (sibling.empty())
            return std::nullopt;
        return Node(sibling);
    }

    [[nodiscard]] std::optional<Node> previous_sibling() const noexcept {
        auto sibling = node_.previous_sibling();
        if (sibling.empty())
            return std::nullopt;
        return Node(sibling);
    }

    [[nodiscard]] std::optional<Node> parent() const noexcept {
        auto par = node_.parent();
        if (par.empty())
            return std::nullopt;
        return Node(par);
    }

    // Fluent API for adding children
    Node append_child(std::string_view name) {
        auto child = node_.append_child(name.data());
        if (child.empty()) {
            throw XmlException("Failed to append child: " + std::string(name));
        }
        return Node(child);
    }

    Node prepend_child(std::string_view name) {
        auto child = node_.prepend_child(name.data());
        if (child.empty()) {
            throw XmlException("Failed to prepend child: " + std::string(name));
        }
        return Node(child);
    }

    // Remove operations
    bool remove_child(std::string_view name) noexcept {
        return node_.remove_child(name.data());
    }

    bool remove_attribute(std::string_view name) noexcept {
        return node_.remove_attribute(name.data());
    }

    // XPath support with modern interface
    template <StringLike T>
    [[nodiscard]] std::vector<Node> select_nodes(T&& xpath) const {
        std::vector<Node> result;
        auto nodes = node_.select_nodes(std::string_view(xpath).data());

        result.reserve(nodes.size());
        for (const auto& selected : nodes) {
            result.emplace_back(selected.node());
        }

        return result;
    }

    template <StringLike T>
    [[nodiscard]] std::optional<Node> select_node(T&& xpath) const noexcept {
        auto selected = node_.select_node(std::string_view(xpath).data());
        if (!selected)
            return std::nullopt;
        return Node(selected.node());
    }

    // Functional programming support
    template <typename Predicate>
    [[nodiscard]] std::vector<Node> filter_children(Predicate&& pred) const {
        std::vector<Node> result;

        for (auto child : children()) {
            if (pred(child)) {
                result.push_back(child);
            }
        }

        return result;
    }

    template <typename Transform>
    [[nodiscard]] auto transform_children(Transform&& transform) const {
        std::vector<std::invoke_result_t<Transform, Node>> result;

        for (auto child : children()) {
            result.push_back(transform(child));
        }

        return result;
    }

    // Structured binding support
    template <size_t N>
    [[nodiscard]] std::array<std::optional<Node>, N> get_children() const {
        std::array<std::optional<Node>, N> result;
        size_t i = 0;

        for (auto child : children()) {
            if (i >= N)
                break;
            result[i++] = child;
        }

        return result;
    }

    // Comparison and utility
    [[nodiscard]] bool operator==(const Node& other) const noexcept {
        return node_ == other.node_;
    }

    [[nodiscard]] pugi::xml_node native() const noexcept { return node_; }

    // Hash support for use in containers
    [[nodiscard]] size_t hash() const noexcept {
        return std::hash<void*>{}(node_.internal_object());
    }
};

// Implementation of NodeIterator::operator*
inline Node NodeIterator::operator*() const { return Node(*iter_); }

}  // namespace atom::extra::pugixml

// Hash specialization
template <>
struct std::hash<atom::extra::pugixml::Node> {
    size_t operator()(const atom::extra::pugixml::Node& node) const noexcept {
        return node.hash();
    }
};