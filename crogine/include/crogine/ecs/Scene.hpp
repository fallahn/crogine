/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/postprocess/PostProcess.hpp>

#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

#include <functional>

namespace cro
{
    class MessageBus;
    class Renderable;
    class EnvironmentMap;

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

        /*!
        \brief Constructor
        \param messageBus A reference to the App's active message bus
        \param initialPoolSize How much memory to allocate up front for the Scene's component pool
        This is the number of components in the pool before it automatically resizes. Reserving
        a larger size on construction can prevent interruptions caused by the reallocation
        of resources, however is not strictly necessary. The maximum value is 1024 and Defaults to 128.
        Debug builds will print a message to the console when a pool resizes, which can help decide
        what this initial value will be.
        \param infoFlags A bitwise value made from combining INFO_FLAG values by OR'ing
        them together. \see InfoFlags.hpp
        */
        explicit Scene(MessageBus& messageBus, std::size_t initialPoolSize = 128, std::uint32_t infoFlags = 0);

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
        \brief Creates a new entity in the Scene, and returns a handle to it
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
        \returns A pointer to newly created system (or existing system
        if it has been previously created)
        */
        template <typename T, typename... Args>
        T* addSystem(Args&&... args);


        /*!
        \brief Returns a pointer to the Scene's system of this type, if it exists
        else returns nullptr
        */
        template <typename T>
        T* getSystem();

        template <typename T>
        const T* getSystem() const;


        /*!
        \brief Sets the given system type active or inactive in the scene.
        Inactive systems are moved from the processing list and are ignored
        until set active again. If the system type give doesn't exist then
        this function does nothing.
        */
        template <typename T>
        void setSystemActive(bool active);


        /*!
        \brief Adds a Director to the Scene.
        Directors are used to control in game entities and events through
        observed messages and events.
        \see Director
        */
        template <typename T, typename... Args>
        T* addDirector(Args&&... args);


        /*!
        \brief Returns a pointer to the requested Director, if it exists
        else returns a nullptr
        */
        template <typename T>
        T* getDirector();


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
        \param sun An entity which has at least a Transform component and
        Sunlight component
        \see Sunlight
        */
        void setSunlight(Entity sun);


        /*!
        \brief Returns a copy the active Sunlight entity.
        \see Sunlight
        */
        Entity getSunlight() const;


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


        /*!
        \brief Sets the skybox cubemap to that of the given EnvironmentMap.
        As EnvironmentMaps are only available on desktop platforms this will
        do nothing on mobile. Mobile platforms should use setCubemap(std::string)
        instead. Note that as a reference is store to the map, the map should
        be kept valid for at least as long as a Scene using it.
        */
        void setCubemap(const EnvironmentMap& map);


        /*!
        \brief Returns the texture handle to the active cubemap texture.
        This is useful for setting cubemap properties of materials.
        */
        CubemapID getCubemap() const;


        struct SkyColours final
        {
            cro::Colour bottom;
            cro::Colour middle;
            cro::Colour top;
        };

        /*!
        \brief Sets the colours used in the default skybox.
        If a skybox texture or environment map has been set then this does nothing.
        \param bottom The colour for the bottom half of the skybox
        \param middle The colour for the horizon of the skybox
        \param top The colour for the top half of the skybox
        */
        void setSkyboxColours(Colour bottom, Colour middle, Colour top);
        void setSkyboxColours(SkyColours colours);

        
        /*!
        \brief Returns a reference to a struct containing the current skybox colours
        */
        const SkyColours& getSkyboxColours() const { return m_skybox.colours; }


        /*!
        \brief Sets the strength of the star rendering on the default skybox.
        If a skybox texture or environment map has been set then this does nothing.
        \param amount A value between 0 (no stars) and 1 (full stars)
        */
        void setStarsAmount(float amount);

        /*!
        \brief Returns the current mix value for the stars amount
        \see setStarsAmount()
        */
        float getStarsAmount() const { return m_skybox.starsAmount; }


