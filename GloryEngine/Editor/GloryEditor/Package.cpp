#include "Package.h"
#include "ProjectSettings.h"
#include "SettingsEnums.h"
#include "EditorAssetDatabase.h"
#include "EditorPipelineManager.h"
#include "EditorSceneManager.h"
#include "EditorApplication.h"
#include "EditorResourceManager.h"
#include "AssetCompiler.h"
#include "EditableResource.h"
#include "Dispatcher.h"
#include "SystemTools.h"

#include <JobManager.h>

#include <ShaderSourceData.h>
#include <GScene.h>

#include <Engine.h>
#include <Debug.h>
#include <AssetDatabase.h>
#include <AssetArchive.h>
#include <BinaryStream.h>
#include <AssetManager.h>
#include <PipelineData.h>

#include <filesystem>
#include <tchar.h>

#ifdef ADD_PACKAGE_LAG
#define PACKAGE_LAG std::this_thread::sleep_for(std::chrono::milliseconds(100));
#else
#define PACKAGE_LAG
#endif

namespace Glory::Editor
{
	static const uint32_t SceneHash = ResourceTypes::GetHash<GScene>();
	static const uint32_t ShaderSourceHash = ResourceTypes::GetHash<ShaderSourceData>();
	static const uint32_t PipelineDataHash = ResourceTypes::GetHash<PipelineData>();

	std::atomic<size_t> m_CurrentTask;
	size_t m_TaskCount;
	std::vector<PackageTask> PackagingTasks;
	PackageTaskState PackagingTaskState;

	std::atomic<bool> Running = false;
	std::atomic<bool> Cancel = false;
	std::atomic<bool> Canceled = false;
	std::atomic<bool> Failed = false;

	std::vector<UUID> ScenesToPackage;
	std::vector<GScene*> LoadedScenesToPackage;

	std::vector<UUID> GlobalAssets;
	std::map<UUID, std::vector<UUID>> AssetsPerScene;
	//std::map<UUID, std::vector<UUID>> ShadersPerScene;
	std::map<UUID, std::vector<UUID>> PipelinesPerScene;
	std::vector<UUID> SharedAssets;
	//std::vector<UUID> SharedShaders;
	//std::vector<UUID> Shaders;

	std::vector<UUID> SharedPipelines;
	std::vector<UUID> Pipelines;

	std::vector<UUID> UsedAssets;
	std::vector<AssetLocation> AssetLocations;

	UUID EntryScene = 0;

	bool PackageJob(Engine* pEngine, std::filesystem::path packageRoot)
	{
		Running = true;
		for (size_t i = 0; i < PackagingTasks.size(); ++i)
		{
			if (Cancel)
			{
				Canceled = true;
				Cancel = false;
				break;
			}

			PackagingTaskState.m_SubTaskName = "Sub task:";
			PackagingTaskState.m_ProcessedSubTasks = 0;
			PackageTask& task = PackagingTasks[i];
			PackagingTaskState.m_TotalSubTasks = task.m_TotalSubTasks;
			const bool success = task.m_Callback(pEngine, packageRoot, PackagingTaskState);
			if (!success)
			{
				pEngine->GetDebug().LogError("Packaging failed, check the console for errors");
				Failed = true;
				Running = false;
				return false;
			}
			++m_CurrentTask;
		}

		Running = false;
		return true;
	}

	void CollectPackagingScenes(std::vector<UUID>& scenes)
	{
		/* First we must gather what scenes will be packaged */
		ProjectSettings* packageSettings = ProjectSettings::Get("Packaging");
		Utils::YAMLFileRef& file = **packageSettings;
		auto scenePackageMode = file["Scenes/PackageScenesMode"];
		auto scenesToPackage = file["Scenes/List"];
		auto entryScene = file["Scenes/EntryScene"];

		const PackageScenes mode = scenePackageMode.AsEnum<PackageScenes>();
		switch (mode)
		{
		case PackageScenes::All: {
			EditorAssetDatabase::GetAllAssetsOfType(SceneHash, scenes);
			break;
		}
		case PackageScenes::Opened: {
			break;
		}
		case PackageScenes::List: {
			for (size_t i = 0; i < scenesToPackage.Size(); ++i)
			{
				const UUID sceneID = scenesToPackage[i].As<uint64_t>();
				if (!sceneID) continue;
				scenes.push_back(sceneID);
			}
			break;
		}
		default:
			break;
		}

		EntryScene = entryScene.As<uint64_t>();
	}

	void ScanForAssetsInResource(Engine* pEngine, Resource* pResource, std::vector<UUID>& assets)
	{
		if (!pResource) return;
		const size_t count = assets.size();
		pResource->References(pEngine, assets);
		for (size_t i = count; i < assets.size(); ++i)
		{
			const UUID id = assets[i];
			pResource = pEngine->GetAssetManager().GetAssetImmediate(id);
			ScanForAssetsInResource(pEngine, pResource, assets);
		}
	}

