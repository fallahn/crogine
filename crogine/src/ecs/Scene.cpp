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

#include "../detail/GLCheck.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/AudioListener.hpp>
#include <crogine/ecs/Renderable.hpp>

#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

namespace
{
    //same order as GL_TEXTURE_CUBE_MAP_XXXX_YYYY
    enum CubemapDirection
    {
        Left, Right, Up, Down, Front, Back, Count
    };

    const std::string skyboxVertex = 
        R"(
        uniform mat4 u_projectionMatrix;
        uniform mat4 u_viewMatrix;        

        ATTRIBUTE vec3 a_position;

        VARYING_OUT vec3 v_texCoords;

        void main()
        {
            v_texCoords = a_position;
            vec4 pos = u_projectionMatrix * u_viewMatrix * vec4(a_position, 1.0);
            gl_Position = pos.xyww;
        })";
    const std::string skyboxFrag = 
        R"(
        OUTPUT

        uniform LOW vec3 u_darkColour;
        uniform LOW vec3 u_lightColour;

        uniform LOW vec4 u_skyColour;

        VARYING_IN vec3 v_texCoords;

        //const LOW vec3 lightColour = vec3(0.82, 0.98, 0.99);
        //const LOW vec3 darkColour = vec3(0.3, 0.28, 0.21);

        const LOW vec3 darkColour = vec3(0.82, 0.98, 0.99);
        const LOW vec3 lightColour = vec3(0.21, 0.5, 0.96);

        void main()
        {
            float dist = normalize(v_texCoords).y; /*v_texCoords.y + 0.5*/

            vec3 colour = mix(u_darkColour, u_lightColour, smoothstep(0.04, 0.88, dist));
            FRAG_OUT = vec4(colour * u_skyColour.rgb, 1.0);
        })";
    const std::string skyboxFragTextured =
        R"(
        OUTPUT

        uniform samplerCube u_skybox;
        uniform vec4 u_skyColour;

        VARYING_IN vec3 v_texCoords;

        void main()
        {
            vec3 texCoords = v_texCoords;
            //texCoords.y = 1.0 - texCoords.y;
            vec3 colour = TEXTURE_CUBE(u_skybox, texCoords).rgb;
#if defined(GAMMA_CORRECT)
            colour = colour / (colour + vec3(1.0));
            colour = pow(colour, vec3(1.0/2.2));
#endif
            FRAG_OUT = vec4(colour * u_skyColour.rgb, 1.0);
        })";

    const float DefaultFOV = 35.f * Util::Const::degToRad;

    void updateView(cro::Camera& camera)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        size.y = ((size.x / 16.f) * 9.f) / size.y;
        size.x = 1.f;

        camera.projectionMatrix = glm::perspective(DefaultFOV, 16.f / 9.f, 0.1f, 280.f);
        camera.viewport.bottom = (1.f - size.y) / 2.f;
        camera.viewport.height = size.y;
    }
}

Scene::Scene(MessageBus& mb, std::size_t initialPoolSize)
    : m_messageBus          (mb),
    m_entityManager         (mb, m_componentManager, initialPoolSize),
    m_systemManager         (*this, m_componentManager),
    m_projectionMapCount    (0),
    m_activeSkyboxTexture   (0),
    m_shaderIndex           (0)
{
    auto defaultCamera = createEntity();
    defaultCamera.addComponent<Transform>();
    defaultCamera.addComponent<Camera>().resizeCallback = std::bind(&updateView, std::placeholders::_1);
    defaultCamera.addComponent<AudioListener>();
    updateView(defaultCamera.getComponent<Camera>());

    m_defaultCamera = defaultCamera;
    m_activeCamera = m_defaultCamera;
    m_activeListener = m_defaultCamera;

    m_sunlight = createEntity();
    m_sunlight.addComponent<Transform>();
    m_sunlight.addComponent<Sunlight>();

    using namespace std::placeholders;
    currentRenderPath = std::bind(&Scene::defaultRenderPath, this, _1, _2, _3);
}

Scene::~Scene()
{
    destroySkybox();
}

