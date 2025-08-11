-- filepath: d:\msys64\home\qwdma\Atom\atom\web\time\xmake.lua
-- xmake configuration for Atom-Web-Time submodule
-- Author: Max Qian
-- License: GPL3

-- Define source files
local time_sources = {
    "time_manager.cpp",
    "time_manager_impl.cpp",
    "time_utils.cpp"
}

-- Define header files
local time_headers = {
    "time_manager.hpp",
    "time_manager_impl.hpp",
    "time_error.hpp",
    "time_utils.hpp"
}

-- Build Object Library
target("atom-web-time-object")
    set_kind("object")

    -- Add files
    add_headerfiles(table.unpack(time_headers))
    add_files(table.unpack(time_sources))

    -- Add dependencies
    add_packages("loguru")

    -- Add include directories
    add_includedirs("$(projectdir)/atom", {public = true})

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("wsock32", "ws2_32")
    end

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Register sources and headers for parent module
function get_time_sources()
    return time_sources
end

function get_time_headers()
    return time_headers
end