        /*!
        \brief Sets a global level in the Scene to use when rendering an
        infinite water plane.
        The CameraSystem uses this value to update the reflection pass
        matrices, and on built-in shaders the uniform value u_clipPlane
        is set to this.
        When the active Pass of the camera is set to Reflection or Refraction
        this value is used to clip Model geometry and particle geometry.

        This value can be read an applied to custom Renderer systems and
        shaders for continuity of the effect.

        \param level The water level, in world units, relative to the Y zero axis
        */
        void setWaterLevel(float level) { m_waterLevel = level; }


        /*!
        \brief Returns the current water level
        \see setWasterLevel()
        */
        float getWaterLevel() const { return m_waterLevel; }


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
        using the currently active camera, to the current active RenderTarget
        \param doPost If post process effects have been added to the Scene setting this to false
        will ignore them. Useful for rendering multi-pass effects, for example a reflection buffer
        \see setActiveCamera()
        */
        void render(bool doPost = true);


        /*!
        \brief Draws any renderable systems in this scene, in the order in which they were added
        using the given list of camera entities to the current RenderTarget. Useful for split
        screen views for example
        \param cameras Vector of camera entities to draw the Scene with
        \param doPost If post process effects have been added to the Scene setting this to false
        will ignore them. Useful for rendering multi-pass effects, for example a reflection buffer
        */
        void render(const std::vector<Entity>& cameras, bool doPost = true);


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


        /*!
        \brief Posts a message on the system wide message bus
        */
        template <typename T>
        T* postMessage(Message::ID id);


        /*
        \brief Sets the orientation of the skybox to the given quaternion
        */
        void setSkyboxOrientation(glm::quat);


        /*!
        \brief Returns a reference to the Scene's active message bus
        */
        MessageBus& getMessageBus() { return m_messageBus; }
        const MessageBus& getMessageBus() const { return m_messageBus; }


        /*!
        \brief Returns a unique ID for the current Scene instance
        */
        std::size_t getInstanceID() const { return m_uid; }

    private:
        MessageBus& m_messageBus;
        std::size_t m_uid;

        Entity m_defaultCamera;
        Entity m_activeCamera;
        Entity m_activeListener;
        Entity m_sunlight;

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

        float m_waterLevel;

        RenderTexture m_sceneBuffer;
        std::array<RenderTexture, 2u> m_postBuffers;
        std::vector<std::unique_ptr<PostProcess>> m_postEffects;

        cro::CubemapTexture m_skyboxCubemap;
        struct Skybox final
        {
            std::uint32_t vbo = 0;
            std::uint32_t vao = 0;
            std::uint32_t modelViewUniform = 0;
            std::uint32_t projectionUniform = 0;
            std::uint32_t texture = 0;
            std::int32_t textureUniform = -1;
            std::int32_t skyColourUniform = -1;

            const Shader* activeShader = nullptr;

            glm::mat4 modelMatrix = glm::mat4(1.f);

            SkyColours colours;
            float starsAmount = 0.f;

            void setShader(const Shader& shader)
            {
                activeShader = &shader;

                modelViewUniform = shader.getUniformMap().at("u_modelViewMatrix");
                projectionUniform = shader.getUniformMap().at("u_projectionMatrix");
                textureUniform = shader.getUniformID("u_skybox");
                skyColourUniform = shader.getUniformID("u_skyColour");
            }
        }m_skybox;

        std::uint32_t m_activeSkyboxTexture;

        enum SkyboxType
        {
            Cubemap, Environment, Coloured,
            Count
        };
        std::int32_t m_starsUniform;
        std::array<std::int32_t, 3u> m_skyColourUniforms;
        std::array<Shader, SkyboxType::Count> m_skyboxShaders;
        std::size_t m_shaderIndex;
        void applySkyboxColours();
        void applyStars();

        //we use a pointer here so we can create an array of just one
        //camera without having to create a vector from it
        void defaultRenderPath(const RenderTarget&, const Entity* cameraList, std::size_t cameraCount);
        void postRenderPath(const RenderTarget&, const Entity* cameraList, std::size_t cameraCount);
        std::function<void(const RenderTarget&, const Entity*, std::size_t)> currentRenderPath;

        void destroySkybox();

        void resizeBuffers(glm::uvec2);
    };

#include "Scene.inl"
}