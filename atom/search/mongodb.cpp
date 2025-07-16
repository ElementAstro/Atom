/**
 * @file mongodb.cpp
 * @brief Implementation of the high-performance, thread-safe MongoDB client.
 * @date 2025-07-16
 */

#include "mongodb.hpp"

#include <spdlog/spdlog.h>

#include <fstream>

namespace atom::database {

// A global instance for the mongocxx driver
namespace {
mongocxx::instance instance{};
}  // namespace

class MongoDB::Impl {
public:
    Impl(std::string_view uri_string, std::string_view db_name)
        : pool_(mongocxx::uri{uri_string}), db_name_(db_name) {}

    mongocxx::pool pool_;
    std::string db_name_;
};

MongoDB::MongoDB(std::string_view uri_string, std::string_view db_name)
    : p_impl_(std::make_unique<Impl>(uri_string, db_name)) {}

MongoDB::~MongoDB() = default;

std::optional<mongocxx::result::insert_one> MongoDB::insert_one(
    std::string_view collection_name, const bsoncxx::document::view& doc) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    return collection.insert_one(doc);
}

std::optional<bsoncxx::document::value> MongoDB::find_one(
    std::string_view collection_name, const bsoncxx::document::view& filter,
    const bsoncxx::stdx::optional<mongocxx::options::find>& opts) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    return collection.find_one(filter, opts);
}

std::optional<mongocxx::result::update> MongoDB::update_one(
    std::string_view collection_name, const bsoncxx::document::view& filter,
    const bsoncxx::document::view& update,
    const bsoncxx::stdx::optional<mongocxx::options::update>& opts) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    return collection.update_one(filter, update, opts);
}

std::optional<mongocxx::result::delete_result> MongoDB::delete_one(
    std::string_view collection_name, const bsoncxx::document::view& filter,
    const bsoncxx::stdx::optional<mongocxx::options::delete_options>& opts) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    return collection.delete_one(filter, opts);
}

std::optional<mongocxx::result::insert_many> MongoDB::insert_many(
    std::string_view collection_name,
    const std::vector<bsoncxx::document::view>& docs) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    return collection.insert_many(docs);
}

std::optional<mongocxx::result::update> MongoDB::update_many(
    std::string_view collection_name, const bsoncxx::document::view& filter,
    const bsoncxx::document::view& update,
    const bsoncxx::stdx::optional<mongocxx::options::update>& opts) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    return collection.update_many(filter, update, opts);
}

std::optional<mongocxx::result::delete_result> MongoDB::delete_many(
    std::string_view collection_name, const bsoncxx::document::view& filter,
    const bsoncxx::stdx::optional<mongocxx::options::delete_options>& opts) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    return collection.delete_many(filter, opts);
}

std::future<std::optional<mongocxx::result::insert_one>>
MongoDB::async_insert_one(std::string_view collection_name,
                        bsoncxx::document::value doc) {
    return std::async(std::launch::async, [this, collection_name,
                                           doc = std::move(doc)]() mutable {
        return insert_one(collection_name, doc.view());
    });
}

std::future<std::optional<bsoncxx::document::value>> MongoDB::async_find_one(
    std::string_view collection_name, bsoncxx::document::value filter) {
    return std::async(std::launch::async, [this, collection_name,
                                           filter = std::move(filter)]() mutable {
        return find_one(collection_name, filter.view());
    });
}

std::future<std::optional<mongocxx::result::update>> MongoDB::async_update_one(
    std::string_view collection_name, bsoncxx::document::value filter,
    bsoncxx::document::value update) {
    return std::async(std::launch::async, [this, collection_name,
                                           filter = std::move(filter),
                                           update = std::move(update)]() mutable {
        return update_one(collection_name, filter.view(), update.view());
    });
}

std::future<std::optional<mongocxx::result::delete_result>>
MongoDB::async_delete_one(std::string_view collection_name,
                        bsoncxx::document::value filter) {
    return std::async(std::launch::async, [this, collection_name,
                                           filter = std::move(filter)]() mutable {
        return delete_one(collection_name, filter.view());
    });
}

