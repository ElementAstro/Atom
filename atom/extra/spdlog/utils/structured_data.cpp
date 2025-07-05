#include "structured_data.h"

#include <sstream>

namespace modern_log {

bool StructuredData::has(std::string_view key) const {
    return fields_.contains(std::string(key));
}

bool StructuredData::remove(std::string_view key) {
    return fields_.erase(std::string(key)) > 0;
}

void StructuredData::clear() { fields_.clear(); }

size_t StructuredData::size() const { return fields_.size(); }

bool StructuredData::empty() const { return fields_.empty(); }

std::string StructuredData::to_json() const {
    if (fields_.empty()) {
        return "{}";
    }

    std::ostringstream oss;
    oss << "{";

    bool first = true;
    for (const auto& [key, value] : fields_) {
        if (!first)
            oss << ",";
        oss << "\"" << key << "\":" << any_to_string(value);
        first = false;
    }

    oss << "}";
    return oss.str();
}

StructuredData StructuredData::from_json(std::string_view json) {
    // 简化的JSON解析实现
    // 实际项目中应使用专业的JSON库如nlohmann/json
    StructuredData data;

    // 这里只是一个占位实现
    // 真实实现需要完整的JSON解析器

    return data;
}

StructuredData StructuredData::merge(const StructuredData& other) const {
    StructuredData result = *this;

    for (const auto& [key, value] : other.fields_) {
        result.fields_[key] = value;
    }

    return result;
}

std::vector<std::string> StructuredData::keys() const {
    std::vector<std::string> result;
    result.reserve(fields_.size());

    for (const auto& [key, value] : fields_) {
        result.push_back(key);
    }

    return result;
}

std::string StructuredData::any_to_string(const std::any& value) const {
    try {
        if (value.type() == typeid(std::string)) {
            return "\"" + std::any_cast<std::string>(value) + "\"";
        } else if (value.type() == typeid(const char*)) {
            return "\"" + std::string(std::any_cast<const char*>(value)) + "\"";
        } else if (value.type() == typeid(int)) {
            return std::to_string(std::any_cast<int>(value));
        } else if (value.type() == typeid(long)) {
            return std::to_string(std::any_cast<long>(value));
        } else if (value.type() == typeid(long long)) {
            return std::to_string(std::any_cast<long long>(value));
        } else if (value.type() == typeid(unsigned int)) {
            return std::to_string(std::any_cast<unsigned int>(value));
        } else if (value.type() == typeid(unsigned long)) {
            return std::to_string(std::any_cast<unsigned long>(value));
        } else if (value.type() == typeid(unsigned long long)) {
            return std::to_string(std::any_cast<unsigned long long>(value));
        } else if (value.type() == typeid(float)) {
            return std::to_string(std::any_cast<float>(value));
        } else if (value.type() == typeid(double)) {
            return std::to_string(std::any_cast<double>(value));
        } else if (value.type() == typeid(bool)) {
            return std::any_cast<bool>(value) ? "true" : "false";
        }
    } catch (const std::bad_any_cast&) {
        // 处理转换失败的情况
    }

    return "null";
}

}  // namespace modern_log