	void ScanForAssets(Engine* pEngine, Utils::NodeValueRef& node, std::vector<UUID>& assets)
	{
		if (node.IsMap())
		{
			for (auto itor = node.Begin(); itor != node.End(); ++itor)
			{
				const std::string key = *itor;
				ScanForAssets(pEngine, node[key], assets);
			}
		}
		else if (node.IsSequence())
		{
			for (size_t i = 0; i < node.Size(); ++i)
			{
				ScanForAssets(pEngine, node[i], assets);
			}
		}
		else if (node.IsScalar())
		{
			const std::string assetIDStr = node.As<std::string>();
			try
			{
				const UUID assetID = std::stoull(assetIDStr);
				if (!EditorAssetDatabase::AssetExists(assetID)) return;
				ResourceMeta meta;
				EditorAssetDatabase::GetAssetMetadata(assetID, meta);
				AssetLocation location;
				EditorAssetDatabase::GetAssetLocation(assetID, location);
				if (meta.Hash() == SceneHash) return;

				assets.push_back(assetID);
				if (!location.SubresourcePath.empty())
				{
					/* Add the parent asset */
					const UUID parentID = EditorAssetDatabase::FindAssetUUID(location.Path);
					assets.push_back(parentID);
					Resource* pResource = pEngine->GetAssetManager().GetAssetImmediate(assetID);
					if (!pResource) return;
					ScanForAssetsInResource(pEngine, pResource, assets);
				}

				/* We also have to search this asset recursively for any referenced assets */
				EditableResource* pResource = EditorApplication::GetInstance()->GetResourceManager().GetEditableResource(assetID);
				if (!pResource) return;
				YAMLResourceBase* pYAMLResource = dynamic_cast<YAMLResourceBase*>(pResource);
				if (!pYAMLResource) return;

				Utils::YAMLFileRef& assetFile = **pYAMLResource;
				ScanSceneFileForAssets(pEngine, assetFile, assets);
			}
			catch (const std::exception&)
			{
				return;
			}
		}
	}

	void ScanSceneFileForAssets(Engine* pEngine, Utils::YAMLFileRef& file, std::vector<UUID>& assets)
	{
		Utils::NodeValueRef root = file.RootNodeRef().ValueRef();
		ScanForAssets(pEngine, root, assets);
	}

	size_t NumScenesToPackage()
	{
		return ScenesToPackage.size();
	}

	UUID SceneToPackage(size_t index)
	{
		return ScenesToPackage[index];
	}

	GScene* LoadedSceneToPackage(size_t index)
	{
		return LoadedScenesToPackage[index];
	}

#pragma region Tasks

	bool CalculateAssetGroupsTask(Engine* pEngine, const std::filesystem::path&, PackageTaskState& task)
	{
		task.m_SubTaskName = "Calculating";

		AssetsPerScene.clear();
		PipelinesPerScene.clear();
		SharedAssets.clear();
		SharedPipelines.clear();
		Pipelines.clear();
		GlobalAssets.clear();

		/* Find assets used by modules */
		for (size_t i = 0; i < pEngine->ModulesCount(); ++i)
		{
			Module* pModule = pEngine->GetModule(i);
			pModule->CollectReferences(GlobalAssets);
		}

		/* Add global assets and shaders */
		for (size_t i = 0; i < GlobalAssets.size(); ++i)
		{
			ResourceMeta meta;
			EditorAssetDatabase::GetAssetMetadata(GlobalAssets[i], meta);

			/* Ignore shaders */
			if (meta.Hash() == ShaderSourceHash)
				continue;

			/* We need pipelines to figure out which shaders to package */
			if (meta.Hash() == PipelineDataHash)
			{
				Pipelines.push_back(GlobalAssets[i]);
				SharedPipelines.push_back(GlobalAssets[i]);
			}
			SharedAssets.push_back(GlobalAssets[i]);
		}

		/* Find all resources in use for each scene */
		std::map<UUID, std::vector<UUID>> assetScenes;
		for (size_t i = 0; i < LoadedScenesToPackage.size(); ++i)
		{
			std::vector<UUID> assets;
			ScanForAssetsInResource(pEngine, LoadedScenesToPackage[i], assets);
			for (size_t j = 0; j < assets.size(); ++j)
			{
				const UUID asset = assets[j];
				std::vector<UUID>& assetScene = assetScenes[asset];
				if (assetScene.empty() || std::find(assetScene.begin(), assetScene.end(), ScenesToPackage[i]) == assetScene.end())
					assetScene.push_back(ScenesToPackage[i]);
			}
		}

		/* Find unique assets and assign them to a scene */
		for (auto itor = assetScenes.begin(); itor != assetScenes.end(); ++itor)
		{
			ResourceMeta meta;
			EditorAssetDatabase::GetAssetMetadata(itor->first, meta);

			/* Ignore shaders */
			if (meta.Hash() == ShaderSourceHash)
				continue;

			/* We need pipelines to figure out which shaders to package */
			if (meta.Hash() == PipelineDataHash)
			{
				Pipelines.push_back(itor->first);
				if (itor->second.size() == 1)
				{
					PipelinesPerScene[itor->second[0]].push_back(itor->first);
				}
				else
					SharedPipelines.push_back(itor->first);
			}

			if (itor->second.size() == 1)
			{
				AssetsPerScene[itor->second[0]].push_back(itor->first);
				continue;
			}
			SharedAssets.push_back(itor->first);
		}

		++task.m_ProcessedSubTasks;
		return true;
	}

