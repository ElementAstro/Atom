#include "atom/search/mysql.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(mysql, m) {
    m.doc() = "MySQL/MariaDB database module for the atom package";

    // Register exception translations
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

    // ConnectionParams struct binding
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

    // TransactionIsolation enum
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

    // Row class binding
    py::class_<atom::database::Row>(
        m, "Row",
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

    // ResultSet class binding
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

    // PreparedStatement class binding
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

    // MysqlDB class binding
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
        .def("connect", &atom::database::MysqlDB::connect,
             R"(Connect to the database with stored parameters.

Returns:
    True if connection was successful
)")
        .def("reconnect", &atom::database::MysqlDB::reconnect,
             R"(Reconnect to the database if connection was lost.

Returns:
    True if reconnection was successful
)")
        .def("disconnect", &atom::database::MysqlDB::disconnect,
             "Disconnect from the database")
        .def("is_connected", &atom::database::MysqlDB::isConnected,
             R"(Check if database connection is alive.

Returns:
    True if connected to the database
)")
        .def("execute_query", &atom::database::MysqlDB::executeQuery,
             py::arg("query"),
             R"(Execute a SQL query.

Args:
    query: SQL query to execute

Returns:
    True if query execution was successful
)")
        .def("execute_query_with_results",
             &atom::database::MysqlDB::executeQueryWithResults,
             py::arg("query"),
             R"(Execute a query and return results.

Args:
    query: SQL query to execute

Returns:
    ResultSet object containing query results
)")
        .def("execute_update", &atom::database::MysqlDB::executeUpdate,
             py::arg("query"),
             R"(Execute a data modification query and return affected rows.

Args:
    query: SQL update/insert/delete query to execute

Returns:
    Number of affected rows
)")
        .def("get_int_value", &atom::database::MysqlDB::getIntValue,
             py::arg("query"),
             R"(Get a single integer value from a query.

Args:
    query: SQL query that returns a single integer value

Returns:
    Integer result
)")
        .def("get_double_value", &atom::database::MysqlDB::getDoubleValue,
             py::arg("query"),
             R"(Get a single double value from a query.

Args:
    query: SQL query that returns a single double value

Returns:
    Double result
)")
        .def("get_string_value", &atom::database::MysqlDB::getStringValue,
             py::arg("query"),
             R"(Get a single string value from a query.

Args:
    query: SQL query that returns a single string value

Returns:
    String result
)")
        .def("search_data", &atom::database::MysqlDB::searchData,
             py::arg("query"), py::arg("column"), py::arg("search_term"),
             R"(Search for data matching criteria.

Args:
    query: Base SQL query
    column: Column name to search in
    search_term: Term to search for

Returns:
    True if matching data found
)")
        .def("prepare_statement", &atom::database::MysqlDB::prepareStatement,
             py::arg("query"),
             R"(Create a prepared statement for safe query execution.

Args:
    query: SQL query with parameter placeholders

Returns:
    PreparedStatement object
)")
        .def("begin_transaction", &atom::database::MysqlDB::beginTransaction,
             R"(Begin a new transaction.

Returns:
    True if transaction was started successfully
)")
        .def("commit_transaction", &atom::database::MysqlDB::commitTransaction,
             R"(Commit the current transaction.

Returns:
    True if transaction was committed successfully
)")
        .def("rollback_transaction",
             &atom::database::MysqlDB::rollbackTransaction,
             R"(Rollback the current transaction.

Returns:
    True if transaction was rolled back successfully
)")
        .def("set_savepoint", &atom::database::MysqlDB::setSavepoint,
             py::arg("savepoint_name"),
             R"(Set a savepoint within the current transaction.

Args:
    savepoint_name: Name for the savepoint

Returns:
    True if savepoint was set successfully
)")
        .def("rollback_to_savepoint",
             &atom::database::MysqlDB::rollbackToSavepoint,
             py::arg("savepoint_name"),
             R"(Rollback to a specific savepoint.

Args:
    savepoint_name: Name of the savepoint to rollback to

Returns:
    True if rollback was successful
)")
        .def("set_transaction_isolation",
             &atom::database::MysqlDB::setTransactionIsolation,
             py::arg("level"),
             R"(Set transaction isolation level.

Args:
    level: TransactionIsolation enum value

Returns:
    True if isolation level was set successfully
)")
        .def("execute_batch", &atom::database::MysqlDB::executeBatch,
             py::arg("queries"),
             R"(Execute multiple queries in sequence.

Args:
    queries: List of SQL queries to execute

Returns:
    True if all queries executed successfully
)")
        .def("execute_batch_transaction",
             &atom::database::MysqlDB::executeBatchTransaction,
             py::arg("queries"),
             R"(Execute multiple queries as a single transaction.

Args:
    queries: List of SQL queries to execute

Returns:
    True if all queries executed successfully
)")
        .def("call_procedure", &atom::database::MysqlDB::callProcedure,
             py::arg("procedure_name"), py::arg("params"),
             R"(Call a stored procedure.

Args:
    procedure_name: Name of the stored procedure
    params: List of parameter values

Returns:
    ResultSet object containing procedure results
)")
        .def("get_databases", &atom::database::MysqlDB::getDatabases,
             R"(Get a list of all databases.

Returns:
    List of database names
)")
        .def("get_tables", &atom::database::MysqlDB::getTables,
             R"(Get a list of all tables in the current database.

Returns:
    List of table names
)")
        .def("get_columns", &atom::database::MysqlDB::getColumns,
             py::arg("table_name"),
             R"(Get a list of all columns in a table.

Args:
    table_name: Name of the table

Returns:
    List of column names
)")
        .def("get_last_error", &atom::database::MysqlDB::getLastError,
             R"(Get the last error message.

Returns:
    Error message string
)")
        .def("get_last_error_code", &atom::database::MysqlDB::getLastErrorCode,
             R"(Get the last error code.

Returns:
    Error code number
)")
        .def("set_error_callback", &atom::database::MysqlDB::setErrorCallback,
             py::arg("callback"),
             R"(Set a callback for error handling.

Args:
    callback: Function to call when an error occurs
)")
        .def("escape_string", &atom::database::MysqlDB::escapeString,
             py::arg("str"),
             R"(Escape a string for safe use in SQL queries.

Args:
    str: String to escape

Returns:
    Escaped string
)")
        .def("get_last_insert_id", &atom::database::MysqlDB::getLastInsertId,
             R"(Get the ID generated for the last INSERT operation.

Returns:
    Last insert ID
)")
        .def("get_affected_rows", &atom::database::MysqlDB::getAffectedRows,
             R"(Get the number of rows affected by the last query.

Returns:
    Number of affected rows
)")
        .def("execute_query_with_pagination",
             &atom::database::MysqlDB::executeQueryWithPagination,
             py::arg("query"), py::arg("limit"), py::arg("offset"),
             R"(Execute a query with pagination.

Args:
    query: SQL query to execute
    limit: Maximum number of rows to return
    offset: Number of rows to skip

Returns:
    ResultSet object with paginated results
)");
}