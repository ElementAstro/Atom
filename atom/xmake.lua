-- filepath: d:\msys64\home\qwdma\Atom\atom\xmake.lua
-- Main xmake configuration for Atom project
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom")
set_version("1.0.0")
set_xmakever("2.5.1")
set_license("GPL3")

-- Build options
option("build_python")
    set_default(false)
    set_showmenu(true)
    set_description("Enable Python bindings")
option_end()

option("shared_libs")
    set_default(false)
    set_showmenu(true)
    set_description("Build shared libraries instead of static")
option_end()

-- Set common configurations
set_languages("c++20")
set_optimize("faster")

-- Include all module subdirectories
includes(
    "algorithm", 
    "async", 
    "components", 
    "connection", 
    "error", 
    "image",
    "io", 
    "log",
    "memory",
    "meta",
    "search",
    "secret",
    "serial", 
    "sysinfo", 
    "system", 
    "type", 
    "utils", 
    "web"
)

-- Define third-party package dependencies
local atom_packages = {
    "loguru",
    "cpp-httplib",
    "libzippp"
}

-- Define internal module dependencies
local atom_libs = {
    "atom-meta",
    "atom-algorithm",
    "atom-async",
    "atom-io",
    "atom-component",
    "atom-type",
    "atom-utils",
    "atom-search",
    "atom-web",
    "atom-system"
}

-- Object Library (collection of compiled objects)
target("atom_object")
    set_kind("object")
    
    -- Add source files
    add_files("log/logger.cpp", "log/syslog.cpp")
    add_headerfiles("log/logger.hpp", "log/syslog.hpp")
    
    -- Add dependencies
    add_deps(table.unpack(atom_libs))
    add_packages(table.unpack(atom_packages))
    
    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("setupapi", "wsock32", "ws2_32", "shlwapi", "iphlpapi")
    elseif is_plat("linux") then
        add_syslinks("pthread", "dl")
    end
target_end()

-- Main Atom Library
target("atom")
    -- Set library type based on option
    set_kind(has_config("shared_libs") and "shared" or "static")
    
    -- Add dependencies
    add_deps("atom_object")
    add_packages(table.unpack(atom_packages))
    
    -- Version info with build timestamp
    set_version("1.0.0", {build = "%Y%m%d%H%M"})
    
    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")
    
    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("**/*.hpp", path.join(target:installdir(), "include"), {rootdir = "."})
    end)
target_end()

-- Python Support
if has_config("build_python") then
    add_requires("python 3.x", {system = false})
    add_requires("pybind11", {system = false})
    
    target("atom_python")
        set_kind("shared")
        add_files("python_binding.cpp")
        add_deps("atom")
        add_packages("python", "pybind11")
        
        -- Set output name for Python module
        set_filename("atom.so")
        set_targetdir("$(buildir)/python")
        
        -- Install configuration for Python module
        on_install(function (target)
            os.cp(target:targetfile(), path.join(target:installdir(), "python"))
        end)
    target_end()
end
