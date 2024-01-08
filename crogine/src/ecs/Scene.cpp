/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <crogine/gui/Gui.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

namespace
{
    std::size_t uid = 0;

    //same order as GL_TEXTURE_CUBE_MAP_XXXX_YYYY
    enum CubemapDirection
    {
        Right, Left, Up, Down, Front, Back, Count
    };

    const std::string skyboxVertex = 
        R"(
        uniform mat4 u_projectionMatrix;
        uniform mat4 u_modelViewMatrix;   

        ATTRIBUTE vec3 a_position;

        VARYING_OUT vec3 v_texCoords;

        void main()
        {
            v_texCoords = a_position;
            vec4 pos = u_projectionMatrix * u_modelViewMatrix * vec4(a_position, 1.0);
            gl_Position = pos.xyww;
        })";
    const std::string skyboxFrag = 
        R"(
        OUTPUT

        uniform LOW vec3 u_darkColour;
        uniform LOW vec3 u_midColour;
        uniform LOW vec3 u_lightColour;

        uniform float u_stepStart = 0.48;//0.495;
        uniform float u_stepEnd = 0.505;
        uniform float u_starsAmount = 0.0;

        VARYING_IN vec3 v_texCoords;

        //const LOW vec3 lightColour = vec3(0.82, 0.98, 0.99);
        //const LOW vec3 lightColour = vec3(0.21, 0.5, 0.96);

        const vec3 Up = vec3(0.0, 1.0, 0.0);

        //3D Gradient noise from: https://www.shadertoy.com/view/Xsl3Dl
        //The MIT License
        //Copyright 2013 Inigo Quilez

        vec3 hash(vec3 p)
        {
            p = vec3(dot(p,vec3(127.1,311.7, 74.7)),
                     dot(p,vec3(269.5,183.3,246.1)),
                     dot(p,vec3(113.5,271.9,124.6)));

            return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
        }

        float noise(vec3 p)
        {
            vec3 i = floor(p);
            vec3 f = fract(p);
    
            vec3 u = f * f * (3.0 - 2.0 * f);

            return mix(mix(mix(dot(hash(i + vec3(0.0, 0.0, 0.0)), f - vec3(0.0, 0.0, 0.0)), 
                               dot(hash(i + vec3(1.0, 0.0, 0.0)), f - vec3(1.0, 0.0, 0.0)), u.x),
                           mix(dot(hash(i + vec3(0.0, 1.0, 0.0)), f - vec3(0.0, 1.0, 0.0)), 
                               dot(hash(i + vec3(1.0, 1.0, 0.0)), f - vec3(1.0, 1.0, 0.0)), u.x), u.y),
                       mix(mix(dot(hash(i + vec3(0.0, 0.0, 1.0)), f - vec3(0.0, 0.0, 1.0)), 
                               dot(hash(i + vec3(1.0, 0.0, 1.0)), f - vec3(1.0, 0.0, 1.0)), u.x),
                           mix(dot(hash(i + vec3(0.0, 1.0, 1.0)), f - vec3(0.0, 1.0, 1.0)), 
                               dot(hash(i + vec3(1.0, 1.0, 1.0)), f - vec3(1.0, 1.0, 1.0)), u.x), u.y), u.z );
        }

        const float Threshold = 8.0;
        const float Exposure = 100.0;

        void main()
        {
            float amount = dot(normalize(v_texCoords), Up);
            amount += 1.0;
            amount /= 2.0;

            vec3 top = mix(u_midColour, u_lightColour, smoothstep(u_stepEnd, /*0.545*/u_stepEnd + 0.03, amount));
            FRAG_OUT = vec4(mix(u_darkColour, top, smoothstep(u_stepStart, u_stepEnd, amount)), 1.0);

            vec3 viewDirection = normalize(v_texCoords);
            float stars = pow(clamp(noise(viewDirection * 200.0), 0.0f, 1.0f), Threshold) * Exposure;
            stars *= mix(0.4, 1.4, noise(viewDirection * 100.0));

            FRAG_OUT.rgb = mix(FRAG_OUT.rgb, vec3(1.0), stars * amount * u_starsAmount);

        })";
    const std::string skyboxFragTextured =
        R"(
        OUTPUT

        uniform samplerCube u_skybox;
        uniform vec3 u_skyColour;

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
            FRAG_OUT = vec4(colour * u_skyColour, 1.0);
        })";

    const float DefaultFOV = 35.f * Util::Const::degToRad;

    void updateView(cro::Camera& camera)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        if (camera.isOrthographic())
        {
            camera.setOrthographic(0.f, size.x, 0.f, size.y, 0.f, 10.f);
            camera.viewport = { 0.f, 0.f, 1.f, 1.f };
        }
        else
        {
            size.y = ((size.x / 16.f) * 9.f) / size.y;
            size.x = 1.f;

            camera.setPerspective(DefaultFOV, 16.f / 9.f, 0.1f, 280.f);
            camera.viewport.bottom = (1.f - size.y) / 2.f;
            camera.viewport.height = size.y;
        }
        LOG("Default camera resize callback used: are you missing a callback assignment?", cro::Logger::Type::Warning);
    }
}

