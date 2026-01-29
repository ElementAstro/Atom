-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-async")
set_version("1.0.0", {build = "%Y%m%d%H%M"})

-- Set languages (match CMake C++20)
set_languages("c11", "cxx20")

-- Add build modes
add_rules("mode.debug", "mode.release", "mode.minsizerel")

-- Add required packages (use spdlog instead of loguru to match CMake)
local use_system_packages = has_config("use_system_packages")
add_requires("spdlog", {system = use_system_packages, configs = {fmt_external = true}})
add_requires("fmt", {system = use_system_packages})

-- Define the main target
target("atom-async")
    -- Set target kind
    set_kind("static")

    -- Add source files from new structure
    add_files("core/*.cpp")
    add_files("threading/*.cpp")
    add_files("sync/*.cpp")
    add_files("utils/*.cpp")

    -- Add header files from new structure
    add_headerfiles("*.hpp")  -- Backwards compatibility headers
    add_headerfiles("core/*.hpp")
    add_headerfiles("threading/*.hpp")
    add_headerfiles("messaging/*.hpp")
    add_headerfiles("execution/*.hpp")
    add_headerfiles("sync/*.hpp")
    add_headerfiles("utils/*.hpp")

    -- Add include directories
    add_includedirs(".", {public = true})

    -- Add packages (use spdlog instead of loguru)
    add_packages("spdlog", "fmt")

    -- Add dependencies (assuming atom-utils is another xmake target)
    add_deps("atom-utils")

    -- Add system libraries
    if is_plat("linux") then
        add_syslinks("pthread")
    end

    -- Enable position independent code for static library
    add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
    add_cflags("-fPIC", {tools = {"gcc", "clang"}})

    -- Set target directory
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Set version info
    set_version("1.0.0")

    -- Set output name (equivalent to OUTPUT_NAME)
    set_basename("atom-async")

    -- Installation rules
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        -- Install static library
        os.cp(target:targetfile(), path.join(installdir, "lib"))
        -- Install headers
        os.cp("*.hpp", path.join(installdir, "include", "atom-async"))
    end)

-- Optional: Create an object library equivalent (if needed elsewhere)
target("atom-async-object")
    set_kind("object")

    -- Add the same source files
    add_files("core/*.cpp")
    add_files("threading/*.cpp")
    add_files("sync/*.cpp")
    add_files("utils/*.cpp")
    add_headerfiles(
        "*.hpp",  -- Backwards compatibility headers
        "core/*.hpp",
        "threading/*.hpp",
        "messaging/*.hpp",
        "execution/*.hpp",
        "sync/*.hpp",
        "utils/*.hpp")

    add_includedirs(".", {public = true})
    add_packages("spdlog", "fmt")
    add_deps("atom-utils")

    if is_plat("linux") then
        add_syslinks("pthread")
    end

    add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
    add_cflags("-fPIC", {tools = {"gcc", "clang"}})

    set_objectdir("$(buildir)/obj")
target_end()
