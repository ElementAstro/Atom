#ifndef ATOM_EXTRA_CURL_MULTIPART_HPP
#define ATOM_EXTRA_CURL_MULTIPART_HPP

#include <curl/curl.h>
#include <string_view>
class MultipartForm {
public:
    MultipartForm() : form_(nullptr) {}

    ~MultipartForm() {
        if (form_) {
            curl_mime_free(form_);
        }
    }

    // 禁止拷贝
    MultipartForm(const MultipartForm&) = delete;
    MultipartForm& operator=(const MultipartForm&) = delete;

    // 允许移动
    MultipartForm(MultipartForm&& other) noexcept : form_(other.form_) {
        other.form_ = nullptr;
    }

    MultipartForm& operator=(MultipartForm&& other) noexcept {
        if (this != &other) {
            if (form_) {
                curl_mime_free(form_);
            }
            form_ = other.form_;
            other.form_ = nullptr;
        }
        return *this;
    }

    // 添加文件
    void add_file(std::string_view name, std::string_view filepath,
                  std::string_view content_type = "") {
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

    // 添加内存数据作为文件
    void add_buffer(std::string_view name, const void* data, size_t size,
                    std::string_view filename,
                    std::string_view content_type = "") {
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

    // 添加表单字段
    void add_field(std::string_view name, std::string_view content) {
        if (!form_) {
            initialize();
        }

        curl_mimepart* part = curl_mime_addpart(form_);
        curl_mime_name(part, name.data());
        curl_mime_data(part, content.data(), content.size());
    }

    // 添加表单字段，指定内容类型
    void add_field_with_type(std::string_view name, std::string_view content,
                             std::string_view content_type) {
        if (!form_) {
            initialize();
        }

        curl_mimepart* part = curl_mime_addpart(form_);
        curl_mime_name(part, name.data());
        curl_mime_data(part, content.data(), content.size());
        curl_mime_type(part, content_type.data());
    }

    curl_mime* handle() const { return form_; }

private:
    curl_mime* form_;

    void initialize() {
        CURL* curl = curl_easy_init();
        form_ = curl_mime_init(curl);
        curl_easy_cleanup(curl);
    }

    friend class Session;
};

#endif