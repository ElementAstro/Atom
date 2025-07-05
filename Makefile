# Makefile for Atom project
# Provides a unified interface for different build systems
# Author: Max Qian

.PHONY: all build clean test install docs help validate
.DEFAULT_GOAL := help

# Configuration
BUILD_TYPE ?= Release
BUILD_SYSTEM ?= cmake
PARALLEL_JOBS ?= $(shell nproc 2>/dev/null || echo 4)
BUILD_DIR ?= build
INSTALL_PREFIX ?= /usr/local

# Feature flags
WITH_PYTHON ?= OFF
WITH_TESTS ?= ON
WITH_EXAMPLES ?= ON
WITH_DOCS ?= OFF

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
NC := \033[0m

## Display this help message
help:
	@echo "$(BLUE)Atom Project Build System$(NC)"
	@echo "=========================="
	@echo ""
	@echo "$(GREEN)Usage:$(NC)"
	@echo "  make <target> [BUILD_TYPE=<type>] [BUILD_SYSTEM=<system>] [options...]"
	@echo ""
	@echo "$(GREEN)Main Targets:$(NC)"
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / {printf "  $(BLUE)%-15s$(NC) %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo ""
	@echo "$(GREEN)Build Types:$(NC)"
	@echo "  Debug, Release, RelWithDebInfo, MinSizeRel"
	@echo ""
	@echo "$(GREEN)Build Systems:$(NC)"
	@echo "  cmake (default), xmake"
	@echo ""
	@echo "$(GREEN)Configuration Variables:$(NC)"
	@echo "  BUILD_TYPE      Build configuration (default: Release)"
	@echo "  BUILD_SYSTEM    Build system to use (default: cmake)"
	@echo "  PARALLEL_JOBS   Number of parallel jobs (default: auto-detected)"
	@echo "  BUILD_DIR       Build directory (default: build)"
	@echo "  INSTALL_PREFIX  Installation prefix (default: /usr/local)"
	@echo "  WITH_PYTHON     Enable Python bindings (default: OFF)"
	@echo "  WITH_TESTS      Build tests (default: ON)"
	@echo "  WITH_EXAMPLES   Build examples (default: ON)"
	@echo "  WITH_DOCS       Build documentation (default: OFF)"
	@echo ""
	@echo "$(GREEN)Examples:$(NC)"
	@echo "  make build                    # Build with default settings"
	@echo "  make debug                    # Quick debug build"
	@echo "  make python                   # Build with Python bindings"
	@echo "  make BUILD_TYPE=Debug test    # Build and run tests in debug mode"
	@echo "  make BUILD_SYSTEM=xmake all  # Build everything with XMake"

## Build the project with current configuration
build: check-deps
	@echo "$(GREEN)Building Atom with $(BUILD_SYSTEM) ($(BUILD_TYPE))...$(NC)"
ifeq ($(BUILD_SYSTEM),cmake)
	@cmake -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DATOM_BUILD_PYTHON_BINDINGS=$(WITH_PYTHON) \
		-DATOM_BUILD_TESTS=$(WITH_TESTS) \
		-DATOM_BUILD_EXAMPLES=$(WITH_EXAMPLES) \
		-DATOM_BUILD_DOCS=$(WITH_DOCS) \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@cmake --build $(BUILD_DIR) --config $(BUILD_TYPE) --parallel $(PARALLEL_JOBS)
else ifeq ($(BUILD_SYSTEM),xmake)
	@xmake f -m $(shell echo $(BUILD_TYPE) | tr A-Z a-z) \
		$(if $(filter ON,$(WITH_PYTHON)),--python=y) \
		$(if $(filter ON,$(WITH_TESTS)),--tests=y) \
		$(if $(filter ON,$(WITH_EXAMPLES)),--examples=y)
	@xmake -j $(PARALLEL_JOBS)
else
	@echo "$(RED)Error: Unknown build system '$(BUILD_SYSTEM)'$(NC)"
	@exit 1
endif
	@echo "$(GREEN)Build completed successfully!$(NC)"

## Quick debug build
debug:
	@$(MAKE) build BUILD_TYPE=Debug

## Quick release build
release:
	@$(MAKE) build BUILD_TYPE=Release

## Build with Python bindings
python:
	@$(MAKE) build WITH_PYTHON=ON

## Build everything (tests, examples, docs, Python)
all:
	@$(MAKE) build WITH_PYTHON=ON WITH_TESTS=ON WITH_EXAMPLES=ON WITH_DOCS=ON

## Clean build artifacts
clean:
	@echo "$(YELLOW)Cleaning build artifacts...$(NC)"
ifeq ($(BUILD_SYSTEM),cmake)
	@rm -rf $(BUILD_DIR)
else ifeq ($(BUILD_SYSTEM),xmake)
	@xmake clean
	@xmake distclean
endif
	@rm -rf *.egg-info dist build-*
	@echo "$(GREEN)Clean completed!$(NC)"

## Run tests
test: build
	@echo "$(GREEN)Running tests...$(NC)"
ifeq ($(BUILD_SYSTEM),cmake)
	@cd $(BUILD_DIR) && ctest --output-on-failure --parallel $(PARALLEL_JOBS)
else ifeq ($(BUILD_SYSTEM),xmake)
	@xmake test
endif

## Run tests with coverage analysis
test-coverage:
	@$(MAKE) build BUILD_TYPE=Debug CMAKE_ARGS="-DCMAKE_CXX_FLAGS=--coverage"
	@$(MAKE) test
	@echo "$(GREEN)Generating coverage report...$(NC)"
	@which gcov >/dev/null && find $(BUILD_DIR) -name "*.gcno" -exec gcov {} \; || echo "$(YELLOW)gcov not found$(NC)"

## Install the project
install: build
	@echo "$(GREEN)Installing Atom to $(INSTALL_PREFIX)...$(NC)"
ifeq ($(BUILD_SYSTEM),cmake)
	@cmake --build $(BUILD_DIR) --target install
else ifeq ($(BUILD_SYSTEM),xmake)
	@xmake install -o $(INSTALL_PREFIX)
endif

## Generate documentation
docs:
	@echo "$(GREEN)Generating documentation...$(NC)"
	@which doxygen >/dev/null || (echo "$(RED)Error: doxygen not found$(NC)" && exit 1)
	@doxygen Doxyfile
	@echo "$(GREEN)Documentation generated in docs/html/$(NC)"

## Format code with clang-format
format:
	@echo "$(GREEN)Formatting source code...$(NC)"
	@find atom -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | xargs clang-format -i
	@echo "$(GREEN)Code formatting completed!$(NC)"

## Run static analysis with clang-tidy
analyze: build
	@echo "$(GREEN)Running static analysis...$(NC)"
	@which clang-tidy >/dev/null || (echo "$(YELLOW)clang-tidy not found, skipping analysis$(NC)" && exit 0)
	@run-clang-tidy -p $(BUILD_DIR) -header-filter='.*' atom/

## Validate build system configuration
validate:
	@echo "$(GREEN)Validating build system...$(NC)"
	@python3 validate-build.py

## Setup development environment
setup-dev:
	@echo "$(GREEN)Setting up development environment...$(NC)"
	@which pre-commit >/dev/null && pre-commit install || echo "$(YELLOW)pre-commit not found$(NC)"
	@which ccache >/dev/null && echo "ccache available" || echo "$(YELLOW)Consider installing ccache$(NC)"
	@$(MAKE) validate

## Create Python package
package-python: python
	@echo "$(GREEN)Creating Python package...$(NC)"
	@python3 -m pip install --upgrade build
	@python3 -m build

## Create distribution packages
package: build
	@echo "$(GREEN)Creating distribution packages...$(NC)"
ifeq ($(BUILD_SYSTEM),cmake)
	@cd $(BUILD_DIR) && cpack
endif

## Run benchmarks
benchmark: build
	@echo "$(GREEN)Running benchmarks...$(NC)"
	@find $(BUILD_DIR) -name "*benchmark*" -executable -exec {} \;

## Quick smoke test
smoke-test:
	@echo "$(GREEN)Running smoke test...$(NC)"
	@$(MAKE) build BUILD_TYPE=Debug WITH_TESTS=OFF WITH_EXAMPLES=OFF BUILD_DIR=build-smoke
	@rm -rf build-smoke
	@echo "$(GREEN)Smoke test passed!$(NC)"

# Internal targets

## Check build dependencies
check-deps:
	@echo "$(BLUE)Checking dependencies...$(NC)"
ifeq ($(BUILD_SYSTEM),cmake)
	@which cmake >/dev/null || (echo "$(RED)Error: cmake not found$(NC)" && exit 1)
else ifeq ($(BUILD_SYSTEM),xmake)
	@which xmake >/dev/null || (echo "$(RED)Error: xmake not found$(NC)" && exit 1)
endif
	@which git >/dev/null || (echo "$(RED)Error: git not found$(NC)" && exit 1)

# Auto-completion setup
## Generate shell completion scripts
completion:
	@echo "$(GREEN)Generating shell completion...$(NC)"
	@mkdir -p completion
	@echo '_make_completion() { COMPREPLY=($$(compgen -W "build debug release python all clean test install docs format analyze validate setup-dev package benchmark smoke-test help" -- $${COMP_WORDS[COMP_CWORD]})); }' > completion/atom-make-completion.bash
	@echo 'complete -F _make_completion make' >> completion/atom-make-completion.bash
	@echo "Add 'source $$(pwd)/completion/atom-make-completion.bash' to your .bashrc"

# Display configuration
config:
	@echo "$(BLUE)Current Configuration:$(NC)"
	@echo "  BUILD_TYPE:      $(BUILD_TYPE)"
	@echo "  BUILD_SYSTEM:    $(BUILD_SYSTEM)"
	@echo "  PARALLEL_JOBS:   $(PARALLEL_JOBS)"
	@echo "  BUILD_DIR:       $(BUILD_DIR)"
	@echo "  INSTALL_PREFIX:  $(INSTALL_PREFIX)"
	@echo "  WITH_PYTHON:     $(WITH_PYTHON)"
	@echo "  WITH_TESTS:      $(WITH_TESTS)"
	@echo "  WITH_EXAMPLES:   $(WITH_EXAMPLES)"
	@echo "  WITH_DOCS:       $(WITH_DOCS)"