//public
void Scene::simulate(float dt)
{
    //update the sun entity to make sure the direction is correctly rotated
    auto& sun = m_sunlight.getComponent<Sunlight>();
    sun.m_directionRotated = glm::quat_cast(m_sunlight.getComponent<Transform>().getWorldTransform()) * sun.m_direction;

    //update directors first as they'll be working on data from the last frame
    for (auto& d : m_directors)
    {
        d->process(dt);
    }

    for (const auto& entity : m_pendingEntities)
    {
        m_systemManager.addToSystems(entity);
    }
    m_pendingEntities.clear();

    /*
    Destroyed ents are double buffered to any calls
    to destroyEntity which further destroy more entities
    don't affect the entity vector mid iteration
    */
    m_destroyedEntities.swap(m_destroyedBuffer);
    for (const auto& entity : m_destroyedEntities)
    {
        m_systemManager.removeFromSystems(entity);
        m_entityManager.destroyEntity(entity);
    }
    m_destroyedEntities.clear();

    m_systemManager.process(dt);
    for (auto& p : m_postEffects)
    {
        p->process(dt);
    }
}

Entity Scene::createEntity()
{
    m_pendingEntities.push_back(m_entityManager.createEntity());
    return m_pendingEntities.back();
}

void Scene::destroyEntity(Entity entity)
{
    m_destroyedBuffer.push_back(entity);
    entity.m_destroyed = true;
}

Entity Scene::getEntity(Entity::ID id) const
{
    return m_entityManager.getEntity(id);
}

void Scene::setPostEnabled(bool enabled)
{
    using namespace std::placeholders;

    if (enabled && !m_postEffects.empty())
    {
        currentRenderPath = std::bind(&Scene::postRenderPath, this, _1, _2, _3);
        auto size = App::getWindow().getSize();
        m_sceneBuffer.create(size.x, size.y, true);
        for (auto& p : m_postEffects)
        {
            p->resizeBuffer(size.x, size.y);
        }
    }
    else
    {       
        currentRenderPath = std::bind(&Scene::defaultRenderPath, this, _1, _2, _3);
    }
}

void Scene::setSunlight(Entity sunlight)
{
    CRO_ASSERT(sunlight.hasComponent<Transform>(), "Must have a transform component");
    m_sunlight = sunlight;
}

Entity Scene::getSunlight() const
{
    return m_sunlight;
}

void Scene::enableSkybox()
{
    if (!m_skybox.vbo)
    {
        if (m_skyboxShaders[SkyboxType::Coloured].getGLHandle() ||
            m_skyboxShaders[SkyboxType::Coloured].loadFromString(skyboxVertex, skyboxFrag))
        {
            //only using positions
            std::array<float, 108> verts = {
                -0.5f,  0.5f, -0.5f,
                -0.5f, -0.5f, -0.5f,
                 0.5f, -0.5f, -0.5f,
                 0.5f, -0.5f, -0.5f,
                 0.5f,  0.5f, -0.5f,
                -0.5f,  0.5f, -0.5f,

                -0.5f, -0.5f,  0.5f,
                -0.5f, -0.5f, -0.5f,
                -0.5f,  0.5f, -0.5f,
                -0.5f,  0.5f, -0.5f,
                -0.5f,  0.5f,  0.5f,
                -0.5f, -0.5f,  0.5f,

                 0.5f, -0.5f, -0.5f,
                 0.5f, -0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f, -0.5f,
                 0.5f, -0.5f, -0.5f,

                -0.5f, -0.5f,  0.5f,
                -0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                 0.5f, -0.5f,  0.5f,
                -0.5f, -0.5f,  0.5f,

                -0.5f,  0.5f, -0.5f,
                 0.5f,  0.5f, -0.5f,
                 0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                -0.5f,  0.5f,  0.5f,
                -0.5f,  0.5f, -0.5f,

                -0.5f, -0.5f, -0.5f,
                -0.5f, -0.5f,  0.5f,
                 0.5f, -0.5f, -0.5f,
                 0.5f, -0.5f, -0.5f,
                -0.5f, -0.5f,  0.5f,
                 0.5f, -0.5f,  0.5f
            };

#ifdef PLATFORM_DESKTOP
            glCheck(glGenVertexArrays(1, &m_skybox.vao));
            glCheck(glBindVertexArray(m_skybox.vao));
#endif
            glCheck(glGenBuffers(1, &m_skybox.vbo));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_skybox.vbo));
            glCheck(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW));

