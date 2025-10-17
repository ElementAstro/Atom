#include "atom/search/mysql.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

/**
 * @brief Registers exception translations for the MySQL module.
 *
 * This function sets up proper exception handling to translate C++ exceptions
 * to appropriate Python exceptions for better error reporting.
 *
 * @param m The pybind11 module to register exceptions for
 */
void registerExceptionTranslations(py::module_& m) {
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::database::MySQLException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });
}

/**
 * @brief Binds the ConnectionParams struct to Python.
 *
 * This function creates Python bindings for the ConnectionParams struct
 * which holds database connection parameters.
 *
 * @param m The pybind11 module to bind to
 */
void bindConnectionParams(py::module_& m) {
    py::class_<atom::database::ConnectionParams>(
        m, "ConnectionParams",
        R"(Structure to hold database connection parameters.

This class stores the connection parameters needed to connect to a MySQL/MariaDB database.

Examples:
    >>> from atom.search.mysql import ConnectionParams
    >>> params = ConnectionParams()
    >>> params.host = "localhost"
    >>> params.user = "root"
    >>> params.password = "password"
    >>> params.database = "mydb"
)")
        .def(py::init<>())
        .def_readwrite("host", &atom::database::ConnectionParams::host,
                       "Database server hostname or IP address")
        .def_readwrite("user", &atom::database::ConnectionParams::user,
                       "Database username")
        .def_readwrite("password", &atom::database::ConnectionParams::password,
                       "Database password")
        .def_readwrite("database", &atom::database::ConnectionParams::database,
                       "Database name")
        .def_readwrite("port", &atom::database::ConnectionParams::port,
                       "Database server port (default: 3306)")
        .def_readwrite("socket", &atom::database::ConnectionParams::socket,
                       "Unix socket path (if applicable)")
        .def_readwrite("client_flag",
                       &atom::database::ConnectionParams::clientFlag,
                       "MySQL client flags");
}

/**
 * @brief Binds the TransactionIsolation enum to Python.
 *
 * This function creates Python bindings for the TransactionIsolation enum
 * which defines database transaction isolation levels.
 *
 * @param m The pybind11 module to bind to
 */
void bindTransactionIsolation(py::module_& m) {
    py::enum_<atom::database::TransactionIsolation>(
        m, "TransactionIsolation",
        R"(Database transaction isolation levels.

Determines how transactions interact with other transactions.)")
        .value("READ_UNCOMMITTED",
               atom::database::TransactionIsolation::READ_UNCOMMITTED,
               "Lowest isolation level, allows dirty reads")
        .value("READ_COMMITTED",
               atom::database::TransactionIsolation::READ_COMMITTED,
               "Prevents dirty reads, but allows non-repeatable reads and "
               "phantom reads")
        .value("REPEATABLE_READ",
               atom::database::TransactionIsolation::REPEATABLE_READ,
               "Prevents dirty reads and non-repeatable reads, but allows "
               "phantom reads")
        .value("SERIALIZABLE",
               atom::database::TransactionIsolation::SERIALIZABLE,
               "Highest isolation level, prevents all concurrency anomalies")
        .export_values();
}

/**
 * @brief Binds the Row class to Python.
 *
 * This function creates Python bindings for the Row class which represents
 * a database result row with methods to access column values.
 *
 * @param m The pybind11 module to bind to
 */
void bindRow(py::module_& m) {
    py::class_<atom::database::Row>(m, "Row",
                                    R"(Class representing a database result row.

Provides methods to access column values in different data types.

Examples:
    >>> row = result_set.current_row
    >>> name = row.get_string(0)
    >>> age = row.get_int(1)
)")
        .def("get_string", &atom::database::Row::getString, py::arg("index"),
             "Get column value as string")
        .def("get_int", &atom::database::Row::getInt, py::arg("index"),
             "Get column value as integer")
        .def("get_double", &atom::database::Row::getDouble, py::arg("index"),
             "Get column value as double")
        .def("get_bool", &atom::database::Row::getBool, py::arg("index"),
             "Get column value as boolean")
        .def("is_null", &atom::database::Row::isNull, py::arg("index"),
             "Check if column value is NULL")
        .def("get_field_count", &atom::database::Row::getFieldCount,
             "Get number of fields in this row");
}

