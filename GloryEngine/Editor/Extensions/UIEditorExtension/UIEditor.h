#pragma once
#include <EditorWindow.h>
#include <AudioData.h>

namespace Glory
{
	class UIDocumentData;
	class UIDocument;
}

namespace Glory::Editor
{
	class UIMainWindow;

	class UIEditor : public EditorWindowTemplate<UIEditor>
	{
	public:
		UIEditor();
		virtual ~UIEditor();

	private:
		virtual void OnGUI() override;
		virtual void Update() override;
		virtual void Draw() override;

		void MenuBar(UIMainWindow* pMainWindow);

	private:
		UIMainWindow* GetMainWindow();
	};
}
