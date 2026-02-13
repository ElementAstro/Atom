# Virtual Module Examples CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_SOURCE_DIR})

# Define example sources
set(VIRTUAL_EXAMPLE_SOURCES virtual_demo.cpp virtual_benchmark.cpp)

# Create example executables
add_executable(virtual_demo virtual_demo.cpp)
add_executable(virtual_benchmark virtual_benchmark.cpp)

# Link libraries
target_link_libraries(virtual_demo PRIVATE atom_sysinfo_virtual)

target_link_libraries(virtual_benchmark PRIVATE atom_sysinfo_virtual)

# Set C++ standard
target_compile_features(virtual_demo PUBLIC cxx_std_20)
target_compile_features(virtual_benchmark PUBLIC cxx_std_20)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(virtual_demo PRIVATE -Wall -Wextra -Wpedantic
                                              -Wno-unused-parameter)
  target_compile_options(virtual_benchmark PRIVATE -Wall -Wextra -Wpedantic
                                                   -Wno-unused-parameter)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(
    virtual_demo PRIVATE /W4 /wd4100 # unreferenced formal parameter
  )
  target_compile_options(
    virtual_benchmark PRIVATE /W4 /wd4100 # unreferenced formal parameter
  )
endif()

# Create custom target for all examples
add_custom_target(
  virtual_examples
  DEPENDS virtual_demo virtual_benchmark
  COMMENT "Building virtual module examples")

# Custom target to run the demo
add_custom_target(
  run_virtual_demo
  COMMAND virtual_demo
  DEPENDS virtual_demo
  COMMENT "Running virtual detection demo")

# Custom target to run the benchmark
add_custom_target(
  run_virtual_benchmark
  COMMAND virtual_benchmark
  DEPENDS virtual_benchmark
  COMMENT "Running virtual detection benchmark")

# Installation rules for examples
install(TARGETS virtual_demo virtual_benchmark
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)

# Install source files as documentation
install(FILES ${VIRTUAL_EXAMPLE_SOURCES}
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples/virtual)

message(STATUS "Virtual module examples configured successfully")
message(STATUS "Available example targets:")
message(STATUS "  - virtual_demo: Virtual environment detection demonstration")
message(STATUS "  - virtual_benchmark: Virtual detection performance benchmark")
message(STATUS "  - virtual_examples: Build all virtual examples")
message(STATUS "  - run_virtual_demo: Run the virtual detection demo")
message(STATUS "  - run_virtual_benchmark: Run the virtual detection benchmark")
