/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include <crogine/Config.hpp>
#include <crogine/core/App.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/Director.hpp>
#include <crogine/ecs/Sunlight.hpp>
#include <crogine/ecs/Renderable.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/postprocess/PostProcess.hpp>

#include <functional>

namespace cro
{
    class MessageBus;
    class Renderable;

    /*!
    \brief Encapsulates a single scene.
    The scene class contains everything needed to create a scene graph by encapsulating
    the ECS and providing factory functions for entities and systems. Multple scenes
    can exist at one time, for instance one to draw the game world, and another to draw
    the HUD. Everything is rendered through renderable systems which, in turn, require
    an ECS - therefore every state which wishes to draw something requires at least a scene,
    right down to menus. All systems should be created before attempting to create any
    entities else existing entities will not be processed by new systems.
    */

    class CRO_EXPORT_API Scene final
    {
    public:
        explicit Scene(MessageBus&);

        ~Scene();
        Scene(const Scene&) = delete;
        Scene(const Scene&&) = delete;
        Scene& operator = (const Scene&) = delete;
        Scene& operator = (const Scene&&) = delete;

        /*!
        \brief Executes one simulation step.
        \param dt The time elapsed since the last simulation step
        fixed at 60Hz
        */
        void simulate(float dt);

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
        \brief Adds a Director to the Scene.
        Directors are used to control in game entities and events through
        observed messages and events.
        \see Director
        */
        template <typename T, typename... Args>
        T& addDirector(Args&&... args);

        /*!
        \brief Returns a reference to the the requested Director, if it exists
        */
        template <typename T>
        T& getDirector();

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
        \brief Sets the active Sunlight object.
        \see Sunlight
        */
        void setSunlight(const Sunlight&);

        /*!
        \brief Returns a reference to the active Sunlight object.
        \see Sunlight
        */
        const Sunlight& getSunlight() const;
        Sunlight& getSunlight();

        /*!
        \brief Enables the skybox.
        Enabling this creates the default coloured skybox for the scene.
        Skyboxes may be cubemapped by supplying a cubemap texture. By
        default skyboxes are disabled until enableSkybox() or setCubemap()
        are called.
        \see setCubemap()
        */
        void enableSkybox();

        /*!
        \brief Attempts to load the cubemap at the given path and applies it to the skybox.
        This will automatically enable the skybox if it has not been enabled already.
        */
        void setCubemap(const std::string& path);

        /*!
        \brief Enables or disables tinting the skybox with the sunlight colour.
        This is disabled by default, and requires the skybox to be enabled to do
        anything, else calling this function has not effect on the Scene.
        */
        void setSkyTintEnabled(bool enabled);

        /*!
        \brief Returns a copy of the entity containing the default camera
        */
        Entity getDefaultCamera() const;

        /*!
        \brief Sets the active camera used when rendering.
        This camera will be used for the entire scene. This means that
        a scene should be constructed entirely as a 3D or 2D scene, but not both.
        Rendering overlays such as a UI or menu is usually done by constructing
        a specific scene, which is then rendered after any 3D scenes, using a 2D
        (orthographic) camera. A single scene may also be rendered multiple times
        with different cameras, for example when rendering split screen.
        \returns Entity used as the previous camera for the scene
        */
        Entity setActiveCamera(Entity);

        /*!
        \brief Returns the entity containing the active camera component
        */
        Entity getActiveCamera() const;

        /*!
        \brief Sets the active listener when processing audio.
        Usually this will be on the same entity as the active camera,
        although effects such as remote monitoring can be used by setting
        a different listener.
        \returns Entity containing the component used as the previous active listener.
        */
        Entity setActiveListener(Entity);

        /*!
        \brief Returns the entity containing the active AudioListener component
        */
        Entity getActiveListener() const;

        /*!
        \brief Forwards any events to Directors in the Scene
        */
        void forwardEvent(const Event&);

        /*!
        \brief Forwards messages to the systems in the scene
        */
        void forwardMessage(const Message&);

        /*!
        \brief Draws any renderable systems in this scene, in the order in which they were addeded
        */
        void render();

        /*!
        \brief Returns a pointer to the array of active projection maps, with the count.
        */
        std::pair<const float*, std::size_t> getActiveProjectionMaps() const;

    private:
        MessageBus& m_messageBus;
        Entity::ID m_defaultCamera;
        Entity::ID m_activeCamera;
        Entity::ID m_activeListener;
        Sunlight m_sunlight;

        std::vector<Entity> m_pendingEntities;
        std::vector<Entity> m_destroyedEntities;

        EntityManager m_entityManager;
        SystemManager m_systemManager;

        std::vector<std::unique_ptr<Director>> m_directors;

        std::vector<Renderable*> m_renderables;

        std::array<glm::mat4, 8u> m_projectionMaps;
        std::size_t m_projectionMapCount;
        friend class ProjectionMapSystem;

        RenderTexture m_sceneBuffer;
        std::array<RenderTexture, 2u> m_postBuffers;
        std::vector<std::unique_ptr<PostProcess>> m_postEffects;

        struct Skybox final
        {
            std::uint32_t vbo = 0;
            std::uint32_t viewUniform = 0;
            std::uint32_t projectionUniform = 0;
        }m_skybox;
        Shader m_skyboxShader;

        void defaultRenderPath();
        void postRenderPath();
        std::function<void()> currentRenderPath;

        void destroySkybox();
    };

#include "Scene.inl"
}