/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "GolfState.hpp"
#include "PoissonDisk.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "CloudSystem.hpp"
#include "WeatherAnimationSystem.hpp"
#include "../ErrorCheck.hpp"

#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/CircleMeshBuilder.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/util/Random.hpp>

namespace
{
#include "shaders/WireframeShader.inl"
#include "shaders/CloudShader.inl"

    const std::string WeatherVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_worldViewMatrix;
    uniform mat4 u_projectionMatrix;
    uniform vec4 u_clipPlane;

    layout (std140) uniform WindValues
    {
        vec4 u_windData; //dirX, strength, dirZ, elapsedTime
    };

    layout (std140) uniform PixelScale
    {
        float u_pixelScale;
    };

#if defined (CULLED)
    uniform HIGH vec3 u_cameraWorldPosition;
#endif

    VARYING_OUT LOW vec4 v_colour;

    const float SystemHeight = 80.0;

#if defined(EASE_SNOW)
    const float PI = 3.1412;
    float ease(float i)
    {
        return sin((i * PI) / 2.0);
    }

    const float FallSpeed = 1.0;
    const float WindEffect = 40.0;
#else
    float ease(float i)
    {
        //return sqrt(1.0 - pow(i - 1.0, 2.0));
        return i;
    }
    const float FallSpeed = 32.0;//16.0;
    const float WindEffect = 10.0;
#endif

    void main()
    {
        mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
        vec4 position = a_position;

        float p = position.y - (u_windData.w * FallSpeed);
        p = mod(p, SystemHeight);
        //p = ease(0.2 + ((p / SystemHeight) * 0.8));
        p = ease((p / SystemHeight));

        position.y  = p * SystemHeight;

        position.x -= p * u_windData.x * u_windData.y * WindEffect;
        position.z -= p * u_windData.z * u_windData.y * WindEffect;


        gl_Position = wvp * position;
        gl_PointSize = u_projectionMatrix[1][1] / gl_Position.w * 10.0 * u_pixelScale * (FallSpeed);

        vec4 worldPos = u_worldMatrix * position;
        v_colour = a_colour;

#if defined (CULLED)
        vec3 distance = worldPos.xyz - u_cameraWorldPosition;
        v_colour.a *= smoothstep(12.25, 16.0, dot(distance, distance));
#endif

        gl_ClipDistance[0] = dot(worldPos, u_clipPlane);
    }
)";

    static const std::string RainFragment = R"(
    #define USE_MRT
    #include OUTPUT_LOCATION

    uniform vec4 u_colour = vec4(1.0);

    VARYING_IN vec4 v_colour;

    float ease(float i)
    {
        //return sqrt(pow(i, 2.0));
        return pow(i, 5.0);
    }

    void main()
    {
        vec4 colour = v_colour * u_colour;
        
        //if alpha blended
        //colour.rgb += (1.0 - colour.a) * colour.rgb;
        colour.a *= 0.5 + (0.5 * ease(gl_PointCoord.y));

        float crop = step(0.495, gl_PointCoord.x);
        crop *= 1.0 - step(0.505, gl_PointCoord.x);

        //if(crop < 0.05) discard;

        colour.a *= u_colour.a * crop;
        FRAG_OUT = colour;

        LIGHT_OUT = vec4(vec3(0.0), colour.a);
    }
)";

    constexpr std::int32_t GridX = 3;
    constexpr std::int32_t GridY = 3;
}

