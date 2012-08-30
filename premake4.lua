solution "Saintly"
    location "build"
    configurations {"Debug", "Release"}

    project "Saintly"
        kind "SharedLib"
        language "C++"
        location "build"
        includedirs {"extlibs/mysql-c/include", "extlibs/boost_1_50_0/"}
        libdirs {"extlibs/mysql-c/lib", "extlibs/boost_1_50_0/stage/lib"}
        files { "src/**.h", "src/**.cpp"}

		configuration "Debug"
			flags { "Symbols" }

		configuration "Release"
			flags { "Optimize" }
