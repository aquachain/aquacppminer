workspace "aquacppminer"
	location "prj"

	configurations { "Debug", "Rel", "RelAVX", "RelAVX2" }
	platforms { "x64", "win32", "linux32" }

	-- depending on LUA version, unpack is different
	if (unpack == nil) then
		unpack = table.unpack;
	end
	
	-- shared by all configs & platforms
	floatingpoint "Fast"
	rtti "Off"
	exceptionhandling ("Off")
	targetdir "bin/"
	debugdir "bin/"

	-- windows
	filter { "system:windows" }
		defines { "_CONSOLE", "CURL_STATICLIB" }
	filter {}

	filter { "system:windows", "platforms:x64" }
		defines { 'ARCH="Win64"' }
		architecture "x86_64"
	filter {}

	filter { "system:windows", "platforms:win32" }
		defines { 'ARCH="Win32"' }
		architecture "x86"
		targetdir "bin32/"
		debugdir "bin32/"
	filter {}

	-- linux
	filter { "system:linux", "platforms:x64" }
		defines { 'ARCH="Linux64"' }
		architecture "x86_64"

	filter { "system:linux", "platforms:linux32" }
		defines { 'ARCH="Linux32"' }
		architecture "x86"
		targetdir "bin32/"
		debugdir "bin32/"

	-- mac
	filter { "system:macosx" }
		defines { 'ARCH="MacOS64"' }
		architecture "x86_64"

	filter {}

	-- configurations
	local argon2_defines = {"ARGON2_NO_THREADS", "ARGON2_NO_SECURE_WIPE"};
	local common_defines = {"_CRT_SECURE_NO_WARNINGS", "_CONSOLE", "_HAS_EXCEPTIONS=0", unpack(argon2_defines)};

	-- Debug config
	filter {"configurations:Debug"}
		defines { "_DEBUG", unpack(common_defines) }
		symbols "On"
		targetsuffix "_d"

	-- Release configs
	filter {"configurations:Rel*"}
		defines { "NDEBUG", unpack(common_defines) }
		optimize "Full"
	filter {"configurations:RelAVX"}
		targetsuffix "_avx"
	filter {"configurations:RelAVX2"}
		targetsuffix "_avx2"
	filter { "configurations:RelAVX", "system:windows" }
		buildoptions { "/arch:AVX" }
	filter { "configurations:RelAVX", "system:linux or macosx" }
		buildoptions { "-mavx" }
	filter { "configurations:RelAVX2", "system:windows" }
		buildoptions { "/arch:AVX2" }
	filter { "configurations:RelAVX2", "system:linux or macosx" }
		buildoptions { "-mavx2" }
	filter {}

	-- WIN64 LIB DIRS
	local curl_lib_path_win = "libcurl/lib/static-";
	local common_lib_pathes_win64 = { "openssl-1.1.0f-vs2015/lib64" };
	filter { "system:windows", "configurations:Debug", "platforms:x64" }
		libdirs { 
			curl_lib_path_win .. "debug-x64", "mpir-3.0.0/lib/x64/Debug", unpack(common_lib_pathes_win64)
		}
	filter { "system:windows", "configurations:Rel*", "platforms:x64" }
		libdirs { 
			curl_lib_path_win .. "release-x64", "mpir-3.0.0/lib/x64/Release", unpack(common_lib_pathes_win64)
		}
	filter {}

	-- WIN32 LIB DIRS
	local common_lib_pathes_win32 = { "openssl-1.1.0f-vs2015/lib" };
	filter { "system:windows", "configurations:Debug", "platforms:win32" }
		libdirs { 
			curl_lib_path_win .. "debug-x86", "mpir-3.0.0/lib/win32/Debug", unpack(common_lib_pathes_win32)
		}
	filter { "system:windows", "configurations:Rel*", "platforms:win32" }
		libdirs { 
			curl_lib_path_win .. "release-x86", "mpir-3.0.0/lib/win32/Release", unpack(common_lib_pathes_win32)
		}
	filter {}

	-- include pathes
	filter { "system:windows" }
		includedirs { "src", "phc-winner-argon2/include", "openssl-1.1.0f-vs2015/include64", "mpir-3.0.0/lib/x64/Release", "libcurl/include", "rapidjson/include" }
	filter { "system:linux or macosx" }
		includedirs { "src", "phc-winner-argon2/include", "rapidjson/include" }
	filter {}
               
	-- main project
	project	"aquacppminer"
		kind "ConsoleApp"
		language "C++"
		location "prj"

		files {
			"src/*.h",
			"src/*.cpp",
			"phc-winner-argon2/src/blake2/*.*",
			"phc-winner-argon2/src/argon2.*",
			"phc-winner-argon2/src/core.*",
			"phc-winner-argon2/src/encoding.*",
			"phc-winner-argon2/src/opt.*",
			"phc-winner-argon2/src/thread.*",
			"*.sh",
			"scripts_win/*.sh",
			"premake5.lua",
			"*.md"
		}
		
		filter { "system:windows" }
			files {
				"src/windows/*.h",
				"src/windows/*.cpp",
			}
		
		filter {}

		flags { "NoMinimalRebuild", "MultiProcessorCompile", "NoRuntimeChecks", "NoBufferSecurityCheck", "NoFramePointer" }
		filter { "system:windows" }
			flags { "StaticRuntime" }
		filter {}

		filter { "system:windows", "platforms:win32" }
			largeaddressaware "on"
		filter {}

		if (cppdialect ~= nil) then
			cppdialect "C++11"
		end

		--libs
		filter { "system:windows", "configurations:Debug" }
			links { "libcryptoMT", "mpir", "libcurl_a_debug" }
		filter { "system:windows", "configurations:Rel*" }
			links { "libcryptoMT", "mpir", "libcurl_a" }

		filter { "system:linux", "platforms:linux32" }
			buildoptions {"-msse3"}

		filter { "system:linux" }
 			linkoptions { 
				"-lgmp -lpthread -lcrypto", 
				"`curl-config --libs`" 
			}
		filter { "system:macosx" }
                        linkoptions { 
				"/usr/local/opt/openssl/lib/libcrypto.a",
				"/usr/local/opt/openssl/lib/libssl.a",
                                "/usr/local/opt/gmp/lib/libgmp.a",
				 "-lcurl -lpthread -lz"
			}
