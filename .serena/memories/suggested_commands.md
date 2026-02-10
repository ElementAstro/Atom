# Suggested Commands for Atom Development

## Build Commands

### Quick Build (Recommended)

```bash
# Windows
scripts\build.bat --release --tests --examples

# Unix/Linux/macOS
./scripts/build.sh --release --tests --examples
```

### Using CMake Presets

```bash
# Configure
cmake --preset debug           # Debug build
cmake --preset release         # Release build
cmake --preset relwithdebinfo  # Release with debug info
cmake --preset debug-msys2     # MSYS2 MinGW64 (Windows)
cmake --preset debug-vs        # MSVC (Windows)

# Build
cmake --build --preset debug -j
```

### Manual CMake Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_ALL=ON

# Build
cmake --build build -j

# Install
cmake --install build
```

### Module Selection

```bash
# Build specific modules
cmake -DBUILD_ALL=OFF -DATOM_BUILD_ALGORITHM=ON -DATOM_BUILD_IMAGE=ON

# Auto-resolve dependencies
cmake -DATOM_AUTO_RESOLVE_DEPS=ON
```

## Testing Commands

### Run All Tests

```bash
# Build and run
cd build
ctest --output-on-failure

# Run with verbose output
ctest -V

# Run specific test module
ctest -R "algorithm_*" --output-on-failure
```

### Test Categories

```bash
# Unit tests
ctest -R "^test_"

# Performance tests
ctest -R "perf_"

# Specific module tests
ctest -R "image_*"
ctest -R "async_*"
```

## Code Quality Commands

### Formatting

```bash
# Format C++ code
clang-format -i path/to/file.cpp

# Format all files (use with caution)
find . -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Format Python code
black python/
isort python/
```

### Linting

```bash
# Run pre-commit hooks
pre-commit run --all-files

# Install pre-commit hooks
pre-commit install

# Run specific hooks
pre-commit run clang-format --all-files
pre-commit run black --all-files
pre-commit run ruff --all-files
```

### Static Analysis (if available)

```bash
# Cppcheck (if installed)
cppcheck --enable=all --std=c++20 atom/

# Clang-tidy (if configured)
cmake --build build --target tidy
```

## Git Commands

### Common Git Operations

```bash
# Windows (PowerShell/CMD)
git status
git add .
git commit -m "message"
git push
git pull

# View git log
git log --oneline -10

# View changes
git diff
git diff --staged
```

### Branch Management

```bash
# List branches
git branch -a

# Create and switch branch
git checkout -b feature/new-feature

# Merge branch
git merge feature/new-feature
```

## File System Commands (Windows)

### Directory Operations

```bash
# List directory
dir              # Windows CMD
ls               # PowerShell
Get-ChildItem    # PowerShell (detailed)

# Change directory
cd path\to\directory
Set-Location path  # PowerShell

# Create directory
mkdir directory_name
New-Item -ItemType Directory directory_name  # PowerShell
```

### File Operations

```bash
# View file content
type file.txt          # Windows CMD
Get-Content file.txt   # PowerShell
cat file.txt           # Git Bash/MSYS2

# Search in files
findstr "pattern" file.txt           # Windows CMD
Select-String "pattern" file.txt     # PowerShell
grep "pattern" file.txt              # Git Bash/MSYS2
```

### System Commands

```bash
# Check processes
tasklist
Get-Process    # PowerShell

# Kill process
taskkill /PID 1234
Stop-Process -Id 1234    # PowerShell
```

## Python Commands

### Virtual Environment

```bash
# Create virtual environment
python -m venv .venv
py -m venv .venv       # Windows (py launcher)

# Activate
.venv\Scripts\activate     # Windows CMD/PowerShell
source .venv/bin/activate  # Git Bash/MSYS2

# Install dependencies
pip install -e .
```

### Python Testing

```bash
# Run pytest
pytest tests/

# Run with coverage
pytest --cov=atom tests/
```

## Documentation Commands

### Generate Documentation

```bash
# Generate Doxygen docs (if configured)
cd build
make doxygen
doxygen Doxyfile

# View documentation
# Open build/docs/html/index.html
```

## CMake-Specific Commands

### Configure Options

```bash
# Enable optional features
cmake -DATOM_USE_OPENCV=ON
cmake -DATOM_USE_CFITSIO=ON
cmake -DATOM_USE_BOOST=ON
cmake -DATOM_BUILD_PYTHON_BINDINGS=ON

# Set build type
cmake -DCMAKE_BUILD_TYPE=Debug
cmake -DCMAKE_BUILD_TYPE=Release
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### Clean Build

```bash
# Remove build directory
rm -rf build           # Git Bash/MSYS2
Remove-Item -Recurse -Force build  # PowerShell

# Reconfigure from scratch
cmake --preset debug --fresh
```