/**
 * @brief Binds the ResultSet class to Python.
 *
 * This function creates Python bindings for the ResultSet class which
 * represents a database query result set with navigation methods.
 *
 * @param m The pybind11 module to bind to
 */
void bindResultSet(py::module_& m) {
    py::class_<atom::database::ResultSet>(
        m, "ResultSet",
        R"(Class representing a database query result set.

Provides methods to navigate through the result rows.

Examples:
    >>> result = db.execute_query_with_results("SELECT * FROM users")
    >>> while result.next():
    ...     row = result.current_row
    ...     print(row.get_string(0))
)")
        .def("next", &atom::database::ResultSet::next,
             "Move to the next row in the result set")
        .def("current_row", &atom::database::ResultSet::getCurrentRow,
             "Get the current row")
        .def("get_field_count", &atom::database::ResultSet::getFieldCount,
             "Get number of fields in the result set")
        .def("get_field_name", &atom::database::ResultSet::getFieldName,
             py::arg("index"), "Get the name of a field")
        .def("get_row_count", &atom::database::ResultSet::getRowCount,
             "Get the total number of rows in the result set")
        .def(
            "__iter__",
            [](atom::database::ResultSet& rs) {
                return py::make_iterator(rs.begin(), rs.end());
            },
            py::keep_alive<0, 1>());
}

/**
 * @brief Binds the PreparedStatement class to Python.
 *
 * This function creates Python bindings for the PreparedStatement class which
 * provides safe execution of parameterized SQL queries.
 *
 * @param m The pybind11 module to bind to
 */
void bindPreparedStatement(py::module_& m) {
    py::class_<atom::database::PreparedStatement>(
        m, "PreparedStatement",
        R"(Class for prepared SQL statements.

Allows safe execution of parameterized SQL queries.

Examples:
    >>> stmt = db.prepare_statement("SELECT * FROM users WHERE id = ?")
    >>> stmt.bind_int(1, 42)
    >>> result = stmt.execute_query()
)")
        .def("bind_string", &atom::database::PreparedStatement::bindString,
             py::arg("index"), py::arg("value"), "Bind string parameter")
        .def("bind_int", &atom::database::PreparedStatement::bindInt,
             py::arg("index"), py::arg("value"), "Bind integer parameter")
        .def("bind_double", &atom::database::PreparedStatement::bindDouble,
             py::arg("index"), py::arg("value"), "Bind double parameter")
        .def("bind_bool", &atom::database::PreparedStatement::bindBool,
             py::arg("index"), py::arg("value"), "Bind boolean parameter")
        .def("bind_null", &atom::database::PreparedStatement::bindNull,
             py::arg("index"), "Bind NULL parameter")
        .def("execute", &atom::database::PreparedStatement::execute,
             "Execute the prepared statement")
        .def("execute_query", &atom::database::PreparedStatement::executeQuery,
             "Execute the prepared statement and return results")
        .def("execute_update",
             &atom::database::PreparedStatement::executeUpdate,
             "Execute the prepared statement and return affected row count")
        .def("reset", &atom::database::PreparedStatement::reset,
             "Reset the prepared statement")
        .def("clear_parameters",
             &atom::database::PreparedStatement::clearParameters,
             "Clear parameter bindings");
}

/**
 * @brief Binds the MysqlDB class to Python.
 *
 * This function creates Python bindings for the MysqlDB class which provides
 * comprehensive MySQL/MariaDB database interaction capabilities.
 *
 * @param m The pybind11 module to bind to
 */
