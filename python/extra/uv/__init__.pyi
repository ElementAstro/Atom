"""
Type stubs for the atom.extra.uv module.

This module provides libuv-based asynchronous operations including:
- Message bus for inter-component communication
- Subprocess management with callbacks
- Coroutine support for async I/O operations
- TCP/UDP networking
- File system operations
- HTTP client
"""

from typing import Callable, Dict, List, Optional, Any
from datetime import datetime
from enum import Enum
import sys

if sys.version_info >= (3, 8):
    from typing import Protocol
else:
    from typing_extensions import Protocol

# ===== UV Coro Module =====

class UvError(RuntimeError):
    """Exception class for libuv errors."""
    def __init__(self, error_code: int) -> None: ...
    def error_code(self) -> int:
        """Get the libuv error code."""
        ...

class Scheduler:
    """
    Controls the libuv event loop for coroutines.

    The Scheduler manages the libuv event loop and allows scheduling
    of coroutine tasks.
    """
    def __init__(self) -> None:
        """Create a scheduler with the default event loop."""
        ...

    def run(self) -> None:
        """Run the event loop until there are no more active handles."""
        ...

    def run_once(self) -> None:
        """Run the event loop once."""
        ...

    def stop(self) -> None:
        """Stop the event loop."""
        ...

class TcpClient:
    """
    High-level TCP client with coroutine-based interface.

    Provides asynchronous TCP client operations using coroutines.
    """
    def __init__(self, loop: Optional[Any] = None) -> None:
        """Create a TCP client with optional custom event loop."""
        ...

    def close(self) -> None:
        """Close the TCP connection."""
        ...

    def get_handle(self) -> Any:
        """Get the underlying libuv TCP handle."""
        ...

class FileSystem:
    """
    High-level file system operations with coroutine-based interface.

    Provides asynchronous file I/O operations using coroutines.
    """
    def __init__(self, loop: Optional[Any] = None) -> None:
        """Create a FileSystem instance with optional custom event loop."""
        ...

class HttpResponse:
    """HTTP response containing status, headers, and body."""
    status_code: int
    headers: Dict[str, str]
    body: str

    def __init__(self) -> None:
        """Create an empty HTTP response."""
        ...

class HttpClient:
    """
    Simple HTTP client built using TcpClient.

    Provides basic HTTP GET functionality using coroutines.
    """
    def __init__(self, loop: Optional[Any] = None) -> None:
        """Create an HTTP client with optional custom event loop."""
        ...

class ProcessOptions:
    """Options for spawning processes in coroutines."""
    file: str
    args: List[str]

    def __init__(self) -> None:
        """Create default process options."""
        ...

class ProcessResult:
    """Result of a process execution."""
    exit_code: int
    stdout_data: str
    stderr_data: str

    def __init__(self) -> None:
        """Create an empty process result."""
        ...

class UdpSendResult:
    """Result of a UDP send operation."""
    bytes_sent: int

    def __init__(self) -> None:
        """Create an empty UDP send result."""
        ...

class UdpReceiveResult:
    """Result of a UDP receive operation."""
    data: str
    from_ip: str
    from_port: int

    def __init__(self) -> None:
        """Create an empty UDP receive result."""
        ...

def get_scheduler() -> Scheduler:
    """Get the global scheduler instance."""
    ...

def make_tcp_client() -> TcpClient:
    """Create a TCP client using the global scheduler."""
    ...

def make_http_client() -> HttpClient:
    """Create an HTTP client using the global scheduler."""
    ...

def make_file_system() -> FileSystem:
    """Create a FileSystem instance using the global scheduler."""
    ...

# ===== Message Bus Module =====

class MessageBusError(Enum):
    """Error types for message bus operations."""
    InvalidTopic = ...
    HandlerNotFound = ...
    QueueFull = ...
    SerializationError = ...
    NetworkError = ...
    ShutdownInProgress = ...

