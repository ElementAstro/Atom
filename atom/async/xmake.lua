-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-async")
set_version("1.0.0", {build = "%Y%m%d%H%M"})

-- Set languages
set_languages("c11", "cxx17")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- Add required packages
add_requires("loguru")

-- Define the main target
target("atom-async")
    -- Set target kind
    set_kind("static")

    -- Add source files (explicitly specified)
    add_files("limiter.cpp", "lock.cpp", "timer.cpp")

    -- Add header files (explicitly specified)
    add_headerfiles(
        "async.hpp",
        "daemon.hpp",
        "eventstack.hpp",
        "limiter.hpp",
        "lock.hpp",
        "message_bus.hpp",
        "message_queue.hpp",
        "pool.hpp",
        "queue.hpp",
        "safetype.hpp",
        "thread_wrapper.hpp",
        "timer.hpp",
        "trigger.hpp"
    )

    -- Add include directories
    add_includedirs(".", {public = true})

    -- Add packages
    add_packages("loguru")

    -- Add dependencies (assuming atom-utils is another xmake target)
    add_deps("atom-utils")

    -- Add system libraries
    add_syslinks("pthread")

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
    add_files("limiter.cpp", "lock.cpp", "timer.cpp")
    add_headerfiles(
        "async.hpp",
        "daemon.hpp",
        "eventstack.hpp",