#ifdef PLATFORM_DESKTOP
            const auto& attribs = m_skyboxShaders[SkyboxType::Coloured].getAttribMap();
            glCheck(glEnableVertexAttribArray(attribs[0]));
            glCheck(glVertexAttribPointer(attribs[0], 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(3 * sizeof(float)), reinterpret_cast<void*>(static_cast<intptr_t>(0))));
            glCheck(glEnableVertexAttribArray(0));

            glCheck(glBindVertexArray(0));
#endif
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            m_skybox.setShader(m_skyboxShaders[Coloured]);
            setSkyboxColours();

            m_shaderIndex = SkyboxType::Coloured;
        }
        else
        {
            Logger::log("Failed to create skybox", cro::Logger::Type::Error);
        }
    }
}

void Scene::disableSkybox()
{
    destroySkybox();
}

void Scene::setCubemap(const std::string& path)
{
    enableSkybox();

    //open the file, check it's valid
    cro::ConfigFile cfg;
    if (!cfg.loadFromFile(path))
    {
        cro::Logger::log("Failed to open cubemap " + path, cro::Logger::Type::Error);
        return;
    }

    std::array<std::string, CubemapDirection::Count> paths;
    const auto& properties = cfg.getProperties();
    for (const auto& prop : properties)
    {
        auto name = prop.getName();
        if (name == "up")
        {
            paths[CubemapDirection::Up] = prop.getValue<std::string>();
        }
        else if (name == "down")
        {
            paths[CubemapDirection::Down] = prop.getValue<std::string>();
        }
        else if (name == "left")
        {
            paths[CubemapDirection::Left] = prop.getValue<std::string>();
        }
        else if (name == "right")
        {
            paths[CubemapDirection::Right] = prop.getValue<std::string>();
        }
        else if (name == "front")
        {
            paths[CubemapDirection::Front] = prop.getValue<std::string>();
        }
        else if (name == "back")
        {
            paths[CubemapDirection::Back] = prop.getValue<std::string>();
        }
    }

    //create shader if it doesn't exist
    if (m_skyboxShaders[SkyboxType::Cubemap].getGLHandle() == 0)
    {
        if (!m_skyboxShaders[SkyboxType::Cubemap].loadFromString(skyboxVertex, skyboxFragTextured))
        {
            LogE << "Failed creatng cubemap shader" << std::endl;
            return;
        }
    }

    m_skybox.setShader(m_skyboxShaders[Cubemap]);
    m_shaderIndex = SkyboxType::Cubemap;

    if (m_skybox.texture == 0)
    {
        glCheck(glGenTextures(1, &m_skybox.texture));
    }

    //load textures, filling in fallback where needed
    cro::Image fallback;
    fallback.create(2, 2, cro::Colour::Magenta(), cro::ImageFormat::RGB);

    cro::Image side(true);

    glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_skybox.texture));

    cro::Image* currImage = &fallback;
    GLenum format = GL_RGB;
    for (auto i = 0u; i < 6u; i++)
    {
        if (side.loadFromFile(paths[i]))
        {
            currImage = &side;
            if (currImage->getFormat() == cro::ImageFormat::RGB)
            {
                format = GL_RGB;
            }
            else if (currImage->getFormat() == cro::ImageFormat::RGBA)
            {
                 format = GL_RGBA;
            }
            else
            {
                currImage = &fallback;
                format = GL_RGB;
            }
        }
        else
        {
            currImage = &fallback;
            format = GL_RGB;
        }

        auto size = currImage->getSize();
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, size.x, size.y, 0, format, GL_UNSIGNED_BYTE, currImage->getPixelData());
    }
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));

    m_activeSkyboxTexture = m_skybox.texture;
}

void Scene::setCubemap(const EnvironmentMap& map)
{
    enableSkybox();

    if (m_skyboxShaders[SkyboxType::Environment].getGLHandle() == 0)
    {
        m_skyboxShaders[SkyboxType::Environment].loadFromString(skyboxVertex, skyboxFragTextured, "#define GAMMA_CORRECT\n");
    }

    m_activeSkyboxTexture = map.m_textures[0];
    m_skybox.setShader(m_skyboxShaders[Environment]);
    m_shaderIndex = SkyboxType::Environment;
}

CubemapID Scene::getCubemap() const
{
    return CubemapID(m_activeSkyboxTexture);
}

