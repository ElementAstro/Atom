#include "multipart.hpp"

#include <curl/curl.h>

namespace atom::extra::curl {
MultipartForm::MultipartForm() : form_(nullptr) {}

MultipartForm::~MultipartForm() {
    if (form_) {
        curl_mime_free(form_);
    }
}

MultipartForm::MultipartForm(MultipartForm&& other) noexcept
    : form_(other.form_) {
    other.form_ = nullptr;
}

MultipartForm& MultipartForm::operator=(MultipartForm&& other) noexcept {
    if (this != &other) {
        if (form_) {
            curl_mime_free(form_);
        }
        form_ = other.form_;
        other.form_ = nullptr;
    }
    return *this;
}

void MultipartForm::add_file(std::string_view name, std::string_view filepath,
                             std::string_view content_type) {
    if (!form_) {
        initialize();
    }

    curl_mimepart* part = curl_mime_addpart(form_);
    curl_mime_name(part, name.data());
    curl_mime_filedata(part, filepath.data());
    if (!content_type.empty()) {
        curl_mime_type(part, content_type.data());
    }
}

void MultipartForm::add_buffer(std::string_view name, const void* data,
                               size_t size, std::string_view filename,
                               std::string_view content_type) {
    if (!form_) {
        initialize();
    }

    curl_mimepart* part = curl_mime_addpart(form_);
    curl_mime_name(part, name.data());
    curl_mime_data(part, static_cast<const char*>(data), size);
    curl_mime_filename(part, filename.data());
    if (!content_type.empty()) {
        curl_mime_type(part, content_type.data());
    }
}

void MultipartForm::add_field(std::string_view name, std::string_view content) {
    if (!form_) {
        initialize();
    }

    curl_mimepart* part = curl_mime_addpart(form_);
    curl_mime_name(part, name.data());
    curl_mime_data(part, content.data(), content.size());
}

void MultipartForm::add_field_with_type(std::string_view name,
                                        std::string_view content,
                                        std::string_view content_type) {
    if (!form_) {
        initialize();
    }

    curl_mimepart* part = curl_mime_addpart(form_);
    curl_mime_name(part, name.data());
    curl_mime_data(part, content.data(), content.size());
    curl_mime_type(part, content_type.data());
}

curl_mime* MultipartForm::handle() const { return form_; }

void MultipartForm::initialize() {
    CURL* curl = curl_easy_init();
    form_ = curl_mime_init(curl);
    curl_easy_cleanup(curl);
}
}  // namespace atom::extra::curl
