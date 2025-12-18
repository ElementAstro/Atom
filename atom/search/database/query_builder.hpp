/*
 * query_builder.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file query_builder.hpp
 * @brief Fluent SQL query builder for database operations.
 */

#ifndef ATOM_SEARCH_DATABASE_QUERY_BUILDER_HPP
#define ATOM_SEARCH_DATABASE_QUERY_BUILDER_HPP

#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "types.hpp"

namespace atom::search::database {

/**
 * @brief SQL query builder with fluent interface.
 */
class QueryBuilder {
public:
    enum class JoinType { Inner, Left, Right, Full, Cross };

    enum class OrderDirection { Asc, Desc };

    QueryBuilder() = default;

    // SELECT operations
    QueryBuilder& select(std::string_view columns = "*") {
        query_.str("");
        query_ << "SELECT " << columns;
        return *this;
    }

    QueryBuilder& selectDistinct(std::string_view columns = "*") {
        query_.str("");
        query_ << "SELECT DISTINCT " << columns;
        return *this;
    }

    QueryBuilder& from(std::string_view table) {
        query_ << " FROM " << table;
        table_ = table;
        return *this;
    }

    // INSERT operations
    QueryBuilder& insertInto(std::string_view table) {
        query_.str("");
        query_ << "INSERT INTO " << table;
        table_ = table;
        return *this;
    }

    QueryBuilder& columns(std::initializer_list<std::string_view> cols) {
        query_ << " (";
        bool first = true;
        for (const auto& col : cols) {
            if (!first)
                query_ << ", ";
            query_ << col;
            first = false;
        }
        query_ << ")";
        return *this;
    }

    QueryBuilder& values(std::initializer_list<std::string_view> vals) {
        query_ << " VALUES (";
        bool first = true;
        for (const auto& val : vals) {
            if (!first)
                query_ << ", ";
            query_ << val;
            first = false;
        }
        query_ << ")";
        return *this;
    }

    QueryBuilder& valuesPlaceholders(size_t count) {
        query_ << " VALUES (";
        for (size_t i = 0; i < count; ++i) {
            if (i > 0)
                query_ << ", ";
            query_ << "?";
        }
        query_ << ")";
        return *this;
    }

    // UPDATE operations
    QueryBuilder& update(std::string_view table) {
        query_.str("");
        query_ << "UPDATE " << table;
        table_ = table;
        return *this;
    }

    QueryBuilder& set(std::string_view column, std::string_view value) {
        if (setCount_ == 0) {
            query_ << " SET ";
        } else {
            query_ << ", ";
        }
        query_ << column << " = " << value;
        ++setCount_;
        return *this;
    }

    QueryBuilder& setPlaceholder(std::string_view column) {
        return set(column, "?");
    }

    // DELETE operations
    QueryBuilder& deleteFrom(std::string_view table) {
        query_.str("");
        query_ << "DELETE FROM " << table;
        table_ = table;
        return *this;
    }

    // WHERE clause
    QueryBuilder& where(std::string_view condition) {
        query_ << " WHERE " << condition;
        return *this;
    }

    QueryBuilder& whereEqual(std::string_view column, std::string_view value) {
        query_ << " WHERE " << column << " = " << value;
        return *this;
    }

    QueryBuilder& wherePlaceholder(std::string_view column) {
        return whereEqual(column, "?");
    }

    QueryBuilder& andWhere(std::string_view condition) {
        query_ << " AND " << condition;
        return *this;
    }

    QueryBuilder& orWhere(std::string_view condition) {
        query_ << " OR " << condition;
        return *this;
    }

    QueryBuilder& whereIn(std::string_view column, size_t count) {
        query_ << " WHERE " << column << " IN (";
        for (size_t i = 0; i < count; ++i) {
            if (i > 0)
                query_ << ", ";
            query_ << "?";
        }
        query_ << ")";
        return *this;
    }

    QueryBuilder& whereLike(std::string_view column, std::string_view pattern) {
        query_ << " WHERE " << column << " LIKE " << pattern;
        return *this;
    }

    QueryBuilder& whereBetween(std::string_view column) {
        query_ << " WHERE " << column << " BETWEEN ? AND ?";
        return *this;
    }

    QueryBuilder& whereNull(std::string_view column) {
        query_ << " WHERE " << column << " IS NULL";
        return *this;
    }

