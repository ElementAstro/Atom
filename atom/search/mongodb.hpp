/**
 * @file mongodb.hpp
 * @brief A high-performance, thread-safe MongoDB client for Atom Search.
 * @date 2025-07-16
 */

#ifndef ATOM_SEARCH_MONGODB_HPP
#define ATOM_SEARCH_MONGODB_HPP

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/aggregate.hpp>
#include <mongocxx/options/count.hpp>
#include <mongocxx/options/delete.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/options/index.hpp>
#include <mongocxx/options/update.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/result/delete.hpp>
#include <mongocxx/result/insert_many.hpp>
#include <mongocxx/result/insert_one.hpp>
#include <mongocxx/result/update.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/change_stream.hpp>
#include <mongocxx/gridfs/bucket.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/stdx/optional.hpp>
#include <bsoncxx/types.hpp>

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace atom::database {

/**
 * @brief Custom exception for MongoDB-related errors.
 */
class MongoException : public std::runtime_error {
public:
    explicit MongoException(const std::string& message)
        : std::runtime_error(message) {}
};

class MongoCursor;
class MongoChangeStream;
class GridFSBucket;

/**
 * @class MongoDB
 * @brief A high-performance, thread-safe MongoDB client.
 *
 * This class uses the official mongocxx driver's connection pool to manage
 * connections and provide a safe, modern C++ interface for database operations.
 */
class MongoDB {
public:
    /**
     * @brief Constructs a MongoDB client.
     * @param uri_string The MongoDB connection string URI.
     * @param db_name The name of the database to use.
     * @throws MongoException if the client cannot be initialized.
     */
    explicit MongoDB(std::string_view uri_string, std::string_view db_name);

    ~MongoDB();

    MongoDB(const MongoDB&) = delete;
    MongoDB& operator=(const MongoDB&) = delete;
    MongoDB(MongoDB&&) = delete;
    MongoDB& operator=(MongoDB&&) = delete;

    // --- Single Document CRUD ---
    std::optional<mongocxx::result::insert_one> insert_one(
        std::string_view collection_name, const bsoncxx::document::view& doc);

    [[nodiscard]] std::optional<bsoncxx::document::value> find_one(
        std::string_view collection_name,
        const bsoncxx::document::view& filter,
        const bsoncxx::stdx::optional<mongocxx::options::find>& opts =
            bsoncxx::stdx::nullopt);

    std::optional<mongocxx::result::update> update_one(
        std::string_view collection_name,
        const bsoncxx::document::view& filter,
        const bsoncxx::document::view& update,
        const bsoncxx::stdx::optional<mongocxx::options::update>& opts =
            bsoncxx::stdx::nullopt);

    std::optional<mongocxx::result::delete_result> delete_one(
        std::string_view collection_name,
        const bsoncxx::document::view& filter,
        const bsoncxx::stdx::optional<mongocxx::options::delete_options>& opts =
            bsoncxx::stdx::nullopt);

    // --- Bulk Operations ---
    std::optional<mongocxx::result::insert_many> insert_many(
        std::string_view collection_name,
        const std::vector<bsoncxx::document::view>& docs);

    std::optional<mongocxx::result::update> update_many(
        std::string_view collection_name,
        const bsoncxx::document::view& filter,
        const bsoncxx::document::view& update,
        const bsoncxx::stdx::optional<mongocxx::options::update>& opts =
            bsoncxx::stdx::nullopt);

    std::optional<mongocxx::result::delete_result> delete_many(
        std::string_view collection_name,
        const bsoncxx::document::view& filter,
        const bsoncxx::stdx::optional<mongocxx::options::delete_options>& opts =
            bsoncxx::stdx::nullopt);

    // --- Asynchronous Operations ---
    [[nodiscard]] std::future<std::optional<mongocxx::result::insert_one>>
    async_insert_one(std::string_view collection_name,
                     bsoncxx::document::value doc);

    [[nodiscard]] std::future<std::optional<bsoncxx::document::value>>
    async_find_one(std::string_view collection_name,
                   bsoncxx::document::value filter);

    [[nodiscard]] std::future<std::optional<mongocxx::result::update>>
    async_update_one(std::string_view collection_name,
                     bsoncxx::document::value filter,
                     bsoncxx::document::value update);

    [[nodiscard]] std::future<std::optional<mongocxx::result::delete_result>>
    async_delete_one(std::string_view collection_name,
                     bsoncxx::document::value filter);

    // --- Queries, Aggregation & Counting ---
    [[nodiscard]] MongoCursor find(
        std::string_view collection_name,
        const bsoncxx::document::view& filter,
        const bsoncxx::stdx::optional<mongocxx::options::find>& opts =
            bsoncxx::stdx::nullopt);

    [[nodiscard]] MongoCursor aggregate(
        std::string_view collection_name,
        const bsoncxx::pipeline& pipeline,
        const bsoncxx::stdx::optional<mongocxx::options::aggregate>& opts =
            bsoncxx::stdx::nullopt);

    [[nodiscard]] int64_t count_documents(
        std::string_view collection_name,
        const bsoncxx::document::view& filter,
        const bsoncxx::stdx::optional<mongocxx::options::count>& opts =
            bsoncxx::stdx::nullopt);

    // --- Change Streams ---
    [[nodiscard]] MongoChangeStream watch(const bsoncxx::pipeline& pipeline = {});

    // --- GridFS ---
    [[nodiscard]] GridFSBucket gridfs_bucket(
        const bsoncxx::stdx::optional<mongocxx::options::gridfs::bucket>& opts =
            bsoncxx::stdx::nullopt);

    // --- Collection & Index Management ---
    void create_index(std::string_view collection_name,
                      const bsoncxx::document::view& keys,
                      const mongocxx::options::index& opts);

    void drop_collection(std::string_view collection_name);

    [[nodiscard]] std::vector<std::string> list_collection_names();

    // --- Transactions & Admin ---
    void with_transaction(
        const std::function<void(mongocxx::client_session&)>& func);

    bsoncxx::document::value ping();

private:
    friend class GridFSBucket;
    class Impl;
    std::unique_ptr<Impl> p_impl_;
};

/**
 * @class MongoCursor
 * @brief A RAII wrapper for a mongocxx::cursor.
 */
class MongoCursor {
public:
    MongoCursor(mongocxx::cursor&& cursor);
    ~MongoCursor();

    MongoCursor(const MongoCursor&) = delete;
    MongoCursor& operator=(const MongoCursor&) = delete;
    MongoCursor(MongoCursor&&) noexcept;
    MongoCursor& operator=(MongoCursor&&) noexcept;

    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = bsoncxx::document::view;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        iterator(mongocxx::cursor::iterator iter);

        reference operator*() const;
        pointer operator->() const;
        iterator& operator++();
        bool operator!=(const iterator& other) const;

    private:
        mongocxx::cursor::iterator iter_;
    };

    [[nodiscard]] iterator begin();
    [[nodiscard]] iterator end();

private:
    mongocxx::cursor cursor_;
};