Scene::Scene(MessageBus& mb, std::size_t initialPoolSize, std::uint32_t infoFlags)
    : m_messageBus          (mb),
    m_uid                   (++uid),
    m_entityManager         (mb, m_componentManager, initialPoolSize),
    m_systemManager         (*this, m_componentManager, infoFlags),
    m_projectionMapCount    (0),
    m_waterLevel            (0.f),
    m_activeSkyboxTexture   (0),
    m_starsUniform          (-1),
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

    std::fill(m_skyColourUniforms.begin(), m_skyColourUniforms.end(), -1);
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
    Destroyed ents are double buffered so any calls
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
    m_entityManager.markDestroyed(entity);
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
        resizeBuffers(size);
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
            //only using positions - remember we looking from
            //the inside so wind the verts accordingly...
            std::array<float, 108> verts = {
                 //far
                -0.5f,  0.5f, -0.5f,
                -0.5f, -0.5f, -0.5f,
                 0.5f, -0.5f, -0.5f,
                 0.5f, -0.5f, -0.5f,
                 0.5f,  0.5f, -0.5f,
                -0.5f,  0.5f, -0.5f,
                 //left
                -0.5f, -0.5f,  0.5f,
                -0.5f, -0.5f, -0.5f,
                -0.5f,  0.5f, -0.5f,
                -0.5f,  0.5f, -0.5f,
                -0.5f,  0.5f,  0.5f,
                -0.5f, -0.5f,  0.5f,
                 //right
                 0.5f, -0.5f, -0.5f,
                 0.5f, -0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f, -0.5f,
                 0.5f, -0.5f, -0.5f,
                 //front
                -0.5f, -0.5f,  0.5f,
                -0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                 0.5f, -0.5f,  0.5f,
                -0.5f, -0.5f,  0.5f,
                 //top
                -0.5f,  0.5f, -0.5f,
                 0.5f,  0.5f, -0.5f,
                 0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                -0.5f,  0.5f,  0.5f,
                -0.5f,  0.5f, -0.5f,
                 //bottom
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
            setSkyboxColours(cro::Colour::Blue, cro::Colour::Green, cro::Colour::Red);

            m_shaderIndex = SkyboxType::Coloured;

            m_skyColourUniforms[0] = m_skyboxShaders[SkyboxType::Coloured].getUniformID("u_darkColour");
            m_skyColourUniforms[1] = m_skyboxShaders[SkyboxType::Coloured].getUniformID("u_midColour");
            m_skyColourUniforms[2] = m_skyboxShaders[SkyboxType::Coloured].getUniformID("u_lightColour");
            m_starsUniform = m_skyboxShaders[SkyboxType::Coloured].getUniformID("u_starsAmount");
        }
        else
        {
            Logger::log("Failed to create skybox", cro::Logger::Type::Error);
        }
    }
    glCheck(glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS));
}

void Scene::disableSkybox()
{
    destroySkybox();
}

void Scene::setCubemap(const std::string& path)
{
    //open the file, check it's valid
    if (!m_skyboxCubemap.loadFromFile(path))
    {
        cro::Logger::log("Failed to open cubemap " + path, cro::Logger::Type::Error);
        return;
    }
    enableSkybox();

    //create shader if it doesn't exist
    if (m_skyboxShaders[SkyboxType::Cubemap].getGLHandle() == 0)
    {
        if (!m_skyboxShaders[SkyboxType::Cubemap].loadFromString(skyboxVertex, skyboxFragTextured))
        {
            LogE << "Failed creating cubemap shader" << std::endl;
            return;
        }
    }

    m_skybox.setShader(m_skyboxShaders[Cubemap]);
    m_shaderIndex = SkyboxType::Cubemap;

    m_skybox.texture = m_skyboxCubemap.getGLHandle();
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

    m_skyboxCubemap = {};
}

CubemapID Scene::getCubemap() const
{
    return CubemapID(m_activeSkyboxTexture);
}

void Scene::setSkyboxColours(Colour dark, Colour mid, Colour light)
{
    m_skybox.colours.bottom = dark;
    m_skybox.colours.middle = mid;
    m_skybox.colours.top = light;
    applySkyboxColours();
}

void Scene::setSkyboxColours(SkyColours colours)
{
    m_skybox.colours = colours;
    applySkyboxColours();
}

void Scene::setStarsAmount(float amount)
{
    m_skybox.starsAmount = std::clamp(amount, 0.f, 1.f);
    applyStars();
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

    //auto& cam = entity.getComponent<cro::Camera>();
    //we don't want to call this if we've explicitly
    //set the view elsewhere first...
    /*if (cam.resizeCallback)
    {
        cam.resizeCallback(cam);
    }*/

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
            resizeBuffers({ data.data0, data.data1 });
        }
    }
}

