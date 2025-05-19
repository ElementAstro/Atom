-- xmake.lua for Atom Project
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom")
set_version("1.0.0")
set_languages("c++20")
set_license("GPL3")

-- Add options
option("shared_libs")
    set_default(false)
    set_showmenu(true)
    set_description("Build shared libraries instead of static")
option_end()

option("build_python")
    set_default(false)
    set_showmenu(true)
    set_description("Build Python bindings")
option_end()

option("build_examples")
    set_default(false)
    set_showmenu(true)
    set_description("Build examples")
option_end()

option("build_tests")
    set_default(false)
    set_showmenu(true)
    set_description("Build tests")
option_end()

-- Add required packages
add_requires("asio", {system = false})
add_requires("loguru", {system = false})
add_requires("zlib", {system = false})
add_requires("libzippp", {system = false})
add_requires("cpp-httplib", {system = false})
add_requires("tinyxml2", {system = false})

-- Optional packages
add_requires("cfitsio", {optional = true})
add_requires("libssh", {optional = true})

-- Windows-specific packages
if is_plat("windows") then
    add_requires("dlfcn-win32", {system = false})
end

-- Conditionally add Python requirements
if has_config("build_python") then
    add_requires("python 3.x", {system = false})
    add_requires("pybind11", {system = false})
end

-- Include atom subdirectory
includes("atom")

-- Include examples if enabled
if has_config("build_examples") then
    includes("example")
end

-- Include tests if enabled
if has_config("build_tests") then
    includes("tests")
end

-- Create a task for easy installation
task("install")
    on_run(function()
        import("core.project.project")
        import("core.platform.platform")
        
        -- Set install prefix
        local prefix = option.get("prefix") or "/usr/local"
        
        -- Build the project
        os.exec("xmake build")
        
        -- Install the project
        os.exec("xmake install -o " .. prefix)
        
        cprint("${bright green}Atom has been installed to " .. prefix)
    end)
    
    set_menu {
        usage = "xmake install",
        description = "Install Atom libraries and headers"
    }
task_end()
