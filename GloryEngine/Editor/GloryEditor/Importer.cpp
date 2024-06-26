#include "Importer.h"
#include "EditorApplication.h"
#include "EditorAssetDatabase.h"

#include <Debug.h>
#include <sstream>
#include <Resource.h>

namespace Glory::Editor
{
	std::vector<Importer*> Importer::m_pImporters;
	std::vector<Importer*> Importer::m_pOwnedImporters;

	ImportedResource Importer::Import(const std::filesystem::path& path, void* userSettings)
	{
		auto itor = std::find_if(m_pImporters.begin(), m_pImporters.end(), [&](const Importer* pImporter) {
			return pImporter->SupportsExtension(path.extension());
		});

		if (itor == m_pImporters.end())
		{
			std::stringstream str;
			str << "Could not find importer for extension: " << path.extension() << " for file: " << path;
			EditorApplication::GetInstance()->GetEngine()->GetDebug().LogWarning(str.str());
			return nullptr;
		}

		std::stringstream str;
		str << "Found importer " << (*itor)->Name() << " for loading " << path.extension() << " files to import: " << path;
		EditorApplication::GetInstance()->GetEngine()->GetDebug().LogInfo(str.str());
		return (*itor)->Load(path, userSettings);
	}

	Importer* Importer::GetImporter(const std::filesystem::path& path)
	{
		auto itor = std::find_if(m_pImporters.begin(), m_pImporters.end(), [&](const Importer* pImporter) {
			return pImporter->SupportsExtension(path.extension());
		});

		if (itor == m_pImporters.end())
			return nullptr;

		return *itor;
	}

	bool Importer::Export(const std::filesystem::path& path, Resource* pResource)
	{
		auto itor = std::find_if(m_pImporters.begin(), m_pImporters.end(), [&](const Importer* pImporter) {
			return pImporter->SupportsExtension(path.extension());
		});

		if (itor == m_pImporters.end())
		{
			std::stringstream str;
			str << "Could not find importer for extension: " << path.extension() << " for exporting file: " << path;
			EditorApplication::GetInstance()->GetEngine()->GetDebug().LogWarning(str.str());
			return false;
		}

		std::stringstream str;
		str << "Found importer " << (*itor)->Name() << " for exporting " << path.extension() << " files to export: " << path;
		EditorApplication::GetInstance()->GetEngine()->GetDebug().LogInfo(str.str());

		return (*itor)->Save(path, pResource);
	}

	void Importer::Register(Importer* pImporter)
	{
		m_pImporters.push_back(pImporter);
		pImporter->Initialize();
	}

	EditableResource* Importer::CreateEditableResource(const std::filesystem::path& path)
	{
		auto itor = std::find_if(m_pImporters.begin(), m_pImporters.end(), [&](const Importer* pImporter) {
			return pImporter->SupportsExtension(path.extension());
		});

		if (itor == m_pImporters.end())
		{
			std::stringstream str;
			str << "Could not find importer for extension: " << path.extension() << " for file: " << path;
			EditorApplication::GetInstance()->GetEngine()->GetDebug().LogWarning(str.str());
			return nullptr;
		}

		return (*itor)->GetEditableResource(path);
	}

	void Importer::RegisterOwned(Importer* pImporter)
	{
		m_pOwnedImporters.push_back(pImporter);
		m_pImporters.push_back(pImporter);
		pImporter->Initialize();
	}

	void Importer::CleanupAll()
	{
		for (size_t i = 0; i < m_pImporters.size(); ++i)
		{
			m_pImporters[i]->Cleanup();
		}
		m_pImporters.clear();

		for (size_t i = 0; i < m_pOwnedImporters.size(); ++i)
		{
			delete m_pOwnedImporters[i];
		}
		m_pOwnedImporters.clear();
	}
}
