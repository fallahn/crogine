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
#include <crogine/core/App.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/PostProcess.hpp>

#include <functional>

namespace cro
{
    class Time;
    class MessageBus;
    class Renderable;

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
        explicit Scene(MessageBus&);

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

        /*|
        \brief Returns the entity with the given ID if it exists
        */
        Entity getEntity(Entity::ID) const;

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
        \brief Returns a reference to the Scene's system of this type, if it exists
        */
        template <typename T>
        T& getSystem();

        /*!
        \brief Adds a post process effect to the scene.
        Any post processes added to the scene are performed on the *entire* output.
        To add post processes to a portion of the scene such as only 3D parts then
        a second scene should be created to draw overlays such as the UI
        */
        template <typename T, typename... Args>
        T& addPostProcess(Args&&... args);

        /*!
        \brief Enables or disables any added post processes added to the scene
        */
        void setPostEnabled(bool);

        /*!
        \brief Returns a copy of the entity containing the default camera
        */
        Entity getDefaultCamera() const;

        /*!
        \brief Forwards messages to the systems in the scene
        */
        void forwardMessage(const Message&);

        /*!
        \brief Draws any renderable systems in this scene, in the order in which they were addeded
        */
        void render();

    private:
        MessageBus& m_messageBus;
        Entity::ID m_defaultCamera;

        std::vector<Entity> m_pendingEntities;
        std::vector<Entity> m_destroyedEntities;

        EntityManager m_entityManager;
        SystemManager m_systemManager;

        std::vector<Renderable*> m_renderables;

        RenderTexture m_sceneBuffer;
        std::vector<std::unique_ptr<PostProcess>> m_postEffects;

        void postRenderPath();
        std::function<void()> currentRenderPath;
    };

    template <typename T, typename... Args>
    T& Scene::addSystem(Args&&... args)
    {
        auto& system = m_systemManager.addSystem<T>(std::forward<Args>(args)...);
        if (std::is_base_of<Renderable, T>::value)
        {
            m_renderables.push_back(dynamic_cast<Renderable*>(&system));
        }
        return system;
    }

    template <typename T>
    T& Scene::getSystem()
    {
        return m_systemManager.getSystem<T>();
    }

    template <typename T, typename... Args>
    T& Scene::addPostProcess(Args&&... args)
    {
        static_assert(std::is_base_of<PostProcess, T>::value, "Must be a post process type");
        auto size = App::getWindow().getSize();
        if (!m_sceneBuffer.available())
        {           
            if (m_sceneBuffer.create(size.x, size.y))
            {
                //set render path
                currentRenderPath = std::bind(&Scene::postRenderPath, this);
            }
            else
            {
                Logger::log("Failed settings scene render buffer - post process is disabled", Logger::Type::Error, Logger::Output::All);
            }
        }
        m_postEffects.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
        m_postEffects.back()->resizeBuffer(size.x, size.y);
        return *dynamic_cast<T*>(m_postEffects.back().get());
    }
}

#endif //CRO_SCENE_HPP_