	//bool CompileShadersTask(Engine* pEngine, const std::filesystem::path&, PackageTaskState& task)
	//{
	//	task.m_TotalSubTasks = Shaders.size();

	//	/* Compile shaders */
	//	/** @todo Actually compile the shaders for the chosen platform */
	//	for (size_t i = 0; i < Shaders.size(); ++i)
	//	{
	//		const UUID uuid = Shaders[i];
	//		ResourceMeta meta;
	//		EditorAssetDatabase::GetAssetMetadata(uuid, meta);
	//		task.m_SubTaskName = meta.Name();
	//		PACKAGE_LAG

	//		Resource* pResource = pEngine->GetAssetManager().FindResource(uuid);
	//		if (!pResource)
	//		{
	//			task.m_SubTaskName = "";
	//			continue;
	//		}
	//		ShaderSourceData* pShaderSource = static_cast<ShaderSourceData*>(pResource);

	//		FileData* pCompiledShader = pShaderSource->GetCompiledShader();
	//		if (pCompiledShader)
	//			delete pCompiledShader;

	//		/** @todo */
	//		//FileData* pNewCompiled = pEngine->GetPipelineManager().GetCompiledShaderFile(uuid);
	//		//pCompiledShader = new FileData(pNewCompiled);
	//		//pShaderSource->SetCompiledShader(pCompiledShader);
	//		++task.m_ProcessedSubTasks;
	//		task.m_SubTaskName = "";
	//	}

	//	return true;
	//}

	bool PackageScenesTask(Engine* pEngine, const std::filesystem::path& packageRoot, PackageTaskState& task)
	{
		/* Open every scene and package them individually along with their assets */
		std::filesystem::path relativeDataPath = "Data";
		for (size_t i = 0; i < ScenesToPackage.size(); ++i)
		{
			std::filesystem::path relativeScenePath = relativeDataPath;
			relativeScenePath.append(std::to_string(ScenesToPackage[i])).replace_extension("gcs");
			{
				ResourceMeta meta;
				EditorAssetDatabase::GetAssetMetadata(ScenesToPackage[i], meta);
				task.m_SubTaskName = meta.Name();
				PACKAGE_LAG

				std::filesystem::path path = packageRoot;
				path.append(relativeScenePath.string());
				BinaryFileStream sceneFile{ path };
				AssetArchive archive{ &sceneFile, AssetArchiveFlags::WriteNew };

				archive.Serialize(LoadedScenesToPackage[i]);

				UsedAssets.push_back(ScenesToPackage[i]);
				AssetLocations.push_back({ relativeScenePath.string(), "", 0 });
				task.m_SubTaskName = "";

			}
			++task.m_ProcessedSubTasks;
		}

		return true;
	}

