-- xmake configuration for Atom Test Suite
-- This project is licensed under the terms of the GPL3 license.
--
-- Project Name: Atom Test Suite
-- Description: Comprehensive test suite for Atom framework
-- Author: Max Qian
-- License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-test-suite")
set_version("1.0.0")
set_license("GPL-3.0")

-- Set C++ standard (match CMake)
set_languages("c++20")

-- Add required packages (matching CMake)
local use_system_packages = has_config("use_system_packages")
add_requires("gtest", {system = use_system_packages})

-- =============================================================================
-- Test Module Options
-- =============================================================================

-- Define test modules matching CMake structure
local test_modules = {
    "algorithm", "async", "components", "connection", "containers",
    "error", "extra", "image", "io", "log", "memory", "meta",
    "search", "secret", "serial", "sysinfo", "system", "type",
    "utils", "web"
}

-- Create options for each test module
for _, module in ipairs(test_modules) do
    option("test_" .. module)
        set_default(false)
        set_description("Build " .. module .. " tests")
        set_showmenu(true)
    option_end()
end

-- Option to build all tests
option("test_all")
    set_default(false)
    set_description("Build all tests")
    set_showmenu(true)
option_end()

-- =============================================================================
-- Include Test Subdirectories
-- =============================================================================

-- Include test infrastructure first
if os.isdir("tests") and os.isfile(path.join("tests", "xmake.lua")) then
    includes("tests")
end

-- Conditionally add test subdirectories based on build options
for _, module in ipairs(test_modules) do
    local should_build = has_config("test_" .. module) or has_config("test_all")
    local module_dir = path.join(os.scriptdir(), module)

    if should_build and os.isdir(module_dir) then
        if os.isfile(path.join(module_dir, "xmake.lua")) then
            includes(module)
            print("Including " .. module .. " tests")
        else
            print("Skipping " .. module .. " tests (no xmake.lua)")
        end
    end
end

-- =============================================================================
-- Custom Test Targets
-- =============================================================================

-- Create custom target for running all tests
target("test_all_modules")
    set_kind("phony")
    set_group("Tests/Custom")

    on_run(function(target)
        print("Running all module tests...")
        os.exec("xmake run -g tests")
    end)
target_end()

-- Create custom target for running core module tests
target("test_core_modules")
    set_kind("phony")
    set_group("Tests/Custom")

    on_run(function(target)
        print("Running core module tests (error, log, meta, type, utils)...")
        for _, module in ipairs({"error", "log", "meta", "type", "utils"}) do
            local test_dir = path.join(os.scriptdir(), module)
            if os.isdir(test_dir) then
                os.exec("xmake run test_" .. module)
            end
        end
    end)
target_end()

-- Create custom target for running I/O module tests
target("test_io_modules")
    set_kind("phony")
    set_group("Tests/Custom")

    on_run(function(target)
        print("Running I/O module tests (io, image, serial)...")
        for _, module in ipairs({"io", "image", "serial"}) do
            local test_dir = path.join(os.scriptdir(), module)
            if os.isdir(test_dir) then
                os.exec("xmake run test_" .. module)
            end
        end
    end)
target_end()

-- Create custom target for running system module tests
target("test_system_modules")
    set_kind("phony")
    set_group("Tests/Custom")

    on_run(function(target)
        print("Running system module tests (system, sysinfo)...")
        for _, module in ipairs({"system", "sysinfo"}) do
            local test_dir = path.join(os.scriptdir(), module)
            if os.isdir(test_dir) then
                os.exec("xmake run test_" .. module)
            end
        end
    end)
target_end()

-- Create custom target for running network module tests
target("test_network_modules")
    set_kind("phony")
    set_group("Tests/Custom")

    on_run(function(target)
        print("Running network module tests (web, connection)...")
        for _, module in ipairs({"web", "connection"}) do
            local test_dir = path.join(os.scriptdir(), module)
            if os.isdir(test_dir) then
                os.exec("xmake run test_" .. module)
            end
        end
    end)
target_end()

-- Print configuration summary
after_load(function ()
    print("Atom Test Suite configuration completed")
end)
