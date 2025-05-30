#include "AudioScene.h"

#include <BinaryStream.h>
#include <MeshData.h>

namespace Glory
{
	AudioScene::AudioScene(): m_SceneID(0)
	{
	}

	AudioScene::AudioScene(UUID sceneID): m_SceneID(sceneID)
	{
	}

	AudioScene::AudioScene(AudioScene&& other) noexcept:
		m_SceneID(other.m_SceneID), m_MeshDatas(std::move(other.m_MeshDatas)),
		m_Meshes(std::move(other.m_Meshes)), m_Materials(std::move(other.m_Materials)),
		m_IsAssetBits(std::move(other.m_IsAssetBits))
	{
	}

	void AudioScene::operator=(AudioScene&& other) noexcept
	{
		m_SceneID = other.m_SceneID;
		m_MeshDatas = std::move(other.m_MeshDatas);
		m_Meshes = std::move(other.m_Meshes);
		m_Materials = std::move(other.m_Materials);
		m_IsAssetBits = std::move(other.m_IsAssetBits);
	}

	void AudioScene::AddMesh(UUID meshID, SoundMaterial&& material)
	{
		const size_t materialIndex = m_Materials.size();
		m_Materials.push_back(std::move(material));
		const size_t index = m_Meshes.size();
		m_Meshes.push_back({ meshID, materialIndex });
		m_IsAssetBits.Reserve(m_Meshes.size());
		m_IsAssetBits.Set(index);
	}

	void AudioScene::AddMesh(MeshData&& meshData, SoundMaterial&& material)
	{
		const size_t materialIndex = m_Materials.size();
		m_Materials.push_back(std::move(material));
		const size_t index = m_Meshes.size();
		m_MeshDatas.push_back(std::move(meshData));
		m_Meshes.push_back({ index, materialIndex });
		m_IsAssetBits.Reserve(m_Meshes.size());
		m_IsAssetBits.UnSet(index);
	}

	void AudioScene::Serialize(BinaryStream& stream) const
	{
		stream.Write(m_SceneID);
		stream.Write(m_MeshDatas.size());
		for (size_t i = 0; i < m_MeshDatas.size(); ++i)
		{
			m_MeshDatas[i].Serialize(stream);
		}

		stream.Write(m_Meshes);
		stream.Write(m_Materials);
		stream.Write(m_IsAssetBits.Data(), m_IsAssetBits.DataSize());
	}

	void AudioScene::Deserialize(BinaryStream& stream)
	{
		stream.Read(m_SceneID);
		size_t size;
		stream.Read(size);
		m_MeshDatas.resize(size);
		for (size_t i = 0; i < m_MeshDatas.size(); ++i)
		{
			m_MeshDatas[i].Deserialize(stream);
		}

		stream.Read(m_Meshes);
		stream.Read(m_Materials);
		m_IsAssetBits.Reserve(m_Meshes.size());
		stream.Read(m_IsAssetBits.Data(), m_IsAssetBits.DataSize());
	}

	UUID AudioScene::SceneID() const
	{
		return m_SceneID;
	}

	size_t AudioScene::MeshCount() const
	{
		return m_Meshes.size();
	}

	const MeshData& AudioScene::Mesh(size_t index) const
	{
		const AudioMesh& mesh = m_Meshes[index];
		if (!m_IsAssetBits.IsSet(index))
		{
			return m_MeshDatas[mesh.m_Mesh.m_Index];
		}
		throw new std::exception("Mesh assets are not yet supported for audio scenes");
	}

	MeshData& AudioScene::Mesh(size_t index)
	{
		const AudioMesh& mesh = m_Meshes[index];
		if (!m_IsAssetBits.IsSet(index))
		{
			return m_MeshDatas[mesh.m_Mesh.m_Index];
		}
		throw new std::exception("Mesh assets are not yet supported for audio scenes");
	}

	const SoundMaterial& AudioScene::Material(size_t index) const
	{
		return m_Materials[m_Meshes[index].m_Material];
	}

	SoundMaterial& AudioScene::Material(size_t index)
	{
		return m_Materials[m_Meshes[index].m_Material];
	}

	AudioSceneData::AudioSceneData()
	{
		APPEND_TYPE(AudioSceneData);
	}

	AudioSceneData::AudioSceneData(AudioScene&& audioScene): m_AudioScene(std::move(audioScene))
	{
		APPEND_TYPE(AudioSceneData);
	}

	void AudioSceneData::Serialize(BinaryStream& container) const
	{
		m_AudioScene.Serialize(container);
	}

	void AudioSceneData::Deserialize(BinaryStream& container)
	{
		m_AudioScene.Deserialize(container);
	}
}
