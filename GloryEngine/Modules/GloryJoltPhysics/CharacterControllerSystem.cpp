#include "CharacterControllerSystem.h"
#include "JoltPhysicsModule.h"
#include "JoltCharacterManager.h"
#include "JoltShapeManager.h"
#include "JoltPhysicsModule.h"
#include "PhysicsSystem.h"
#include "Debug.h"

#include <Engine.h>
#include <GScene.h>
#include <SceneManager.h>
#include <PhysicsComponents.h>

#include <Components.h>
#include <Engine.h>
#include <glm/gtx/matrix_decompose.hpp>

namespace Glory
{
	void CharacterControllerSystem::OnStart(Utils::ECS::EntityRegistry* pRegistry, Utils::ECS::EntityID entity, CharacterController& pComponent)
	{
		GScene* pScene = pRegistry->GetUserData<GScene*>();
		Engine* pEngine = pScene->Manager()->GetEngine();

		pComponent.m_BodyID = PhysicsBody::InvalidBodyID;
		JoltPhysicsModule* pPhysics = pEngine->GetOptionalModule<JoltPhysicsModule>();
		JoltCharacterManager* pCharacters = pPhysics->GetCharacterManager();
		JoltShapeManager* pShapes = pPhysics->GetShapeManager();

		const Shape* pShape = pComponent.m_Shape.BaseShapePointer();
		if (pShape->m_ShapeType == ShapeType::None)
		{
			pEngine->GetDebug().LogWarning("CharacterController does not have a shape!");
			return;
		}

		const Transform& transform = pRegistry->GetComponent<Transform>(entity);
		glm::quat rotation;
		glm::vec3 translation;
		glm::vec3 scale;
		glm::vec3 skew;
		glm::vec4 perspective;
		if (!glm::decompose(transform.MatTransform, scale, rotation, translation, skew, perspective)) return;

		const UUID shapeID = pShapes->CreateShape(*pShape);
		const ShapeData* pShapeData = pShapes->GetShape(shapeID);

		pComponent.m_CurrentLayerIndex = pRegistry->HasComponent<LayerComponent>(entity) ? pRegistry->GetComponent<LayerComponent>(entity).m_Layer.m_LayerIndex : 0;
		pComponent.m_ShapeID = shapeID;
		pComponent.m_CharacterID = pCharacters->CreateCharacter(pComponent.m_MaxSlopeAngle, pComponent.m_Mass, pComponent.m_Friction,
			pComponent.m_GravityFactor, pComponent.m_CurrentLayerIndex, translation, rotation, *pShapeData);
		pComponent.m_BodyID = pCharacters->GetBodyID(pComponent.m_CharacterID);
		const UUID entityUUID = pScene->GetEntityUUID(entity);
		pPhysics->SetBodyUserData(pComponent.m_BodyID, entityUUID);
		PhysicsSystem::AddToSceneIDsCache(entityUUID, pScene->GetUUID());
	}

	void CharacterControllerSystem::OnStop(Utils::ECS::EntityRegistry* pRegistry, Utils::ECS::EntityID entity, CharacterController& pComponent)
	{
		if (!pComponent.m_CharacterID) return;

		GScene* pScene = pRegistry->GetUserData<GScene*>();
		Engine* pEngine = pScene->Manager()->GetEngine();

		JoltPhysicsModule* pPhysics = pEngine->GetOptionalModule<JoltPhysicsModule>();
		JoltCharacterManager* pCharacters = pPhysics->GetCharacterManager();

		pCharacters->DestroyCharacter(pComponent.m_CharacterID);
		PhysicsSystem::RemoveFromSceneIDsCache(pScene->GetEntityUUID(entity));
		pComponent.m_CharacterID = 0;
		pComponent.m_BodyID = 0;
		pComponent.m_ShapeID = 0;
	}

	void CharacterControllerSystem::OnValidate(Utils::ECS::EntityRegistry* pRegistry, Utils::ECS::EntityID entity, CharacterController& pComponent)
	{
		GScene* pScene = pRegistry->GetUserData<GScene*>();
		Engine* pEngine = pScene->Manager()->GetEngine();

		JoltPhysicsModule* pPhysics = pEngine->GetOptionalModule<JoltPhysicsModule>();
		if (!pPhysics)
		{
			pEngine->GetDebug().LogWarning("A CharacterController was added to an entity but no PhysicsModule was loaded");
			return;
		}

		JoltCharacterManager* pCharacters = pPhysics->GetCharacterManager();
	}

	void CharacterControllerSystem::OnUpdate(Utils::ECS::EntityRegistry* pRegistry, Utils::ECS::EntityID entity, CharacterController& pComponent)
	{
		if (!pComponent.m_CharacterID) return;

		GScene* pScene = pRegistry->GetUserData<GScene*>();
		Engine* pEngine = pScene->Manager()->GetEngine();

		JoltPhysicsModule* pPhysics = pEngine->GetOptionalModule<JoltPhysicsModule>();

		JoltCharacterManager* pCharacters = pPhysics->GetCharacterManager();
		if (!pCharacters) return;
		
		Transform& transform = pRegistry->GetComponent<Transform>(entity);
		transform.Position = pCharacters->GetPosition(pComponent.m_CharacterID);
		transform.Rotation = pCharacters->GetRotation(pComponent.m_CharacterID);

		Utils::ECS::EntityView* pEntityView = pRegistry->GetEntityView(entity);
		Entity parent = pRegistry->GetUserData<GScene*>()->GetEntityByEntityID(pEntityView->Parent());
		if (parent.IsValid())
		{
			Transform& parentTransform = parent.GetComponent<Transform>();
			const glm::mat4 inverse = glm::inverse(parentTransform.MatTransform);
			glm::vec3 scale;
			glm::quat rotation;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;
			if (!glm::decompose(inverse, scale, rotation, translation, skew, perspective)) return;
			transform.Position = inverse * glm::vec4(transform.Position, 1.0f);
			transform.Rotation = transform.Rotation * glm::conjugate(rotation);
		}

		if (pRegistry->HasComponent<LayerComponent>(entity))
		{
			const uint32_t layerIndex = pRegistry->GetComponent<LayerComponent>(entity).m_Layer.m_LayerIndex;
			if (pComponent.m_CurrentLayerIndex != layerIndex)
			{
				pCharacters->SetLayer(pComponent.m_CharacterID, layerIndex);
				pComponent.m_CurrentLayerIndex = layerIndex;
			}
		}
		else
		{
			if (pComponent.m_CurrentLayerIndex != 0)
			{
				pCharacters->SetLayer(pComponent.m_CharacterID, 0);
				pComponent.m_CurrentLayerIndex = 0;
			}
		}

		pRegistry->SetEntityDirty(entity);
	}
}