void bindMysqlDB(py::module_& m) {
    py::class_<atom::database::MysqlDB>(
        m, "MysqlDB",
        R"(Enhanced class for interacting with a MySQL/MariaDB database.

Provides connection management and various query execution methods.

Args:
    host: Database server hostname or IP address
    user: Database username
    password: Database password
    database: Database name
    port: Database server port
    socket: Unix socket path
    client_flag: MySQL client flags

Examples:
    >>> from atom.search.mysql import MysqlDB
    >>> db = MysqlDB("localhost", "user", "password", "mydb")
    >>> db.connect()
    True
    >>> result = db.execute_query_with_results("SELECT * FROM users")
)")
        .def(py::init<const atom::database::ConnectionParams&>(),
             py::arg("params"), "Construct with connection parameters")
        .def(py::init<const std::string&, const std::string&,
                      const std::string&, const std::string&, unsigned int,
                      const std::string&, unsigned long>(),
             py::arg("host"), py::arg("user"), py::arg("password"),
             py::arg("database"), py::arg("port") = 3306,
             py::arg("socket") = "", py::arg("client_flag") = 0,
             "Construct with individual connection parameters")

        // Connection management methods
        .def("connect", &atom::database::MysqlDB::connect,
             "Connect to the database with stored parameters.")
        .def("reconnect", &atom::database::MysqlDB::reconnect,
             "Reconnect to the database if connection was lost.")
        .def("disconnect", &atom::database::MysqlDB::disconnect,
             "Disconnect from the database")
        .def("is_connected", &atom::database::MysqlDB::isConnected,
             "Check if database connection is alive.")

        // Query execution methods
        .def("execute_query", &atom::database::MysqlDB::executeQuery,
             py::arg("query"), "Execute a SQL query.")
        .def("execute_query_with_results",
             &atom::database::MysqlDB::executeQueryWithResults,
             py::arg("query"), "Execute a query and return results.")
        .def("execute_update", &atom::database::MysqlDB::executeUpdate,
             py::arg("query"),
             "Execute a data modification query and return affected rows.")
        .def("execute_query_with_pagination",
             &atom::database::MysqlDB::executeQueryWithPagination,
             py::arg("query"), py::arg("limit"), py::arg("offset"),
             "Execute a query with pagination.")

        // Value retrieval methods
        .def("get_int_value", &atom::database::MysqlDB::getIntValue,
             py::arg("query"), "Get a single integer value from a query.")
        .def("get_double_value", &atom::database::MysqlDB::getDoubleValue,
             py::arg("query"), "Get a single double value from a query.")
        .def("get_string_value", &atom::database::MysqlDB::getStringValue,
             py::arg("query"), "Get a single string value from a query.")
        .def("search_data", &atom::database::MysqlDB::searchData,
             py::arg("query"), py::arg("column"), py::arg("search_term"),
             "Search for data matching criteria.")

        // Prepared statement methods
        .def("prepare_statement", &atom::database::MysqlDB::prepareStatement,
             py::arg("query"),
             "Create a prepared statement for safe query execution.")

        // Transaction management methods
        .def("begin_transaction", &atom::database::MysqlDB::beginTransaction,
             "Begin a new transaction.")
        .def("commit_transaction", &atom::database::MysqlDB::commitTransaction,
             "Commit the current transaction.")
        .def("rollback_transaction",
             &atom::database::MysqlDB::rollbackTransaction,
             "Rollback the current transaction.")
        .def("set_savepoint", &atom::database::MysqlDB::setSavepoint,
             py::arg("savepoint_name"),
             "Set a savepoint within the current transaction.")
        .def("rollback_to_savepoint",
             &atom::database::MysqlDB::rollbackToSavepoint,
             py::arg("savepoint_name"), "Rollback to a specific savepoint.")
        .def("set_transaction_isolation",
             &atom::database::MysqlDB::setTransactionIsolation,
             py::arg("level"), "Set transaction isolation level.")

        // Batch operation methods
        .def("execute_batch", &atom::database::MysqlDB::executeBatch,
             py::arg("queries"), "Execute multiple queries in sequence.")
        .def("execute_batch_transaction",
             &atom::database::MysqlDB::executeBatchTransaction,
             py::arg("queries"),
             "Execute multiple queries as a single transaction.")

        // Stored procedure methods
        .def("call_procedure", &atom::database::MysqlDB::callProcedure,
             py::arg("procedure_name"), py::arg("params"),
             "Call a stored procedure.")

        // Schema information methods
        .def("get_databases", &atom::database::MysqlDB::getDatabases,
             "Get a list of all databases.")
        .def("get_tables", &atom::database::MysqlDB::getTables,
             "Get a list of all tables in the current database.")
        .def("get_columns", &atom::database::MysqlDB::getColumns,
             py::arg("table_name"), "Get a list of all columns in a table.")

        // Error handling and utility methods
        .def("get_last_error", &atom::database::MysqlDB::getLastError,
             "Get the last error message.")
        .def("get_last_error_code", &atom::database::MysqlDB::getLastErrorCode,
             "Get the last error code.")
        .def("set_error_callback", &atom::database::MysqlDB::setErrorCallback,
             py::arg("callback"), "Set a callback for error handling.")
        .def("escape_string", &atom::database::MysqlDB::escapeString,
             py::arg("str"), "Escape a string for safe use in SQL queries.")
        .def("get_last_insert_id", &atom::database::MysqlDB::getLastInsertId,
             "Get the ID generated for the last INSERT operation.")
        .def("get_affected_rows", &atom::database::MysqlDB::getAffectedRows,
             "Get the number of rows affected by the last query.");
}

