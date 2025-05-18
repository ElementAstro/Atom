-- xmake.lua for Atom-Web-Time
-- This project is licensed under the terms of the GPL3 license.
--
-- Project Name: Atom-Web-Time
-- Description: Time Management API
-- Author: Max Qian
-- License: GPL3

-- Sources
local time_sources = {
    "time_manager.cpp",
    "time_manager_impl.cpp",
    "time_utils.cpp"
}

-- Headers
local time_headers = {
    "time_manager.hpp",
    "time_manager_impl.hpp",
    "time_error.hpp",
    "time_utils.hpp"
}

-- Build Object Library
target("atom-web-time-object")
    set_kind("object")
    add_files(time_headers, {public = true})
    add_files(time_sources, {public = false})
    add_packages("loguru")
    add_includedirs("$(projectdir)/atom")

    if is_plat("windows") then
        add_syslinks("wsock32", "ws2_32")
    end

-- Register sources and headers with parent
function get_time_sources()
    return time_sources
end

function get_time_headers()
    return time_headers
end