void GolfState::createWeather(std::int32_t weatherType)
{
    //cro::Clock clock;
    auto points = pd::PoissonDiskSampling(2.3f, WeatherAnimationSystem::AreaStart, WeatherAnimationSystem::AreaEnd, 30u, static_cast<std::uint32_t>(std::time(nullptr)));

    //auto t = static_cast<float>(clock.elapsed().asMilliseconds()) / 1000.f;
    //LogI << "Generated " << points.size() << " in " << t << " seconds" << std::endl;

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));

    auto* meshData = &m_resources.meshes.getMesh(meshID);
    std::vector<float> verts;
    std::vector<std::uint32_t> indices;
    std::uint32_t stride = weatherType == WeatherType::Snow ? 1 : 2;
    for (auto i = 0u; i < points.size(); i += stride)
    {
        verts.push_back(points[i][0]);
        verts.push_back(points[i][1]);
        verts.push_back(points[i][2]);
        verts.push_back(1.f);
        verts.push_back(1.f);
        verts.push_back(1.f);
        verts.push_back(1.f);

        indices.push_back(i);
    }

    meshData->vertexCount = points.size() / stride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    meshData->boundingBox[0] = { WeatherAnimationSystem::AreaStart[0], WeatherAnimationSystem::AreaStart[1], WeatherAnimationSystem::AreaStart[2] };
    meshData->boundingBox[1] = { WeatherAnimationSystem::AreaEnd[0], WeatherAnimationSystem::AreaEnd[1], WeatherAnimationSystem::AreaEnd[2] };
    meshData->boundingSphere.centre = meshData->boundingBox[0] + ((meshData->boundingBox[1] - meshData->boundingBox[0]) / 2.f);
    meshData->boundingSphere.radius = glm::length((meshData->boundingBox[1] - meshData->boundingBox[0]) / 2.f);

    auto weatherColour = LeaderboardTextLight;
    auto blendMode = cro::Material::BlendMode::None;
    if (weatherType == WeatherType::Snow)
    {
        m_resources.shaders.loadFromString(ShaderID::Weather, WeatherVertex, WireframeFragment, "#define EASE_SNOW\n");
    }
    else
    {
        m_resources.shaders.loadFromString(ShaderID::Weather, WeatherVertex, RainFragment);
        weatherColour = cro::Colour(0.86f, 0.87f, 0.873f);
        blendMode = cro::Material::BlendMode::Custom;

        //create audio entity
        cro::AudioScape audioscape;
        if (audioscape.loadFromFile("assets/golf/sound/rain.xas", m_resources.audio))
        {
            const auto callback = 
                [&](cro::Entity e, float)
            {
                const auto& [vol, pos] = e.getComponent<cro::Callback>().getUserData<const std::pair<float, glm::vec3>>();
                e.getComponent<cro::AudioEmitter>().setVolume(vol * m_gameScene.getSystem<WeatherAnimationSystem>()->getOpacity());

                //really we need to set the height at ground height - but is
                //it worth doing all that ray casting?
                auto camPos = m_gameScene.getActiveListener().getComponent<cro::Transform>().getWorldPosition();
                camPos.y = 0.f;

                e.getComponent<cro::Transform>().setPosition(pos + camPos);

                auto state = e.getComponent<cro::AudioEmitter>().getState();
                if (vol == 0 && state == cro::AudioEmitter::State::Playing)
                {
                    e.getComponent<cro::AudioEmitter>().pause();
                }
                else if (vol != 0 && state == cro::AudioEmitter::State::Paused)
                {
                    e.getComponent<cro::AudioEmitter>().play();
                }
            };

            const std::array EmitterNames =
            {
                std::string("01"),
                std::string("02"),
                std::string("03"),
                std::string("04"),
                std::string("05"),
                std::string("06"),
            };

            constexpr std::array Positions =
            {
                glm::vec3(-0.865, 2.f, -0.5f),
                glm::vec3(0.f, 2.f, 1.f),
                glm::vec3(0.865, 2.f, -0.5f),
                glm::vec3(-0.865, 2.f, 0.5f),
                glm::vec3(0.f, 2.f, 1.f),
                glm::vec3(0.865, 2.f, 0.5f),
            };

            for (auto i = 0u; i < Positions.size(); ++i)
            {
                auto audioEnt = m_gameScene.createEntity();
                audioEnt.addComponent<cro::Transform>();
                audioEnt.addComponent<cro::AudioEmitter>() = audioscape.getEmitter(EmitterNames[i]);

                auto offset = (i % 3) + 1;
                float playingOffset = 60.f / offset;
                audioEnt.getComponent<cro::AudioEmitter>().play();
                audioEnt.getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(playingOffset));

                const float baseVolume = audioEnt.getComponent<cro::AudioEmitter>().getVolume();
                audioEnt.addComponent<cro::Callback>().active = true;
                audioEnt.getComponent<cro::Callback>().setUserData<const std::pair<float, glm::vec3>>(baseVolume, Positions[i]);
                audioEnt.getComponent<cro::Callback>().function = callback;
            }
        }
    }
    auto& shader = m_resources.shaders.get(ShaderID::Weather);
    auto materialID = m_resources.materials.add(shader);
    auto material = m_resources.materials.get(materialID);
    //material.setProperty("u_colour", weatherColour); //this will override the fade animation
    material.blendMode = cro::Material::BlendMode::Alpha;
    //material.blendMode = blendMode;
    ////ignored by snow as it has a different blendmode, but that's fine
    //material.blendData.blendFunc = { GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA };
    //material.blendData.enableProperties = { GL_BLEND, GL_DEPTH_TEST };
    //material.blendData.equation = GL_FUNC_ADD;
    //material.blendData.writeDepthMask = GL_TRUE;

    m_gameScene.getSystem<WeatherAnimationSystem>()->setMaterialData(shader, weatherColour);

    constexpr glm::vec3 Offset(WeatherAnimationSystem::AreaEnd[0] * -1.5f, 0.f, WeatherAnimationSystem::AreaEnd[2] * -1.5f);
    for (auto y = 0; y < GridY; ++y)
    {
        for (auto x = 0; x < GridX; ++x)
        {
            glm::vec3 basePos(WeatherAnimationSystem::AreaEnd[0] * x, 0.f, WeatherAnimationSystem::AreaEnd[2] * y);
            basePos += Offset;

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
            entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::FlightCam));
            entity.addComponent<WeatherAnimation>().basePosition = basePos;
        }
    }
    m_windBuffer.addShader(shader);
    m_scaleBuffer.addShader(shader);

    //if (weatherType == WeatherType::Rain)
    {
        m_gameScene.setSystemActive<WeatherAnimationSystem>(true);
        m_gameScene.getSystem<WeatherAnimationSystem>()->setHidden(false);
    }
}

