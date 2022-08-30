/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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
#include "../ErrorCheck.hpp"

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/util/Random.hpp>

namespace
{
#include "WireframeShader.inl"
#include "CloudShader.inl"

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

#else
    float ease(float i)
    {
        return sqrt(1.0 - pow(i - 1.0, 2.0));
    }
#endif

    void main()
    {
        mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
        vec4 position = a_position;

        float p = position.y - u_windData.w;
        p = mod(p, SystemHeight);
        //p = ease(0.2 + ((p / SystemHeight) * 0.8));
        p = ease((p / SystemHeight));

        position.y  = p * SystemHeight;

        position.x -= p * u_windData.x * u_windData.y * 40.0;
        position.z -= p * u_windData.z * u_windData.y * 40.0;


        gl_Position = wvp * position;
        gl_PointSize = u_projectionMatrix[1][1] / gl_Position.w * 10.0 * u_pixelScale;

        vec4 worldPos = u_worldMatrix * position;
        v_colour = a_colour;

#if defined (CULLED)
        vec3 distance = worldPos.xyz - u_cameraWorldPosition;
        v_colour.a *= smoothstep(12.25, 16.0, dot(distance, distance));
#endif

        gl_ClipDistance[0] = dot(worldPos, u_clipPlane);
    }
)";


    constexpr std::array<float, 3u> AreaStart = { 0.f, 0.f, 0.f };
    constexpr std::array<float, 3u> AreaEnd = { 20.f, 80.f, 20.f };

    constexpr std::int32_t GridX = 3;
    constexpr std::int32_t GridY = 3;
}

void GolfState::createWeather()
{
    //cro::Clock clock;
    auto points = pd::PoissonDiskSampling(2.3f, AreaStart, AreaEnd, 30u, static_cast<std::uint32_t>(std::time(nullptr)));

    //auto t = static_cast<float>(clock.elapsed().asMilliseconds()) / 1000.f;
    //LogI << "Generated " << points.size() << " in " << t << " seconds" << std::endl;

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));

    auto* meshData = &m_resources.meshes.getMesh(meshID);
    std::vector<float> verts;
    std::vector<std::uint32_t> indices;
    for (auto i = 0u; i < points.size(); ++i)
    {
        verts.push_back(points[i][0]);
        verts.push_back(points[i][1]);
        verts.push_back(points[i][2]);
        verts.push_back(1.f);
        verts.push_back(1.f);
        verts.push_back(1.f);
        verts.push_back(1.f);

        /*
        Problem with 2 verts is that when the first wraps
        around it stretches to the other still at the top.
        */

        /*verts.push_back(points[i][0]);
        verts.push_back(points[i][1] + 0.1f);
        verts.push_back(points[i][2]);
        verts.push_back(0.5f);
        verts.push_back(0.f);
        verts.push_back(1.f);
        verts.push_back(1.f);*/

        indices.push_back(i);
    }

    //for (auto i = 0u; i < points.size() * 2u; ++i)
    //{
    //    indices.push_back(i);
    //}

    meshData->vertexCount = points.size();
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    meshData->boundingBox[0] = { AreaStart[0], AreaStart[1], AreaStart[2] };
    meshData->boundingBox[1] = { AreaEnd[0], AreaEnd[1], AreaEnd[2] };
    meshData->boundingSphere.centre = meshData->boundingBox[0] + ((meshData->boundingBox[1] - meshData->boundingBox[0]) / 2.f);
    meshData->boundingSphere.radius = glm::length((meshData->boundingBox[1] - meshData->boundingBox[0]) / 2.f);

    m_resources.shaders.loadFromString(ShaderID::Weather, "#define EASE_SNOW\n" + WeatherVertex, WireframeFragment);
    auto& shader = m_resources.shaders.get(ShaderID::Weather);
    auto materialID = m_resources.materials.add(shader);
    auto material = m_resources.materials.get(materialID);
    material.setProperty("u_colour", LeaderboardTextLight);
    //material.setProperty("u_colour", WaterColour);

    constexpr glm::vec3 Offset(AreaEnd[0] * -1.5f, 0.f, AreaEnd[2] * -1.5f);
    for (auto y = 0; y < GridY; ++y)
    {
        for (auto x = 0; x < GridX; ++x)
        {
            glm::vec3 basePos(AreaEnd[0] * x, 0.f, AreaEnd[2] * y);
            basePos += Offset;

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
            entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

            //TODO replace this with a System?
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, basePos](cro::Entity e, float)
            {
                //if this was in a system we'd only have to work out the cam pos
                //once per update...
                auto camPos = m_gameScene.getActiveCamera().getComponent<cro::Transform>().getPosition();
                camPos.x = std::floor(camPos.x / AreaEnd[0]);
                camPos.z = std::floor(camPos.z / AreaEnd[2]);

                camPos.x *= AreaEnd[0];
                camPos.y = 0.f;
                camPos.z *= AreaEnd[2];

                e.getComponent<cro::Transform>().setPosition(basePos + camPos);
            };
        }
    }
    m_windBuffer.addShader(shader);
    m_scaleBuffer.addShader(shader);
}

