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
#include <crogine/ecs/Component.hpp>
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
        explicit Scene(MessageBus&, std::size_t initialPoolSize = 128);

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
        \brief Returns the number of active entities in the scene
        */
        std::size_t getEntityCount() const { return m_entityManager.getEntityCount(); }

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
        \brief Disables the skybox.
        Note that this also removes any skybox textures added with setCubemap
        */
        void disableSkybox();

        /*!
        \brief Attempts to load the cubemap at the given path and applies it to the skybox.
        This will automatically enable the skybox if it has not been enabled already.
        \param path Should be a path to a ConfigFile containing the following:
        \begincode
        cubemap <mapname>
        {
            up = "path/to/up.png"
            down = "path/to/udown.png"
            left = "path/to/left.png"
            right = "path/to/right.png"
            front = "path/to/front.png"
            back = "path/to/back.png"
        }
        \endcode
        */
        void setCubemap(const std::string& path);
        void setTestMap(std::uint32_t test) { m_activeSkyboxTexture = test; }
        /*!
        \brief Sets the colours used in the default skybox.
        If a skybox texture has been set then this does nothing.
        */
        void setSkyboxColours(cro::Colour top = cro::Colour(0.82f, 0.98f, 0.99f), cro::Colour bottom = cro::Colour(0.21f, 0.5f, 0.96f));

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
        \brief Draws any renderable systems in this scene, in the order in which they were added
        using the currently active camera
        \param target The target to be rendered to, either the active window or a render texture
        \see setActiveCamera()
        */
        void render(const RenderTarget& target);

        /*!
        \brief Draws any renderable systems in this scene, in the order in which they were added
        using the given list of camera entities. Useful for split screen views for example
        \param target The target to be rendered to, either the active window or a render texture
        \param cameras Vector of camera entities to draw the Scene with
        */
        void render(const RenderTarget& target, const std::vector<Entity>& cameras);

        /*!
        \brief Returns a pointer to the array of active projection maps, with the count.
        */
        std::pair<const float*, std::size_t> getActiveProjectionMaps() const;

        /*!
        \brief Passes the given camera to each Renderable system so that they
        may update the Camera's drawList property.
        This is called automatically by the CameraSystem to update all active
        Camera components in the Scene
        \see Renderable::updateDrawLists()
        \see Camera::drawList
        */
        void updateDrawLists(Entity);

    private:
        MessageBus& m_messageBus;

        Entity m_defaultCamera;
        Entity m_activeCamera;
        Entity m_activeListener;

        Sunlight m_sunlight;

        std::vector<Entity> m_pendingEntities;
        std::vector<Entity> m_destroyedEntities;
        std::vector<Entity> m_destroyedBuffer;

        ComponentManager m_componentManager;
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
            std::uint32_t vao = 0;
            std::uint32_t viewUniform = 0;
            std::uint32_t projectionUniform = 0;
            std::uint32_t texture = 0;
            std::uint32_t textureUniform = 0;
        }m_skybox;
        Shader m_skyboxShader;

        std::uint32_t m_activeSkyboxTexture;

        //we use a pointer here so we can create an array of just one
        //camera without having to create a vector from it
        void defaultRenderPath(const RenderTarget&, const Entity* cameraList, std::size_t cameraCount);
        void postRenderPath(const RenderTarget&, const Entity* cameraList, std::size_t cameraCount);
        std::function<void(const RenderTarget&, const Entity*, std::size_t)> currentRenderPath;

        void destroySkybox();
    };

#include "Scene.inl"
}