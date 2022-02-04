#pragma once
#include <SceneObject.h>
#include "Entity.h"

namespace Glory
{
    class EntitySceneObject : public SceneObject
    {
    public:
        EntitySceneObject(Entity entity);
        EntitySceneObject(Entity entity, const std::string& name);
        EntitySceneObject(Entity entity, const std::string& name, UUID uuid);
        virtual ~EntitySceneObject();

        virtual SceneObject* GetParent() override;
        virtual void SetParent(SceneObject* pParent) override;

        Entity GetEntityHandle();

    private:
        virtual void Initialize() override;

    private:
        Entity m_Entity;
        EntitySceneObject* m_pParent;
    };
}