void GolfState::createClouds(const std::string& cloudPath)
{
    auto spritePath = cloudPath.empty() ? "assets/golf/sprites/clouds.spt" : cloudPath;

    cro::SpriteSheet spriteSheet;
    if (spriteSheet.loadFromFile(spritePath, m_resources.textures)
        && spriteSheet.getSprites().size() > 1)
    {
        const auto& sprites = spriteSheet.getSprites();
        std::vector<cro::Sprite> randSprites;
        for (auto [_, sprite] : sprites)
        {
            randSprites.push_back(sprite);
        }

        m_resources.shaders.loadFromString(ShaderID::Cloud, CloudVertex, CloudFragment);
        auto& shader = m_resources.shaders.get(ShaderID::Cloud);
        m_scaleBuffer.addShader(shader);

        auto centreID = shader.getUniformID("u_worldCentre");
        glCheck(glUseProgram(shader.getGLHandle()));
        glCheck(glUniform2f(centreID, 160.f, -100.f));


        auto matID = m_resources.materials.add(shader);
        auto material = m_resources.materials.get(matID);
        material.blendMode = cro::Material::BlendMode::Alpha;
        material.setProperty("u_texture", *spriteSheet.getTexture());

        auto seed = static_cast<std::uint32_t>(std::time(nullptr));
        static constexpr std::array MinBounds = { 0.f, 0.f };
        static constexpr std::array MaxBounds = { 320.f, 320.f };
        auto positions = pd::PoissonDiskSampling(100.f, MinBounds, MaxBounds, 30u, seed);


        std::vector<cro::Entity> delayedUpdates;

        for (const auto& position : positions)
        {
            float height = cro::Util::Random::value(20, 40) + PlaneHeight;
            glm::vec3 cloudPos(position[0], height, -position[1]);


            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(cloudPos);
            entity.addComponent<Cloud>().speedMultiplier = static_cast<float>(cro::Util::Random::value(10, 22)) / 100.f;
            entity.addComponent<cro::Sprite>() = randSprites[cro::Util::Random::value(0u, randSprites.size() - 1)];
            entity.addComponent<cro::Model>();

            auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
            bounds.width /= PixelPerMetre;
            bounds.height /= PixelPerMetre;
            entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, 0.f });

            float scale = static_cast<float>(cro::Util::Random::value(1, 2));
            entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));
            entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, 90.f * cro::Util::Const::degToRad);
            entity.getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, 180.f * cro::Util::Const::degToRad);

            delayedUpdates.push_back(entity);
        }

        //this is a work around because changing sprite 3D materials
        //require at least once scene update to be run first.
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, material, delayedUpdates](cro::Entity e, float)
        {
            for (auto en : delayedUpdates)
            {
                en.getComponent<cro::Model>().setMaterial(0, material);
            }

            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        };
    }
}

void GolfState::buildBow()
{
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
    submesh->indexCount = indices.size();
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