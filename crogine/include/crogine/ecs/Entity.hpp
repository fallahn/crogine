/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
http://trederia.blogspot.com

crogine - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#pragma once

#include <crogine/detail/Detail.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/Config.hpp>

#include <crogine/ecs/ComponentPool.hpp>
#include <crogine/ecs/Component.hpp>

#include <bitset>
#include <vector>
#include <deque>
#include <memory>

namespace cro
{  
    using ComponentMask = std::bitset<Detail::MaxComponents>;
    class EntityManager;

    /*!
    \brief Entity class - Basically just an ID.
    The ID is generated as a combination of the index in the
    memory pool and the generation - that is the nth time the
    index has been used.

    The entity class is a handle which refers to the underlying
    data owned by the Scene which created it. As such they may
    be treated similarly to pointers - they are lightweight and
    easy to copy around, although they may become invalid should
    the entity to which they refer is destroyed. The utility
    functions isValid() and destroyed() can be used to determine
    the current status and an Entity handle.
    */
    class CRO_EXPORT_API Entity final
    {
    public:
        using ID = std::uint32_t;
        using Generation = std::uint8_t;

        Entity();

        /*
        \brief Returns the index of this entity
        */
        ID getIndex() const;
        /*!
        \brief Returns the generation of this entity
        */
        Generation getGeneration() const;

        /*!
        \brief Returns true if the entity is marked for destruction
        */
        bool destroyed() const;

        /*!
        \brief Adds a copy of the given instance of a component to
        the entity
        */
        template <typename T>
        void addComponent(const T&);

        /*
        \brief Constructs a component from the given parameters,
        adds it to the entity and returns a reference to it
        */
        template <typename T, typename... Args>
        T& addComponent(Args&&...);

        /*!
        \brief returns true if the component type exists on the entity
        */
        template <typename T>
        bool hasComponent() const;

        /*!
        \brief Returns a reference to the component if it exists
        */
        template <typename T>
        T& getComponent();

        template <typename T>
        const T& getComponent() const;

        /*!
        \brief Returns a reference to the CompnentMask associated with this entity
        */
        const ComponentMask& getComponentMask() const;

        /*!
        \brief Returns true if this entity currently belongs to a scene
        */
        bool isValid() const;

        /*!
        \brief Applies the given label to this entity
        \param label String containing the label to apply
        */
        void setLabel(const std::string& label);

        /*!
        \brief Returns the current label string of this entity
        */
        const std::string& getLabel() const;

        bool operator == (Entity r)
        {
            return getIndex() == r.getIndex();
        }
        bool operator != (Entity r)
        {
            return getIndex() != r.getIndex();
        }
        bool operator < (Entity r)
        {
            return getIndex() < r.getIndex();
        }
        bool operator <= (Entity r)
        {
            return getIndex() <= r.getIndex();
        }
        bool operator > (Entity r)
        {
            return getIndex() > r.getIndex();
        }
        bool operator >= (Entity r)
        {
            return getIndex() >= r.getIndex();
        }

        friend bool operator < (const Entity& l, const Entity& r)
        {
            return (l.getIndex() < r.getIndex());
        }

        friend bool operator == (const Entity& l, const Entity& r)
        {
            return (l.getIndex() == r.getIndex());
        }
    private:

        explicit Entity(ID index, Generation generation);

        ID m_id;
        EntityManager* m_entityManager;
        friend class EntityManager;
        friend class Scene;
    };

    class MessageBus;
    /*!
    \brief Manages the relationship between an Entity and its components
    */
    class CRO_EXPORT_API EntityManager final
    {
    public:
        EntityManager(MessageBus&, ComponentManager&, std::size_t = 128);

        ~EntityManager() = default;
        EntityManager(const EntityManager&) = delete;
        EntityManager(const EntityManager&&) = delete;
        EntityManager& operator = (const EntityManager&) = delete;
        EntityManager& operator = (const EntityManager&&) = delete;

        /*!
        \brief Creates a new Entity
        */
        Entity createEntity();

        /*!
        \brief Destroys the given Entity
        */
        void destroyEntity(Entity);

        /*!
        \brief Returns true if the entity is destroyed or marked for destruction
        */
        bool entityDestroyed(Entity) const;

        /*!
        \brief Returns true if the given entity is associated with a valid manager
        */
        bool entityValid(Entity) const;

        /*!
        \brief Returns the entity at the given index if it exists TODO what if it doesn't?
        */
        Entity getEntity(Entity::ID) const;

        /*!
        \brief Adds a copy of the given component to the given Entity
        */
        template <typename T>
        void addComponent(Entity, T);

        /*!
        \brief Constructs a component on the given entity using the given params
        */
        template <typename T, typename... Args>
        T& addComponent(Entity, Args&&... args);

        /*!
        \brief Returns true if the given Entity has a component of this type
        */
        template <typename T>
        bool hasComponent(Entity) const;

        /*!
        \brief Returns a pointer to the component of this type if it exists
        on the given Entity else returns nullptr
        */
        template <typename T>
        T& getComponent(Entity);

        /*!
        \brief Returns a reference to the component mask of the given Entity.
        Component masks are used to identify whether an Entity has a particular component
        */
        const ComponentMask& getComponentMask(Entity) const;

        /*!
        \brief Returns true if this manager owns the given entity
        */
        bool owns(Entity) const;

        /*!
        \brief Sets the given label on the given entity
        */
        void setLabel(Entity entity, const std::string& label);

        /*!
        \brief Returns the current label of the given entity
        */
        const std::string& getLabel(Entity entity) const;

        /*!
        \brief Returns the number of currently active entities
        */
        std::size_t getEntityCount() const { return m_entityCount; }

        /*!
        \brief Marks this entity for destruction
        This doesn't actually destroy an entity, rather it is used by a Scene
        to mark an entity as pending destruction.
        */
        void markDestroyed(Entity entity);


    private:
        MessageBus& m_messageBus;
        std::size_t m_initialPoolSize;
        std::deque<Entity::ID> m_freeIDs;
        std::vector<Entity::Generation> m_generations; // < indexed by entity ID
        std::vector<std::unique_ptr<Detail::Pool>> m_componentPools; // < index is component ID. Pool index is entity ID.
        std::vector<ComponentMask> m_componentMasks;

        std::size_t m_entityCount;
        std::vector<std::string> m_labels;
        std::vector<bool> m_destructionFlags;

        ComponentManager& m_componentManager;

        template <typename T>
        Detail::ComponentPool<T>& getPool();
    };

#include "Entity.inl"
#include "EntityManager.inl"
}