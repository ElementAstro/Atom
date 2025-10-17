"""
Atom Connection Module

This module provides comprehensive networking and communication functionality
for the Atom package, including TCP/UDP clients and servers, SSH communication,
FIFO/named pipe handling, TTY/serial communication, and socket management.

Key Components:
- TCP Client/Server: Asynchronous and synchronous TCP communication
- UDP Client/Server: UDP socket management and communication
- SSH Client: Secure shell communication (conditional on libssh)
- FIFO Server: Named pipe communication
- Socket Hub: Advanced socket management with SSL and authentication
- TTY Base: Terminal and serial communication
- Utility Functions: Connection testing, configuration helpers

Quick Start Examples:

TCP Communication:
    >>> from atom.connection import TcpClient, ConnectionConfig
    >>> client = TcpClient()
    >>> if client.connect("example.com", 80):
    ...     client.send_string("GET / HTTP/1.1\\r\\nHost: example.com\\r\\n\\r\\n")
    ...     response = client.receive_string(1024)

UDP Communication:
    >>> from atom.connection import UdpSocketHub
    >>> hub = UdpSocketHub()
    >>> hub.start(8080)
    >>> hub.send_to("Hello, UDP!", "192.168.1.100", 8081)

SSH Communication:
    >>> from atom.connection import SSHClient
    >>> if SSHClient.is_libssh_available():
    ...     ssh = SSHClient()
    ...     ssh.connect("example.com", "username", "password")

FIFO Communication:
    >>> from atom.connection import FIFOServer
    >>> server = FIFOServer("/tmp/myfifo")
    >>> server.start()
    >>> server.send_message("Hello, FIFO!")

TTY/Serial Communication:
    >>> from atom.connection import TTYBase, TTYResponse
    >>> tty = TTYBase("SerialDevice")
    >>> result = tty.connect("/dev/ttyUSB0", 9600, 8, 0, 1)
    >>> if result == TTYResponse.OK:
    ...     tty.write_string("AT\\r\\n")

For detailed documentation and examples, see individual module documentation.
"""

# Import all connection modules and their key classes
try:
    # TCP Client modules (async and sync)
    from .tcpclient import (
        ConnectionConfig,
        ConnectionState,
        ConnectionStats,
        ProxyConfig,
        TcpClient,
        connection_state_to_string,
        create_ssl_config,
        create_tcp_client,
        test_connection,
    )
except ImportError as e:
    print(f"Warning: Could not import tcpclient module: {e}")

try:
    from .sync_tcpclient import (
        SyncConnectionConfig,
        SyncConnectionStats,
        SyncTcpClient,
        create_sync_tcp_client,
        test_sync_connection,
    )
except ImportError as e:
    print(f"Warning: Could not import sync_tcpclient module: {e}")

try:
    # UDP modules
    from .udp import UdpClient, UdpClientConfig, UdpClientStats, create_udp_client
except ImportError as e:
    print(f"Warning: Could not import udp module: {e}")

try:
    from .udpsockethub import UdpError, UdpSocketHub
except ImportError as e:
    print(f"Warning: Could not import udpsockethub module: {e}")

try:
    # Socket Hub module
    from .sockethub import (
        AuthenticationMethod,
        ClientInfo,
        GroupInfo,
        SocketHub,
        SocketHubConfig,
        SocketHubStats,
        create_socket_hub,
    )
except ImportError as e:
    print(f"Warning: Could not import sockethub module: {e}")

try:
    # FIFO modules
    from .fifo import AsyncFifoClient
except ImportError as e:
    print(f"Warning: Could not import fifo module: {e}")

try:
    from .fifoserver import AsyncFifoServer
except ImportError as e:
    print(f"Warning: Could not import fifoserver module: {e}")

try:
    from .sync_fifoserver import (
        FIFOServer,
        LogLevel,
        MessagePriority,
        ServerConfig,
        ServerStatistics,
    )
except ImportError as e:
    print(f"Warning: Could not import sync_fifoserver module: {e}")

try:
    # SSH module (conditional)
    from .sshclient import (
        SSHClient,
        get_default_mode,
        get_default_ssh_port,
        get_default_timeout,
        get_ssh_info,
        is_libssh_available,
    )
except ImportError as e:
    print(f"Warning: Could not import sshclient module: {e}")

try:
    # TTY module
    from .ttybase import TTYBase, TTYResponse, response_to_string
except ImportError as e:
    print(f"Warning: Could not import ttybase module: {e}")

# Define what gets exported when using "from atom.connection import *"
__all__ = [
    # TCP Client classes and functions
    'TcpClient', 'ConnectionConfig', 'ProxyConfig', 'ConnectionStats', 'ConnectionState',
    'create_tcp_client', 'test_connection', 'create_ssl_config', 'connection_state_to_string',

    # Synchronous TCP Client
    'SyncTcpClient', 'SyncConnectionConfig', 'SyncConnectionStats',
    'create_sync_tcp_client', 'test_sync_connection',

    # UDP classes
    'UdpClient', 'UdpClientConfig', 'UdpClientStats', 'create_udp_client',
    'UdpSocketHub', 'UdpError',

    # Socket Hub
    'SocketHub', 'SocketHubConfig', 'SocketHubStats', 'ClientInfo', 'GroupInfo',
    'AuthenticationMethod', 'create_socket_hub',

    # FIFO classes
    'AsyncFifoClient', 'AsyncFifoServer',
    'FIFOServer', 'ServerConfig', 'ServerStatistics', 'LogLevel', 'MessagePriority',

    # SSH classes (conditional)
    'SSHClient', 'is_libssh_available', 'get_ssh_info',
    'get_default_ssh_port', 'get_default_timeout', 'get_default_mode',

    # TTY classes
    'TTYBase', 'TTYResponse', 'response_to_string',
]
