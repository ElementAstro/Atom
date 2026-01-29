"""
Atom Error Module - Comprehensive Error Handling System

This module provides a complete error handling infrastructure including:
- Error codes and classifications
- Error context with detailed debugging information
- Multiple error formatters (Plain, JSON, HTML, Colored, etc.)
- Thread-safe error reporting and aggregation
- Error recovery strategies (retry policies, circuit breakers, fallbacks)
- Rich exception hierarchy
- Stack trace capture and formatting

Examples:
    Basic error handling:
    >>> from atom.error import ErrorContext, FileError
    >>> context = ErrorContext.create(int(FileError.NotFound), "Config file missing")
    >>> context.add_tag("critical")
    >>> print(context.to_string())

    Error formatting:
    >>> from atom.error import JsonFormatter
    >>> formatter = JsonFormatter()
    >>> json_output = formatter.format(context)

    Error recovery:
    >>> from atom.error import RecoveryStrategyFactory
    >>> from datetime import timedelta
    >>> retry_policy = RecoveryStrategyFactory.create_exponential_backoff(5, timedelta(milliseconds=100))

    Stack traces:
    >>> from atom.error import StackTrace
    >>> trace = StackTrace()
    >>> print(trace.to_string())
"""

# Import submodule packages
from . import context, core, exception, handler, stacktrace
from .context.error_context import *  # noqa: F401, F403

# Import all submodules from new structure
from .core.error_code import *  # noqa: F401, F403
from .exception.exception import *  # noqa: F401, F403
from .handler.error_formatter import *  # noqa: F401, F403
from .handler.error_handler import *  # noqa: F401, F403
from .handler.error_recovery import *  # noqa: F401, F403
from .stacktrace.stacktrace import *  # noqa: F401, F403

__all__ = [
    # Error codes and enums
    "ErrorSeverity",
    "ErrorCategory",
    "ErrorRecoveryStrategy",
    "ErrorCodeBase",
    "FileError",
    "DeviceError",
    "NetworkError",
    "DatabaseError",
    "MemoryError",
    "UserInputError",
    "ConfigError",
    "ProcessError",
    "ServerError",
    "ErrorMetadata",
    "ErrorCodeMapper",
    "severity_to_string",
    "category_to_string",
    "recovery_strategy_to_string",
    # Error context
    "ErrorContext",
    "ErrorContextManager",
    "ScopedErrorContext",
    "generate_error_id",
    # Error formatters
    "OutputFormat",
    "Color",
    "ErrorFormatter",
    "PlainTextFormatter",
    "JsonFormatter",
    "ColoredFormatter",
    "HtmlFormatter",
    "StructuredFormatter",
    "TemplateFormatter",
    "ErrorLocalizer",
    "ErrorFormatterFactory",
    "ErrorDisplayManager",
    # Error handlers
    "AggregationStrategy",
    "ErrorReporter",
    "ErrorAggregator",
    "GlobalErrorHandler",
    "ThreadLocalErrorHandler",
    # Error recovery
    "CircuitBreakerState",
    "RetryPolicy",
    "FixedIntervalRetryPolicy",
    "ExponentialBackoffRetryPolicy",
    "JitteredRetryPolicy",
    "CircuitBreaker",
    "Bulkhead",
    "RecoveryStrategyFactory",
    # Exceptions
    "Exception",
    "SystemErrorException",
    "RuntimeError",
    "LogicError",
    "UnlawfulOperation",
    "OutOfRange",
    "OverflowException",
    "UnderflowException",
    "LengthException",
    "Unknown",
    "ObjectAlreadyExist",
    "ObjectAlreadyInitialized",
    "ObjectNotExist",
    "ObjectUninitialized",
    "SystemCollapse",
    "NullPointer",
    "NotFound",
    "WrongArgument",
    "InvalidArgument",
    "MissingArgument",
    "FileNotFound",
    "FileNotReadable",
    "FileNotWritable",
    "FailToOpenFile",
    "FailToCloseFile",
    "FailToCreateFile",
    "FailToDeleteFile",
    "FailToCopyFile",
    "FailToMoveFile",
    "FailToReadFile",
    "FailToWriteFile",
    "FailToLoadDll",
    "FailToUnloadDll",
    "FailToLoadSymbol",
    "FailToCreateProcess",
    "FailToTerminateProcess",
    "JsonParseError",
    "JsonValueError",
    "CurlInitializationError",
    "CurlRuntimeError",
    # Stack trace
    "StackTrace",
    "StackTraceConfig",
    "StackFrame",
    "demangle",
    "prettify",
    "format_address",
    "get_base_name",
    "contains_mangled_names",
    "current",
    "capture_stack_trace",
    "print_stack_trace",
    "format_exception_with_traceback",
    "trace_decorator",
]

__version__ = "1.0.0"
__author__ = "Max Qian"
