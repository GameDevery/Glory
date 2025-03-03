#pragma once
#include <RendererModule.h>
#include <FileData.h>
#include <glm/glm.hpp>

namespace Glory
{
	class MaterialData;
	class RenderTexture;
	class UIDocumentData;
	class FontData;
	class UIDocument;

	namespace Utils::ECS
	{
		class ComponentTypes;
	}

	struct UIRenderData
	{
		UUID m_DocumentID;
		UUID m_ObjectID;

		UUID m_TargetCamera;
		glm::uvec2 m_Resolution;
	};

	class UIRendererModule : public Module
	{
	public:
		UIRendererModule();
		virtual ~UIRendererModule();

		virtual void CollectReferences(std::vector<UUID>& references) override;

		virtual const std::type_info& GetModuleType() override;

		Utils::ECS::ComponentTypes* GetComponentTypes() { return m_pComponentTypes; }

		void Submit(UIRenderData&& data);

		FontData* GetFont();
		MaterialData* PrepassMaterial();

		GLORY_MODULE_VERSION_H(0,1,0);

	private:
		virtual void Initialize() override;
		virtual void PostInitialize() override;
		virtual void Update() override;
		virtual void Cleanup() override;
		virtual void UIPrepass(CameraRef, const RenderFrame&);
		virtual void UIOverlayPass(CameraRef camera, const RenderFrame&);

		virtual void LoadSettings(ModuleSettings& settings) override;

		UIDocument& GetDocument(const UIRenderData& data, UIDocumentData* pDocument);

	private:
		MaterialData* m_pUIPrepassMaterial = nullptr;
		MaterialData* m_pUIOverlayMaterial = nullptr;

		Utils::ECS::ComponentTypes* m_pComponentTypes = nullptr;

		std::map<UUID, UIDocument> m_Documents;

		std::vector<UIRenderData> m_Frame;
	};
}
