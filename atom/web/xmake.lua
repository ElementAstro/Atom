-- xmake.lua for Atom-Web
-- This project is licensed under the terms of the GPL3 license.
--
-- Project Name: Atom-Web
-- Description: Web API
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-web")
set_version("1.0.0")
set_license("GPL3")

-- Include time subdirectory
includes("time/xmake.lua")

-- Sources
local sources = {
    "address.cpp",
    "downloader.cpp",
    "httpclient.cpp",
    "httplite.cpp",
    "utils.cpp",
    "utils/addr_info.cpp",
    "utils/dns.cpp",
    "utils/ip.cpp",
    "utils/network.cpp",
    "utils/port.cpp",
    "utils/socket.cpp"
}

-- Add time module sources
for _, src in ipairs(get_time_sources()) do
    table.insert(sources, "time/" .. src)
end

-- Headers
local headers = {
    "address.hpp",
    "downloader.hpp",
    "httpclient.hpp",
    "httplite.hpp",
    "utils.hpp",
    "time.hpp", -- 保留兼容头文件
    "utils/common.hpp",
    "utils/addr_info.hpp",
    "utils/dns.hpp",
    "utils/ip.hpp",
    "utils/network.hpp",
    "utils/port.hpp",
    "utils/socket.hpp"
}

-- Add time module headers
for _, hdr in ipairs(get_time_headers()) do
    table.insert(headers, "time/" .. hdr)
end

-- Build Object Library
target("atom-web-object")
    set_kind("object")
    add_files(headers, {public = true})
    add_files(sources, {public = false})
    add_packages("loguru")

-- Build Static Library
target("atom-web")
    set_kind("static")
    add_deps("atom-web-object")
    add_packages("loguru", "cpp-httplib")
    add_includedirs(".", {public = true})

    if is_plat("windows") then
        add_syslinks("wsock32", "ws2_32")
    end

    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    after_build(function (target)
        os.cp("$(buildir)/lib", "$(projectdir)/lib")
        os.cp("$(projectdir)/*.hpp", "$(projectdir)/include")
    end)
