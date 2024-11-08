#include "JoltPhysicsModule.h"
#include "JoltPhysicsModule.h"

#include <PhysicsComponents.h>
#include <PhysicsSystem.h>
#include <Engine.h>
#include <GScene.h>
#include <SceneManager.h>
#include <Components.h>
#include <glm/gtx/matrix_decompose.hpp>

namespace Glory
{
	std::map<uint32_t, std::pair<Utils::ECS::EntityRegistry*, Utils::ECS::EntityID>> PhysicsSystem::m_BodyOwners;

	PhysicsSystem::~PhysicsSystem()
	{
		OnBodyActivated_Callback = NULL;
		OnBodyDeactivated_Callback = NULL;
		OnContactAdded_Callback = NULL;
		OnContactPersisted_Callback = NULL;
		OnContactRemoved_Callback = NULL;
	}

	void PhysicsSystem::OnStart(Utils::ECS::EntityRegistry* pRegistry, Utils::ECS::EntityID entity, PhysicsBody& pComponent)
	{
		GScene* pScene = pRegistry->GetUserData<GScene*>();
		Engine* pEngine = pScene->Manager()->GetEngine();

		JoltPhysicsModule* pPhysics = pEngine->GetOptionalModule<JoltPhysicsModule>();
		if (!pPhysics)
		{
			pEngine->GetDebug().LogWarning("A PhysicsBody was added to an entity but no PhysocsModule was loaded");
			return;
		}

		SetupBody(pPhysics, pRegistry, entity, pComponent);
	}

	void PhysicsSystem::OnStop(Utils::ECS::EntityRegistry* pRegistry, Utils::ECS::EntityID entity, PhysicsBody& pComponent)
	{
		GScene* pScene = pRegistry->GetUserData<GScene*>();
		Engine* pEngine = pScene->Manager()->GetEngine();

		JoltPhysicsModule* pPhysics = pEngine->GetOptionalModule<JoltPhysicsModule>();
		if (!pPhysics) return;
		if (pComponent.m_BodyID == PhysicsBody::InvalidBodyID) return;
		m_BodyOwners.erase(pComponent.m_BodyID);
		pPhysics->DestroyPhysicsBody(pComponent.m_BodyID);
	}

	void PhysicsSystem::OnValidate(Utils::ECS::EntityRegistry* pRegistry, Utils::ECS::EntityID entity, PhysicsBody& pComponent)
	{
		GScene* pScene = pRegistry->GetUserData<GScene*>();
		Engine* pEngine = pScene->Manager()->GetEngine();

		JoltPhysicsModule* pPhysics = pEngine->GetOptionalModule<JoltPhysicsModule>();
		if (!pPhysics)
		{
			pEngine->GetDebug().LogWarning("A PhysicsBody was added to an entity but no PhysicsModule was loaded");
			return;
		}

		/* If no body was created the game has not started yet */
		if(pComponent.m_BodyID == PhysicsBody::InvalidBodyID) return;

		/* Destroy the body */
		m_BodyOwners.erase(pComponent.m_BodyID);
		pPhysics->DestroyPhysicsBody(pComponent.m_BodyID);

		/* Create new body */
		SetupBody(pPhysics, pRegistry, entity, pComponent);
	}