void Scene::setSkyboxColours(cro::Colour dark, cro::Colour light)
{
    if (m_skyboxShaders[SkyboxType::Coloured].getGLHandle())
    {
        glCheck(glUseProgram(m_skyboxShaders[SkyboxType::Coloured].getGLHandle()));
        glCheck(glUniform3f(m_skyboxShaders[SkyboxType::Coloured].getUniformMap().at("u_darkColour"), dark.getRed(), dark.getGreen(), dark.getBlue()));
        glCheck(glUniform3f(m_skyboxShaders[SkyboxType::Coloured].getUniformMap().at("u_lightColour"), light.getRed(), light.getGreen(), light.getBlue()));
        glCheck(glUseProgram(0));
    }
}

Entity Scene::getDefaultCamera() const
{
    return m_defaultCamera;
}

Entity Scene::setActiveCamera(Entity entity)
{
    CRO_ASSERT(entity.hasComponent<Transform>() && entity.hasComponent<Camera>(), "Entity requires at least a transform and a camera component");
    CRO_ASSERT(m_entityManager.owns(entity), "This entity must belong to this scene!");

    auto oldCam = m_activeCamera;
    m_activeCamera = entity;

    auto& cam = entity.getComponent<cro::Camera>();
    if (cam.resizeCallback)
    {
        cam.resizeCallback(cam);
    }

    //assume if we're using any kind of custom camera
    //we're not going to use the default one any more
    //(this prevents draw lists being unnecessarily updated
    //on the default camera if it is not used)
    m_defaultCamera.getComponent<Camera>().active = (entity == m_defaultCamera);

    return oldCam;
}

Entity Scene::setActiveListener(Entity entity)
{
    CRO_ASSERT(entity.hasComponent<Transform>() && entity.hasComponent<AudioListener>(), "Entity requires at least a transform and a camera component");
    CRO_ASSERT(m_entityManager.owns(entity), "This entity must belong to this scene!");
    auto oldListener = m_activeListener;
    m_activeListener = entity;
    return oldListener;
}

Entity Scene::getActiveListener() const
{
    return m_activeListener;
}

Entity Scene::getActiveCamera() const
{
    return m_activeCamera;
}

void Scene::forwardEvent(const Event& evt)
{
    for (auto& d : m_directors)
    {
        d->handleEvent(evt);
    }
}

void Scene::forwardMessage(const Message& msg)
{
    m_systemManager.forwardMessage(msg);
    for (auto& d : m_directors)
    {
        d->handleMessage(msg);
    }

    if (msg.id == Message::WindowMessage)
    {
        const auto& data = msg.getData<Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            //resizes the post effect buffer if it is in use
            if (m_sceneBuffer.available())
            {
                m_sceneBuffer.create(data.data0, data.data1);
                for (auto& p : m_postEffects) p->resizeBuffer(data.data0, data.data1);
            }

            if (m_postBuffers[0].available())
            {
                m_postBuffers[0].create(data.data0, data.data1, false);
            }

            if (m_postBuffers[1].available())
            {
                m_postBuffers[1].create(data.data0, data.data1, false);
            }
        }
    }
}

void Scene::render(const RenderTarget& rt)
{
    currentRenderPath(rt, &m_activeCamera, 1);
}

void Scene::render(const RenderTarget& rt, const std::vector<Entity>& cameras)
{
    currentRenderPath(rt, cameras.data(), cameras.size());
}

std::pair<const float*, std::size_t> Scene::getActiveProjectionMaps() const
{
    return std::pair<const float*, std::size_t>(&m_projectionMaps[0][0].x, m_projectionMapCount);
}

void Scene::updateDrawLists(Entity camera)
{
    CRO_ASSERT(camera.hasComponent<Camera>(), "Camera component missing!");
    CRO_ASSERT(m_entityManager.owns(camera), "This entity must belong to this scene!");

    for (auto r : m_renderables)
    {
        r->updateDrawList(camera);
    }
}