	bool PackageAssetsTask(Engine* pEngine, const std::filesystem::path& packageRoot, PackageTaskState& task)
	{
		size_t totalAssets = 0;
		for (size_t i = 0; i < ScenesToPackage.size(); ++i)
		{
			totalAssets += AssetsPerScene[ScenesToPackage[i]].size();
		}
		totalAssets += SharedAssets.size();
		task.m_TotalSubTasks = totalAssets;

		std::filesystem::path relativeDataPath = "Data";
		for (size_t i = 0; i < ScenesToPackage.size(); ++i)
		{
			std::filesystem::path relativeScenePath = relativeDataPath;
			relativeScenePath.append(std::to_string(ScenesToPackage[i])).replace_extension("gcag");
			{
				std::filesystem::path path = packageRoot;
				path.append(relativeScenePath.string());
				BinaryFileStream sceneFile{ path };
				AssetArchive archive{ &sceneFile, AssetArchiveFlags::WriteNew };

				const std::vector<UUID>& assets = AssetsPerScene[ScenesToPackage[i]];
				for (size_t j = 0; j < assets.size(); ++j)
				{
					const UUID assetID = assets[j];
					ResourceMeta meta;
					if (!EditorAssetDatabase::GetAssetMetadata(assetID, meta))
					{
						++task.m_ProcessedSubTasks;
						task.m_SubTaskName = "";
					}
					task.m_SubTaskName = meta.Name();
					PACKAGE_LAG

					Resource* pResource = pEngine->GetAssetManager().FindResource(assetID);
					archive.Serialize(pResource);
			
					AssetLocations.push_back({ relativeScenePath.string(), "", j });
					UsedAssets.push_back(assetID);
					++task.m_ProcessedSubTasks;
					task.m_SubTaskName = "";
				}
			}
		}

		if (!SharedAssets.empty())
		{
			/* Package other asset groups */
			std::filesystem::path relativePath = "Data";
			relativePath.append("Shared.gcag");

			std::filesystem::path sharedAssetsPath = packageRoot;
			sharedAssetsPath.append(relativePath.string());
			BinaryFileStream sharedAssetsFile{ sharedAssetsPath };
			AssetArchive archive{ &sharedAssetsFile, AssetArchiveFlags::WriteNew };
			for (size_t i = 0; i < SharedAssets.size(); ++i)
			{
				const UUID assetID = SharedAssets[i];
				ResourceMeta meta;
				EditorAssetDatabase::GetAssetMetadata(assetID, meta);
				task.m_SubTaskName = meta.Name();
				PACKAGE_LAG

				Resource* pResource = pEngine->GetAssetManager().FindResource(assetID);
				archive.Serialize(pResource);

				AssetLocations.push_back({ relativePath.string(), "", i });
				UsedAssets.push_back(assetID);
				++task.m_ProcessedSubTasks;
				task.m_SubTaskName = "";
			}
		}

		return true;
	}

	bool PackageShadersTask(Engine* pEngine, const std::filesystem::path& packageRoot, PackageTaskState& task)
	{
		task.m_TotalSubTasks = Pipelines.size();

		std::filesystem::path relativeDataPath = "Data";

		for (size_t i = 0; i < ScenesToPackage.size(); ++i)
		{
			std::filesystem::path relativeScenePath = relativeDataPath;
			relativeScenePath.append(std::to_string(ScenesToPackage[i])).replace_extension("gcsp");
			{
				std::filesystem::path path = packageRoot;
				path.append(relativeScenePath.string());
				BinaryFileStream sceneFile{ path };
				AssetArchive archive{ &sceneFile, AssetArchiveFlags::WriteNew };

				const std::vector<UUID>& pipelines = PipelinesPerScene[ScenesToPackage[i]];
				for (size_t j = 0; j < pipelines.size(); ++j)
				{
					const UUID assetID = pipelines[j];
					ResourceMeta meta;
					EditorAssetDatabase::GetAssetMetadata(assetID, meta);
					task.m_SubTaskName = meta.Name();
					PACKAGE_LAG
					
					auto& compiledShaders = pEngine->GetPipelineManager().GetPipelineCompiledShaders(assetID);
					for (const FileData& pCompiledShader : compiledShaders)
					{
						archive.Serialize(&pCompiledShader);
					}
					++task.m_ProcessedSubTasks;
					task.m_SubTaskName = "";
				}
			}
		}

		if (!SharedPipelines.empty())
		{
			/* Package other asset groups */
			std::filesystem::path relativePath = "Data";
			relativePath.append("Shared.gcsp");

			std::filesystem::path sharedShadersPath = packageRoot;
			sharedShadersPath.append(relativePath.string());
			BinaryFileStream sharedShadersFile{ sharedShadersPath };
			AssetArchive archive{ &sharedShadersFile, AssetArchiveFlags::WriteNew };
			for (size_t i = 0; i < SharedPipelines.size(); ++i)
			{
				const UUID assetID = SharedPipelines[i];
				ResourceMeta meta;
				EditorAssetDatabase::GetAssetMetadata(assetID, meta);
				task.m_SubTaskName = meta.Name();
				PACKAGE_LAG

				auto& compiledShaders = pEngine->GetPipelineManager().GetPipelineCompiledShaders(assetID);
				for (const FileData& pCompiledShader : compiledShaders)
				{
					archive.Serialize(&pCompiledShader);
				}
				++task.m_ProcessedSubTasks;
				task.m_SubTaskName = "";
			}
		}

		return true;
	}

	bool PackageAssetDatabase(Engine* pEngine, const std::filesystem::path& packageRoot, PackageTaskState& task)
	{
		task.m_TotalSubTasks = UsedAssets.size();

		std::filesystem::path dataPath = packageRoot;
		dataPath.append("Data");

		{
			/* Package asset database but only for used assets */
			std::filesystem::path databasePath = dataPath;
			databasePath.append("Assets.gcdb");
			BinaryFileStream dbFile{ databasePath };
			BinaryStream* stream = &dbFile;
			stream->Write(EntryScene);
			for (size_t i = 0; i < UsedAssets.size(); ++i)
			{
				const UUID assetID = UsedAssets[i];

				const AssetLocation& location = AssetLocations[i];
				ResourceMeta meta;
				EditorAssetDatabase::GetAssetMetadata(assetID, meta);
				task.m_SubTaskName = meta.Name();
				PACKAGE_LAG

				stream->Write(meta.Name());
				stream->Write(meta.ID());
				stream->Write(meta.Hash());
				stream->Write(location.Path);
				stream->Write(location.Index);
				++task.m_ProcessedSubTasks;
				task.m_SubTaskName = "";
			}
		}

		return true;
	}

