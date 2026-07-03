#pragma once
#include "engine_visibility.h"

namespace Glory::Utils
{
	class BinaryStream;
	namespace Reflect
	{
		struct FieldData;
		struct TypeData;
	}

	/**
	 * @brief Serialize data to binary
	 * @param container Container to write to
	 * @param pTypeData @ref Utils::Reflect::TypeData of the data to serialize
	 * @param data Raw pointer to the root data
	 */
	GLORY_ENGINE_API void SerializeData(BinaryStream& container, const Utils::Reflect::TypeData* pTypeData, void* data);

	/**
	 * @overload
	 * @param container Container to write to
	 * @param pFieldData @ref Utils::Reflect::FieldData of the data to serialize
	 * @param data Raw pointer to the fields data
	 */
	GLORY_ENGINE_API void SerializeData(BinaryStream& container, const Utils::Reflect::FieldData* pFieldData, void* data);

	/**
	 * @brief Deserialize data from binary
	 * @param container Container to read from
	 * @param pFieldData @ref Utils::Reflect::TypeData of the data to deserialize
	 * @param data Raw pointer to the root data
	 */
	GLORY_ENGINE_API void DeserializeData(BinaryStream& container, const Utils::Reflect::TypeData* pTypeData, void* data);

	/**
	 * @overload
	 * @param container Container to read from
	 * @param pFieldData @ref Utils::Reflect::FieldData of the data to deserialize
	 * @param data Raw pointer to the fields data
	 */
	GLORY_ENGINE_API void DeserializeData(BinaryStream& container, const Utils::Reflect::FieldData* pFieldData, void* data);
}