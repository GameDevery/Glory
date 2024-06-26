#pragma once
#include "ImGuiImpl.h"
#include "NewProjectWindow.h"

namespace Glory::EditorLauncher
{
	enum HubMenu : int
	{
		ProjectList,
		EditorList,
		Support,
		About,

		MAX,
	};

	class LauncherHub
	{
	public:
		LauncherHub(ImGuiImpl* pHubWindow);
		virtual ~LauncherHub();

		void Run();

		static ImFont* DefaultFont;
		static ImFont* BoldLargeFont;
		static ImFont* IconFontHuge;

	private:
		void Draw();

		std::string FormatTimestamp(long long timestamp);

		void OnProjectNotFoundGui();

		bool ProjectExists(const std::string& path, const std::string& name);
		std::filesystem::path GetProjectPath(const std::string& path, const std::string& name);

		void DrawLeftPanel();
		void DrawRightPanel();
		void DrawProjectList();
		void DrawInstalledEditorsList();
		void DrawSupport();
		void DrawAbout();
		void DrawPopups();

	private:
		ImGuiImpl* m_pImGuiImpl;

		size_t m_RemovingIndex;

		bool m_OpenErrorPopup;
		bool m_OpenNewProjectPopup;
		bool m_OpenProjectOpenError;
		bool m_OpenMissingEditorError;

		char m_PathText[100];
		char m_ProjectNameText[100] = "New Project";

		std::string m_BrowsingPath;
		std::string m_ProjectFolder;
		std::string m_DefaultProjectsFolder = "%userprofile%\\Documents\\Glorious\\Projects";

		HubMenu m_CurrentMenu = HubMenu::ProjectList;

		NewProjectWindow m_pNewProjectWindow;
	};
}
