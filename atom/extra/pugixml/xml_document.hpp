#pragma once
#include "xml_node_wrapper.hpp"

#include <filesystem>
#include <sstream>

namespace atom::extra::pugixml {

// Load options wrapper
struct LoadOptions {
    unsigned int options = pugi::parse_default;
    pugi::xml_encoding encoding = pugi::encoding_auto;

    constexpr LoadOptions& set_parse_options(unsigned int opts) noexcept {
        options = opts;
        return *this;
    }

    constexpr LoadOptions& set_encoding(pugi::xml_encoding enc) noexcept {
        encoding = enc;
        return *this;
    }

    // Fluent options
    constexpr LoadOptions& minimal() noexcept {
        options = pugi::parse_minimal;
        return *this;
    }

    constexpr LoadOptions& full() noexcept {
        options = pugi::parse_full;
        return *this;
    }

    constexpr LoadOptions& no_escapes() noexcept {
        options &= ~pugi::parse_escapes;
        return *this;
    }

    constexpr LoadOptions& trim_whitespace() noexcept {
        options |= pugi::parse_trim_pcdata;
        return *this;
    }
};

// Save options wrapper
struct SaveOptions {
    const char* indent = "\t";
    unsigned int flags = pugi::format_default;
    pugi::xml_encoding encoding = pugi::encoding_auto;

    constexpr SaveOptions& set_indent(const char* ind) noexcept {
        indent = ind;
        return *this;
    }

    constexpr SaveOptions& set_flags(unsigned int f) noexcept {
        flags = f;
        return *this;
    }

    constexpr SaveOptions& set_encoding(pugi::xml_encoding enc) noexcept {
        encoding = enc;
        return *this;
    }

    // Fluent options
    constexpr SaveOptions& raw() noexcept {
        flags = pugi::format_raw;
        return *this;
    }

    constexpr SaveOptions& no_declaration() noexcept {
        flags |= pugi::format_no_declaration;
        return *this;
    }

    constexpr SaveOptions& write_bom() noexcept {
        flags |= pugi::format_write_bom;
        return *this;
    }
};

// RAII Document wrapper
class Document {
private:
    std::unique_ptr<pugi::xml_document> doc_;

public:
    // Constructors
    Document() : doc_(std::make_unique<pugi::xml_document>()) {}

    // Move semantics only
    Document(Document&&) = default;
    Document& operator=(Document&&) = default;

    // Copy is expensive, so we make it explicit
    [[nodiscard]] Document clone() const {
        Document copy;
        copy.doc_->reset(*doc_);
        return copy;
    }

    // Disable copy construction/assignment
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // Factory methods with modern interface
    [[nodiscard]] static Document from_string(std::string_view xml,
                                              LoadOptions options = {}) {
        Document doc;
        auto result = doc.doc_->load_string(xml.data(), options.options);

        if (!result) {
            throw ParseException(std::string("Failed to parse XML: ") +
                                 result.description());
        }

        return doc;
    }

    [[nodiscard]] static Document from_file(const std::filesystem::path& path,
                                            LoadOptions options = {}) {
        Document doc;
        auto result = doc.doc_->load_file(path.c_str(), options.options,
                                          options.encoding);

        if (!result) {
            throw ParseException(std::string("Failed to load file '") +
                                 path.string() + "': " + result.description());
        }

        return doc;
    }

    [[nodiscard]] static Document from_stream(std::istream& stream,
                                              LoadOptions options = {}) {
        Document doc;
        auto result = doc.doc_->load(stream, options.options, options.encoding);

        if (!result) {
            throw ParseException(std::string("Failed to parse from stream: ") +
                                 result.description());
        }

        return doc;
    }

    // Create empty document with declaration
    [[nodiscard]] static Document create_empty(
        std::string_view version = "1.0", std::string_view encoding = "UTF-8",
        std::string_view standalone = "") {
        Document doc;

        auto decl = doc.doc_->append_child(pugi::node_declaration);
        decl.append_attribute("version") = version.data();
        decl.append_attribute("encoding") = encoding.data();

        if (!standalone.empty()) {
            decl.append_attribute("standalone") = standalone.data();
        }

        return doc;
    }

    // Root node access
    [[nodiscard]] Node root() const noexcept {
        return Node(doc_->document_element());
    }

    [[nodiscard]] Node document_element() const noexcept {
        return Node(doc_->document_element());
    }

    // Create root element
    Node create_root(std::string_view name) {
        auto root_node = doc_->append_child(name.data());
        if (root_node.empty()) {
            throw XmlException("Failed to create root element: " +
                               std::string(name));
        }
        return Node(root_node);
    }

    // Full document access for advanced operations
    [[nodiscard]] Node document() const noexcept { return Node(*doc_); }

    // Save operations
    void save_to_file(const std::filesystem::path& path,
                      SaveOptions options = {}) const {
        if (!doc_->save_file(path.c_str(), options.indent, options.flags,
                             options.encoding)) {
            throw XmlException("Failed to save to file: " + path.string());
        }
    }

    void save_to_stream(std::ostream& stream, SaveOptions options = {}) const {
        doc_->save(stream, options.indent, options.flags, options.encoding);
    }

    [[nodiscard]] std::string to_string(SaveOptions options = {}) const {
        std::ostringstream oss;
        save_to_stream(oss, options);
        return oss.str();
    }

    // Query operations
    template <StringLike T>
    [[nodiscard]] std::vector<Node> select_nodes(T&& xpath) const {
        return document().select_nodes(std::forward<T>(xpath));
    }

    template <StringLike T>
    [[nodiscard]] std::optional<Node> select_node(T&& xpath) const {
        return document().select_node(std::forward<T>(xpath));
    }

    // Validation
    [[nodiscard]] bool empty() const noexcept { return doc_->empty(); }

    [[nodiscard]] bool has_root() const noexcept {
        return !doc_->document_element().empty();
    }

    // Clear document
    void clear() noexcept { doc_->reset(); }

    // Access to native pugixml document
    [[nodiscard]] pugi::xml_document& native() noexcept { return *doc_; }
    [[nodiscard]] const pugi::xml_document& native() const noexcept {
        return *doc_;
    }
};

}  // namespace atom::extra::pugixml