	bool CopyFilesTask(Engine* pEngine, const std::filesystem::path& packageRoot, PackageTaskState& task)
	{
		/* Copy modules and their resources and settings */
		std::filesystem::path modulesPath = packageRoot;
		modulesPath.append("Modules");
		std::filesystem::path settingsPath = modulesPath;
		settingsPath.append("Config");

		if (!std::filesystem::exists(settingsPath))
			std::filesystem::create_directories(settingsPath);

		for (size_t i = 0; i < pEngine->ModulesCount(); i++)
		{
			Module* pModule = pEngine->GetModule(i);
			const ModuleMetaData& meta = pModule->GetMetaData();
			const std::string& moduleName = meta.Name();

			if (moduleName.empty() || meta.Path().empty()) continue;

			task.m_SubTaskName = moduleName;
			PACKAGE_LAG

			std::filesystem::path moduleRoot = meta.Path();
			moduleRoot = moduleRoot.parent_path();
			std::filesystem::path destination = modulesPath;
			destination.append(moduleRoot.filename().string());
			if (!std::filesystem::exists(destination))
				std::filesystem::create_directories(destination);

			std::filesystem::copy(moduleRoot, destination,
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);

			std::filesystem::path editorPath = destination;
			editorPath.append("Editor");
			std::filesystem::remove_all(editorPath);

			std::filesystem::path assetsPath = destination;
			assetsPath.append("Assets");
			std::filesystem::remove_all(assetsPath);

			ModuleSettings& settings = pModule->Settings();
			std::filesystem::path moduleSettingsPath = settingsPath;
			moduleSettingsPath.append(moduleName + ".yaml");
			YAML::Emitter out;
			out << settings.Node();
			std::ofstream outFile(moduleSettingsPath);
			outFile << out.c_str();
			outFile.close();
			++task.m_ProcessedSubTasks;
			task.m_SubTaskName = "";
		}

		/* Copy runtime and application */
		task.m_SubTaskName = "GloryRuntime.dll";
		PACKAGE_LAG
		std::filesystem::path runtimePath = "./EditorAssets/Runtime";
		runtimePath.append("GloryRuntime.dll");
		std::filesystem::copy(runtimePath, packageRoot, std::filesystem::copy_options::overwrite_existing);
		++task.m_ProcessedSubTasks;
		task.m_SubTaskName = "GloryRuntimeApplication.dll";
		PACKAGE_LAG
		runtimePath.replace_filename("GloryRuntimeApplication.dll");
		std::filesystem::copy(runtimePath, packageRoot, std::filesystem::copy_options::overwrite_existing);
		++task.m_ProcessedSubTasks;

		task.m_SubTaskName = "Splash screen";
		PACKAGE_LAG
		runtimePath.replace_filename("Splash.bmp");
		std::filesystem::path dataPath = packageRoot;
		dataPath.append("Data");
		std::filesystem::copy(runtimePath, dataPath, std::filesystem::copy_options::overwrite_existing);
		++task.m_ProcessedSubTasks;

		task.m_SubTaskName = "Executable";
		PACKAGE_LAG
		std::filesystem::path packagingCachePath = ProjectSpace::GetOpenProject()->CachePath();
		std::filesystem::path exePath = packagingCachePath;
		exePath.append("Packaging/bin").append(ProjectSpace::GetOpenProject()->Name()).replace_extension(".exe");
		std::filesystem::copy(exePath, packageRoot, std::filesystem::copy_options::overwrite_existing);
		++task.m_ProcessedSubTasks;

		return true;
	}

