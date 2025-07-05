#ifndef ATOM_EXTRA_CURL_MULTIPART_HPP
#define ATOM_EXTRA_CURL_MULTIPART_HPP

#include <curl/curl.h>
#include <string_view>

namespace atom::extra::curl {
/**
 * @brief A class for building multipart/form-data requests.
 *
 * This class simplifies the creation of multipart/form-data requests,
 * which are commonly used for uploading files and submitting forms
 * with various data types. It uses the libcurl's mime API to construct
 * the form data.
 */
class MultipartForm {
public:
    /**
     * @brief Constructor for the MultipartForm class.
     *
     * Initializes an empty multipart form.
     */
    MultipartForm();

    /**
     * @brief Destructor for the MultipartForm class.
     *
     * Frees the resources used by the multipart form.
     */
    ~MultipartForm();

    // Disable copy construction
    MultipartForm(const MultipartForm&) = delete;

    // Disable copy assignment
    MultipartForm& operator=(const MultipartForm&) = delete;

    // Allow move construction
    MultipartForm(MultipartForm&& other) noexcept;

    // Allow move assignment
    MultipartForm& operator=(MultipartForm&& other) noexcept;

    /**
     * @brief Adds a file to the multipart form.
     *
     * @param name The name of the form field.
     * @param filepath The path to the file to be added.
     * @param content_type The content type of the file (optional). If not
     * specified, libcurl will attempt to determine the content type
     * automatically.
     */
    void add_file(std::string_view name, std::string_view filepath,
                  std::string_view content_type = "");

    /**
     * @brief Adds a buffer of data as a file to the multipart form.
     *
     * @param name The name of the form field.
     * @param data A pointer to the data buffer.
     * @param size The size of the data buffer in bytes.
     * @param filename The filename to be associated with the data.
     * @param content_type The content type of the data (optional).
     */
    void add_buffer(std::string_view name, const void* data, size_t size,
                    std::string_view filename,
                    std::string_view content_type = "");

    /**
     * @brief Adds a form field to the multipart form.
     *
     * @param name The name of the form field.
     * @param content The content of the form field.
     */
    void add_field(std::string_view name, std::string_view content);

    /**
     * @brief Adds a form field to the multipart form with a specified content
     * type.
     *
     * @param name The name of the form field.
     * @param content The content of the form field.
     * @param content_type The content type of the form field.
     */
    void add_field_with_type(std::string_view name, std::string_view content,
                             std::string_view content_type);

    /**
     * @brief Gets the curl_mime handle associated with the multipart form.
     *
     * This handle can be used with libcurl to set the request body.
     *
     * @return A pointer to the curl_mime handle.
     */
    curl_mime* handle() const;

private:
    /** @brief The curl_mime handle for the multipart form. */
    curl_mime* form_;

    /**
     * @brief Initializes the curl_mime handle.
     *
     * This method is called when the first part is added to the form.
     */
    void initialize();

    /**
     * @brief Friend class declaration for Session.
     *
     * Allows the Session class to access private members of the MultipartForm
     * class.
     */
    friend class Session;
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_MULTIPART_HPP