MongoCursor MongoDB::find(
    std::string_view collection_name, const bsoncxx::document::view& filter,
    const bsoncxx::stdx::optional<mongocxx::options::find>& opts) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    return MongoCursor(collection.find(filter, opts));
}

MongoCursor MongoDB::aggregate(
    std::string_view collection_name, const bsoncxx::pipeline& pipeline,
    const bsoncxx::stdx::optional<mongocxx::options::aggregate>& opts) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    return MongoCursor(collection.aggregate(pipeline, opts));
}

int64_t MongoDB::count_documents(
    std::string_view collection_name, const bsoncxx::document::view& filter,
    const bsoncxx::stdx::optional<mongocxx::options::count>& opts) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    return collection.count_documents(filter, opts);
}

MongoChangeStream MongoDB::watch(const bsoncxx::pipeline& pipeline) {
    auto client = p_impl_->pool_.acquire();
    auto db = (*client)[p_impl_->db_name_];
    return MongoChangeStream(db.watch(pipeline));
}

GridFSBucket MongoDB::gridfs_bucket(
    const bsoncxx::stdx::optional<mongocxx::options::gridfs::bucket>& opts) {
    return GridFSBucket(p_impl_->pool_.acquire(), p_impl_->db_name_, opts);
}

void MongoDB::create_index(std::string_view collection_name,
                         const bsoncxx::document::view& keys,
                         const mongocxx::options::index& opts) {
    auto client = p_impl_->pool_.acquire();
    auto collection = (*client)[p_impl_->db_name_][collection_name];
    collection.create_index(keys, opts);
}

void MongoDB::drop_collection(std::string_view collection_name) {
    auto client = p_impl_->pool_.acquire();
    (*client)[p_impl_->db_name_][collection_name].drop();
}

std::vector<std::string> MongoDB::list_collection_names() {
    auto client = p_impl_->pool_.acquire();
    auto collections = client->database(p_impl_->db_name_).list_collection_names();
    std::vector<std::string> names;
    for (const auto& name : collections) {
        names.push_back(name);
    }
    return names;
}

void MongoDB::with_transaction(
    const std::function<void(mongocxx::client_session&)>& func) {
    auto client = p_impl_->pool_.acquire();
    auto session = client->start_session();
    try {
        session.start_transaction();
        func(session);
        session.commit_transaction();
    } catch (const mongocxx::exception& e) {
        if (session.in_transaction()) {
            session.abort_transaction();
        }
        spdlog::error("MongoDB transaction failed: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        if (session.in_transaction()) {
            session.abort_transaction();
        }
        spdlog::error("An unexpected error occurred in transaction: {}", e.what());
        throw;
    }
}

bsoncxx::document::value MongoDB::ping() {
    auto client = p_impl_->pool_.acquire();
    return client->database("admin").run_command(
        bsoncxx::builder::stream::document{} << "ping" << 1
                                             << bsoncxx::builder::stream::finalize);
}

// MongoCursor Implementation
MongoCursor::MongoCursor(mongocxx::cursor&& cursor) : cursor_(std::move(cursor)) {}

MongoCursor::~MongoCursor() = default;

MongoCursor::MongoCursor(MongoCursor&&) noexcept = default;
MongoCursor& MongoCursor::operator=(MongoCursor&&) noexcept = default;

MongoCursor::iterator::iterator(mongocxx::cursor::iterator iter)
    : iter_(std::move(iter)) {}

MongoCursor::iterator::reference MongoCursor::iterator::operator*() const {
    return *iter_;
}

MongoCursor::iterator::pointer MongoCursor::iterator::operator->() const {
    return &(*iter_);
}

MongoCursor::iterator& MongoCursor::iterator::operator++() {
    ++iter_;
    return *this;
}

bool MongoCursor::iterator::operator!=(const iterator& other) const {
    return iter_ != other.iter_;
}