void GolfState::setFog(float density)
{
    auto* shader = &m_resources.shaders.get(ShaderID::Composite);
    glUseProgram(shader->getGLHandle());
    glUniform1f(shader->getUniformID("u_density"), density);

    auto skyColour = m_gameScene.getSunlight().getComponent<cro::Sunlight>().getColour().getVec4();
    glUniform4f(shader->getUniformID("u_lightColour"), skyColour.r, skyColour.g, skyColour.b, skyColour.a);

    shader = &m_resources.shaders.get(ShaderID::Minimap);
    glUseProgram(shader->getGLHandle());
    glUniform1f(shader->getUniformID("u_fogEnd"), 280.f);
    glUniform1f(shader->getUniformID("u_density"), density);
    glUniform4f(shader->getUniformID("u_lightColour"), skyColour.r, skyColour.g, skyColour.b, skyColour.a);
}

void GolfState::createClouds()
{
    const std::array Paths =
    {
        std::string("assets/golf/models/skybox/clouds/cloud05.cmt"),
        std::string("assets/golf/models/skybox/clouds/cloud06.cmt"),
        std::string("assets/golf/models/skybox/clouds/cloud07.cmt"),
    };

    cro::ModelDefinition md(m_resources);
    std::vector<cro::ModelDefinition> definitions;
    for (const auto& path : Paths)
    {
        if (md.loadFromFile(path))
        {
            definitions.push_back(md);
        }
    }

    if (!definitions.empty())
    {
        std::string wobble;
        if (m_sharedData.vertexSnap)
        {
            wobble = "#define WOBBLE\n";
        }

        m_resources.shaders.loadFromString(ShaderID::Cloud, CloudOverheadVertex, CloudOverheadFragment, "#define FEATHER_EDGE\n" + wobble);
        auto& shader = m_resources.shaders.get(ShaderID::Cloud);

        if (m_sharedData.vertexSnap)
        {
            m_resolutionBuffer.addShader(shader);
        }

        auto matID = m_resources.materials.add(shader);
        auto material = m_resources.materials.get(matID);

        material.setProperty("u_skyColourTop", m_skyScene.getSkyboxColours().top);
        material.setProperty("u_skyColourBottom", m_skyScene.getSkyboxColours().middle);
        material.setProperty("u_worldCentre", glm::vec2(160.f, -100.f));

        auto seed = static_cast<std::uint32_t>(std::time(nullptr));
        static constexpr std::array MinBounds = { 0.f, 0.f };
        static constexpr std::array MaxBounds = { 320.f, 320.f };
        auto positions = pd::PoissonDiskSampling(80.f, MinBounds, MaxBounds, 30u, seed);

        std::size_t modelIndex = 0;

        for (const auto& position : positions)
        {
            float height = cro::Util::Random::value(20, 40) + PlaneHeight;
            glm::vec3 cloudPos(position[0], height, -position[1]);


            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(cloudPos);
            entity.addComponent<Cloud>().speedMultiplier = static_cast<float>(cro::Util::Random::value(10, 22)) / 100.f;
            definitions[modelIndex].createModel(entity);
            entity.getComponent<cro::Model>().setMaterial(0, material);
            entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::MiniGreen));

            float scale = static_cast<float>(cro::Util::Random::value(24, 30));
            entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));

            modelIndex = (modelIndex + 1) % definitions.size();
        }
    }

    //createSun();
}

