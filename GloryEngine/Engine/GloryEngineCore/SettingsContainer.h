#pragma once
#include <type_traits>
#include <unordered_map>
#include <functional>
#include <filesystem>

/** @brief Generate string name of a property */
#define SETTING_NAME(x) STRINGIZE(x)

namespace Glory
{
	namespace Utils::Reflect
	{
		struct TypeData;
	}

	/** @brief Base class for settings container */
	struct SettingsBase
	{
	public:
		/**
		 * @brief Constructor
		 * @param data Raw non-owning pointer to settings data
		 */
		SettingsBase(void* data):
			m_pSettingsData(data) {}

		/**
		 * @brief Dereference operator overload
		 * @returns Raw pointer to the settings data
		 */
		inline void* operator*()
		{
			return m_pSettingsData;
		}

		/** @brief Get the type of the settings struct attached to this container */
		virtual const Utils::Reflect::TypeData* GetType() const = 0;

		using Callback = std::function<void()>;

		/**
		 * @brief Register a settings change handler
		 * @param prop Name of the property in the form of 'classname::propname' to listen for
		 * @param handler Callback to call when a change is made to this property
		 */
		inline void RegisterChangeHandler(const std::string& prop, Callback handler)
		{
			m_ChangeHandlers.emplace(prop, handler);
		}

		/**
		 * @brief Notify a change to a settings property
		 * @param prop Name of the property in the form of 'classname::propname' that changed
		 */
		inline void NotifyChange(const std::string& prop)
		{
			auto handlerIter = m_ChangeHandlers.find(prop);
			if (handlerIter == m_ChangeHandlers.end()) return;
			handlerIter->second();
		}

		/** @brief Notify all registered change handlers */
		inline void NotifyFullChange()
		{
			for (const auto& [name, callback] : m_ChangeHandlers)
				callback();
		}

		/**
		 * @brief Insert a group name before a settings property, purely for use in an editor
		 * @param prop Name of the property in the form of 'classname::propname'
		 * @param group Name of the group to insert
		 */
		inline void InsertGroupBefore(std::string&& prop, std::string&& group)
		{
			m_Groups.emplace(std::move(prop), std::move(group));
		}

		/**
		 * @brief Get the group that was inserted before a settings property
		 * @param prop Name of the property in the form of 'classname::propname' to get the group from
		 */
		inline std::string_view GetGroup(const std::string& prop) const
		{
			const auto iter = m_Groups.find(prop);
			return iter != m_Groups.end() ? iter->second : std::string_view{};
		}

	private:
		void* m_pSettingsData;
		std::unordered_map<std::string, Callback> m_ChangeHandlers;
		std::unordered_map<std::string, std::string> m_Groups;
	};

	template<typename T>
	concept ReflectableType = requires {
		T();
		T::GetTypeData();
	};

	/**
	 * @brief Templated settings container
	 * @param Data Type of the settings struct/class, must be a reflectable type
	 */
	template<ReflectableType Data>
	struct SettingsContainer final : public SettingsBase
	{
	public:
		/** @brief Constructor */
		SettingsContainer():
			SettingsBase(&m_Data) {}

		/**
		 * @brief Arrow operator overload, gets the underlying data of the settings
		 * @returns A pointer to the settings data
		 */
		Data* operator->()
		{
			return &m_Data;
		}

		/** @overload */
		const Data* operator->() const
		{
			return &m_Data;
		}

		/** @brief Returns the type data for the attached settings struct/class */
		virtual const Utils::Reflect::TypeData* GetType() const override
		{
			return Data::GetTypeData();
		}

	private:
		Data m_Data;
	};
}