MongoCursor::iterator MongoCursor::begin() { return iterator(cursor_.begin()); }

MongoCursor::iterator MongoCursor::end() { return iterator(cursor_.end()); }

// MongoChangeStream Implementation
MongoChangeStream::MongoChangeStream(mongocxx::change_stream&& stream)
    : stream_(std::move(stream)) {}

MongoChangeStream::~MongoChangeStream() = default;

MongoChangeStream::MongoChangeStream(MongoChangeStream&&) noexcept = default;
MongoChangeStream& MongoChangeStream::operator=(MongoChangeStream&&) noexcept =
    default;

MongoChangeStream::iterator::iterator(mongocxx::change_stream::iterator iter)
    : iter_(std::move(iter)) {}

MongoChangeStream::iterator::reference
MongoChangeStream::iterator::operator*() const {
    return *iter_;
}

MongoChangeStream::iterator::pointer
MongoChangeStream::iterator::operator->() const {
    return &(*iter_);
}

MongoChangeStream::iterator& MongoChangeStream::iterator::operator++() {
    ++iter_;
    return *this;
}

bool MongoChangeStream::iterator::operator!=(const iterator& other) const {
    return iter_ != other.iter_;
}

MongoChangeStream::iterator MongoChangeStream::begin() {
    return iterator(stream_.begin());
}

MongoChangeStream::iterator MongoChangeStream::end() {
    return iterator(stream_.end());
}

// GridFSBucket Implementation
GridFSBucket::GridFSBucket(
    mongocxx::pool::entry&& client,
    const std::string& db_name,
    const bsoncxx::stdx::optional<mongocxx::options::gridfs::bucket>& opts)
    : client_(std::move(client)),
      bucket_((*client_)[db_name].gridfs_bucket(opts)) {}

GridFSBucket::~GridFSBucket() = default;

GridFSBucket::GridFSBucket(GridFSBucket&&) noexcept = default;
GridFSBucket& GridFSBucket::operator=(GridFSBucket&&) noexcept = default;

bsoncxx::types::value GridFSBucket::upload_from_file(
    std::string_view filename, std::string_view source_path) {
    std::ifstream stream(std::string(source_path), std::ios::binary);
    if (!stream) {
        throw MongoException("Failed to open source file for GridFS upload");
    }
    auto uploader = bucket_.open_upload_stream(filename);
    char buffer[4096];
    while (stream.read(buffer, sizeof(buffer))) {
        uploader.write(reinterpret_cast<const std::uint8_t*>(buffer),
                       stream.gcount());
    }
    uploader.write(reinterpret_cast<const std::uint8_t*>(buffer), stream.gcount());
    return uploader.close().id();
}

void GridFSBucket::download_to_file(const bsoncxx::types::value& file_id,
                                    std::string_view destination_path) {
    std::ofstream stream(std::string(destination_path), std::ios::binary);
    if (!stream) {
        throw MongoException("Failed to open destination file for GridFS download");
    }
    auto downloader = bucket_.open_download_stream(file_id);
    auto file_length = downloader.file_length();
    auto bytes_read = 0;
    const auto buffer_size = 4096;
    auto buffer = std::make_unique<std::uint8_t[]>(buffer_size);
    while (bytes_read < file_length) {
        auto bytes_to_read = downloader.read(
            buffer.get(), buffer_size > (file_length - bytes_read)
                              ? (file_length - bytes_read)
                              : buffer_size);
        stream.write(reinterpret_cast<const char*>(buffer.get()), bytes_to_read);
        bytes_read += bytes_to_read;
    }
}

void GridFSBucket::delete_file(const bsoncxx::types::value& file_id) {
    bucket_.delete_file(file_id);
}

std::optional<bsoncxx::document::value> GridFSBucket::find_file(
    const bsoncxx::document::view& filter) {
    auto cursor = bucket_.find(filter);
    auto it = cursor.begin();
    if (it != cursor.end()) {
        return *it;
    }
    return std::nullopt;
}

}  // namespace atom::database
