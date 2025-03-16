set_project("atom-meta")
    set_version("1.0.0")
    set_xmakever("2.5.1")

    -- Set the C++ standard
    set_languages("cxx20")

    -- Source files
    local source_files = {
        "global_ptr.cpp"
    }

    -- Header files
    local header_files = {
        "global_ptr.hpp"
    }

    -- Object Library
    target("atom-meta_object")
        set_kind("object")
        add_files(table.unpack(source_files))
        add_headerfiles(table.unpack(header_files))
    target_end()

    -- Static Library
    target("atom-meta")
        set_kind("static")
        add_deps("atom-meta_object")
        add_files(table.unpack(source_files))
        add_headerfiles(table.unpack(header_files))
        add_includedirs(".")
        set_targetdir("$(buildir)/lib")
        set_installdir("$(installdir)/lib")
        set_version("1.0.0", {build = "%Y%m%d%H%M"})
        on_install(function (target)
            os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        end)
    target_end()
