#!/usr/bin/env python3
"""
Example usage of the atom.extra.uv module.

This script demonstrates the key features of the UV module including:
- Message bus communication
- Subprocess management
- Event loop scheduling
"""

import sys

try:
    from atom.extra import uv
except ImportError:
    print("Error: atom.extra.uv module not found.")
    print("Make sure the module is built and installed.")
    sys.exit(1)


def example_message_bus():
    """Demonstrate message bus functionality."""
    print("\n=== Message Bus Example ===\n")

    # Create message bus with custom configuration
    config = uv.BackPressureConfig()
    config.max_queue_size = 1000
    config.timeout = 500  # milliseconds
    config.drop_oldest = True

    bus = uv.MessageBus(config)

    # Track received messages
    received_messages = []

    # Subscribe to user events
    def on_user_event(payload: str):
        print(f"[USER EVENT] {payload}")
        received_messages.append(payload)

    # Subscribe to system events
    def on_system_event(payload: str):
        print(f"[SYSTEM EVENT] {payload}")
        received_messages.append(payload)

    # Create subscriptions
    user_sub = bus.subscribe("user.*", on_user_event)
    system_sub = bus.subscribe("system.*", on_system_event)

    # Publish some messages
    print("Publishing messages...")
    bus.publish("user.login", "User alice logged in", sender_id="auth_service")
    bus.publish("user.logout", "User bob logged out", sender_id="auth_service")
    bus.publish("system.startup", "System started", sender_id="core")
    bus.publish("system.shutdown", "System shutting down", sender_id="core")

    # Process messages
    print("\nProcessing messages...")
    bus.process_messages()

    # Get statistics
    stats = bus.get_stats()
    print("\nQueue Statistics:")
    print(f"  Pending messages: {stats.pending_messages}")
    print(f"  Max queue size: {stats.max_queue_size}")
    print(f"  Total handlers: {stats.total_handlers}")
    print(f"  Avg delivery time: {stats.avg_delivery_time} ms")

    print(f"\nReceived {len(received_messages)} messages")

    # Cleanup
    del user_sub
    del system_sub
    bus.shutdown()

    print("\nMessage bus example completed successfully!")


def example_subprocess():
    """Demonstrate subprocess management."""
    print("\n=== Subprocess Example ===\n")

    # Example 1: Simple process spawn
    print("Example 1: Simple echo command")
    process = uv.UvProcess()

    if sys.platform == "win32":
        success = process.spawn("cmd.exe", ["/c", "echo", "Hello from subprocess!"])
    else:
        success = process.spawn("echo", ["Hello from subprocess!"])

    if success:
        print(f"Process spawned with PID: {process.get_pid()}")
        process.wait_for_exit()
        print(f"Process exited with code: {process.get_exit_code()}")
    else:
        print("Failed to spawn process")

    # Example 2: Process with callbacks
    print("\nExample 2: Process with stdout callback")

    stdout_data = []
    exit_info = {}

    def on_stdout(data: bytes, size: int):
        decoded = data.decode("utf-8", errors="replace")
        print(f"[STDOUT] {decoded.strip()}")
        stdout_data.append(decoded)

    def on_exit(exit_code: int, signal: int):
        print(f"[EXIT] Code: {exit_code}, Signal: {signal}")
        exit_info["code"] = exit_code
        exit_info["signal"] = signal

    process2 = uv.UvProcess()

    if sys.platform == "win32":
        process2.spawn(
            "cmd.exe", ["/c", "dir"], stdout_callback=on_stdout, exit_callback=on_exit
        )
    else:
        process2.spawn("ls", ["-la"], stdout_callback=on_stdout, exit_callback=on_exit)

    # Wait for process to complete
    if process2.wait_for_exit(timeout_ms=5000):
        print("Process completed successfully")
    else:
        print("Process timed out")
        process2.kill_forcefully()

    # Example 3: Advanced options
    print("\nExample 3: Process with advanced options")

    options = uv.UvProcessOptions()

    if sys.platform == "win32":
        options.file = "cmd.exe"
        options.args = ["/c", "echo", "Advanced options test"]
    else:
        options.file = "/bin/sh"
        options.args = ["-c", "echo 'Advanced options test'"]

    options.timeout = 5000  # 5 seconds
    options.env = {"CUSTOM_VAR": "test_value"}
    options.inherit_parent_env = True

    process3 = uv.UvProcess()

    def on_timeout():
        print("[TIMEOUT] Process timed out!")

    def on_error(error_msg: str):
        print(f"[ERROR] {error_msg}")

    success = process3.spawn_with_options(
        options,
        exit_callback=on_exit,
        stdout_callback=on_stdout,
        timeout_callback=on_timeout,
        error_callback=on_error,
    )

    if success:
        print(f"Process status: {process3.get_status()}")
        process3.wait_for_exit()

    print("\nSubprocess example completed successfully!")


def example_scheduler():
    """Demonstrate scheduler functionality."""
    print("\n=== Scheduler Example ===\n")

    # Get the global scheduler
    scheduler = uv.get_scheduler()

    print("Scheduler created")
    print("Note: Scheduler is primarily used with C++ coroutines")
    print("In Python, it's mainly used for running the event loop")

    # Run the event loop once
    print("Running event loop once...")
    scheduler.run_once()

    print("\nScheduler example completed successfully!")


def example_error_handling():
    """Demonstrate error handling."""
    print("\n=== Error Handling Example ===\n")

    try:
        # Try to create a process with invalid executable
        process = uv.UvProcess()
        process.spawn("/nonexistent/executable", ["arg1", "arg2"])
    except Exception as e:
        print(f"Caught expected error: {type(e).__name__}: {e}")

    try:
        # Try to publish to a shutdown bus
        bus = uv.MessageBus()
        bus.shutdown()
        bus.publish("test.topic", "This should fail")
    except Exception as e:
        print(f"Caught expected error: {type(e).__name__}: {e}")

    print("\nError handling example completed successfully!")


def main():
    """Run all examples."""
    print("=" * 60)
    print("Atom UV Module - Python Bindings Examples")
    print("=" * 60)

    examples = [
        ("Message Bus", example_message_bus),
        ("Subprocess", example_subprocess),
        ("Scheduler", example_scheduler),
        ("Error Handling", example_error_handling),
    ]

    for name, example_func in examples:
        try:
            example_func()
        except Exception as e:
            print(f"\n[ERROR in {name}] {type(e).__name__}: {e}")
            import traceback

            traceback.print_exc()

    print("\n" + "=" * 60)
    print("All examples completed!")
    print("=" * 60)


if __name__ == "__main__":
    main()