/**
 * @class MongoChangeStream
 * @brief A RAII wrapper for a mongocxx::change_stream.
 */
class MongoChangeStream {
public:
    MongoChangeStream(mongocxx::change_stream&& stream);
    ~MongoChangeStream();

    MongoChangeStream(const MongoChangeStream&) = delete;
    MongoChangeStream& operator=(const MongoChangeStream&) = delete;
    MongoChangeStream(MongoChangeStream&&) noexcept;
    MongoChangeStream& operator=(MongoChangeStream&&) noexcept;

    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = bsoncxx::document::view;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        iterator(mongocxx::change_stream::iterator iter);

        reference operator*() const;
        pointer operator->() const;
        iterator& operator++();
        bool operator!=(const iterator& other) const;

    private:
        mongocxx::change_stream::iterator iter_;
    };

    [[nodiscard]] iterator begin();
    [[nodiscard]] iterator end();

private:
    mongocxx::change_stream stream_;
};

/**
 * @class GridFSBucket
 * @brief Provides an interface for GridFS operations.
 */
class GridFSBucket {
public:
    ~GridFSBucket();

    GridFSBucket(const GridFSBucket&) = delete;
    GridFSBucket& operator=(const GridFSBucket&) = delete;
    GridFSBucket(GridFSBucket&&) noexcept;
    GridFSBucket& operator=(GridFSBucket&&) noexcept;

    [[nodiscard]] bsoncxx::types::value upload_from_file(
        std::string_view filename, std::string_view source_path);

    void download_to_file(const bsoncxx::types::value& file_id,
                          std::string_view destination_path);

    void delete_file(const bsoncxx::types::value& file_id);

    [[nodiscard]] std::optional<bsoncxx::document::value> find_file(
        const bsoncxx::document::view& filter);

private:
    friend class MongoDB;
    GridFSBucket(mongocxx::pool::entry&& client, const std::string& db_name,
                 const bsoncxx::stdx::optional<mongocxx::options::gridfs::bucket>& opts);

    mongocxx::pool::entry client_;
    mongocxx::gridfs::bucket bucket_;
};

}  // namespace atom::database

#endif  // ATOM_SEARCH_MONGODB_HPP
