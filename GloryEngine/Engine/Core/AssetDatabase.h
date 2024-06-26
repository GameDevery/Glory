#pragma once
#include "UUID.h"
#include "AssetLocation.h"
#include "ResourceMeta.h"

#include <map>

namespace Glory
{
	class AssetDatabase
	{
	public:
		virtual ~AssetDatabase();

		bool GetAssetLocation(UUID uuid, AssetLocation& location);
		bool GetResourceMeta(UUID uuid, ResourceMeta& meta);
		UUID GetAssetUUID(const std::string& path);
		bool AssetExists(UUID uuid);
		bool AssetExists(const std::string& path);

		void GetAllAssetsOfType(uint32_t typeHash, std::vector<UUID>& out);
		void GetAllAssetsOfType(uint32_t typeHash, std::vector<std::string>& out);

		std::string GetAssetName(UUID uuid);

		void Clear();

		void SetIDAndName(Resource* pResource, UUID newID, const std::string& name);

		void SetAsset(AssetLocation& assetLocation, const ResourceMeta& meta);

		void Remove(UUID uuid);

		/** @brief Set the root path where assets are located */
		void SetAssetPath(const std::filesystem::path& path);
		/** @brief Set the root path where engine and module settings are located */
		void SetSettingsPath(const std::filesystem::path& path);

		/** @brief Get the root path where assets are located */
		const std::string_view GetAssetPath() const { return m_AssetPath; }
		/** @brief Get the root path where engine and module settings are located */
		const std::string_view GetSettingsPath() const { return m_SettingsPath; };

		/**
		 * @brief Get the ID of a scene by name
		 * @param name Name of the scene to find
		 * @returns ID of the scene or 0 if not found
		 */
		UUID FindSceneID(const std::string name) const;

		struct WriteLock
		{
		public:
			WriteLock(AssetDatabase* pDatabase);
			~WriteLock();

		private:
			static size_t m_LockCounter;
			AssetDatabase* m_pDatabase;
		};

		void SetEntryScene(UUID uuid);
		UUID GetEntryScene() const;

	private:
		void Initialize();
		void Destroy();

	private:
		friend class Engine;
		friend class AssetManager;
		AssetDatabase();

	private:
		bool m_IsWriting;
		bool m_IsReading;
		std::map<UUID, AssetLocation> m_AssetLocations;
		std::map<std::string, UUID> m_SceneNameToID;
		std::map<std::string, UUID> m_PathToUUID;
		std::map<UUID, ResourceMeta> m_Metas;
		std::map<uint32_t, std::vector<UUID>> m_AssetsByType;
		std::string m_AssetPath;
		std::string m_SettingsPath;
		UUID m_EntrySceneID;

		struct ReadLock
		{
		public:
			ReadLock(AssetDatabase* pDatabase);
			~ReadLock();

		private:
			static size_t m_LockCounter;
			AssetDatabase* m_pDatabase;
		};
	};
}
