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

#ifndef CRO_SCENE_HPP_
#define CRO_SCENE_HPP_

#include <crogine/Config.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/System.hpp>

namespace cro
{
    class Time;

    /*!
    \brief Encapsulates a single scene.
    The scene class contains everything needed to create a scene graph by encapsulating
    the ECS and providing factory functions for entities and systems. Multple scenes
    can exist at one time, for instance one to draw the game world, and another to draw
    the HUD. Everything is rendered through renderable systems which, in turn, require
    and ECS - therefor every state which wish to draw something requires at least a scene,
    right down to menus. All systems should be created before attempting to create any
    entities else existing entities will not be processed by new systems.
    */

    class CRO_EXPORT_API Scene final
    {
    public:
        Scene();

        ~Scene() = default;
        Scene(const Scene&) = delete;
        Scene(const Scene&&) = delete;
        Scene& operator = (const Scene&) = delete;
        Scene& operator = (const Scene&&) = delete;

        /*!
        \brief Executes one simulations step.
        \param dt The time elapsed since the last simulation step
        */
        void simulate(Time dt);

        /*!
        \brief Creates a new entity in the Scene, and returns a copy of it
        */
        Entity createEntity();

        /*!
        \brief Destroys the given entity and removes it from the scene
        */
        void destroyEntity(Entity);

        /*!
        \brief Creates a new system of the given type.
        All systems need to be fully created before adding entities, else
        entities will not be correctly processed by desired systems.
        \returns Reference to newly created system (or existing system
        if it has been previously created)
        */
        template <typename T, typename... Args>
        T& addSystem(Args&&... args);

        /*!
        \brief Returns a copy of the entity containing the default camera
        */
        Entity getDefaultCamera() const;

    private:

        Entity::ID m_defaultCamera;

        std::vector<Entity> m_pendingEntities;
        std::vector<Entity> m_destroyedEntities;

        EntityManager m_entityManager;
        SystemManager m_systemManager;
    };

    template <typename T, typename... Args>
    T& Scene::addSystem(Args&&... args)
    {
        return m_systemManager.addSystem<T>(std::forward<Args>(args)...);
    }
}

#endif //CRO_SCENE_HPP_