/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef CRO_ENTITY_HPP_
#define CRO_ENTITY_HPP_

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
	namespace Detail
	{
		enum
		{
			MaxComponents = 64,
			IndexBits = 24,
			GenerationBits = 8,
			MinFreeIDs = 1024 //after this generation is incremented and we go back to zero
		};
	}
	
	using ComponentMask = std::bitset<Detail::MaxComponents>;
    class EntityManager;

	/*!
	\brief Entity class - Basically just an ID.
	The ID is generated as a combination of the index in the
	memory pool and the generation - that is the nth time the
	index has been used.
	*/
	class CRO_EXPORT_API Entity final
	{
	public:
		using ID = uint32;
		using Generation = uint8;

		Entity(ID index, Generation generation);

		/*
		\brief Returns the index of this entity
		*/
		ID getIndex() const;
		/*!
		\brief Returns the generation of this entity
		*/
		Generation getGeneration() const;

		/*!
		\brief Marks the entity for destruction
		*/
		void destroy();

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
		\brief Removes the component of this type if it exists
		*/
		template <typename T>
		void removeComponent();

		/*!
		\brief returns true if the component type exists on thie entity
		*/
		template <typename T>
		bool hasComponent() const;

		/*!
		\brief Returns a pointer to the component if it exists, else nullptr
		*/
		template <typename T>
        T* getComponent();

		//TODO add tags of types

        

	private:

		ID m_id;
        EntityManager* m_entityManager;
        friend class EntityManager;
	};

    /*!
    \brief Manages the relationship between an Entity and its components
    */
    class CRO_EXPORT_API EntityManager final
    {
    public:
        EntityManager();

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
        \brief Removes this component type for the given Entity
        */
        template <typename T>
        void removeComponent(Entity);

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
        T* getComponent(Entity);

        /*!
        \brief Returns a reference to the component mask of the given Entity.
        Component masks are used to identify whether an Entity has a particular component
        */
        const ComponentMask& getComponentMask(Entity) const;

    private:

        std::deque<Entity::ID> m_freeIDs;
        std::vector<Entity::Generation> m_generations; // < indexed by entity ID
        std::vector<std::unique_ptr<Detail::Pool>> m_componentPools; // < index is component ID. Pool index is entity ID.
        std::vector<ComponentMask> m_componentMasks;

        template <typename T>
        Detail::ComponentPool<T>& getPool();
    };

#include "Entity.inl"
#include "EntityManager.inl"
}


#endif //CRO_ENTITY_HPP_