#include "RuntimeSceneManager.h"
#include "GloryRuntime.h"

#include <GScene.h>
#include <Engine.h>
#include <Debug.h>
#include <AssetDatabase.h>
#include <filesystem>
#include <BinaryStream.h>
#include <AssetArchive.h>
#include <AssetManager.h>

namespace Glory
{
	RuntimeSceneManager::RuntimeSceneManager(GloryRuntime* pRuntime):
		m_pRuntime(pRuntime), SceneManager(pRuntime->GetEngine()) {}

	RuntimeSceneManager::~RuntimeSceneManager() = default;

	GScene* RuntimeSceneManager::NewScene(const std::string& name, bool additive)
    {
        if (!additive)
            UnloadAllScenes();
        GScene* pNewScene = new GScene(name);
		pNewScene->SetManager(this);
        m_pOpenScenes.push_back(pNewScene);
        return pNewScene;
    }

    void RuntimeSceneManager::OnLoadScene(UUID uuid)
    {
		AssetLocation location;
		if (!m_pEngine->GetAssetDatabase().GetAssetLocation(uuid, location))
		{
			m_pEngine->GetDebug().LogFatalError("Failed to load scene, scene does not exist");
			return;
		}

		std::filesystem::path path = m_pRuntime->GetDataPath();
		path = path.parent_path();
		path.append(location.Path);
		LoadScene(path);
    }

	void RuntimeSceneManager::OnUnloadScene(GScene* pScene)
	{
		/* Everything is already handled in the base class */
	}

	void RuntimeSceneManager::OnUnloadAllScenes()
	{
		/* Everything is already handled in the base class */
	}

	void RuntimeSceneManager::LoadScene(const std::filesystem::path& path)
	{
		/* Load shader pack */
		std::filesystem::path shaderPackPath = path;
		shaderPackPath.replace_extension("gcsp");
		m_pRuntime->LoadShaderPack(shaderPackPath);

		/* Load asset group */
		std::filesystem::path assetGroupPath = path;
		assetGroupPath.replace_extension("gcag");
		m_pRuntime->LoadAssetGroup(assetGroupPath);

		/* Load scene */
		std::filesystem::path scenePath = path;
		scenePath.replace_extension("gcs");
		LoadSceneOnly(scenePath);
	}

	void RuntimeSceneManager::LoadSceneOnly(const std::filesystem::path& path)
	{
		if (!std::filesystem::exists(path))
		{
			m_pEngine->GetDebug().LogFatalError("Missing scene file");
			return;
		}

		BinaryFileStream file{ path, true };
		AssetArchive archive{ &file };
		archive.Deserialize(m_pEngine);

		if (archive.Size() == 0) return;

		Resource* pRoot = archive.Get(m_pEngine, 0);
		GScene* pScene = dynamic_cast<GScene*>(pRoot);
		if (!pScene) return;

		/** There might be extra data in the scene */
		for (size_t i = 1; i < archive.Size(); ++i)
		{
			Resource* pResource = archive.Get(m_pEngine, i);

			bool claimed = false;
			for (size_t j = 0; j < m_pEngine->ModulesCount(); ++j)
			{
				if (!m_pEngine->GetModule(j)->ClaimExtraSceneData(pResource)) continue;
				claimed = true;
				break;
			}

			if (!claimed)
			{
				/* Send it to the asset manager instead */
				m_pEngine->GetAssetManager().AddLoadedResource(pResource);
			}
		}

		pScene->SetManager(m_pEngine->GetSceneManager());

		/* Have to make sure every components add callbacks are called */
		pScene->GetRegistry().InvokeAll(Utils::ECS::InvocationType::OnAdd, NULL);
		m_pOpenScenes.push_back(pScene);
		if (m_pRuntime->IsRunning())
			pScene->Start();
	}
}
