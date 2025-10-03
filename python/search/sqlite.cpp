#include "atom/search/sqlite.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>


namespace py = pybind11;

PYBIND11_MODULE(sqlite, m) {
    m.doc() = "SQLite database module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const SQLiteException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Bind vector of strings (row data)
    py::bind_vector<std::vector<std::string>>(
        m, "RowData", "A single row of data from a SQLite query result");

    // Bind vector of row data (result set)
    py::bind_vector<std::vector<std::vector<std::string>>>(
        m, "ResultSet", "A complete result set from a SQLite query");

    // SqliteDB class binding
    py::class_<SqliteDB>(
        m, "SqliteDB",
        R"(A thread-safe class for managing SQLite database operations.

This class provides methods to execute queries, manage transactions, and retrieve data
from SQLite databases.

Args:
    db_path: Path to the SQLite database file

Examples:
    >>> from atom.search.sqlite import SqliteDB
    >>> db = SqliteDB("example.db")
    >>> db.execute_query("CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT) ")
    True
    >>> db.execute_query("INSERT INTO test VALUES (1, 'example') ")
    True
    >>> results = db.select_data("SELECT * FROM test")
    >>> results[0][1]  # Access first row, second column
    'example'
)")
        .def(py::init<std::string_view>(), py::arg("db_path"),
             "Constructs a SqliteDB object with the given database path.")
        .def("execute_query", &SqliteDB::executeQuery, py::arg("query"),
             R"(Execute a SQL query.

Args:
    query: SQL query string

Returns:
    True if the query was executed successfully

Raises:
    RuntimeError: If there's an error executing the query
)")
        .def("select_data", &SqliteDB::selectData, py::arg("query"),
             R"(Query and retrieve data.

Args:
    query: SQL query string

Returns:
    Result set containing all rows from the query

Raises:
    RuntimeError: If there's an error executing the query
)")
        .def("get_int_value", &SqliteDB::getIntValue, py::arg("query"),
             R"(Retrieve an integer value.

Args:
    query: SQL query string that should return a single integer value

Returns:
    Optional integer value (None if query fails)

Raises:
    RuntimeError: On serious database errors
)")
        .def("get_double_value", &SqliteDB::getDoubleValue, py::arg("query"),
             R"(Retrieve a floating-point value.

Args:
    query: SQL query string that should return a single floating-point value

Returns:
    Optional double value (None if query fails)

Raises:
    RuntimeError: On serious database errors
)")
        .def("get_text_value", &SqliteDB::getTextValue, py::arg("query"),
             R"(Retrieve a text value.

Args:
    query: SQL query string that should return a single text value

Returns:
    Optional text value (None if query fails)

Raises:
    RuntimeError: On serious database errors
)")
        .def("search_data", &SqliteDB::searchData, py::arg("query"),
             py::arg("search_term"),
             R"(Search for a specific item in the query results.

Args:
    query: SQL query string
    search_term: Term to search for

Returns:
    Whether a matching item was found

Raises:
    RuntimeError: On serious database errors
)")
        .def("update_data", &SqliteDB::updateData, py::arg("query"),
             R"(Update data in the database.

Args:
    query: SQL update statement

Returns:
    Number of rows affected by the update

Raises:
    RuntimeError: On update errors
)")
        .def("delete_data", &SqliteDB::deleteData, py::arg("query"),
             R"(Delete data from the database.

Args:
    query: SQL delete statement

Returns:
    Number of rows affected by the delete operation

Raises:
    RuntimeError: On delete errors
)")
        .def("begin_transaction", &SqliteDB::beginTransaction,
             R"(Begin a database transaction.

Raises:
    RuntimeError: If transaction cannot be started
)")
        .def("commit_transaction", &SqliteDB::commitTransaction,
             R"(Commit a database transaction.

Raises:
    RuntimeError: If transaction cannot be committed
)")
        .def("rollback_transaction", &SqliteDB::rollbackTransaction,
             R"(Rollback a database transaction.

Raises:
    RuntimeError: If transaction cannot be rolled back
)")
        .def("with_transaction", &SqliteDB::withTransaction,
             py::arg("operations"),
             R"(Execute operations within a transaction.

Args:
    operations: Function containing database operations to execute

Raises:
    Re-throws any exceptions from operations after rollback

Examples:
    >>> def update_operation():
    ...     db.execute_query("UPDATE test SET name = 'updated' WHERE id = 1")
    ...
    >>> db.with_transaction(update_operation)
)")
        .def("validate_data", &SqliteDB::validateData, py::arg("query"),
             py::arg("validation_query"),
             R"(Validate data against a specified query condition.

Args:
    query: SQL query string
    validation_query: Validation condition query string

Returns:
    Validation result

Raises:
    RuntimeError: On validation error
)")
        .def("select_data_with_pagination", &SqliteDB::selectDataWithPagination,
             py::arg("query"), py::arg("limit"), py::arg("offset"),
             R"(Perform paginated data query and retrieval.

Args:
    query: SQL query string
    limit: Number of records per page
    offset: Offset for pagination

Returns:
    Result set containing the paginated rows

Raises:
    RuntimeError: On query error or invalid parameters

Examples:
    >>> # Get records 11-20
    >>> page_results = db.select_data_with_pagination("SELECT * FROM users ORDER BY id", 10, 10)
)")
        .def("set_error_message_callback", &SqliteDB::setErrorMessageCallback,
             py::arg("error_callback"),
             R"(Set an error message callback function.

Args:
    error_callback: Error message callback function

Examples:
    >>> def error_handler(message):
    ...     print(f"SQLite Error: {message}")
    ...
    >>> db.set_error_message_callback(error_handler)
)")
        .def("is_connected", &SqliteDB::isConnected,
             R"(Check if the database connection is valid.

Returns:
    True if connected, False otherwise
)")
        .def("get_last_insert_row_id", &SqliteDB::getLastInsertRowId,
             R"(Get the last insert rowid.

Returns:
    The rowid of the last inserted row
)")
        .def("get_changes", &SqliteDB::getChanges,
             R"(Get the number of rows modified by the last query.

Returns:
    The number of rows modified
)")
        .def("get_total_changes", &SqliteDB::getTotalChanges,
             R"(Get the total number of rows modified since database opened.

Returns:
    The total number of rows modified since the database was opened

Raises:
    RuntimeError: If not connected to database
)")
        .def("table_exists", &SqliteDB::tableExists, py::arg("table_name"),
             R"(Check if a table exists in the database.

Args:
    table_name: Name of the table to check

Returns:
    True if the table exists, False otherwise

Examples:
    >>> if db.table_exists("users"):
    ...     print("Users table exists")
)")
        .def("get_table_schema", &SqliteDB::getTableSchema, py::arg("table_name"),
             R"(Get the schema information for a table.

Args:
    table_name: Name of the table to get schema for

Returns:
    Result set containing column information (name, type, notnull, dflt_value, pk)

Raises:
    RuntimeError: If table doesn't exist or database error occurs

Examples:
    >>> schema = db.get_table_schema("users")
    >>> for row in schema:
    ...     print(f"Column: {row[1]}, Type: {row[2]}")
)")
        .def("vacuum", &SqliteDB::vacuum,
             R"(Execute VACUUM command to optimize database.

The VACUUM command rebuilds the database file, repacking it into a minimal amount of disk space.
This can improve performance and reduce file size.

Returns:
    True if VACUUM was successful

Raises:
    RuntimeError: If VACUUM operation fails

Examples:
    >>> if db.vacuum():
    ...     print("Database optimized successfully")
)")
        .def("analyze", &SqliteDB::analyze,
             R"(Execute ANALYZE command to update query planner statistics.

The ANALYZE command gathers statistics about the content of tables and indices
to help the query planner make better decisions about query optimization.

Returns:
    True if ANALYZE was successful

Raises:
    RuntimeError: If ANALYZE operation fails

Examples:
    >>> if db.analyze():
    ...     print("Database statistics updated successfully")
)");
}