	bool CompileEXETask(Engine* pEngine, const std::filesystem::path& packageRoot, PackageTaskState& task)
	{
		/* Compile exe */
		task.m_SubTaskName = "Copying source files";
		PACKAGE_LAG
		std::filesystem::path packagingCachePath = ProjectSpace::GetOpenProject()->CachePath();
		std::filesystem::path runtimePath = "./EditorAssets/Runtime/Runtime";
		packagingCachePath.append("Packaging");
		if (!std::filesystem::exists(packagingCachePath))
			std::filesystem::create_directories(packagingCachePath);

		std::filesystem::path appCPPPath = packagingCachePath;
		appCPPPath.append("Application.cpp");

		std::filesystem::path runtimeAPIHeaderPath = appCPPPath;
		runtimeAPIHeaderPath.replace_filename("RuntimeAPI.h");

		if (!std::filesystem::exists(appCPPPath))
		{
			runtimePath.replace_filename("Application.cpp");
			std::filesystem::copy(runtimePath, appCPPPath);
		}

		if (!std::filesystem::exists(runtimeAPIHeaderPath))
		{
			runtimePath.replace_filename("RuntimeAPI.h");
			std::filesystem::copy(runtimePath, runtimeAPIHeaderPath);
		}
		++task.m_ProcessedSubTasks;

		task.m_SubTaskName = "Generating Configuration.h";
		PACKAGE_LAG

		const std::string projectName = ProjectSpace::GetOpenProject()->Name();
		/* Generate Configuration.h */
		std::filesystem::path configHeaderPath = packagingCachePath;
		configHeaderPath.append("Configuration.h");
		std::ofstream configHeaderStream(configHeaderPath);
		configHeaderStream << "#pragma once" << std::endl;
		configHeaderStream << "#include \"RuntimeAPI.h\"" << std::endl << std::endl;
		configHeaderStream << "namespace Config" << std::endl;
		configHeaderStream << "{" << std::endl;
		configHeaderStream << "	constexpr char* AppName = \"" << projectName << "\";" << std::endl;
		configHeaderStream << "}" << std::endl << std::endl << std::endl;
		configHeaderStream << "inline void Exec()" << std::endl;
		configHeaderStream << "{" << std::endl;
		EditorApplication::GetInstance()->OnGenerateConfigExec(configHeaderStream);
		configHeaderStream << "}" << std::endl << std::endl;
		configHeaderStream.close();
		++task.m_ProcessedSubTasks;

		task.m_SubTaskName = "Generating premake5.lua";
		PACKAGE_LAG
		/* Generate premake file */
		std::filesystem::path luaPath = packagingCachePath;
		luaPath.append("premake5.lua");
		std::ofstream luaStream(luaPath);
		luaStream << "workspace \"" << projectName << "\"" << std::endl;
		luaStream << "	platforms { \"x64\" }" << std::endl;
		luaStream << "	configurations { \"Debug\", \"Release\" }" << std::endl;
		luaStream << "	flags { \"MultiProcessorCompile\" }" << std::endl;
		luaStream << "project \"" << projectName << "\"" << std::endl;
		luaStream << "	language \"C++\"" << std::endl;
		luaStream << "	cppdialect \"C++17\"" << std::endl;
		luaStream << "	staticruntime \"Off\"" << std::endl;
		luaStream << "	targetdir (\"bin\")" << std::endl;
		luaStream << "	objdir (\"bin/OBJ\")" << std::endl;
		luaStream << "	files " << std::endl;
		luaStream << "	{" << std::endl;
		luaStream << "		\"" << "*.h" << "\"," << std::endl;
		luaStream << "		\"" << "*.cpp" << "\"" << std::endl;
		luaStream << "	}" << std::endl;
		luaStream << "	filter \"" << "system:windows" << "\"" << std::endl;
		luaStream << "		defines \"" << "_CONSOLE" << "\"" << std::endl;
		luaStream << "		toolset \"v143\"" << std::endl;
		luaStream << "	filter \"" << "platforms:x64" << "\"" << std::endl;
		luaStream << "		architecture \"" << "x64" << "\"" << std::endl;
		luaStream << "	filter \"" << "configurations:Debug" << "\"" << std::endl;
		luaStream << "		kind \"" << "ConsoleApp" << "\"" << std::endl;
		luaStream << "		runtime \"" << "Debug" << "\"" << std::endl;
		luaStream << "		defines \"" << "_DEBUG" << "\"" << std::endl;
		luaStream << "		symbols \"" << "On" << "\"" << std::endl;
		luaStream << "	filter \"" << "configurations:Release" << "\"" << std::endl;
		luaStream << "		kind \"" << "ConsoleApp" << "\"" << std::endl;
		luaStream << "		runtime \"" << "Release" << "\"" << std::endl;
		luaStream << "		defines \"" << "NDEBUG" << "\"" << std::endl;
		luaStream << "		optimize \"" << "On" << "\"" << std::endl;
		luaStream.close();
		++task.m_ProcessedSubTasks;

		task.m_SubTaskName = "Generating project files";
		PACKAGE_LAG
		/* Generate project files */
		std::filesystem::path premakePath = std::filesystem::current_path();
		premakePath.append("premake").append("premake5.exe");
		packagingCachePath.append("premake5.lua");

		std::string cmd = "cd \"" + premakePath.parent_path().string() + "\" && " + "premake5.exe vs2019 --file=\"" + packagingCachePath.string() + "\"";
		system(cmd.c_str());
		packagingCachePath = packagingCachePath.parent_path();
		++task.m_ProcessedSubTasks;

		task.m_SubTaskName = "Compiling executable";
		PACKAGE_LAG

#ifdef _DEBUG
		static const std::string config = "Debug";
#else
		static const std::string config = "Release";
#endif

		cmd = "cd \"" + packagingCachePath.string() + "\" && " + "msbuild /m /p:Configuration=" + config + " /p:Platform=x64 .";
		system(cmd.c_str());

		std::filesystem::path exePath = packagingCachePath;
		exePath.append("bin").append(projectName).replace_extension(".exe");
		if (!std::filesystem::exists(exePath))
		{
			/* Try again by finding an MSBuild on the machine */
			std::filesystem::path msBuildPath;
			if (FindVisualStudio(msBuildPath) && FindMSBuildInVSPath(msBuildPath, msBuildPath))
			{
				std::string cmd = "cd \"" + msBuildPath.parent_path().string() + "\" && " + "msbuild /m /p:Configuration=" + config + " /p:Platform=x64 \"" + packagingCachePath.string() + "\"";
				system(cmd.c_str());
			}
		}

		if (!std::filesystem::exists(exePath))
		{
			pEngine->GetDebug().LogError("Failed to compile application EXE");
			return false;
		}

		std::filesystem::copy(exePath, packageRoot, std::filesystem::copy_options::overwrite_existing);
		++task.m_ProcessedSubTasks;
		return true;
	}

#pragma endregion

