#include "ProjectSettings.h"
#include <filesystem>
#include <fstream>

namespace Glory::Editor
{
	ProjectSettings* ProjectSettings::m_pAllSettings[] = {
		new GeneralSettings(),
		new EngineSettings(),
		new LayerSettings(),
		new InputSettings(),
	};

	void ProjectSettings::Load(ProjectSpace* pProject)
	{
		for (size_t i = 0; i < 4; i++)
		{
			m_pAllSettings[i]->LoadSettings(pProject);
		}
	}

	void ProjectSettings::Save(ProjectSpace* pProject)
	{
		for (size_t i = 0; i < 4; i++)
		{
			m_pAllSettings[i]->SaveSettings(pProject);
		}
	}

	void ProjectSettings::Paint(ProjectSettingsType type)
	{
		m_pAllSettings[size_t(type)]->OnGui();
	}

	void ProjectSettings::OnStartPlay()
	{
		for (size_t i = 0; i < 4; i++)
		{
			m_pAllSettings[i]->OnStartPlay_Impl();
		}
	}

	void ProjectSettings::OnStopPlay()
	{
		for (size_t i = 0; i < 4; i++)
		{
			m_pAllSettings[i]->OnStopPlay_Impl();
		}
	}

	ProjectSettings::ProjectSettings(const char* settingsFile)
		: m_SettingsNode(), m_SettingsFile(settingsFile)
	{
	}

	ProjectSettings::~ProjectSettings()
	{
	}

	void ProjectSettings::LoadSettings(ProjectSpace* pProject)
	{
		std::filesystem::path path = pProject->SettingsPath();
		path.append(m_SettingsFile);
		if (!std::filesystem::exists(path))
		{
			/* TODO: Create default file? */
			return;
		}

		m_SettingsNode = YAML::LoadFile(path.string());
		OnSettingsLoaded();
	}

	void ProjectSettings::SaveSettings(ProjectSpace* pProject)
	{
		std::filesystem::path path = pProject->SettingsPath();
		path.append(m_SettingsFile);

		YAML::Emitter out;
		out << m_SettingsNode;

		std::ofstream fileStream(path);
		fileStream << out.c_str();
		fileStream.close();
	}

	GeneralSettings::GeneralSettings() : ProjectSettings("General.yaml")
	{
	}

	EngineSettings::EngineSettings() : ProjectSettings("Engine.yaml")
	{
	}

	LayerSettings::LayerSettings() : ProjectSettings("Layers.yaml")
	{
	}

	InputSettings::InputSettings() : ProjectSettings("Input.yaml")
	{
	}
}