class BackPressureConfig:
    """
    Configuration for message bus back-pressure handling.

    This struct defines how the message bus handles situations where
    message production exceeds consumption capacity.
    """
    max_queue_size: int
    timeout: int  # milliseconds
    drop_oldest: bool

    def __init__(self) -> None:
        """Create default back-pressure configuration."""
        ...

class MessageEnvelope:
    """
    Envelope containing a message and metadata.

    This class wraps messages with additional metadata such as topic,
    timestamp, sender ID, and custom metadata.
    """
    topic: str
    payload: str
    timestamp: datetime
    sender_id: str
    message_id: int
    metadata: Dict[str, str]

    def __init__(self, topic: str, payload: str, sender_id: str = "") -> None:
        """Create a message envelope."""
        ...

class QueueStats:
    """Statistics about message bus queue performance."""
    pending_messages: int
    max_queue_size: int
    total_handlers: int
    avg_delivery_time: int  # milliseconds

class HandlerRegistration:
    """
    Registration handle for a message handler.

    This object represents a subscription to a topic. When destroyed,
    the handler is automatically unsubscribed.
    """
    id: int
    topic_pattern: str

class MessageBus:
    """
    High-performance message bus for inter-component communication.

    This class provides a topic-based message routing system with support
    for pattern matching, filtering, and back-pressure handling.
    """
    def __init__(self, config: BackPressureConfig = ...) -> None:
        """Create a message bus with optional back-pressure configuration."""
        ...

    def subscribe(
        self,
        topic_pattern: str,
        handler: Callable[[str], None]
    ) -> HandlerRegistration:
        """
        Subscribe to messages matching a topic pattern.

        Args:
            topic_pattern: Topic pattern (supports wildcards like "user.*")
            handler: Callback function that receives message payloads

        Returns:
            Subscription handle that automatically unsubscribes when destroyed
        """
        ...

    def publish(
        self,
        topic: str,
        message: str,
        sender_id: str = ""
    ) -> None:
        """
        Publish a message to a topic.

        Args:
            topic: The topic to publish to
            message: The message payload
            sender_id: Optional sender identifier

        Raises:
            RuntimeError: If publishing fails
        """
        ...

    def get_stats(self) -> QueueStats:
        """
        Get message bus statistics.

        Returns:
            QueueStats object with performance metrics
        """
        ...

    def shutdown(self) -> None:
        """Shutdown the message bus and clean up resources."""
        ...

    def process_messages(self) -> None:
        """
        Process pending messages synchronously.

        This method processes all messages currently in the queue.
        """
        ...

    @staticmethod
    def get_instance() -> 'MessageBus':
        """
        Get the singleton message bus instance.

        Returns:
            The global MessageBus instance
        """
        ...

# ===== Subprocess Module =====

class ProcessStatus(Enum):
    """Status of a subprocess."""
    IDLE = ...
    RUNNING = ...
    EXITED = ...
    TERMINATED = ...
    TIMED_OUT = ...
    ERROR = ...


class UvProcessOptions:
    """
    Options for subprocess creation.

    This struct contains various options that control how subprocesses
    are created and managed.
    """
    file: str
    args: List[str]
    cwd: str
    env: Dict[str, str]
    detached: bool
    timeout: int  # milliseconds
    redirect_stderr_to_stdout: bool
    inherit_parent_env: bool
    stdio_count: int

    def __init__(self) -> None:
        """Create default process options."""
        ...

ExitCallback = Callable[[int, int], None]
DataCallback = Callable[[bytes, int], None]
TimeoutCallback = Callable[[], None]
ErrorCallback = Callable[[str], None]