	void StartPackage(Engine* pEngine)
	{
		if (AssetCompiler::IsBusy())
		{
			pEngine->GetDebug().LogError("Cannot package while assets are compiling");
			return;
		}

		Cancel = false;
		Canceled = false;
		Failed = false;

		m_CurrentTask = 0;
		PackagingTasks.clear();
		PackagingTaskState.Reset();

		UsedAssets.clear();
		AssetLocations.clear();

		ProjectSpace* pProject = ProjectSpace::GetOpenProject();
		std::filesystem::path packageRoot = pProject->RootPath();
		packageRoot.append("Build");

		std::filesystem::path dataPath = packageRoot;
		dataPath.append("Data");

		if (!std::filesystem::exists(dataPath))
			std::filesystem::create_directories(dataPath);

		PackageSettings* pPackagingSettings = static_cast<PackageSettings*>(ProjectSettings::Get("Packaging"));
		pPackagingSettings->VerifySettings();

		ProjectSettings::CreateCompileTask();
		ScenesToPackage.clear();
		CollectPackagingScenes(ScenesToPackage);

		if (ScenesToPackage.empty())
		{
			pEngine->GetDebug().LogError("Packaging failed, no scenes to package");
			return;
		}
		if (!EntryScene)
		{
			pEngine->GetDebug().LogError("Packaging failed, no entry scene selected");
			return;
		}

		PackageTask assetGroupsTask;
		assetGroupsTask.m_TaskID = "CalculateAssetGroups";
		assetGroupsTask.m_TaskName = "Calculating asset groups";
		assetGroupsTask.m_TotalSubTasks = 1;
		assetGroupsTask.m_Callback = CalculateAssetGroupsTask;
		PackagingTasks.push_back(std::move(assetGroupsTask));

		//PackageTask compileShadersTask;
		//compileShadersTask.m_TaskID = "CompileShaders";
		//compileShadersTask.m_TaskName = "Compiling shaders";
		//compileShadersTask.m_TotalSubTasks = Shaders.size();
		//compileShadersTask.m_Callback = CompileShadersTask;
		//PackagingTasks.push_back(std::move(compileShadersTask));

		PackageTask packageScenesTask;
		packageScenesTask.m_TaskID = "PackageScenes";
		packageScenesTask.m_TaskName = "Packaging scenes";
		packageScenesTask.m_TotalSubTasks = ScenesToPackage.size();
		packageScenesTask.m_Callback = PackageScenesTask;
		PackagingTasks.push_back(std::move(packageScenesTask));

		PackageTask packageAssetsTask;
		packageAssetsTask.m_TaskID = "PackageAssets";
		packageAssetsTask.m_TaskName = "Packaging assets";
		packageAssetsTask.m_TotalSubTasks = ScenesToPackage.size();
		packageAssetsTask.m_Callback = PackageAssetsTask;
		PackagingTasks.push_back(std::move(packageAssetsTask));

		PackageTask packageShadersTask;
		packageShadersTask.m_TaskID = "PackageShaders";
		packageShadersTask.m_TaskName = "Packaging shaders";
		packageShadersTask.m_TotalSubTasks = Pipelines.size();
		packageShadersTask.m_Callback = PackageShadersTask;
		PackagingTasks.push_back(std::move(packageShadersTask));

		PackageTask packageDBTask;
		packageDBTask.m_TaskID = "PackageDB";
		packageDBTask.m_TaskName = "Packaging asset database";
		packageDBTask.m_TotalSubTasks = UsedAssets.size();
		packageDBTask.m_Callback = PackageAssetDatabase;
		PackagingTasks.push_back(std::move(packageDBTask));
		
		PackageTask compileEXETask;
		compileEXETask.m_TaskID = "CompileEXE";
		compileEXETask.m_TaskName = "Compiling executable";
		compileEXETask.m_TotalSubTasks = 5;
		compileEXETask.m_Callback = CompileEXETask;
		PackagingTasks.push_back(std::move(compileEXETask));

		size_t modulesCount = 0;
		for (size_t i = 0; i < pEngine->ModulesCount(); i++)
		{
			Module* pModule = pEngine->GetModule(i);
			const ModuleMetaData& meta = pModule->GetMetaData();
			const std::string& moduleName = meta.Name();
			if (moduleName.empty() || meta.Path().empty()) continue;
			++modulesCount;
		}

		PackageTask copyFilesTask;
		copyFilesTask.m_TaskID = "CopyFiles";
		copyFilesTask.m_TaskName = "Copying files";
		copyFilesTask.m_TotalSubTasks = modulesCount + 4;
		copyFilesTask.m_Callback = CopyFilesTask;
		PackagingTasks.push_back(std::move(copyFilesTask));

		GatherPackageTasksEvents().Dispatch({});

		m_TaskCount = PackagingTasks.size();

		if (PackagingTasks.empty()) return;

		for (size_t i = 0; i < LoadedScenesToPackage.size(); ++i)
		{
			delete LoadedScenesToPackage[i];
		}
		LoadedScenesToPackage.clear();

		/* Preload the to package scenes because scenes must be loaded on the main thread */
		for (size_t i = 0; i < ScenesToPackage.size(); ++i)
		{
			GScene* pScene = EditorSceneManager::OpenSceneInMemory(ScenesToPackage[i]);
			pScene->GetRegistry().DisableCallbacks();
			LoadedScenesToPackage.push_back(pScene);
		}

		static auto pPackagingJob = pEngine->Jobs().Run<bool, Engine*, std::filesystem::path>();
		pPackagingJob->QueueSingleJob(PackageJob, pEngine, packageRoot);

		PackagingStartedEvent().Dispatch({});
	}

