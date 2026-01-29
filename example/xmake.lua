-- xmake configuration for Atom Examples
-- This project is licensed under the terms of the GPL3 license.
--
-- Project Name: Atom Examples
-- Description: Example programs for Atom framework
-- Author: Max Qian
-- License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("AtomExamples")
set_version("1.0.0")
set_license("GPL-3.0")

-- Set C++ standard (match CMake - use C++23 if available, otherwise C++20)
set_languages("c++23")

-- =============================================================================
-- Example Module Options
-- =============================================================================

-- Define example modules matching CMake structure
local example_modules = {
    "algorithm", "async", "components", "connection", "containers",
    "error", "extra", "image", "io", "log", "memory", "meta",
    "search", "secret", "serial", "sysinfo", "system", "type",
    "utils", "web"
}

-- Create options for each example module
for _, module in ipairs(example_modules) do
    option("example_" .. module)
        set_default(false)
        set_description("Build " .. module .. " examples")
        set_showmenu(true)
    option_end()
end

-- Option to build all examples
option("example_all")
    set_default(false)
    set_description("Build all examples")
    set_showmenu(true)
option_end()

-- =============================================================================
-- Include Example Subdirectories
-- =============================================================================

-- Conditionally add example subdirectories based on build options
for _, module in ipairs(example_modules) do
    local should_build = has_config("example_" .. module) or has_config("example_all")
    local module_dir = path.join(os.scriptdir(), module)

    if should_build and os.isdir(module_dir) then
        if os.isfile(path.join(module_dir, "xmake.lua")) then
            includes(module)
            print("Including " .. module .. " examples")
        else
            print("Skipping " .. module .. " examples (no xmake.lua)")
        end
    end
end

-- Print configuration summary
after_load(function ()
    print("Atom Examples configuration completed")
end)
