solution "Saintly"
    location "build"
    configurations {"Debug", "Release"}

    project "Saintly"
        kind "SharedLib"
        language "C++"
        location "build"
        includedirs {"extlibs/mysql-c/include", "extlibs/boost_1_50_0/", "extlibs/log4cplus-1.1.0-rc8/include"}
        libdirs {"extlibs/mysql-c/lib", "extlibs/boost_1_50_0/stage/lib", "extlibs/log4cplus-1.1.0-rc8/msvc10/Win32/bin.Release"}
        files { "src/**.h", "src/**.cpp"}

		configuration "Debug"
			flags { "Symbols" }

		configuration "Release"
			flags { "Optimize" }