//private
void Scene::defaultRenderPath(const RenderTarget& rt, const Entity* cameraList, std::size_t cameraCount)
{
    CRO_ASSERT(cameraList, "Must not be nullptr");
    CRO_ASSERT(cameraCount, "Needs at least one camera");

    //make sure we're using the active viewport
    //generic target size - this might be a buffer not the window!
    glm::vec2 size(rt.getSize());
    std::array<std::int32_t, 4u> previousViewport;
    glCheck(glGetIntegerv(GL_VIEWPORT, previousViewport.data()));

    for (auto i = 0u; i < cameraCount; ++i)
    {
        //auto camera = m_activeCamera;
        const auto& cam = cameraList[i].getComponent<Camera>();

        auto rect = rt.getViewport(cam.viewport);
        glViewport(rect.left, rect.bottom, rect.width, rect.height);

        //see comment after skybox render
        //for (auto r : m_renderables)
        //{
        //    r->render(camera);
        //}

        //draw the skybox if enabled
        if (m_skybox.vbo)
        {
            //change depth function so depth test passes when values are equal to depth buffer's content
            glCheck(glDepthFunc(GL_LEQUAL));
            glCheck(glEnable(GL_DEPTH_TEST));

            //remove translation from the view matrix
            auto view = glm::mat4(glm::mat3(cam.viewMatrix));

            glCheck(glUseProgram(m_skyboxShaders[m_shaderIndex].getGLHandle()));
            glCheck(glUniformMatrix4fv(m_skybox.viewUniform, 1, GL_FALSE, glm::value_ptr(view)));
            glCheck(glUniformMatrix4fv(m_skybox.projectionUniform, 1, GL_FALSE, glm::value_ptr(cam.projectionMatrix)));

            //bind the texture if it exists
            if (m_activeSkyboxTexture)
            {
                glCheck(glActiveTexture(GL_TEXTURE0));
                glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_activeSkyboxTexture));
                glCheck(glUniform1i(m_skybox.textureUniform, 0));
            }

            //set sun colour if shader expects it
            if (m_skybox.skyColourUniform)
            {
                auto c = m_sunlight.getComponent<Sunlight>().getColour();
                glCheck(glUniform4f(m_skybox.skyColourUniform, c.getRed(), c.getGreen(), c.getBlue(), c.getAlpha()));
            }

            //draw cube
#ifdef PLATFORM_DESKTOP
            glCheck(glBindVertexArray(m_skybox.vao));
            glCheck(glDrawArrays(GL_TRIANGLES, 0, 36));
            glCheck(glBindVertexArray(0));
#else
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_skybox.vbo));

            const auto& attribs = m_skyboxShader.getAttribMap();
            glCheck(glEnableVertexAttribArray(attribs[0]));
            glCheck(glVertexAttribPointer(attribs[0], 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(3 * sizeof(float)), reinterpret_cast<void*>(static_cast<intptr_t>(0))));

            glCheck(glDrawArrays(GL_TRIANGLES, 0, 36));

            glCheck(glDisableVertexAttribArray(attribs[0]));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif //PLATFORM
            glCheck(glUseProgram(0));

            glDisable(GL_DEPTH_TEST);
            glCheck(glDepthFunc(GL_LESS));
        }

        //ideally we want to do this before the skybox to reduce overdraw
        //but this breaks transparent objects... ideally opaque and transparent
        //passes should be separated, but this only affects the model renderer
        //and not other systems.... hum. Ideas on a postcard please.
        for (auto r : m_renderables)
        {
            r->render(cameraList[i], rt);
        }

    }

    //restore old view port
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
}

void Scene::postRenderPath(const RenderTarget&, const Entity* cameraList, std::size_t cameraCount)
{
    //TODO this only needs to be cleared once per frame
    //currently it is cleared for every active camera we draw...
    m_sceneBuffer.clear(Colour::Transparent());
    defaultRenderPath(m_sceneBuffer, cameraList, cameraCount);
    m_sceneBuffer.display();

    RenderTexture* inTex = &m_sceneBuffer;
    RenderTexture* outTex = nullptr;

    for (auto i = 0u; i < m_postEffects.size() - 1; ++i)
    {
        outTex = &m_postBuffers[i % 2];
        outTex->clear();
        m_postEffects[i]->apply(*inTex);
        outTex->display();
        inTex = outTex;
    }

    auto vp = m_sceneBuffer.getDefaultViewport();
    glViewport(vp.left, vp.bottom, vp.width, vp.height);
    m_postEffects.back()->apply(*inTex);
}

void Scene::destroySkybox()
{
    if (m_skybox.vao)
    {
        glCheck(glDeleteVertexArrays(1, &m_skybox.vao));
    }

    if (m_skybox.vbo)
    {
        glCheck(glDeleteBuffers(1, &m_skybox.vbo));
    }

    if (m_skybox.texture)
    {
        glCheck(glDeleteTextures(1, &m_skybox.texture));
    }

    m_skybox = {};
}