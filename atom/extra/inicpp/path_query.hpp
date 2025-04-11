#ifndef ATOM_EXTRA_INICPP_PATH_QUERY_HPP
#define ATOM_EXTRA_INICPP_PATH_QUERY_HPP

#include <string>
#include <vector>
#include "common.hpp"

namespace inicpp {

/**
 * @class PathQuery
 * @brief 提供对嵌套段落和复杂路径的查询支持
 */
class PathQuery {
private:
    std::vector<std::string> pathParts_;

public:
    /**
     * @brief 默认构造函数
     */
    PathQuery() = default;

    /**
     * @brief 从路径字符串构造
     * @param path 格式为 "section.subsection.field" 的路径字符串
     */
    explicit PathQuery(std::string_view path) : pathParts_(splitPath(path)) {}

    /**
     * @brief 从路径部分构造
     * @param pathParts 路径部分的向量
     */
    explicit PathQuery(std::vector<std::string> pathParts) : pathParts_(std::move(pathParts)) {}

    /**
     * @brief 获取路径部分
     * @return 路径部分向量的常量引用
     */
    [[nodiscard]] const std::vector<std::string>& parts() const noexcept {
        return pathParts_;
    }

    /**
     * @brief 获取路径部分
     * @return 路径部分向量的引用
     */
    std::vector<std::string>& parts() noexcept {
        return pathParts_;
    }

    /**
     * @brief 获取段路径
     * @return 段名称的向量，不包括字段名
     */
    [[nodiscard]] std::vector<std::string> getSectionPath() const {
        if (pathParts_.empty()) {
            return {};
        }
        return std::vector<std::string>(pathParts_.begin(), pathParts_.end() - 1);
    }

    /**
     * @brief 获取字段名
     * @return 路径中的字段名
     */
    [[nodiscard]] std::string getFieldName() const {
        if (pathParts_.empty()) {
            return {};
        }
        return pathParts_.back();
    }

    /**
     * @brief 获取根段名称
     * @return 路径中的第一个段名称
     */
    [[nodiscard]] std::string getRootSection() const {
        if (pathParts_.empty()) {
            return {};
        }
        return pathParts_[0];
    }

    /**
     * @brief 检查路径是否为空
     * @return 如果路径为空则为true，否则为false
     */
    [[nodiscard]] bool empty() const noexcept {
        return pathParts_.empty();
    }

    /**
     * @brief 获取路径长度
     * @return 路径部分的数量
     */
    [[nodiscard]] size_t size() const noexcept {
        return pathParts_.size();
    }

    /**
     * @brief 获取完整路径字符串
     * @return 完整的点分隔路径
     */
    [[nodiscard]] std::string toString() const {
        return joinPath(pathParts_);
    }

    /**
     * @brief 获取父路径
     * @return 不包括最后一部分的新路径查询
     */
    [[nodiscard]] PathQuery parent() const {
        if (pathParts_.empty()) {
            return PathQuery();
        }
        return PathQuery(std::vector<std::string>(pathParts_.begin(), pathParts_.end() - 1));
    }

    /**
     * @brief 添加路径部分
     * @param part 要添加的部分
     * @return 此对象的引用
     */
    PathQuery& append(std::string_view part) {
        pathParts_.emplace_back(part);
        return *this;
    }

    /**
     * @brief 基于另一个路径创建子路径
     * @param base 基础路径
     * @param extension 扩展路径
     * @return 组合的新路径查询
     */
    static PathQuery combine(const PathQuery& base, const PathQuery& extension) {
        std::vector<std::string> combinedParts = base.parts();
        const auto& extensionParts = extension.parts();
        combinedParts.insert(combinedParts.end(), extensionParts.begin(), extensionParts.end());
        return PathQuery(std::move(combinedParts));
    }

    /**
     * @brief 检查路径是否有效
     * @return 如果路径有有效格式，则为true
     */
    [[nodiscard]] bool isValid() const noexcept {
        if (pathParts_.empty()) {
            return false;
        }

        // 检查路径中是否有空部分
        for (const auto& part : pathParts_) {
            if (part.empty()) {
                return false;
            }
        }

        return true;
    }
};

} // namespace inicpp

#endif // ATOM_EXTRA_INICPP_PATH_QUERY_HPP