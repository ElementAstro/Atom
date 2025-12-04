-- xmake.lua for Atom Python Bindings
-- Author: Max Qian
-- License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-python-bindings")
set_version("0.0.1")

-- Set languages
set_languages("cxx20")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- Add required packages
-- Note: Using system-installed Python and pybind11 from MSYS2/system
-- instead of xmake package manager due to mingw compatibility
-- add_requires("python 3.x")
-- add_requires("pybind11")

-- Helper function to check if directory should be excluded
function is_excluded_dir(dirname)
    -- Exclude build directories
    if dirname:startswith("build") then return true end
    if dirname:startswith("Release") then return true end
    if dirname:startswith("Debug") then return true end
    if dirname:startswith("RelWithDebInfo") then return true end
    if dirname:startswith("MinSizeRel") then return true end

    -- Exclude hidden directories
    if dirname:startswith(".") then return true end

    -- Exclude other common directories
    local excluded = {"tests", "cmake", "dist", "logs", "__pycache__"}
    for _, exc in ipairs(excluded) do
        if dirname == exc then return true end
    end

    return false
end

-- Helper function to get all module directories
function get_module_dirs()
    local dirs = {}
    local entries = os.dirs("*")

    for _, entry in ipairs(entries) do
        local dirname = path.basename(entry)
        if not is_excluded_dir(dirname) then
            table.insert(dirs, dirname)
        end
    end

    return dirs
end

-- Helper function to collect source files recursively
function collect_sources(module_dir)
    local sources = {}
    local patterns = {
        module_dir .. "/*.cpp",
        module_dir .. "/**/*.cpp"
    }

    for _, pattern in ipairs(patterns) do
        local files = os.files(pattern)
        for _, file in ipairs(files) do
            table.insert(sources, file)
        end
    end

    return sources
end

-- Helper function to add module-specific dependencies
-- This should be called within a target() context
function setup_module_dependencies(module_type)
    -- Link to corresponding C++ library if it exists
    add_deps("atom-" .. module_type, {optional = true})

    -- Module-specific dependency handling
    if module_type == "web" then
        -- Web module requires web-address component and utils library
        add_deps("atom-web-address", {optional = true})
        add_deps("atom-utils", {optional = true})
    elseif module_type == "connection" then
        -- Connection module: Windows-specific socket library
        if is_plat("windows") then
            add_syslinks("mswsock")
        end
    elseif module_type == "algorithm" then
        -- Algorithm module requires type library
        add_deps("atom-type", {optional = true})
    elseif module_type == "image" then
        -- Image module requires OpenCV
        add_packages("opencv", {optional = true})
    elseif module_type == "io" then
        -- IO module may require utils library
        add_deps("atom-utils", {optional = true})
    elseif module_type == "system" or module_type == "sysinfo" then
        -- System/Sysinfo modules: Windows-specific libraries
        if is_plat("windows") then
            add_syslinks("pdh")
        end
    end

    -- Common dependencies for all modules
    add_deps("atom-error", {optional = true})
    add_packages("spdlog", {optional = true})
end

-- Get all module directories
local module_dirs = get_module_dirs()

-- Create a target for each module
for _, module_type in ipairs(module_dirs) do
    -- Collect source files
    local sources = collect_sources(module_type)

    -- Only create target if sources exist
    if #sources > 0 then
        target("atom_" .. module_type)
            -- Set as Python shared module
            set_kind("shared")

            -- Add source files
            for _, src in ipairs(sources) do
                add_files(src)
            end

            -- Set target properties
            set_targetdir("$(projectdir)/python")
            set_prefixname("")  -- Remove 'lib' prefix

            -- Set extension based on platform
            if is_plat("windows") then
                set_extension(".pyd")
            else
                set_extension(".so")
            end

            -- Add packages
            -- Use Python and pybind11 from uv virtual environment (.venv)
            -- Python include from uv-managed CPython
            add_includedirs("$(env APPDATA)/uv/python/cpython-3.12.11-windows-x86_64-none/Include", {public = false})
            -- pybind11 include from .venv
            add_includedirs("$(projectdir)/.venv/Lib/site-packages/pybind11/include", {public = false})
            -- Python library from uv-managed CPython
            add_linkdirs("$(env APPDATA)/uv/python/cpython-3.12.11-windows-x86_64-none/libs")
            add_links("python312")

            -- Add include directories
            add_includedirs("$(projectdir)")  -- Atom root directory

            -- Add module-specific dependencies
            setup_module_dependencies(module_type)

            -- Set C++ standard
            set_languages("cxx20")

            -- Platform-specific flags
            if is_plat("windows") then
                add_cxxflags("/bigobj")
                add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
            else
                add_cxxflags("-fvisibility=hidden")
            end

            -- Installation rules
            after_build(function (target)
                local module_dir = path.join("$(projectdir)", "python", module_type)
                local init_file = path.join(module_dir, "__init__.py")

                -- Check if __init__.py exists
                if os.isfile(init_file) then
                    print("Using existing __init__.py for " .. module_type .. " module")
                else
                    print("Warning: No __init__.py found for " .. module_type .. " module")
                end
            end)

            -- Install target
            on_install(function (target)
                local installdir = path.join(target:installdir(), "python", module_type)
                os.mkdir(installdir)

                -- Install the compiled module
                os.cp(target:targetfile(), installdir)

                -- Install __init__.py
                local module_dir = path.join("$(projectdir)", "python", module_type)
                local init_file = path.join(module_dir, "__init__.py")
                if os.isfile(init_file) then
                    os.cp(init_file, installdir)
                end

                -- Install subdirectory __init__.py files
                local subinit_files = os.files(module_dir .. "/**/__init__.py")
                for _, subinit in ipairs(subinit_files) do
                    local relpath = path.relative(subinit, module_dir)
                    local destdir = path.join(installdir, path.directory(relpath))
                    os.mkdir(destdir)
                    os.cp(subinit, path.join(destdir, "__init__.py"))
                end
            end)
        target_end()

        print("Created Python binding target: atom_" .. module_type .. " with " .. #sources .. " source files")
    end
end

-- Print summary
print("Python bindings configuration complete:")
print("  Modules found: " .. #module_dirs)
print("  Excluded directories handled: build*, .*, tests, cmake, dist, logs, __pycache__")
print("  Module-specific dependencies configured")
print("")
print("Build with: xmake build")
print("Install with: xmake install")
