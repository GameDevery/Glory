#include "EditorWindowImpl.h"
#include "EditorApplication.h"

#include <Engine.h>
#include <WindowModule.h>


namespace Glory::Editor
{
	EditorWindowImpl::EditorWindowImpl() {}

	EditorWindowImpl::~EditorWindowImpl() {}

	void EditorWindowImpl::Initialize()
	{
		WindowModule* pWindowModule = EditorApplication::GetInstance()->GetEngine()->GetMainModule<WindowModule>();
		m_pMainWindow = pWindowModule->GetMainWindow();
	}

	Window* EditorWindowImpl::GetMainWindow()
	{
		return m_pMainWindow;
	}

	void EditorWindowImpl::OnFileDragAndDrop(std::string_view path)
	{
		EditorApplication::GetInstance()->OnFileDragAndDrop(path);
	}
}