project "GloryEditorSDL"
	kind "SharedLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "Off"

	targetdir ("%{modulesOutDir}/GlorySDLWindow/Editor/Backend")
	objdir ("%{outputDir}")

	files
	{
		"**.h",
		"**.cpp",
		"%{SubmodoleDirs.ImGui}/backends/imgui_impl_sdl.h",
		"%{SubmodoleDirs.ImGui}/backends/imgui_impl_sdl.cpp",
		"premake5.lua"
	}

	vpaths
	{
	}

	includedirs
	{
		"%{DepsIncludeDir}",
		"%{DepsIncludeDir}/SDL2",

		"%{IncludeDir.glm}",

		"%{DepIncludesDir}",

		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.ImGui}",
		"%{GloryIncludeDir.core}",
		"%{GloryIncludeDir.editor}",
		"%{GloryIncludeDir.sdlwindow}",
		"%{GloryIncludeDir.threads}",

		"%{IncludeDir.FA}",
		"%{IncludeDir.Reflect}",
		"%{IncludeDir.Version}",
		"%{IncludeDir.ECS}",
		"%{IncludeDir.Utils}",

		"%{SubmodoleDirs.ImGui}/backends/",

		"%{rapidjson}",
	}

	libdirs
	{
		"%{DepsLibDir}",

		"%{LibDirs.glory}",
		"%{LibDirs.glory}/Modules/GlorySDLWindow",

		"%{LibDirs.ImGui}",
	}

	links
	{
		"GloryCore",
		"GloryReflect",
		"GloryECS",
		"GlorySDLWindow",

		"yaml-cpp",
		"ImGui",
		"GloryEditor",
		"GloryUtilsVersion",
		"GloryUtils",
		"SDL2",

		"GloryJobs",
		"GloryThreads",
	}

	defines
	{
		"GLORY_EXPORTS"
	}

	filter "system:windows"
		systemversion "10.0.19041.0"
		toolset "v143"

		defines
		{
			"_LIB"
		}

	filter "platforms:Win32"
		architecture "x86"
		defines "WIN32"

		libdirs
		{
			--"%{vulkanDir}/Third-Party/Bin32"
		}

	filter "platforms:x64"
		architecture "x64"

		libdirs
		{
			--"%{vulkanDir}/Third-Party/Bin"
		}

	filter "configurations:Debug"
		runtime "Debug"
		defines "_DEBUG"
		symbols "On"

	filter "configurations:Release"
		runtime "Release"
		defines "NDEBUG"
		optimize "On"