class UvProcess:
    """
    Asynchronous subprocess management using libuv.

    This class provides a modern interface for creating and managing
    subprocesses with asynchronous I/O and callback support.
    """
    def __init__(self, loop: Optional[Any] = None) -> None:
        """
        Create a new process manager.

        Args:
            loop: Optional custom event loop (defaults to default loop)
        """
        ...

    def spawn(
        self,
        file: str,
        args: List[str],
        cwd: str = "",
        exit_callback: Optional[ExitCallback] = None,
        stdout_callback: Optional[DataCallback] = None,
        stderr_callback: Optional[DataCallback] = None
    ) -> bool:
        """
        Spawn a subprocess with basic options.

        Args:
            file: Path to the executable
            args: Command line arguments
            cwd: Working directory (optional)
            exit_callback: Callback for process exit (optional)
            stdout_callback: Callback for stdout data (optional)
            stderr_callback: Callback for stderr data (optional)

        Returns:
            True if the process was spawned successfully
        """
        ...

    def spawn_with_options(
        self,
        options: UvProcessOptions,
        exit_callback: Optional[ExitCallback] = None,
        stdout_callback: Optional[DataCallback] = None,
        stderr_callback: Optional[DataCallback] = None,
        timeout_callback: Optional[TimeoutCallback] = None,
        error_callback: Optional[ErrorCallback] = None
    ) -> bool:
        """
        Spawn a subprocess with advanced options.

        Args:
            options: ProcessOptions object with configuration
            exit_callback: Callback for process exit (optional)
            stdout_callback: Callback for stdout data (optional)
            stderr_callback: Callback for stderr data (optional)
            timeout_callback: Callback for timeout (optional)
            error_callback: Callback for errors (optional)

        Returns:
            True if the process was spawned successfully
        """
        ...

    def write_to_stdin(self, data: str) -> bool:
        """
        Write data to the process stdin.

        Args:
            data: Data to write

        Returns:
            True if the data was written successfully
        """
        ...

    def close_stdin(self) -> None:
        """Close the process stdin pipe."""
        ...

    def kill(self, signum: int = ...) -> bool:
        """
        Send a signal to the process.

        Args:
            signum: Signal number (default: SIGTERM)

        Returns:
            True if the signal was sent successfully
        """
        ...

    def kill_forcefully(self) -> bool:
        """
        Kill the process with SIGKILL.

        Returns:
            True if the process was killed successfully
        """
        ...

    def is_running(self) -> bool:
        """
        Check if the process is currently running.

        Returns:
            True if the process is running
        """
        ...

    def get_pid(self) -> int:
        """
        Get the process ID.

        Returns:
            Process ID, or -1 if not running
        """
        ...

    def get_status(self) -> ProcessStatus:
        """
        Get the current process status.

        Returns:
            ProcessStatus enum value
        """
        ...

    def get_exit_code(self) -> int:
        """
        Get the process exit code.

        Returns:
            Exit code, or -1 if process hasn't exited
        """
        ...

    def wait_for_exit(self, timeout_ms: int = 0) -> bool:
        """
        Wait for the process to exit.

        Args:
            timeout_ms: Timeout in milliseconds (0 = wait forever)

        Returns:
            True if the process exited, false on timeout
        """
        ...

    def reset(self) -> None:
        """Reset the process object to allow reuse."""
        ...

    def set_error_callback(self, error_callback: ErrorCallback) -> None:
        """
        Set custom error handler.

        Args:
            error_callback: Callback function for errors
        """
        ...

__all__ = [
    # UV Coro
    "UvError",
    "Scheduler",
    "TcpClient",
    "FileSystem",
    "HttpResponse",
    "HttpClient",
    "ProcessOptions",
    "ProcessResult",
    "UdpSendResult",
    "UdpReceiveResult",
    "get_scheduler",
    "make_tcp_client",
    "make_http_client",
    "make_file_system",
    # Message Bus
    "MessageBusError",
    "BackPressureConfig",
    "MessageEnvelope",
    "QueueStats",
    "HandlerRegistration",
    "MessageBus",
    # Subprocess
    "ProcessStatus",
    "UvProcessOptions",
    "UvProcess",
    "ExitCallback",
    "DataCallback",
    "TimeoutCallback",
    "ErrorCallback",
]