void Scene::render(bool doPost)
{
    if (doPost)
    {
        currentRenderPath(*RenderTarget::getActiveTarget(), &m_activeCamera, 1);
    }
    else
    {
        defaultRenderPath(*RenderTarget::getActiveTarget(), &m_activeCamera, 1);
    }
}

void Scene::render(const std::vector<Entity>& cameras, bool doPost)
{
    if (doPost)
    {
        currentRenderPath(*RenderTarget::getActiveTarget(), cameras.data(), cameras.size());
    }
    else
    {
        defaultRenderPath(*RenderTarget::getActiveTarget(), cameras.data(), cameras.size());
    }
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

void Scene::setSkyboxOrientation(glm::quat q)
{
    m_skybox.modelMatrix = glm::mat4(q);
}

//private
void Scene::applySkyboxColours()
{
    const auto& [dark, mid, light] = m_skybox.colours;

    if (m_skyboxShaders[SkyboxType::Coloured].getGLHandle())
    {
        glCheck(glUseProgram(m_skyboxShaders[SkyboxType::Coloured].getGLHandle()));
        glCheck(glUniform3f(m_skyColourUniforms[0], dark.getRed(), dark.getGreen(), dark.getBlue()));
        glCheck(glUniform3f(m_skyColourUniforms[1], mid.getRed(), mid.getGreen(), mid.getBlue()));
        glCheck(glUniform3f(m_skyColourUniforms[2], light.getRed(), light.getGreen(), light.getBlue()));
        glCheck(glUseProgram(0));
    }
}

void Scene::applyStars()
{
    if (m_skyboxShaders[SkyboxType::Coloured].getGLHandle())
    {
        glCheck(glUseProgram(m_skyboxShaders[SkyboxType::Coloured].getGLHandle()));
        glCheck(glUniform1f(m_starsUniform, m_skybox.starsAmount));
        //glCheck(glUseProgram(0));
    }
}

void Scene::defaultRenderPath(const RenderTarget& rt, const Entity* cameraList, std::size_t cameraCount)
{
    CRO_ASSERT(cameraList, "Must not be nullptr");
    CRO_ASSERT(cameraCount, "Needs at least one camera");

    //make sure we're using the active viewport
    //generic target size - this might be a buffer not the window!
    std::array<std::int32_t, 4u> previousViewport;
    glCheck(glGetIntegerv(GL_VIEWPORT, previousViewport.data()));

    for (auto i = 0u; i < cameraCount; ++i)
    {
        const auto& cam = cameraList[i].getComponent<Camera>();
        const auto& pass = cam.getActivePass();

        auto rect = rt.getViewport(cam.viewport);
        glViewport(rect.left, rect.bottom, rect.width, rect.height);

        //TODO the skybox pass ought to be placed between opaque
        //and transparent passes

        //draw the skybox if enabled
        if (m_skybox.vbo)
        {
            //change depth function so depth test passes when values are equal to depth buffer's content
            glCheck(glDepthFunc(GL_LEQUAL));
            glCheck(glEnable(GL_DEPTH_TEST));
            glCheck(glEnable(GL_CULL_FACE));
            glCheck(glCullFace(pass.getCullFace()));

            //remove translation from the view matrix
            auto view = glm::mat4(glm::mat3(pass.viewMatrix)) * m_skybox.modelMatrix;

            glCheck(glUseProgram(m_skyboxShaders[m_shaderIndex].getGLHandle()));
            glCheck(glUniformMatrix4fv(m_skybox.modelViewUniform, 1, GL_FALSE, glm::value_ptr(view)));
            glCheck(glUniformMatrix4fv(m_skybox.projectionUniform, 1, GL_FALSE, glm::value_ptr(cam.getProjectionMatrix())));

            //bind the texture if it exists
            if (m_activeSkyboxTexture)
            {
                glCheck(glActiveTexture(GL_TEXTURE0));
                glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_activeSkyboxTexture));
                glCheck(glUniform1i(m_skybox.textureUniform, 0));
            }

            //set sun colour if shader expects it
            if (m_skybox.skyColourUniform != -1) //this would be -1 if it doesn't exist, not 0
            {
                auto c = m_sunlight.getComponent<Sunlight>().getColour();
                glCheck(glUniform3f(m_skybox.skyColourUniform, c.getRed(), c.getGreen(), c.getBlue()));
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
        //but this breaks transparent objects... opaque and transparent
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
    m_sceneBuffer.clear(Colour::Transparent);
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

void Scene::resizeBuffers(glm::uvec2 size)
{
    if (size != m_sceneBuffer.getSize())
    {
        if (m_sceneBuffer.available())
        {
            m_sceneBuffer.create(size.x, size.y);
            for (auto& p : m_postEffects)
            {
                p->resizeBuffer(size.x, size.y);
            }
        }

        if (m_postBuffers[0].available())
        {
            m_postBuffers[0].create(size.x, size.y, false);
        }

        if (m_postBuffers[1].available())
        {
            m_postBuffers[1].create(size.x, size.y, false);
        }
    }
}