/**
 * @brief Adds comprehensive module documentation and usage examples.
 *
 * This function sets the module's __doc__ attribute with detailed documentation
 * including usage examples for MySQL/MariaDB database operations.
 *
 * @param m The pybind11 module to add documentation to
 */
void addModuleDocumentation(py::module_& m) {
    m.attr("__doc__") = R"(MySQL/MariaDB database module for the atom package.

This module provides comprehensive database interaction capabilities for MySQL and MariaDB databases.

Key Features:
- Connection management with automatic reconnection
- Safe parameterized queries with prepared statements
- Transaction support with savepoints
- Batch operations for improved performance
- Schema introspection (databases, tables, columns)
- Error handling with custom callbacks
- Pagination support for large result sets
- Stored procedure execution

Examples:
    >>> from atom.search.mysql import MysqlDB, ConnectionParams
    >>>
    >>> # Create connection using parameters
    >>> params = ConnectionParams()
    >>> params.host = "localhost"
    >>> params.user = "root"
    >>> params.password = "password"
    >>> params.database = "mydb"
    >>> db = MysqlDB(params)
    >>>
    >>> # Or create connection directly
    >>> db = MysqlDB("localhost", "root", "password", "mydb")
    >>>
    >>> # Connect to database
    >>> if db.connect():
    >>>     print("Connected successfully")
    >>>
    >>> # Execute simple query
    >>> if db.execute_query("CREATE TABLE users (id INT, name VARCHAR(50))"):
    >>>     print("Table created")
    >>>
    >>> # Execute query with results
    >>> result = db.execute_query_with_results("SELECT * FROM users")
    >>> while result.next():
    >>>     row = result.current_row
    >>>     print(f"ID: {row.get_int(0)}, Name: {row.get_string(1)}")
    >>>
    >>> # Use prepared statements for safety
    >>> stmt = db.prepare_statement("INSERT INTO users (id, name) VALUES (?, ?)")
    >>> stmt.bind_int(1, 1)
    >>> stmt.bind_string(2, "John Doe")
    >>> stmt.execute()
    >>>
    >>> # Transaction example
    >>> db.begin_transaction()
    >>> try:
    >>>     db.execute_query("INSERT INTO users (id, name) VALUES (2, 'Jane')")
    >>>     db.execute_query("INSERT INTO users (id, name) VALUES (3, 'Bob')")
    >>>     db.commit_transaction()
    >>> except:
    >>>     db.rollback_transaction()
    >>>
    >>> # Get schema information
    >>> databases = db.get_databases()
    >>> tables = db.get_tables()
    >>> columns = db.get_columns("users")
    >>>
    >>> # Pagination
    >>> result = db.execute_query_with_pagination("SELECT * FROM users", 10, 0)
)";
}

PYBIND11_MODULE(mysql, m) {
    m.doc() = "MySQL/MariaDB database module for the atom package";

    // Register exception translations
    registerExceptionTranslations(m);

    // Bind core data structures
    bindConnectionParams(m);
    bindTransactionIsolation(m);
    bindRow(m);
    bindResultSet(m);
    bindPreparedStatement(m);
    bindMysqlDB(m);

    // Add module documentation
    addModuleDocumentation(m);
}
