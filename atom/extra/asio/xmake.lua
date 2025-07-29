-- Advanced ASIO implementation with cutting-edge C++23 concurrency primitives
-- Author: Atom Framework Team
-- License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-asio-advanced")
set_version("1.0.0", {build = "%Y%m%d%H%M"})
set_license("GPL-3.0")

-- Set C++23 standard for cutting-edge features
set_languages("c++23")

-- Add build modes with advanced optimizations
add_rules("mode.debug", "mode.release", "mode.releasedbg")

-- Advanced compiler configurations
if is_mode("release") then
    set_optimize("aggressive")
    add_cxflags("-march=native", "-mtune=native", "-ffast-math", "-funroll-loops")
    add_cxflags("-fomit-frame-pointer", "-finline-functions", "-fdevirtualize-at-ltrans")
    add_cxflags("-fno-semantic-interposition", "-fipa-pta", "-floop-nest-optimize")
    add_cxflags("-ftree-vectorize", "-fvect-cost-model=dynamic")

    -- Enable LTO for maximum performance
    add_cxflags("-flto")
    add_ldflags("-flto", "-fuse-linker-plugin")

    -- MSVC specific optimizations
    if is_plat("windows") then
        add_cxflags("/O2", "/Oi", "/Ot", "/GL", "/arch:AVX2")
        add_cxflags("/fp:fast", "/Qpar", "/Qvec-report:2")
        add_ldflags("/LTCG", "/OPT:REF", "/OPT:ICF")
    end
end

-- Required packages
add_requires("spdlog", "openssl", "nlohmann_json")

-- Optional packages
add_requires("asio", {optional = true})
add_requires("boost", {optional = true, configs = {system = true}})
add_requires("numa", {optional = true, system = true})

-- Advanced concurrency feature definitions
add_defines(
    "ATOM_ASIO_ENABLE_ADVANCED_CONCURRENCY=1",
    "ATOM_ASIO_ENABLE_LOCK_FREE=1",
    "ATOM_ASIO_ENABLE_PERFORMANCE_MONITORING=1",
    "ATOM_HAS_SPDLOG=1",
    "ATOM_USE_WORK_STEALING_POOL=1",
    "ATOM_ENABLE_NUMA_AWARENESS=1"
)

-- SSL/TLS support
add_defines("USE_SSL")

-- ASIO configuration
if has_package("asio") then
    add_defines("ASIO_STANDALONE")
    add_packages("asio")
elseif has_package("boost") then
    add_defines("USE_BOOST_ASIO")
    add_packages("boost")
else
    -- Fallback to system ASIO
    add_defines("ASIO_STANDALONE")
    add_syslinks("asio")
end

-- NUMA support detection
if has_package("numa") then
    add_defines("ATOM_HAS_NUMA=1")
    add_packages("numa")
end

-- Source files for the advanced ASIO library
local sources = {
    -- Core concurrency framework
    "concurrency/concurrency.cpp",

    -- Enhanced MQTT implementation
    "mqtt/client.cpp",
    "mqtt/packet.cpp",

    -- Enhanced SSE implementation
    "sse/event.cpp",
    "sse/event_store.cpp",
    "sse/server/auth_service.cpp",
    "sse/server/connection.cpp",
    "sse/server/event_queue.cpp",
    "sse/server/event_store.cpp",
    "sse/server/http_request.cpp",
    "sse/server/metrics.cpp",
    "sse/server/server.cpp",
    "sse/server/server_config.cpp"
}