void GolfState::buildBow()
{
    if (Social::isGreyscale())
    {
        createWeather(WeatherType::Rain);
        setFog(0.89f);
        return;
    }

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::UV0, 1, GL_TRIANGLE_STRIP));

    auto* meshData = &m_resources.meshes.getMesh(meshID);
    std::vector<float> verts =
    {
        -5.f, 5.f, -12.f,    1.f, 1.f, 1.f, 1.f,  0.f, 1.f,
        -5.f, -5.f, -12.f,   1.f, 1.f, 1.f, 1.f,  0.f, 0.f,
        5.f, 5.f, -12.f,     1.f, 1.f, 1.f, 1.f,  1.f, 1.f,
        5.f, -5.f, -12.f,    1.f, 1.f, 1.f, 1.f,  1.f, 0.f,
    };
    std::vector<std::uint32_t> indices =
    {
        0,1,2,3
    };

    meshData->vertexCount = verts.size();
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    meshData->boundingBox = { glm::vec3(-5.f, -5.f, -12.1f), glm::vec3(5.f, 5.f, -11.9f) };
    meshData->boundingSphere = meshData->boundingBox;

    m_resources.shaders.loadFromString(ShaderID::Bow, CloudVertex, BowFragment);
    auto materialID = m_resources.materials.add(m_resources.shaders.get(ShaderID::Bow));
    auto material = m_resources.materials.get(materialID);
    material.blendMode = cro::Material::BlendMode::Additive;

    auto entity = m_skyScene.createEntity();
    entity.addComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 220.f * cro::Util::Const::degToRad);
    entity.addComponent<cro::Model>(*meshData, material);
}

void GolfState::handleWeatherChange(std::uint8_t v)
{
    if (m_sharedData.weatherType == WeatherType::Showers)
    {
        m_gameScene.getSystem<WeatherAnimationSystem>()->setHidden(v);
        m_gameScene.setSystemActive<WeatherAnimationSystem>(true);

        struct FogShader final
        {
            std::uint32_t id = 0;
            std::int32_t colour = -1;
            std::int32_t density = -1;
            std::int32_t end = -1;
        };
        FogShader composite;
        FogShader minimap;
        auto skyColour = m_gameScene.getSunlight().getComponent<cro::Sunlight>().getColour().getVec4();

        auto* shader = &m_resources.shaders.get(ShaderID::Composite);
        composite.id = shader->getGLHandle();
        composite.colour = shader->getUniformID("u_lightColour");
        composite.density = shader->getUniformID("u_density");

        shader = &m_resources.shaders.get(ShaderID::Minimap);
        minimap.id = shader->getGLHandle();
        minimap.colour = shader->getUniformID("u_lightColour");
        minimap.density = shader->getUniformID("u_density");
        minimap.end = shader->getUniformID("u_fogEnd");

        static constexpr float FogAmount = 0.3f;
        struct FogProgress final
        {
            float current = 0.f;
            float target = 0.f;
        };
        FogProgress progress;
        //I'm sure this should be the other way, but results speak for themselves..
        progress.current = v ? FogAmount : 0.f;
        progress.target = v ? 0.f : FogAmount;

        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<FogProgress>(progress);
        entity.getComponent<cro::Callback>().function =
            [&, composite, minimap, skyColour, v](cro::Entity e, float dt)
        {
            const float Speed = dt * 0.5f * FogAmount;
            auto& [current, target] = e.getComponent<cro::Callback>().getUserData<FogProgress>();
            if (v)
            {
                current = std::max(target, current - Speed);
            }
            else
            {
                current = std::min(target, current + Speed);
            }

            glUseProgram(composite.id);
            glUniform1f(composite.density, current);
            glUniform4f(composite.colour, skyColour.r, skyColour.g, skyColour.b, skyColour.a);

            glUseProgram(minimap.id);
            glUniform1f(minimap.end, 280.f);
            glUniform1f(minimap.density, current);
            glUniform4f(minimap.colour, skyColour.r, skyColour.g, skyColour.b, skyColour.a);

            if (current == target)
            {
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
        };
    }
}