	void CancelPackage()
	{
		Cancel = true;
	}

	void AddPackagingTask(PackageTask&& task, const std::string& before)
	{
		if (!before.empty())
		{
			auto itor = std::find_if(PackagingTasks.begin(), PackagingTasks.end(), [&before](const PackageTask& task) {
				return task.m_TaskID == before;
			});
			if (itor != PackagingTasks.end())
			{
				PackagingTasks.insert(itor, std::move(task));
				return;
			}
		}

		PackagingTasks.push_back(std::move(task));
	}

	void AddPackagingTaskAfter(PackageTask&& task, const std::string& after)
	{
		if (!after.empty())
		{
			auto itor = std::find_if(PackagingTasks.begin(), PackagingTasks.end(), [&after](const PackageTask& task) {
				return task.m_TaskID == after;
			});
			if (itor != PackagingTasks.end() && ++itor != PackagingTasks.end())
			{
				PackagingTasks.insert(itor, std::move(task));
				return;
			}
		}

		PackagingTasks.push_back(std::move(task));
	}

	bool PackageState(size_t& currentIndex, size_t& count, std::string_view& name, size_t& subIndex, size_t& subCount, std::string_view& subName)
	{
		currentIndex = m_CurrentTask.load();
		count = m_TaskCount;
		subIndex = PackagingTaskState.m_ProcessedSubTasks.load();
		subCount = PackagingTaskState.m_TotalSubTasks.load();

		if (Failed)
		{
			name = "Failed!";
			subName = "Failed!";
			return true;
		}

		if (Canceled)
			return true;

		if (m_CurrentTask.load() >= PackagingTasks.size())
		{
			name = "Done!";
			subName = "Done!";
			return true;
		}

		const PackageTask& task = PackagingTasks[m_CurrentTask];
		name = task.m_TaskName;
		subName = PackagingTaskState.m_SubTaskName;
		return false;
	}

	bool IsPackagingBusy()
	{
		return Running;
	}

	bool PackageFailed()
	{
		return Failed;
	}

    void PackageScene(GScene* pScene, const std::filesystem::path& path)
    {
        BinaryFileStream file{ path };
        AssetArchive archive{ &file, AssetArchiveFlags::WriteNew };
        archive.Serialize(pScene);
    }

	EmptyDispatcher& GatherPackageTasksEvents()
	{
		static EmptyDispatcher dispatcher;
		return dispatcher;
	}

	EmptyDispatcher& PackagingStartedEvent()
	{
		static EmptyDispatcher dispatcher;
		return dispatcher;
	}
	EmptyDispatcher& PackagingEndedEvent()
	{
		static EmptyDispatcher dispatcher;
		return dispatcher;
	}
}
