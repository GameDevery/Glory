#pragma once
#include <ImporterTemplate.h>
#include <MaterialData.h>
#include <yaml-cpp/yaml.h>

namespace Glory::Editor
{
	class MaterialImporter : public ImporterTemplate<MaterialData>
	{
	public:
		MaterialImporter();
		virtual ~MaterialImporter();

		std::string_view Name() const override;

	private:
		virtual bool SupportsExtension(const std::filesystem::path& extension) const override;
		virtual MaterialData* LoadResource(const std::filesystem::path& path) const override;
		bool SaveResource(const std::filesystem::path& path, MaterialData* pResource) const override;

		MaterialData* LoadMaterialData(YAML::Node& rootNode) const;

		void SaveMaterialData(MaterialData* pMaterialData, YAML::Emitter& out) const;
		void ReadShaders(YAML::Node& rootNode, MaterialData* pMaterialData) const;
		void ReadPropertyData(YAML::Node& rootNode, MaterialData* pMaterialData) const;
		void WriteShaders(YAML::Emitter& out, MaterialData* pMaterialData) const;
		void WritePropertyData(YAML::Emitter& out, MaterialData* pMaterialData) const;

	private:
		virtual void Initialize() override;
		virtual void Cleanup() override;
	};
}