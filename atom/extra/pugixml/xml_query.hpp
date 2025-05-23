#pragma once

#include "xml_node_wrapper.hpp"

#include <algorithm>
#include <vector>

namespace atom::extra::pugixml {

// Advanced query utilities using modern C++ features
namespace query {

// Predicate-based filtering
template <typename Predicate>
[[nodiscard]] auto filter(const Node& node, Predicate&& pred) {
    std::vector<Node> result;
    for (auto child : node.children()) {
        if (std::forward<Predicate>(pred)(child)) {
            result.push_back(child);
        }
    }
    return result;
}

// Transform nodes
template <typename Transform>
[[nodiscard]] auto transform(const Node& node, Transform&& trans) {
    std::vector<std::invoke_result_t<Transform, Node>> result;
    for (auto child : node.children()) {
        result.push_back(std::forward<Transform>(trans)(child));
    }
    return result;
}

// Find first matching node
template <typename Predicate>
[[nodiscard]] std::optional<Node> find_first(const Node& node,
                                             Predicate&& pred) {
    for (auto child : node.children()) {
        if (pred(child)) {
            return child;
        }
    }
    return std::nullopt;
}

// Find all matching nodes (recursive)
template <typename Predicate>
[[nodiscard]] std::vector<Node> find_all_recursive(const Node& node,
                                                   Predicate&& pred) {
    std::vector<Node> results;

    std::function<void(const Node&)> search = [&](const Node& current) {
        if (pred(current)) {
            results.push_back(current);
        }

        for (auto child : current.children()) {
            search(child);
        }
    };

    search(node);
    return results;
}

// Count matching nodes
template <typename Predicate>
[[nodiscard]] size_t count_if(const Node& node, Predicate&& pred) {
    size_t count = 0;
    for (auto child : node.children()) {
        if (std::forward<Predicate>(pred)(child)) {
            ++count;
        }
    }
    return count;
}

// Aggregate operations
template <typename T, typename BinaryOp, typename Transform>
[[nodiscard]] T accumulate(const Node& node, T init, BinaryOp&& op,
                           Transform&& transform) {
    T result = init;
    for (auto child : node.children()) {
        result = op(result, std::forward<Transform>(transform)(child));
    }
    return result;
}

// Check if any child matches predicate
template <typename Predicate>
[[nodiscard]] bool any_of(const Node& node, Predicate&& pred) {
    for (auto child : node.children()) {
        if (std::forward<Predicate>(pred)(child)) {
            return true;
        }
    }
    return false;
}

// Check if all children match predicate
template <typename Predicate>
[[nodiscard]] bool all_of(const Node& node, Predicate&& pred) {
    for (auto child : node.children()) {
        if (!std::forward<Predicate>(pred)(child)) {
            return false;
        }
    }
    return true;
}

// Convenience predicates
namespace predicates {

[[nodiscard]] inline auto has_name(std::string_view name) {
    return [name](const Node& node) { return node.name() == name; };
}

[[nodiscard]] inline auto has_attribute(std::string_view attr_name) {
    return [attr_name](const Node& node) {
        return node.attribute(attr_name).has_value();
    };
}

template <typename T>
[[nodiscard]] auto has_attribute_value(std::string_view attr_name, T&& value) {
    return [attr_name, value = std::forward<T>(value)](const Node& node) {
        auto attr = node.attribute(attr_name);
        return attr && attr->value() == value;
    };
}

[[nodiscard]] inline auto has_text() {
    return [](const Node& node) { return !node.text().empty(); };
}

template <typename T>
[[nodiscard]] auto has_text_value(T&& text) {
    return [text = std::forward<T>(text)](const Node& node) {
        return node.text() == text;
    };
}

[[nodiscard]] inline auto is_element() {
    return [](const Node& node) { return node.type() == pugi::node_element; };
}

[[nodiscard]] inline auto has_children() {
    return [](const Node& node) { return node.first_child().has_value(); };
}

}  // namespace predicates

}  // namespace query

// Transform utilities
namespace transform {

// Apply transformation to all matching nodes
template <typename Predicate, typename Transform>
void transform_matching(Node& node, Predicate&& pred, Transform&& trans) {
    for (auto child : node.children()) {
        auto child_node = child;  // Copy to modify
        if (pred(child_node)) {
            trans(child_node);
        }
    }
}

// Recursive transformation
template <typename Transform>
void transform_recursive(Node& node, Transform&& trans) {
    std::function<void(Node&)> apply = [&](Node& current) {
        trans(current);
        for (auto child : current.children()) {
            auto child_node = child;  // Copy for modification
            apply(child_node);
        }
    };

    apply(node);
}

// Sort children by predicate
template <typename Compare>
void sort_children(Node& node, Compare&& comp) {
    // Get all children
    std::vector<Node> children;
    for (auto child : node.children()) {
        children.push_back(child);
    }

    // Sort them
    std::ranges::sort(children, comp);

    // Remove and re-add in order
    for (const auto& child : children) {
        // Note: This is a simplified approach
        // In practice, you'd need to properly handle node moving
        auto name = child.name();
        auto text = child.text();

        // Collect attributes
        std::vector<std::pair<std::string, std::string>> attrs;
        for (auto attr : child.attributes()) {
            attrs.emplace_back(std::string(attr.name()),
                               std::string(attr.value()));
        }

        node.remove_child(name);
        auto new_child = node.append_child(name);
        new_child.set_text(text);

        for (const auto& [attr_name, attr_value] : attrs) {
            new_child.set_attribute(attr_name, attr_value);
        }
    }
}

}  // namespace transform

}  // namespace atom::extra::pugixml