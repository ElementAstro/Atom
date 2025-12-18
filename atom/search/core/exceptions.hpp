#ifndef ATOM_SEARCH_CORE_EXCEPTIONS_HPP
#define ATOM_SEARCH_CORE_EXCEPTIONS_HPP

#include "atom/error/exception.hpp"
#include "types.hpp"

namespace atom::search {

/**
 * @brief Base exception class for search engine errors.
 */
class SearchEngineException : public atom::error::Exception {
public:
    explicit SearchEngineException(
        std::string message,
        SearchErrorCode code = SearchErrorCode::InternalError)
        : atom::error::Exception("", 0, "", std::move(message)),
          errorCode_(code) {}

    [[nodiscard]] SearchErrorCode errorCode() const noexcept {
        return errorCode_;
    }

    [[nodiscard]] std::string_view errorDescription() const noexcept {
        return errorCodeToString(errorCode_);
    }

protected:
    SearchErrorCode errorCode_;
};

/**
 * @brief Exception thrown when a document is not found.
 */
class DocumentNotFoundException : public SearchEngineException {
public:
    explicit DocumentNotFoundException(const String& docId)
        : SearchEngineException("Document not found: " + std::string(docId),
                                SearchErrorCode::DocumentNotFound),
          docId_(docId) {}

    explicit DocumentNotFoundException(std::string_view docId)
        : SearchEngineException("Document not found: " + std::string(docId),
                                SearchErrorCode::DocumentNotFound),
          docId_(docId) {}

    [[nodiscard]] std::string_view documentId() const noexcept {
        return docId_;
    }

private:
    String docId_;
};

/**
 * @brief Exception thrown when there's an issue with document validation.
 */
class DocumentValidationException : public SearchEngineException {
public:
    explicit DocumentValidationException(const std::string& message)
        : SearchEngineException("Document validation error: " + message,
                                SearchErrorCode::DocumentValidationFailed) {}
};

/**
 * @brief Exception thrown when there's an issue with search operations.
 */
class SearchOperationException : public SearchEngineException {
public:
    explicit SearchOperationException(const std::string& message)
        : SearchEngineException("Search operation error: " + message,
                                SearchErrorCode::InternalError) {}
};

/**
 * @brief Exception thrown when a search query is invalid.
 */
class InvalidQueryException : public SearchEngineException {
public:
    explicit InvalidQueryException(const std::string& message)
        : SearchEngineException("Invalid query: " + message,
                                SearchErrorCode::InvalidQuery) {}
};

/**
 * @brief Exception thrown when the index is corrupted.
 */
class IndexCorruptedException : public SearchEngineException {
public:
    explicit IndexCorruptedException(const std::string& message)
        : SearchEngineException("Index corrupted: " + message,
                                SearchErrorCode::IndexCorrupted) {}
};

/**
 * @brief Exception thrown on I/O errors.
 */
class IOErrorException : public SearchEngineException {
public:
    explicit IOErrorException(const std::string& message)
        : SearchEngineException("I/O error: " + message,
                                SearchErrorCode::IOError) {}
};

/**
 * @brief Exception thrown when an operation times out.
 */
class TimeoutException : public SearchEngineException {
public:
    explicit TimeoutException(const std::string& message)
        : SearchEngineException("Timeout: " + message,
                                SearchErrorCode::Timeout) {}
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_CORE_EXCEPTIONS_HPP
