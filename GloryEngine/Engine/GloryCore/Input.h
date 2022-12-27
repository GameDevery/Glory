#pragma once
#include <Reflection.h>
#include "PlayerInput.h"
#include "KeyEnums.h"

REFLECTABLE_ENUM_NS(Glory, InputDeviceType, Keyboard, Mouse, Gamepad)
REFLECTABLE_ENUM_NS(Glory, InputMappingType, Bool, Float)

namespace Glory
{
	struct InputDevice
	{
		InputDevice(const char* name, const InputDeviceType deviceType, size_t deviceID);

		const char* m_Name;
		const InputDeviceType m_DeviceType;
		const size_t m_DeviceID;
		int m_PlayerIndex;
	};

	struct InputMode
	{
		InputMode(const std::string name);
		InputMode(InputMode&& other) noexcept;
		const std::string m_Name;
		std::vector<InputDeviceType> m_DeviceTypes;

		static InputMode None;
	};

	enum class InputState
	{
		KeyUp,
		KeyDown,
		Axis,
	};

	struct InputEvent
	{
		InputDeviceType InputDeviceType;
		size_t SourceDeviceID;
		size_t KeyID;
		InputState State;
		float Value;
		float Delta;
	};

	struct KeyBinding
	{
		KeyBinding(const std::string bindingPath);
		KeyBinding(const KeyBinding& other) noexcept;

		KeyBinding& operator=(const KeyBinding&& other) noexcept;

		std::string m_BindingPath;
		InputDeviceType m_DeviceType;
		size_t m_KeyID;
		bool m_IsAxis;
	};

	struct InputBinding
	{
		InputBinding(const std::string name, const InputMappingType mappingType, const float multiplier, const KeyBinding keybinding);

		const std::string m_Name;
		const InputMappingType m_MappingType;
		const float m_Multiplier;
		KeyBinding m_KeyBinding;

	private:
		//InputBinding(const InputBinding& other) = delete;
		//InputBinding& operator=(const InputBinding&& other) = delete;
	};

	struct InputAction
	{
		InputAction(const std::string, const InputMappingType mappingType);

		const std::string m_Name;
		const InputMappingType m_MappingType;
		std::vector<InputBinding> m_Bindings;

	private:
		//InputAction(const InputAction& other) = delete;
		//InputAction& operator=(const InputAction&& other) = delete;
	};

	struct InputMap
	{
		InputMap(const std::string name);

		const std::string m_Name;
		std::map<std::string, InputAction> m_Actions;

	private:
		//InputMap(const InputMap& other) = delete;
		//InputMap& operator=(const InputMap&& other) = delete;
	};
}