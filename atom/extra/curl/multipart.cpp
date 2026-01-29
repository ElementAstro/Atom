#include "multipart.hpp"

#include <curl/curl.h>

namespace atom::extra::curl {
MultipartForm::MultipartForm() = default;

MultipartForm::~MultipartForm() = default;

MultipartForm::MultipartForm(MultipartForm&& other) noexcept = default;

MultipartForm& MultipartForm::operator=(MultipartForm&& other) noexcept =
    default;

void MultipartForm::add_file(std::string_view name, std::string_view filepath,
                             std::string_view content_type) {
    if (!form_ || !form_->form) {
        initialize();
    }

    curl_mimepart* part = curl_mime_addpart(form_->form);
    curl_mime_name(part, name.data());
    curl_mime_filedata(part, filepath.data());
    if (!content_type.empty()) {
        curl_mime_type(part, content_type.data());
    }
}

void MultipartForm::add_buffer(std::string_view name, const void* data,
                               size_t size, std::string_view filename,
                               std::string_view content_type) {
    if (!form_ || !form_->form) {
        initialize();
    }

    curl_mimepart* part = curl_mime_addpart(form_->form);
    curl_mime_name(part, name.data());
    curl_mime_data(part, static_cast<const char*>(data), size);
    curl_mime_filename(part, filename.data());
    if (!content_type.empty()) {
        curl_mime_type(part, content_type.data());
    }
}

void MultipartForm::add_field(std::string_view name, std::string_view content) {
    if (!form_ || !form_->form) {
        initialize();
    }

    curl_mimepart* part = curl_mime_addpart(form_->form);
    curl_mime_name(part, name.data());
    curl_mime_data(part, content.data(), content.size());
}

void MultipartForm::add_field_with_type(std::string_view name,
                                        std::string_view content,
                                        std::string_view content_type) {
    if (!form_ || !form_->form) {
        initialize();
    }

    curl_mimepart* part = curl_mime_addpart(form_->form);
    curl_mime_name(part, name.data());
    curl_mime_data(part, content.data(), content.size());
    curl_mime_type(part, content_type.data());
}

curl_mime* MultipartForm::handle() const {
    return form_ ? form_->form : nullptr;
}

void MultipartForm::initialize() {
    CURL* curl = curl_easy_init();
    curl_mime* mime = curl_mime_init(curl);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    form_ = std::make_shared<MultipartFormMimeHolder>(mime);
}
}  // namespace atom::extra::curl