-- Header files
local headers = {
    -- Core concurrency framework
    "concurrency/lockfree_queue.hpp",
    "concurrency/adaptive_spinlock.hpp",
    "concurrency/work_stealing_pool.hpp",
    "concurrency/performance_monitor.hpp",
    "concurrency/memory_manager.hpp",
    "concurrency/concurrency.hpp",

    -- Enhanced MQTT implementation
    "mqtt/client.hpp",
    "mqtt/packet.hpp",
    "mqtt/protocol.hpp",
    "mqtt/types.hpp",

    -- Enhanced SSE implementation
    "sse/event.hpp",
    "sse/event_store.hpp",
    "sse/sse.hpp",
    "sse/server/auth_service.hpp",
    "sse/server/connection.hpp",
    "sse/server/event_queue.hpp",
    "sse/server/event_store.hpp",
    "sse/server/http_request.hpp",
    "sse/server/metrics.hpp",
    "sse/server/server.hpp",
    "sse/server/server_config.hpp",

    -- Core compatibility layer
    "asio_compatibility.hpp"
}

-- Main static library target
target("atom-asio-advanced")
    set_kind("static")

    -- Add source files
    add_files(sources)

    -- Add header files
    add_headerfiles(headers)

    -- Include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Required packages
    add_packages("spdlog", "openssl", "nlohmann_json")

    -- System libraries
    add_syslinks("pthread")

    -- Platform-specific libraries
    if is_plat("windows") then
        add_syslinks("ws2_32", "wsock32")
    elseif is_plat("linux") then
        add_syslinks("rt", "dl")
    end

    -- Enable position independent code
    add_cxflags("-fPIC")

    -- Advanced C++23 features
    add_cxflags("-fcoroutines", "-fconcepts", "-fmodules-ts")

    -- Memory safety and debugging (debug mode)
    if is_mode("debug") then
        add_cxflags("-fsanitize=address", "-fsanitize=undefined")
        add_cxflags("-fstack-protector-strong", "-D_FORTIFY_SOURCE=2")
        add_ldflags("-fsanitize=address", "-fsanitize=undefined")
    end

    -- Set target directory
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

-- Test target (optional)
target("atom-asio-tests")
    set_kind("binary")
    set_default(false)

    -- Test source files
    add_files("tests/*.cpp")

    -- Dependencies
    add_deps("atom-asio-advanced")
    add_packages("gtest")

    -- Include directories
    add_includedirs(".")

    -- Enable only if tests are requested
    if has_config("tests") then
        set_default(true)
    end

-- Benchmark target (optional)
target("atom-asio-benchmarks")
    set_kind("binary")
    set_default(false)

    -- Benchmark source files
    add_files("benchmarks/*.cpp")

    -- Dependencies
    add_deps("atom-asio-advanced")
    add_packages("benchmark")

    -- Include directories
    add_includedirs(".")

    -- Enable only if benchmarks are requested
    if has_config("benchmarks") then
        set_default(true)
    end

-- Example applications
target("mqtt-example")
    set_kind("binary")
    set_default(false)

    add_files("examples/mqtt_example.cpp")
    add_deps("atom-asio-advanced")
    add_includedirs(".")

    if has_config("examples") then
        set_default(true)
    end

target("sse-example")
    set_kind("binary")
    set_default(false)

    add_files("examples/sse_example.cpp")
    add_deps("atom-asio-advanced")
    add_includedirs(".")

    if has_config("examples") then
        set_default(true)
    end

-- Custom build options
option("tests")
    set_default(false)
    set_showmenu(true)
    set_description("Build unit tests")

option("benchmarks")
    set_default(false)
    set_showmenu(true)
    set_description("Build performance benchmarks")

option("examples")
    set_default(false)
    set_showmenu(true)
    set_description("Build example applications")

option("numa")
    set_default(false)
    set_showmenu(true)
    set_description("Enable NUMA awareness")

-- Build configuration summary
after_build(function (target)
    print("=== Atom ASIO Advanced Build Summary ===")
    print("Target: " .. target:name())
    print("Kind: " .. target:kind())
    print("Mode: " .. get_config("mode"))
    print("Arch: " .. get_config("arch"))
    print("Plat: " .. get_config("plat"))
    print("C++ Standard: C++23")
    print("Concurrency: Advanced lock-free primitives")
    print("Performance: Work-stealing thread pool")
    print("Monitoring: Real-time performance metrics")
    print("Memory: NUMA-aware allocation")
    print("========================================")
end)
