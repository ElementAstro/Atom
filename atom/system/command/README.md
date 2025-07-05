# Command Module Refactoring

## Overview

The original `command.hpp` and `command.cpp` files have been split into multiple focused modules within the `atom/system/command/` directory to improve code organization, maintainability, and modularity.

## New Structure

The command functionality is now organized into the following components:

### 1. Core Execution (`executor.hpp`/`executor.cpp`)

- **Purpose**: Basic command execution functionality
- **Functions**:
  - `executeCommand()` - Execute a command and return output
  - `executeCommandWithInput()` - Execute a command with input data
  - `executeCommandStream()` - Execute a command with streaming support
  - `executeCommands()` - Execute multiple commands
  - `executeCommandWithStatus()` - Execute command and return status
  - `executeCommandSimple()` - Simple boolean result execution
  - `executeCommandInternal()` - Internal implementation (shared)

### 2. Process Management (`process_manager.hpp`/`process_manager.cpp`)

- **Purpose**: Process control and management
- **Functions**:
  - `killProcessByName()` - Kill process by name
  - `killProcessByPID()` - Kill process by PID
  - `startProcess()` - Start a new process
  - `getProcessesBySubstring()` - Find processes by name substring

### 3. Advanced Execution (`advanced_executor.hpp`/`advanced_executor.cpp`)

- **Purpose**: Advanced execution features like async, timeout, environment
- **Functions**:
  - `executeCommandWithEnv()` - Execute with environment variables
  - `executeCommandAsync()` - Asynchronous execution
  - `executeCommandWithTimeout()` - Execution with timeout
  - `executeCommandsWithCommonEnv()` - Multiple commands with shared environment

### 4. Utilities (`utils.hpp`/`utils.cpp`)

- **Purpose**: Helper and utility functions
- **Functions**:
  - `isCommandAvailable()` - Check if command exists
  - `executeCommandGetLines()` - Get output as lines
  - `pipeCommands()` - Pipe two commands together

### 5. Command History (`history.hpp`/`history.cpp`)

- **Purpose**: Command history tracking
- **Classes**:
  - `CommandHistory` - Track executed commands with status
- **Functions**:
  - `createCommandHistory()` - Factory function

## Backwards Compatibility

The original `command.hpp` now serves as a convenience header that includes all the sub-modules, maintaining 100% backwards compatibility. Existing code that includes `atom/system/command.hpp` will continue to work without any changes.

## Benefits

### 1. **Modularity**

- Each component has a single responsibility
- Easier to understand and maintain individual features
- Reduced compilation dependencies

### 2. **Testability**

- Each module can be tested independently
- More focused unit tests possible
- Easier to mock dependencies

### 3. **Scalability**

- New command-related features can be added as separate modules
- Existing modules can be extended without affecting others
- Better code organization for large teams

### 4. **Performance**

- Reduced include overhead for code that only needs specific functionality
- Faster compilation times for incremental builds
- Better optimization opportunities

## Usage Examples

### Direct Module Usage (Optional)

```cpp
// Include only what you need
#include "atom/system/command/executor.hpp"
#include "atom/system/command/utils.hpp"

// Use specific functionality
auto output = atom::system::executeCommand("ls -la");
bool available = atom::system::isCommandAvailable("git");
```

### Traditional Usage (Recommended for existing code)

```cpp
// Include everything (backwards compatible)
#include "atom/system/command.hpp"

// All functions available as before
auto output = atom::system::executeCommand("ls -la");
auto history = atom::system::createCommandHistory(100);
```

## Implementation Details

### Shared Resources

- `envMutex` - Global mutex for environment variable operations (defined in `command.cpp`)
- All modules share common dependencies like spdlog, error handling

### Platform Support

- All modules maintain cross-platform support (Windows/Linux)
- Platform-specific code properly isolated within each module
- Consistent error handling across all components

### Dependencies

- Each module includes only the dependencies it needs
- Reduced circular dependencies
- Clear dependency hierarchy

## Migration Guide

### For Existing Code

**No changes required!** The original interface is preserved through the main `command.hpp` header.

### For New Code

Consider using specific module headers when you only need subset of functionality:

```cpp
// Instead of including everything
#include "atom/system/command.hpp"

// Include only what you need
#include "atom/system/command/executor.hpp"
#include "atom/system/command/history.hpp"
```

## Build System Integration

The CMakeLists.txt has been updated to include all new source files:

- `command/executor.cpp`
- `command/process_manager.cpp`
- `command/advanced_executor.cpp`
- `command/utils.cpp`
- `command/history.cpp`

## Testing

A test file has been created (`test_command_interface.cpp`) to verify that all interfaces remain functional after the refactoring.

## Future Enhancements

The modular structure makes it easy to add new features:

- New execution modes can be added as separate modules
- Plugin system for custom command processors
- Enhanced logging and monitoring capabilities
- Performance profiling modules

## Conclusion

This refactoring maintains full backwards compatibility while providing a more maintainable and scalable architecture for the command system. The modular design allows for easier testing, debugging, and future enhancements while preserving all existing functionality.
