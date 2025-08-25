# Makefile for Atom Connection Examples
# Provides convenient targets for building and testing connection examples

# Configuration
BUILD_DIR := build
CMAKE_BUILD_TYPE := Debug
PARALLEL_JOBS := $(shell nproc 2>/dev/null || echo 4)

# Colors for output (if supported)
GREEN := \033[0;32m
YELLOW := \033[1;33m
RED := \033[0;31m
BLUE := \033[0;34m
BOLD := \033[1m
NC := \033[0m

# Default target
.PHONY: all
all: build

# Help target
.PHONY: help
help:
	@echo "$(BOLD)Atom Connection Examples - Available Targets:$(NC)"
	@echo ""
	@echo "$(GREEN)Building:$(NC)"
	@echo "  all, build          - Build all connection examples"
	@echo "  configure           - Configure build environment"
	@echo "  clean               - Clean build directory"
	@echo "  rebuild             - Clean and rebuild all examples"
	@echo ""
	@echo "$(GREEN)Individual Examples:$(NC)"
	@echo "  tcp                 - Build TCP examples (tcpclient, sockethub, async variants)"
	@echo "  udp                 - Build UDP examples (udpclient, udpserver, async variants)"
	@echo "  fifo                - Build FIFO examples (fifoclient, fifoserver, async variants)"
	@echo "  ssh                 - Build SSH examples (sshclient, sshserver)"
	@echo "  tty                 - Build TTY examples (ttybase)"
	@echo ""
	@echo "$(GREEN)Testing:$(NC)"
	@echo "  test                - Run all tests (build + execution)"
	@echo "  test-build          - Test building only"
	@echo "  test-tcp            - Test TCP examples"
	@echo "  test-udp            - Test UDP examples"
	@echo ""
	@echo "$(GREEN)Utilities:$(NC)"
	@echo "  list                - List all available examples"
	@echo "  info                - Show build information"
	@echo "  deps                - Check dependencies"
	@echo ""
	@echo "$(YELLOW)Examples:$(NC)"
	@echo "  make build          # Build all examples"
	@echo "  make test-build     # Test compilation only"
	@echo "  make tcp            # Build only TCP examples"
	@echo "  make test-tcp       # Test TCP examples"

# Configure build environment
.PHONY: configure
configure:
	@echo "$(BLUE)Configuring build environment...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake \
		-DATOM_EXAMPLE_CONNECTION_BUILD_ALL=ON \
		-DATOM_EXAMPLE_CONNECTION_VERBOSE=ON \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		../..
	@echo "$(GREEN)Build environment configured successfully$(NC)"

# Build all examples
.PHONY: build
build: configure
	@echo "$(BLUE)Building all connection examples...$(NC)"
	@cd $(BUILD_DIR) && cmake --build . --target connection_examples_all --parallel $(PARALLEL_JOBS)
	@echo "$(GREEN)All examples built successfully$(NC)"

# Clean build directory
.PHONY: clean
clean:
	@echo "$(YELLOW)Cleaning build directory...$(NC)"
	@rm -rf $(BUILD_DIR)
	@echo "$(GREEN)Build directory cleaned$(NC)"

# Rebuild everything
.PHONY: rebuild
rebuild: clean build

# Build TCP examples
.PHONY: tcp
tcp: configure
	@echo "$(BLUE)Building TCP examples...$(NC)"
	@cd $(BUILD_DIR) && cmake --build . --target connection_tcpclient --parallel $(PARALLEL_JOBS)
	@cd $(BUILD_DIR) && cmake --build . --target connection_async_tcpclient --parallel $(PARALLEL_JOBS)
	@cd $(BUILD_DIR) && cmake --build . --target connection_sockethub --parallel $(PARALLEL_JOBS)
	@cd $(BUILD_DIR) && cmake --build . --target connection_async_sockethub --parallel $(PARALLEL_JOBS)
	@echo "$(GREEN)TCP examples built successfully$(NC)"

# Build UDP examples
.PHONY: udp
udp: configure
	@echo "$(BLUE)Building UDP examples...$(NC)"
	@cd $(BUILD_DIR) && cmake --build . --target connection_udpclient --parallel $(PARALLEL_JOBS)
	@cd $(BUILD_DIR) && cmake --build . --target connection_udpserver --parallel $(PARALLEL_JOBS)
	@cd $(BUILD_DIR) && cmake --build . --target connection_async_udpclient --parallel $(PARALLEL_JOBS)
	@cd $(BUILD_DIR) && cmake --build . --target connection_async_udpserver --parallel $(PARALLEL_JOBS)
	@echo "$(GREEN)UDP examples built successfully$(NC)"

# Build FIFO examples
.PHONY: fifo
fifo: configure
	@echo "$(BLUE)Building FIFO examples...$(NC)"
	@cd $(BUILD_DIR) && cmake --build . --target connection_fifoclient --parallel $(PARALLEL_JOBS)
	@cd $(BUILD_DIR) && cmake --build . --target connection_fifoserver --parallel $(PARALLEL_JOBS)
	@cd $(BUILD_DIR) && cmake --build . --target connection_async_fifoclient --parallel $(PARALLEL_JOBS)
	@cd $(BUILD_DIR) && cmake --build . --target connection_async_fifoserver --parallel $(PARALLEL_JOBS)
	@echo "$(GREEN)FIFO examples built successfully$(NC)"

# Build SSH examples
.PHONY: ssh
ssh: configure
	@echo "$(BLUE)Building SSH examples...$(NC)"
	@cd $(BUILD_DIR) && cmake --build . --target connection_sshclient --parallel $(PARALLEL_JOBS)
	@cd $(BUILD_DIR) && cmake --build . --target connection_sshserver --parallel $(PARALLEL_JOBS)
	@echo "$(GREEN)SSH examples built successfully$(NC)"

# Build TTY examples
.PHONY: tty
tty: configure
	@echo "$(BLUE)Building TTY examples...$(NC)"
	@cd $(BUILD_DIR) && cmake --build . --target connection_ttybase --parallel $(PARALLEL_JOBS)
	@echo "$(GREEN)TTY examples built successfully$(NC)"

# Test all examples
.PHONY: test
test: build
	@echo "$(BLUE)Running comprehensive tests...$(NC)"
	@if command -v python3 >/dev/null 2>&1; then \
		python3 test_examples.py; \
	elif command -v python >/dev/null 2>&1; then \
		python test_examples.py; \
	else \
		echo "$(YELLOW)Python not found, using shell script...$(NC)"; \
		bash test_examples.sh; \
	fi

# Test building only
.PHONY: test-build
test-build: build
	@echo "$(BLUE)Testing build only...$(NC)"
	@if command -v python3 >/dev/null 2>&1; then \
		python3 test_examples.py --build-only; \
	elif command -v python >/dev/null 2>&1; then \
		python test_examples.py --build-only; \
	else \
		echo "$(YELLOW)Python not found, using shell script...$(NC)"; \
		bash test_examples.sh --build-only; \
	fi

# Test TCP examples
.PHONY: test-tcp
test-tcp: tcp
	@echo "$(BLUE)Testing TCP examples...$(NC)"
	@if command -v python3 >/dev/null 2>&1; then \
		python3 test_examples.py --example tcpclient; \
		python3 test_examples.py --example async_tcpclient; \
		python3 test_examples.py --example sockethub; \
		python3 test_examples.py --example async_sockethub; \
	else \
		echo "$(YELLOW)Python not found, using shell script...$(NC)"; \
		bash test_examples.sh --example tcpclient; \
		bash test_examples.sh --example async_tcpclient; \
		bash test_examples.sh --example sockethub; \
		bash test_examples.sh --example async_sockethub; \
	fi

# Test UDP examples
.PHONY: test-udp
test-udp: udp
	@echo "$(BLUE)Testing UDP examples...$(NC)"
	@if command -v python3 >/dev/null 2>&1; then \
		python3 test_examples.py --example udpclient; \
		python3 test_examples.py --example udpserver; \
		python3 test_examples.py --example async_udpclient; \
		python3 test_examples.py --example async_udpserver; \
	else \
		echo "$(YELLOW)Python not found, using shell script...$(NC)"; \
		bash test_examples.sh --example udpclient; \
		bash test_examples.sh --example udpserver; \
		bash test_examples.sh --example async_udpclient; \
		bash test_examples.sh --example async_udpserver; \
	fi

# List all available examples
.PHONY: list
list:
	@echo "$(BOLD)Available Connection Examples:$(NC)"
	@echo ""
	@echo "$(GREEN)TCP Examples:$(NC)"
	@echo "  connection_tcpclient        - Enhanced TCP client with advanced features"
	@echo "  connection_async_tcpclient  - Asynchronous TCP client with futures/promises"
	@echo "  connection_sockethub        - Enhanced TCP server with client management"
	@echo "  connection_async_sockethub  - Asynchronous TCP server with advanced patterns"
	@echo ""
	@echo "$(GREEN)UDP Examples:$(NC)"
	@echo "  connection_udpclient        - Enhanced UDP client with multicast/broadcast"
	@echo "  connection_udpserver        - Enhanced UDP server with message routing"
	@echo "  connection_async_udpclient  - Asynchronous UDP client with performance testing"
	@echo "  connection_async_udpserver  - Asynchronous UDP server with load balancing"
	@echo ""
	@echo "$(GREEN)FIFO Examples:$(NC)"
	@echo "  connection_fifoclient       - FIFO client for inter-process communication"
	@echo "  connection_fifoserver       - FIFO server implementation"
	@echo "  connection_async_fifoclient - Asynchronous FIFO client"
	@echo "  connection_async_fifoserver - Asynchronous FIFO server"
	@echo ""
	@echo "$(GREEN)SSH Examples:$(NC)"
	@echo "  connection_sshclient        - SSH client with authentication methods"
	@echo "  connection_sshserver        - SSH server implementation"
	@echo ""
	@echo "$(GREEN)TTY Examples:$(NC)"
	@echo "  connection_ttybase          - Serial communication patterns"

# Show build information
.PHONY: info
info:
	@echo "$(BOLD)Build Information:$(NC)"
	@echo "Build Directory: $(BUILD_DIR)"
	@echo "Build Type: $(CMAKE_BUILD_TYPE)"
	@echo "Parallel Jobs: $(PARALLEL_JOBS)"
	@echo ""
	@echo "$(BOLD)System Information:$(NC)"
	@echo "OS: $$(uname -s 2>/dev/null || echo 'Windows')"
	@echo "Architecture: $$(uname -m 2>/dev/null || echo 'Unknown')"
	@if command -v cmake >/dev/null 2>&1; then \
		echo "CMake: $$(cmake --version | head -n1)"; \
	else \
		echo "$(RED)CMake: Not found$(NC)"; \
	fi
	@if command -v gcc >/dev/null 2>&1; then \
		echo "GCC: $$(gcc --version | head -n1)"; \
	elif command -v clang >/dev/null 2>&1; then \
		echo "Clang: $$(clang --version | head -n1)"; \
	elif command -v cl >/dev/null 2>&1; then \
		echo "MSVC: Available"; \
	else \
		echo "$(RED)Compiler: Not found$(NC)"; \
	fi

# Check dependencies
.PHONY: deps
deps:
	@echo "$(BOLD)Checking Dependencies:$(NC)"
	@echo ""
	@printf "CMake: "
	@if command -v cmake >/dev/null 2>&1; then \
		echo "$(GREEN)✓ Found$(NC)"; \
	else \
		echo "$(RED)✗ Not found$(NC)"; \
	fi
	@printf "C++ Compiler: "
	@if command -v g++ >/dev/null 2>&1 || command -v clang++ >/dev/null 2>&1 || command -v cl >/dev/null 2>&1; then \
		echo "$(GREEN)✓ Found$(NC)"; \
	else \
		echo "$(RED)✗ Not found$(NC)"; \
	fi
	@printf "Make: "
	@if command -v make >/dev/null 2>&1; then \
		echo "$(GREEN)✓ Found$(NC)"; \
	else \
		echo "$(RED)✗ Not found$(NC)"; \
	fi
	@printf "Python: "
	@if command -v python3 >/dev/null 2>&1 || command -v python >/dev/null 2>&1; then \
		echo "$(GREEN)✓ Found$(NC)"; \
	else \
		echo "$(YELLOW)⚠ Not found (optional)$(NC)"; \
	fi
	@printf "SSH: "
	@if command -v ssh >/dev/null 2>&1; then \
		echo "$(GREEN)✓ Found$(NC)"; \
	else \
		echo "$(YELLOW)⚠ Not found (required for SSH examples)$(NC)"; \
	fi

# Quick test target for CI/CD
.PHONY: ci
ci: test-build
	@echo "$(GREEN)CI tests completed successfully$(NC)"

# Development target - build and run a quick test
.PHONY: dev
dev: tcp
	@echo "$(BLUE)Running quick development test...$(NC)"
	@cd $(BUILD_DIR) && ./connection_sockethub &
	@sleep 2
	@pkill -f connection_sockethub || true
	@echo "$(GREEN)Development test completed$(NC)"
