#pragma once
#include "IPlayModeHandler.h"
#include "GloryEditor.h"
#include "Toolbar.h"

#include <IModuleLoopHandler.h>
#include <GScene.h>
#include <vector>
#include <NodeRef.h>

namespace Glory::Editor
{
	class EditorPlayer : public IModuleLoopHandler, IToolChain
	{
	public:
		static GLORY_EDITOR_API void RegisterLoopHandler(IPlayModeHandler* pEditorSceneLoopHandler);

		template<typename T>
		static void UpdateComponentInEditMode()
		{
			UpdateComponentInEditMode(ResourceType::GetHash<T>());
		}
		static GLORY_EDITOR_API void UpdateComponentInEditMode(uint32_t hash);

	private:
		void Start();
		void Stop();
		void TogglePauze();
		void TickFrame();

		void Tick(Engine* pEngine);
		void EndTick();

		virtual bool HandleModuleLoop(Module* pModule) override;
		virtual void DrawToolchain(float& cursor, const ImVec2& maxButtonSize);

	private:
		friend class EditorApplication;
		EditorPlayer();
		virtual ~EditorPlayer();

		Utils::InMemoryYAML m_SerializedScenes;
		size_t m_UndoHistoryIndex;
		UUID m_SelectedObjectBeforeStart;
		bool m_IsPaused;
		bool m_FrameRequested;
		static std::vector<IPlayModeHandler*> m_pSceneLoopHandlers;
	};
}
