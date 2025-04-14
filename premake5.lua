--
-- Premake 5.x build configuration script
--

build_base = "./"
project_name = "ResourceRW"
x64_support = nil -- nil or true

	workspace (project_name)
		configurations { "Release", "Debug" }
		location   (build_base .. _ACTION)
		objdir     (build_base .. _ACTION .. "/obj")
		targetdir  (build_base .. "bin")
		flags
		{
			(x64_support and "No64BitChecks"),

			"NoManifest",		-- Prevent the generation of a manifest.
			"NoMinimalRebuild",	-- Disable minimal rebuild.
--			"NoIncrementalLink",	-- Disable incremental linking.
			"NoPCH",		-- Disable precompiled header support.
		}
		warnings "Default"		-- or "Extra"
		staticruntime "On"

		includedirs
		{
			"../00_simplebinder/",
			"../",
		}

---------------------------------------------------------------- filters

-- platform support (for x86/x64)
	if x64_support then
		platforms { "Win64", "Win32" }

		filter "platforms:Win64"
			targetdir  (build_base .. "bin_x64")

		filter "platforms:Win32"
			targetdir  (build_base .. "bin_x86")
	else
		platforms { "Win32" }
	end
		filter "platforms:Win*"
			system "Windows"

		filter "platforms:Win64"
			architecture "x86_64"

		filter "platforms:Win32"
			architecture "x86"

-- debug/relase options
		filter "configurations:Debug"
			defines     "_DEBUG"
			optimize    "Debug"
			symbols     "On"
			targetsuffix "-d"

		filter "configurations:Release"
			defines     "NDEBUG"
			optimize    "On"
			symbols     "On"	-- pdb output and suppress pdbpath
			targetdir   "../../../../bin/win32/plugin"		-- output modifier

-- pdbpath none
		filter { "action:vs200*", "configurations:Release", "kind:not StaticLib" }
			linkoptions { "/PDBPATH:NONE" } -- undocumented option...

		filter { "action:vs201*", "configurations:Release", "kind:not StaticLib" }
			linkoptions { "/PDBALTPATH:%_PDB%" }

-- xp toolset support
		filter "action:vs2012"
			toolset "v110_xp"

		filter "action:vs2013"
			toolset "v120_xp"

		filter "action:vs2015"
			toolset "v140_xp"

-- for gmake
		filter "action:gmake"
--			characterset ("Unicode")
			defines { "UNICODE", "_UNICODE" }
			buildoptions { "-municode" }
			linkoptions { "-static" }

		filter {}

---------------------------------------------------------------- projects
	project (project_name)
		targetname  (project_name)
		language    "C++"
		kind        "SharedLib"

		files
		{
			"../tp_stub.cpp",
			"../00_simplebinder/v2link.cpp",
			"Main.cpp",
		}
----------------------------------------------------------------
