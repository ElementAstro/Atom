-- xmake configuration for CPU module
-- Author: Max Qian
-- License: GPL3

target("atom-sysinfo-cpu")
    set_kind("static")
    set_languages("c++20")

    -- Source files
    add_files("cpu.cpp", "common.cpp")

    -- Platform-specific sources
    if is_plat("windows") then
        add_files("platform/windows.cpp")
    elseif is_plat("macosx") then
        add_files("platform/macos.cpp")
    elseif is_plat("linux") then
        add_files("platform/linux.cpp")
    elseif is_plat("freebsd") then
        add_files("platform/freebsd.cpp")
    end

    -- Header files
    add_headerfiles("cpu.hpp", "common.hpp")

    -- Dependencies
    add_packages("spdlog")

    -- Include directories
    add_includedirs(".", {public = true})

    -- Platform-specific libraries
    if is_plat("windows") then
        add_syslinks("pdh")
    elseif is_plat("linux", "freebsd") then
        add_syslinks("pthread")
    end

    -- Set output directory
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Installation
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/sysinfo/cpu"))
    end)
target_end()
