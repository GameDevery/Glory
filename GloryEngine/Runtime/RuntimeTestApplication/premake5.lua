project "RuntimeTestApplication"
	language "C++"
	cppdialect "C++17"
	staticruntime "Off"

	targetdir ("%{engineOutDir}")
	objdir ("%{outputDir}")

	files
	{
		"*.h",
		"*.cpp",
		"premake5.lua"
	}

	dependson
	{
		"GloryRuntimeApplication"
	}

	filter "system:windows"
		systemversion "10.0.19041.0"
		toolset "v143"

		defines
		{
			"_CONSOLE"
		}

	filter "platforms:Win32"
		architecture "x86"
		defines "WIN32"

	filter "platforms:x64"
		architecture "x64"

	filter "configurations:Debug"
		kind "ConsoleApp"
		runtime "Debug"
		defines "_DEBUG"
		symbols "On"
		debugdir "%{engineOutDir}"

	filter "configurations:Release"
		kind "WindowedApp"
		runtime "Release"
		defines "NDEBUG"
		optimize "On"
