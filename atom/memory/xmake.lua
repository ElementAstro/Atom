-- xmake script for Memory Module
-- Part of the Atom Project
-- Author: Max Qian
-- License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-memory")
set_version("1.0.0")
set_license("GPL-3.0")

-- Set languages
set_languages("c11", "cxx17")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- Define library name
local lib_name = "atom-memory"

-- Function to check if sources exist
function has_sources()
    local sources = os.files("**.cpp")
    return #sources > 0
end

-- Function to get all source and header files
function get_sources()
    return os.files("**.cpp")
end

function get_headers()
    local headers = {}
    table.join2(headers, os.files("**.h"))
    table.join2(headers, os.files("**.hpp"))
    return headers
end

-- Main target
target(lib_name)
    local sources = get_sources()
    local headers = get_headers()

    if #sources > 0 then
        -- Create library with source files
        set_kind("static")
        add_files(sources)
        add_headerfiles(headers)

        -- Add dependencies
        add_deps("atom-error")

        -- Set include directories
        add_includedirs(".", {public = true})

        -- Enable position independent code
        add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
        add_cflags("-fPIC", {tools = {"gcc", "clang"}})

    else
        -- Create header-only library
        set_kind("headeronly")
        add_headerfiles(headers)

        -- Add dependencies for header-only library
        add_deps("atom-error")

        -- Set include directories
        add_includedirs(".", {public = true})
    end

    -- Set version
    set_version("1.0.0")

    -- Set output name
    set_basename(lib_name)

    -- Installation rules
    after_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        local kind = target:kind()

        if kind ~= "headeronly" then
            -- Install library file
            os.cp(target:targetfile(), path.join(installdir, "lib"))
        end

        -- Install headers
        local headerdir = path.join(installdir, "include", "atom", "memory")
        os.mkdir(headerdir)

        local headers = get_headers()
        for _, header in ipairs(headers) do
            os.cp(header, headerdir)
        end
    end)

    -- Add to global module list (equivalent to CMake's global property)
    after_build(function (target)
        -- Store module information for potential use by parent build system
        local modules_file = path.join("$(buildir)", "atom_modules.txt")
        io.writefile(modules_file, lib_name .. "\n", {append = true})
    end)

-- Print configuration status
after_load(function (target)
    local sources = get_sources()
    if #sources > 0 then
        print("Memory module configured as static library with " .. #sources .. " source files")
    else
        print("Memory module configured as header-only library")
    end
end)
