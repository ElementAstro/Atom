"""
Atom Web Module
===============

This module provides comprehensive web and networking functionality including HTTP clients,
address management, MIME type handling, time synchronization, and network utilities.

Key Components:
- HTTP Operations: CurlWrapper for HTTP requests, DownloadManager for file downloads
- Address Management: IPv4, IPv6, and Unix domain socket address handling
- MIME Types: File type detection and MIME type management
- Time Management: System time operations, NTP synchronization, timezone handling
- Network Utilities: DNS resolution, port scanning, connectivity testing

Examples:
    >>> from atom.web import CurlWrapper, TimeManager, get_ip_addresses
    >>>
    >>> # HTTP request
    >>> curl = CurlWrapper()
    >>> curl.set_url("https://api.example.com/data")
    >>> response = curl.perform()
    >>>
    >>> # Time management
    >>> tm = TimeManager()
    >>> current_time = tm.get_system_time()
    >>>
    >>> # DNS resolution
    >>> ips = get_ip_addresses("google.com")
"""

# Import all existing modules with error handling
try:
    from .address import (
        Address,
        IPv4,
        IPv6,
        UnixDomain,
        is_valid_address,
        parse_address,
    )
except ImportError as e:
    print(f"Warning: Could not import address module: {e}")

try:
    from .downloader import DownloadManager, download_file, download_files
except ImportError as e:
    print(f"Warning: Could not import downloader module: {e}")

try:
    from .httpparser import (
        Cookie,
        HttpHeaderParser,
        HttpMethod,
        HttpStatus,
        HttpVersion,
        create_request,
        create_response,
        parse_request,
        parse_response,
        url_decode,
        url_encode,
    )
except ImportError as e:
    print(f"Warning: Could not import httpparser module: {e}")

try:
    from .mimetype import (
        MimeTypeConfig,
        MimeTypeException,
        MimeTypes,
        create_default_database,
        guess_extension,
        guess_type,
    )
except ImportError as e:
    print(f"Warning: Could not import mimetype module: {e}")

try:
    from .utils import (  # System initialization; Port utilities; DNS utilities; IP validation; Network connectivity
        check_and_kill_program_on_port,
        check_internet_connectivity,
        clear_dns_cache_expired_entries,
        get_ip_addresses,
        get_local_ip_addresses,
        get_process_id_on_port,
        initialize_windows_socket_api,
        is_port_in_use,
        is_port_in_use_async,
        is_valid_ipv4,
        is_valid_ipv6,
        set_dns_cache_ttl,
    )
except ImportError as e:
    print(f"Warning: Could not import utils module: {e}")

# Import new modules
try:
    from .curl import CurlWrapper, simple_get, simple_post
except ImportError as e:
    print(f"Warning: Could not import curl module: {e}")

try:
    from .time import (
        TimeError,
        TimeManager,
        check_ntp_server,
        get_current_timestamp,
        requires_admin,
    )
except ImportError as e:
    print(f"Warning: Could not import time module: {e}")

# Define comprehensive __all__ for proper module exposure
__all__ = [
    # Address classes and functions
    "Address",
    "IPv4",
    "IPv6",
    "UnixDomain",
    "parse_address",
    "is_valid_address",
    # HTTP classes and functions
    "CurlWrapper",
    "simple_get",
    "simple_post",
    "DownloadManager",
    "download_file",
    "download_files",
    "HttpMethod",
    "HttpVersion",
    "HttpStatus",
    "Cookie",
    "HttpHeaderParser",
    "parse_request",
    "parse_response",
    "create_request",
    "create_response",
    "url_encode",
    "url_decode",
    # MIME type classes and functions
    "MimeTypes",
    "MimeTypeConfig",
    "MimeTypeException",
    "guess_type",
    "guess_extension",
    "create_default_database",
    # Time management classes and functions
    "TimeManager",
    "TimeError",
    "get_current_timestamp",
    "check_ntp_server",
    "requires_admin",
    # Network utility functions
    "initialize_windows_socket_api",
    "is_port_in_use",
    "check_and_kill_program_on_port",
    "get_process_id_on_port",
    "is_port_in_use_async",
    "set_dns_cache_ttl",
    "get_ip_addresses",
    "get_local_ip_addresses",
    "clear_dns_cache_expired_entries",
    "is_valid_ipv4",
    "is_valid_ipv6",
    "check_internet_connectivity",
]