	void PhysicsSystem::OnUpdate(Utils::ECS::EntityRegistry* pRegistry, Utils::ECS::EntityID entity, PhysicsBody& pComponent)
	{
		GScene* pScene = pRegistry->GetUserData<GScene*>();
		Engine* pEngine = pScene->Manager()->GetEngine();

		JoltPhysicsModule* pPhysics = pEngine->GetOptionalModule<JoltPhysicsModule>();
		if (!pPhysics)
		{
			pEngine->GetDebug().LogWarning("An Entity has a PhysicsBody but no PhysocsModule was loaded");
			return;
		}

		/* An invalid body cannot be updated */
		if (pComponent.m_BodyID == PhysicsBody::InvalidBodyID) return;

		Transform& transform = pRegistry->GetComponent<Transform>(entity);
		pPhysics->PollPhysicsState(pComponent.m_BodyID, &transform.Position, &transform.Rotation);
		transform.Rotation = glm::conjugate(transform.Rotation);

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
				pPhysics->SetBodyObjectLayer(pComponent.m_BodyID, layerIndex);
				pComponent.m_CurrentLayerIndex = layerIndex;
			}
		}
		else
		{
			if (pComponent.m_CurrentLayerIndex != 0)
			{
				pPhysics->SetBodyObjectLayer(pComponent.m_BodyID, 0);
				pComponent.m_CurrentLayerIndex = 0;
			}
		}

		pRegistry->SetEntityDirty(entity);
	}

	void PhysicsSystem::OnBodyActivated(uint32_t bodyID)
	{
		if (m_BodyOwners.find(bodyID) == m_BodyOwners.end()) return;
		const std::pair<Utils::ECS::EntityRegistry*, Utils::ECS::EntityID>& pair = m_BodyOwners.at(bodyID);
		Utils::ECS::EntityRegistry* pRegistry = pair.first;
		Utils::ECS::EntityID entity = pair.second;
		if (!pRegistry->IsValid(entity)) return;
		if (!Instance()->OnBodyActivated_Callback) return;
		Instance()->OnBodyActivated_Callback(pRegistry, entity, bodyID);
	}

	void PhysicsSystem::OnBodyDeactivated(uint32_t bodyID)
	{
		if (m_BodyOwners.find(bodyID) == m_BodyOwners.end()) return;
		const std::pair<Utils::ECS::EntityRegistry*, Utils::ECS::EntityID>& pair = m_BodyOwners.at(bodyID);
		Utils::ECS::EntityRegistry* pRegistry = pair.first;
		Utils::ECS::EntityID entity = pair.second;
		if (!pRegistry->IsValid(entity)) return;
		if (!Instance()->OnBodyDeactivated_Callback) return;
		Instance()->OnBodyDeactivated_Callback(pRegistry, entity, bodyID);
	}

	void PhysicsSystem::OnContactAdded(uint32_t body1ID, uint32_t body2ID)
	{
		if (m_BodyOwners.find(body1ID) == m_BodyOwners.end()) return;
		if (m_BodyOwners.find(body2ID) == m_BodyOwners.end()) return;
		const std::pair<Utils::ECS::EntityRegistry*, Utils::ECS::EntityID>& pair1 = m_BodyOwners.at(body1ID);
		const std::pair<Utils::ECS::EntityRegistry*, Utils::ECS::EntityID>& pair2 = m_BodyOwners.at(body2ID);
		Utils::ECS::EntityRegistry* pRegistry1 = pair1.first;
		Utils::ECS::EntityRegistry* pRegistry2 = pair2.first;
		Utils::ECS::EntityID entity1 = pair1.second;
		Utils::ECS::EntityID entity2 = pair2.second;
		if (!pRegistry1->IsValid(entity1) || !pRegistry2->IsValid(entity2)) return;
		if (!Instance()->OnContactAdded_Callback) return;
		Instance()->OnContactAdded_Callback(pRegistry1, entity1, body1ID, body2ID);
		Instance()->OnContactAdded_Callback(pRegistry2, entity2, body2ID, body1ID);
	}

	void PhysicsSystem::OnContactPersisted(uint32_t body1ID, uint32_t body2ID)
	{
		if (m_BodyOwners.find(body1ID) == m_BodyOwners.end()) return;
		if (m_BodyOwners.find(body2ID) == m_BodyOwners.end()) return;
		const std::pair<Utils::ECS::EntityRegistry*, Utils::ECS::EntityID>& pair1 = m_BodyOwners.at(body1ID);
		const std::pair<Utils::ECS::EntityRegistry*, Utils::ECS::EntityID>& pair2 = m_BodyOwners.at(body2ID);
		Utils::ECS::EntityRegistry* pRegistry1 = pair1.first;
		Utils::ECS::EntityRegistry* pRegistry2 = pair2.first;
		Utils::ECS::EntityID entity1 = pair1.second;
		Utils::ECS::EntityID entity2 = pair2.second;
		if (!pRegistry1->IsValid(entity1) || !pRegistry2->IsValid(entity2)) return;
		if (!Instance()->OnContactPersisted_Callback) return;
		Instance()->OnContactPersisted_Callback(pRegistry1, entity1, body1ID, body2ID);
		Instance()->OnContactPersisted_Callback(pRegistry2, entity2, body2ID, body1ID);
	}

	void PhysicsSystem::OnContactRemoved(uint32_t body1ID, uint32_t body2ID)
	{
		if (m_BodyOwners.find(body1ID) == m_BodyOwners.end()) return;
		if (m_BodyOwners.find(body2ID) == m_BodyOwners.end()) return;
		const std::pair<Utils::ECS::EntityRegistry*, Utils::ECS::EntityID>& pair1 = m_BodyOwners.at(body1ID);
		const std::pair<Utils::ECS::EntityRegistry*, Utils::ECS::EntityID>& pair2 = m_BodyOwners.at(body2ID);
		Utils::ECS::EntityRegistry* pRegistry1 = pair1.first;
		Utils::ECS::EntityRegistry* pRegistry2 = pair2.first;
		Utils::ECS::EntityID entity1 = pair1.second;
		Utils::ECS::EntityID entity2 = pair2.second;
		if (!pRegistry1->IsValid(entity1) || !pRegistry2->IsValid(entity2)) return;
		if (!Instance()->OnContactRemoved_Callback) return;
		Instance()->OnContactRemoved_Callback(pRegistry1, entity1, body1ID, body2ID);
		Instance()->OnContactRemoved_Callback(pRegistry2, entity2, body2ID, body1ID);
	}

	void PhysicsSystem::AddBody(uint32_t bodyID, Utils::ECS::EntityRegistry* pRegistry, Utils::ECS::EntityID entity)
	{
		m_BodyOwners[bodyID] = { pRegistry, entity };
	}

	void PhysicsSystem::RemoveBody(uint32_t bodyID)
	{
		m_BodyOwners.erase(bodyID);
	}

	PhysicsSystem* PhysicsSystem::Instance()
	{
		static PhysicsSystem Inst;
		return &Inst;
	}

	void PhysicsSystem::SetupBody(JoltPhysicsModule* pPhysics, Utils::ECS::EntityRegistry* pRegistry, Utils::ECS::EntityID entity, PhysicsBody& pComponent)
	{
		pComponent.m_BodyID = PhysicsBody::InvalidBodyID;

		const Transform& transform = pRegistry->GetComponent<Transform>(entity);
		const Shape* pShape = pComponent.m_Shape.BaseShapePointer();
		if (pShape->m_ShapeType == ShapeType::None)
		{
			pPhysics->GetEngine()->GetDebug().LogWarning("PhysicsBody does not have a shape!");
			return;
		}

		pComponent.m_CurrentLayerIndex = pRegistry->HasComponent<LayerComponent>(entity) ? pRegistry->GetComponent<LayerComponent>(entity).m_Layer.m_LayerIndex : 0;

		glm::quat rotation;
		glm::vec3 translation;
		glm::vec3 scale;
		glm::vec3 skew;
		glm::vec4 perspective;
		if (!glm::decompose(transform.MatTransform, scale, rotation, translation, skew, perspective)) return;
		pComponent.m_BodyID = pPhysics->CreatePhysicsBody(*pShape, translation, rotation, scale, pComponent.m_BodyType, pComponent.m_CurrentLayerIndex);
		m_BodyOwners[pComponent.m_BodyID] = { pRegistry, entity };
	}
}
