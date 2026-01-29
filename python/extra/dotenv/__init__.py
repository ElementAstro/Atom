"""
Dotenv module for loading environment variables from .env files.

This module provides a modern C++ interface for loading, parsing, validating,
and applying environment variables from .env files. It supports advanced
features such as schema validation, file watching, and custom logging.

Examples:
    Basic usage:

    >>> from atom.extra import dotenv
    >>> result = dotenv.Dotenv.quick_load(".env")
    >>> if result.success:
    ...     print(f"Loaded {len(result.variables)} variables")

    With validation:

    >>> from atom.extra import dotenv
    >>> schema = dotenv.ValidationSchema()
    >>> schema.required('DATABASE_URL')
    >>> schema.optional('PORT', '3000')
    >>> schema.rule('PORT', dotenv.rules.integer())
    >>>
    >>> loader = dotenv.Dotenv()
    >>> result = loader.load_and_validate('.env', schema)
    >>> if result.success:
    ...     loader.apply_to_environment(result.variables)
"""

try:
    from .dotenv import (  # noqa: I001
        Dotenv,
        DotenvException,
        DotenvOptions,
        EnvEntry,
        FileException,
        FileLoader,
        LoadOptions,
        LoadResult,
        ParseException,
        ParseOptions,
        Parser,
        ValidationException,
        ValidationResult,
        ValidationRule,
        ValidationSchema,
        Validator,
        rules,
    )
except ImportError as e:
    import warnings

    warnings.warn(f"Failed to import atom.extra.dotenv C++ module: {e}", stacklevel=2)

__all__ = [
    "Dotenv",
    "DotenvOptions",
    "LoadOptions",
    "ParseOptions",
    "LoadResult",
    "Parser",
    "EnvEntry",
    "FileLoader",
    "Validator",
    "ValidationSchema",
    "ValidationRule",
    "ValidationResult",
    "rules",
    "DotenvException",
    "FileException",
    "ParseException",
    "ValidationException",
]