    QueryBuilder& whereNotNull(std::string_view column) {
        query_ << " WHERE " << column << " IS NOT NULL";
        return *this;
    }

    // JOIN operations
    QueryBuilder& join(std::string_view table, std::string_view condition,
                       JoinType type = JoinType::Inner) {
        switch (type) {
            case JoinType::Inner:
                query_ << " INNER JOIN ";
                break;
            case JoinType::Left:
                query_ << " LEFT JOIN ";
                break;
            case JoinType::Right:
                query_ << " RIGHT JOIN ";
                break;
            case JoinType::Full:
                query_ << " FULL OUTER JOIN ";
                break;
            case JoinType::Cross:
                query_ << " CROSS JOIN ";
                break;
        }
        query_ << table << " ON " << condition;
        return *this;
    }

    QueryBuilder& innerJoin(std::string_view table,
                            std::string_view condition) {
        return join(table, condition, JoinType::Inner);
    }

    QueryBuilder& leftJoin(std::string_view table, std::string_view condition) {
        return join(table, condition, JoinType::Left);
    }

    QueryBuilder& rightJoin(std::string_view table,
                            std::string_view condition) {
        return join(table, condition, JoinType::Right);
    }

    // ORDER BY
    QueryBuilder& orderBy(std::string_view column,
                          OrderDirection dir = OrderDirection::Asc) {
        query_ << " ORDER BY " << column;
        if (dir == OrderDirection::Desc) {
            query_ << " DESC";
        } else {
            query_ << " ASC";
        }
        return *this;
    }

    QueryBuilder& orderByDesc(std::string_view column) {
        return orderBy(column, OrderDirection::Desc);
    }

    // GROUP BY
    QueryBuilder& groupBy(std::string_view columns) {
        query_ << " GROUP BY " << columns;
        return *this;
    }

    QueryBuilder& having(std::string_view condition) {
        query_ << " HAVING " << condition;
        return *this;
    }

    // LIMIT and OFFSET
    QueryBuilder& limit(int count) {
        query_ << " LIMIT " << count;
        return *this;
    }

    QueryBuilder& offset(int count) {
        query_ << " OFFSET " << count;
        return *this;
    }

    QueryBuilder& paginate(int page, int pageSize) {
        return limit(pageSize).offset((page - 1) * pageSize);
    }

    // Aggregate functions
    static std::string count(std::string_view column = "*") {
        return "COUNT(" + std::string(column) + ")";
    }

    static std::string sum(std::string_view column) {
        return "SUM(" + std::string(column) + ")";
    }

    static std::string avg(std::string_view column) {
        return "AVG(" + std::string(column) + ")";
    }

    static std::string max(std::string_view column) {
        return "MAX(" + std::string(column) + ")";
    }

    static std::string min(std::string_view column) {
        return "MIN(" + std::string(column) + ")";
    }

    // Raw SQL
    QueryBuilder& raw(std::string_view sql) {
        query_ << sql;
        return *this;
    }

    // Build the query
    [[nodiscard]] std::string build() const { return query_.str(); }

    [[nodiscard]] std::string str() const { return build(); }

    operator std::string() const { return build(); }

    // Reset the builder
    QueryBuilder& reset() {
        query_.str("");
        query_.clear();
        table_.clear();
        setCount_ = 0;
        return *this;
    }

private:
    std::ostringstream query_;
    std::string table_;
    size_t setCount_{0};
};

/**
 * @brief Helper for building parameterized queries.
 */
class ParameterizedQuery {
public:
    explicit ParameterizedQuery(std::string query) : query_(std::move(query)) {}

    ParameterizedQuery& bind(const ParamValue& value) {
        params_.push_back(value);
        return *this;
    }

    template <typename T>
    ParameterizedQuery& bind(T&& value) {
        params_.emplace_back(std::forward<T>(value));
        return *this;
    }

    [[nodiscard]] const std::string& query() const { return query_; }
    [[nodiscard]] const ParamList& params() const { return params_; }

    void clearParams() { params_.clear(); }

private:
    std::string query_;
    ParamList params_;
};

}  // namespace atom::search::database

#endif  // ATOM_SEARCH_DATABASE_QUERY_BUILDER_HPP
