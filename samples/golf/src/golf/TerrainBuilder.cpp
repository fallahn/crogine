/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "TerrainBuilder.hpp"
#include "PoissonDisk.hpp"
#include "Terrain.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "CommandIDs.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include <crogine/gui/Gui.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

#include "../ErrorCheck.hpp"

#include <chrono>

namespace
{
#include "TerrainShader.inl"

    //params for poisson disk samples
    constexpr float GrassDensity = 1.7f; //radius for PD sampler
    constexpr float TreeDensity = 4.f;

    //TODO for grass billboard we could shrink the area slightly as we prefer trees further away
    constexpr std::array MinBounds = { 0.f, 0.f };
    constexpr std::array MaxBounds = { static_cast<float>(MapSize.x), static_cast<float>(MapSize.y) };

    constexpr std::uint32_t QuadsPerMetre = 1;

    constexpr float MaxShrubOffset = MaxTerrainHeight + 13.f;
    constexpr std::int32_t SlopeGridSize = 20;
    constexpr std::int32_t HalfGridSize = SlopeGridSize / 2;


    //callback data
    struct SwapData final
    {
        static constexpr float TransitionTime = 2.f;
        float currentTime = 0.f;
        float start = -MaxShrubOffset;
        float destination = 0.f;
        cro::Entity otherEnt;
    };

    //callback for swapping shrub ents
    struct ShrubTransition final
    {
        void operator()(cro::Entity e, float dt)
        {
            auto& swapData = e.getComponent<cro::Callback>().getUserData<SwapData>();
            auto pos = e.getComponent<cro::Transform>().getPosition();

            swapData.currentTime = std::min(SwapData::TransitionTime, swapData.currentTime + dt);

            pos.y = swapData.start + ((swapData.destination - swapData.start) * cro::Util::Easing::easeInOutQuint(swapData.currentTime / SwapData::TransitionTime));
            pos.y = std::min(0.f, std::max(-MaxShrubOffset, pos.y));

            e.getComponent<cro::Transform>().setPosition(pos);

            if (swapData.currentTime == SwapData::TransitionTime)
            {
                pos.y = swapData.destination;
                e.getComponent<cro::Transform>().setPosition(pos);
                e.getComponent<cro::Callback>().active = false;

                if (swapData.otherEnt.isValid())
                {
                    e.getComponent<cro::Model>().setHidden(true);

                    swapData.otherEnt.getComponent<cro::Callback>().active = true;
                    swapData.otherEnt.getComponent<cro::Model>().setHidden(false);

                    terrainEntity.getComponent<cro::Callback>().active = true; //starts terrain morph
                }
            }
        }

        cro::Entity terrainEntity;
    };
}

TerrainBuilder::TerrainBuilder(const std::vector<HoleData>& hd)
    : m_holeData    (hd),
    m_currentHole   (0),
    m_terrainBuffer ((MapSize.x * MapSize.y) / QuadsPerMetre),
    m_threadRunning (false),
    m_wantsUpdate   (false)
{
    m_slopeBuffer.reserve(SlopeGridSize * SlopeGridSize * 4);
#ifdef CRO_DEBUG_
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("Terrain"))
    //        {
    //            /*auto slopePos = slopeEntity.getComponent<cro::Transform>().getPosition();
    //            if (ImGui::SliderFloat("Slope", &slopePos.y, -10.f, 10.f))
    //            {
    //                slopeEntity.getComponent<cro::Transform>().setPosition(slopePos);
    //            }

    //            auto pos = m_terrainEntity.getComponent<cro::Transform>().getPosition();
    //            if (ImGui::SliderFloat("Height", &pos.y, -10.f, 10.f))
    //            {
    //                m_terrainEntity.getComponent<cro::Transform>().setPosition(pos);
    //            }

    //            auto visible = m_terrainEntity.getComponent<cro::Model>().isVisible();
    //            ImGui::Text("Visible: %s", visible ? "yes" : "no");

    //            if (ImGui::SliderFloat("Morph", &m_terrainProperties.morphTime, 0.f, 1.f))
    //            {
    //                glCheck(glUseProgram(m_terrainProperties.shaderID));
    //                glCheck(glUniform1f(m_terrainProperties.morphUniform, m_terrainProperties.morphTime));
    //            }*/
    //            //ImGui::Image(m_normalDebugTexture, { 640.f, 400.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //            //ImGui::Image(m_normalMap.getTexture(), { 320.f, 200.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //        }
    //        ImGui::End();        
    //    });
#endif
}

TerrainBuilder::~TerrainBuilder()
{
    m_threadRunning = false;

    if (m_thread)
    {
        m_thread->join();
    }
}

//public
void TerrainBuilder::create(cro::ResourceCollection& resources, cro::Scene& scene, const ThemeSettings& theme)
{
    //create a mesh for the height map - this is actually one quad short
    //top and left - but hey you didn't notice until you read this did you? :)
    for (auto i = 0u; i < m_terrainBuffer.size(); ++i)
    {
        std::size_t x = i % (MapSize.x / QuadsPerMetre);
        std::size_t y = i / (MapSize.x / QuadsPerMetre);

        m_terrainBuffer[i].position = { static_cast<float>(x * QuadsPerMetre), 0.f, -static_cast<float>(y * QuadsPerMetre) };
        m_terrainBuffer[i].targetPosition = m_terrainBuffer[i].position;
        m_terrainBuffer[i].colour = theme.grassColour.getVec4();
    }

    constexpr auto xCount = static_cast<std::uint32_t>(MapSize.x / QuadsPerMetre);
    constexpr auto yCount = static_cast<std::uint32_t>(MapSize.y / QuadsPerMetre);
    std::vector<std::uint32_t> indices(xCount * yCount * 6);


    for (auto y = 0u; y < yCount - 1; ++y)
    {
        if (y > 0)
        {
            indices.push_back((y + 1) * xCount);
        }

        for (auto x = 0u; x < xCount; x++)
        {
            indices.push_back(((y + 1) * xCount) + x);
            indices.push_back((y * xCount) + x);
        }

        if (y < yCount - 2)
        {
            indices.push_back((y * xCount) + (xCount - 1));
        }
    }


    //use a custom material for morphage
    resources.shaders.loadFromString(ShaderID::Terrain, TerrainVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n");
    const auto& shader = resources.shaders.get(ShaderID::Terrain);
    m_terrainProperties.morphUniform = shader.getUniformID("u_morphTime");
    m_terrainProperties.shaderID = shader.getGLHandle();
    auto materialID = resources.materials.add(shader);
    auto flags = cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal |
        cro::VertexProperty::Tangent | cro::VertexProperty::Bitangent; //use tan/bitan slots to store target position/normal
    auto meshID = resources.meshes.loadMesh(cro::DynamicMeshBuilder(flags, 1, GL_TRIANGLE_STRIP));

    auto entity = scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, TerrainLevel, 0.f });
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        m_terrainProperties.morphTime = std::min(1.f, m_terrainProperties.morphTime + dt);
        glCheck(glUseProgram(m_terrainProperties.shaderID));
        glCheck(glUniform1f(m_terrainProperties.morphUniform, m_terrainProperties.morphTime));

        if (m_terrainProperties.morphTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
    entity.addComponent<cro::Model>(resources.meshes.getMesh(meshID), resources.materials.get(materialID));
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);

    auto* meshData = &entity.getComponent<cro::Model>().getMeshData();
    meshData->vertexCount = static_cast<std::uint32_t>(m_terrainBuffer.size());
    meshData->boundingBox[0] = glm::vec3(0.f, -10.f, 0.f);
    meshData->boundingBox[1] = glm::vec3(MaxBounds[0], 10.f, -MaxBounds[1]);
    meshData->boundingSphere.centre = { MaxBounds[0] / 2.f, 0.f, -MaxBounds[1] / 2.f };
    meshData->boundingSphere.radius = glm::length(meshData->boundingSphere.centre);
    m_terrainProperties.vbo = meshData->vbo;
    //vert data is uploaded to vbo via update()

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    //parent the shrubbery so they always stay the same relative height
    m_terrainEntity = entity;

    //create billboard entities
    cro::ModelDefinition billboardDef(resources);

    for (auto& entity : m_billboardEntities)
    {
        //reload the the model def each time to ensure unique VBOs
        if (billboardDef.loadFromFile(theme.billboardModel))
        {
            //TODO if this fails to load we won't crash but the terrain
            //transition won't complete either so the game effectively gets stuck

            entity = scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 0.f, -MaxShrubOffset, 0.f });
            billboardDef.createModel(entity);
            //if the model def failed to load for some reason this will be
            //missing, so we'll add it here just to stop the thread exploding
            //if it can't find the component
            if (!entity.hasComponent<cro::BillboardCollection>())
            {
                entity.addComponent<cro::BillboardCollection>();
            }

            if (entity.hasComponent<cro::Model>())
            {
                auto transition = ShrubTransition();
                transition.terrainEntity = m_terrainEntity;

                entity.getComponent<cro::Model>().setHidden(true);
                entity.addComponent<cro::Callback>().function = transition;

                m_terrainEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            }
        }
    }

    //load the billboard rects from a sprite sheet and convert to templates
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile(theme.billboardSprites, resources.textures);
    m_billboardTemplates[BillboardID::Grass01] = spriteToBillboard(spriteSheet.getSprite("grass01"));
    m_billboardTemplates[BillboardID::Grass02] = spriteToBillboard(spriteSheet.getSprite("grass02"));
    m_billboardTemplates[BillboardID::Flowers01] = spriteToBillboard(spriteSheet.getSprite("flowers01"));
    m_billboardTemplates[BillboardID::Flowers02] = spriteToBillboard(spriteSheet.getSprite("flowers02"));
    m_billboardTemplates[BillboardID::Flowers03] = spriteToBillboard(spriteSheet.getSprite("flowers03"));
    m_billboardTemplates[BillboardID::Bush01] = spriteToBillboard(spriteSheet.getSprite("hedge01"));
    m_billboardTemplates[BillboardID::Bush02] = spriteToBillboard(spriteSheet.getSprite("hedge02"));

    m_billboardTemplates[BillboardID::Tree01] = spriteToBillboard(spriteSheet.getSprite("tree01"));
    m_billboardTemplates[BillboardID::Tree02] = spriteToBillboard(spriteSheet.getSprite("tree02"));
    m_billboardTemplates[BillboardID::Tree03] = spriteToBillboard(spriteSheet.getSprite("tree03"));
    m_billboardTemplates[BillboardID::Tree04] = spriteToBillboard(spriteSheet.getSprite("tree04"));


    //create a mesh to display the slope data
    flags = cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::UV0;
    meshID = resources.meshes.loadMesh(cro::DynamicMeshBuilder(flags, 1, GL_LINES));
    resources.shaders.loadFromString(ShaderID::Slope, SlopeVertexShader, SlopeFragmentShader);
    auto& slopeShader = resources.shaders.get(ShaderID::Slope);
    m_slopeProperties.positionUniform = slopeShader.getUniformID("u_centrePosition");
    m_slopeProperties.timeUniform = slopeShader.getUniformID("u_time");
    m_slopeProperties.alphaUniform = slopeShader.getUniformID("u_alpha");
    m_slopeProperties.shader = slopeShader.getGLHandle();
    materialID = resources.materials.add(slopeShader);
    resources.materials.get(materialID).blendMode = cro::Material::BlendMode::Alpha;
    //resources.materials.get(materialID).enableDepthTest = false;

    entity = scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::SlopeIndicator;
    entity.addComponent<cro::Model>(resources.meshes.getMesh(meshID), resources.materials.get(materialID));
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::MiniGreen));
    entity.getComponent<cro::Model>().setHidden(true);
    entity.addComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(0.f, 0);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [alpha, direction] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
        if (direction == 0)
        {
            //fade in
            alpha = std::min(1.f, alpha + dt);
            if (alpha == 1)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        }
        else
        {
            alpha = std::max(0.f, alpha - dt);
            if (alpha == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                e.getComponent<cro::Model>().setHidden(true);
            }
        }
        glCheck(glUseProgram(m_slopeProperties.shader));
        glCheck(glUniform1f(m_slopeProperties.alphaUniform, alpha));
    };

    m_slopeProperties.meshData = &entity.getComponent<cro::Model>().getMeshData();
    m_slopeProperties.meshData->boundingBox[0] = glm::vec3(-HalfGridSize, -1.f, HalfGridSize);
    m_slopeProperties.meshData->boundingBox[1] = glm::vec3(HalfGridSize, 1.f, -HalfGridSize);
    m_slopeProperties.meshData->boundingSphere.centre = { HalfGridSize, 0.f, -HalfGridSize };
    m_slopeProperties.meshData->boundingSphere.radius = glm::length(m_slopeProperties.meshData->boundingSphere.centre);
    m_slopeProperties.entity = entity;

    //create and update the initial normal map
    m_normalShader.loadFromString(NormalMapVertexShader, NormalMapFragmentShader);
    glm::mat4 viewMat = glm::rotate(glm::mat4(1.f), cro::Util::Const::PI / 2.f, glm::vec3(1.f, 0.f, 0.f));
    glm::vec2 mapSize(MapSize);
    glm::mat4 projMat = glm::ortho(0.f, mapSize.x, 0.f, mapSize.y, -10.f, 20.f);
    auto normalViewProj = projMat * viewMat;

    //we can set this once so we don't need to store the matrix
    glCheck(glUseProgram(m_normalShader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_normalShader.getUniformID("u_projectionMatrix"), 1, GL_FALSE, &normalViewProj[0][0]));
    glCheck(glUseProgram(0));

    m_normalMap.create(MapSize.x, MapSize.y, false);
    if (m_currentHole < m_holeData.size())
    {
        renderNormalMap();
    }

    //launch the thread - wants update is initially true
    //so we should create the first layout right away
    m_wantsUpdate = m_holeData.size() > m_currentHole;
    m_threadRunning = true;
    m_thread = std::make_unique<std::thread>(&TerrainBuilder::threadFunc, this);
}

void TerrainBuilder::update(std::size_t holeIndex)
{
    //wait for thread to finish (usually only the first time)
    //this *shouldn't* ever block unless something goes wrong
    //in which case we need to implement a get-out clause
    while (m_wantsUpdate) {}

    if (holeIndex == m_currentHole)
    {
        auto first = (holeIndex + 1) % 2;
        auto second = holeIndex % 2;

        if (m_billboardEntities[first].isValid()
            && m_billboardEntities[second].isValid())
        {
            //update the billboard data
            SwapData swapData;
            swapData.start = m_billboardEntities[first].getComponent<cro::Transform>().getPosition().y;
            swapData.destination = 0.f;
            swapData.currentTime = 0.f;
            m_billboardEntities[first].getComponent<cro::BillboardCollection>().setBillboards(m_billboardBuffer);
            m_billboardEntities[first].getComponent<cro::Callback>().setUserData<SwapData>(swapData);

            swapData.start = m_billboardEntities[second].getComponent<cro::Transform>().getPosition().y;
            swapData.destination = -MaxShrubOffset;
            swapData.otherEnt = m_billboardEntities[first];
            swapData.currentTime = 0.f;
            m_billboardEntities[second].getComponent<cro::Callback>().setUserData<SwapData>(swapData);
            m_billboardEntities[second].getComponent<cro::Callback>().active = true;
        }

        //upload terrain data
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_terrainProperties.vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(TerrainVertex) * m_terrainBuffer.size(), m_terrainBuffer.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
        
        m_terrainProperties.morphTime = 0.f;
        glCheck(glUseProgram(m_terrainProperties.shaderID));
        glCheck(glUniform1f(m_terrainProperties.morphUniform, m_terrainProperties.morphTime));
        //terrain callback is set active when shrubbery callback switches

        //upload the slope buffer data
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_slopeProperties.meshData->vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(SlopeVertex) * m_slopeBuffer.size(), m_slopeBuffer.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        auto* submesh = &m_slopeProperties.meshData->indexData[0];
        submesh->indexCount = static_cast<std::uint32_t>(m_slopeIndices.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), m_slopeIndices.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

        m_slopeProperties.entity.getComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].pin);

        //signal to the thread we want to update the buffers
        //ready for next time
        m_currentHole++;
        if (m_currentHole < m_holeData.size())
        {
            renderNormalMap();
            m_wantsUpdate = true;
        }
    }
}

void TerrainBuilder::updateTime(float elapsed)
{
    glCheck(glUseProgram(m_slopeProperties.shader));
    glCheck(glUniform1f(m_slopeProperties.timeUniform, elapsed));
}

void TerrainBuilder::setSlopePosition(glm::vec3 position)
{
    //why do this here when we could set it in update() when we set the entity position??
    glCheck(glUseProgram(m_slopeProperties.shader));
    glCheck(glUniform3f(m_slopeProperties.positionUniform, position.x, position.y, position.z));
    glCheck(glUseProgram(0));
}

//private
void TerrainBuilder::threadFunc()
{
    const auto readHeightMap = [&](std::uint32_t x, std::uint32_t y)
    {
        auto size = m_normalMapImage.getSize();
        x = std::min(size.x - 1, std::max(0u, x));
        y = std::min(size.y - 1, std::max(0u, y));

        float height = static_cast<float>(m_normalMapImage.getPixel(x, y)[3]) / 255.f;
        return m_holeHeight.bottom + (m_holeHeight.height * height);
    };

    while (m_threadRunning)
    {
        if (m_wantsUpdate)
        {
            //we checked the file validity when the game starts.
            //if the map file is broken now something more drastic happened...
            cro::Image mapImage;
            if (mapImage.loadFromFile(m_holeData[m_currentHole].mapPath))
            {
                //recreate the distribution(s)
                auto seed = static_cast<std::uint32_t>(std::time(nullptr));
                auto grass = pd::PoissonDiskSampling(GrassDensity, MinBounds, MaxBounds, 30u, seed);
                auto trees = pd::PoissonDiskSampling(TreeDensity, MinBounds, MaxBounds);
                auto flowers = pd::PoissonDiskSampling(TreeDensity * 0.5f, MinBounds, MaxBounds, 30u, seed / 2);

                //filter distribution by map area
                m_billboardBuffer.clear();
                for (auto [x, y] : grass)
                {
                    auto [terrain, _] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Rough)
                    {
                        float scale = static_cast<float>(cro::Util::Random::value(14, 16)) / 10.f;
                        float height = readHeightMap(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));

                        auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Grass01, BillboardID::Grass02)]);
                        bb.position = { x, height, -y };
                        bb.size *= scale;
                    }
                }
                
                for (auto [x, y] : trees)
                {
                    auto [terrain, height] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Scrub)
                    {
                        float scale = static_cast<float>(cro::Util::Random::value(12, 22)) / 10.f;
                        float height2 = readHeightMap(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)); //check if model mesh is higher than terrain
                        height = std::max(height, height2);

                        auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Tree01, BillboardID::Tree04)]);
                        bb.position = { x, height - 0.05f, -y }; //small vertical offset to stop floating billboards
                        bb.size *= scale;
                    }
                }
                
                for (auto [x, y] : flowers)
                {
                    auto [terrain, height] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Scrub
                        /*&& height > 0.6f*/)
                    {
                        float height2 = readHeightMap(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                        height = std::max(height, height2);

                        float scale = static_cast<float>(cro::Util::Random::value(13, 17)) / 10.f;

                        auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Flowers01, BillboardID::Bush02)]);
                        bb.position = { x, height + 0.05f, -y };
                        bb.size *= scale;
                    }
                }

                //this isn't the same as the readHeightMap above - it scales the
                //result to MaxTerrainHeight, whereas the above returns world coords
                const auto heightAt = [&](std::uint32_t x, std::uint32_t y)
                {
                    auto size = mapImage.getSize();

                    x = std::min(size.x - 1, std::max(0u, x));
                    y = std::min(size.y - 1, std::max(0u, y));

                    auto index = y * size.x + x;
                    index *= 4;
                    return (static_cast<float>(mapImage.getPixelData()[index + 1]) / 255.f) * MaxTerrainHeight;
                };

                //update vertex data for scrub terrain mesh
                for (auto i = 0u; i < m_terrainBuffer.size(); ++i)
                {
                    //for each vert copy the target to the current (as this is where we should be)
                    //then update the target with the new map height at that position
                    std::uint32_t x = i % (MapSize.x / QuadsPerMetre) * QuadsPerMetre;
                    std::uint32_t y = i / (MapSize.x / QuadsPerMetre) * QuadsPerMetre;

                    auto height = heightAt(x, y);

                    //normal calc
                    auto l = heightAt(x - 1, y);
                    auto r = heightAt(x + 1, y);
                    auto u = heightAt(x, y + 1);
                    auto d = heightAt(x, y - 1);

                    glm::vec3 normal = { l - r, d - u, 2.f };
                    normal = glm::normalize(normal);

                    m_terrainBuffer[i].position = m_terrainBuffer[i].targetPosition;
                    m_terrainBuffer[i].normal = m_terrainBuffer[i].targetNormal;
                    m_terrainBuffer[i].targetPosition.y = height;
                    m_terrainBuffer[i].targetNormal = normal;
                } 

                //update the vertex data for the slope indicator
                loadNormalMap(m_normalMapBuffer, m_normalMapImage); //image is populated when rendering texture

                m_slopeBuffer.clear();
                m_slopeIndices.clear();

                std::uint32_t currIndex = 0u;
                float lowestHeight = std::numeric_limits<float>::max();
                float highestHeight = std::numeric_limits<float>::lowest();
                //we can optimise this by only looping the grid around the pin pos
                auto pinPos = m_holeData[m_currentHole].pin;
                const std::int32_t startX = std::max(0, static_cast<std::int32_t>(std::floor(pinPos.x)) - HalfGridSize);
                const std::int32_t startY = std::max(0, static_cast<std::int32_t>(-std::floor(pinPos.z)) - HalfGridSize);
                constexpr float DashCount = 40.f; //actual div by TAU cos its sin but eh.
                constexpr float SlopeSpeed = 40.f;

                for (auto y = startY; y < startY + SlopeGridSize; ++y)
                {
                    for (auto x = startX; x < startX + SlopeGridSize; ++x)
                    {
                        auto terrain = readMap(mapImage, x, y).first;
                        if (terrain == TerrainID::Green)
                        {
                            static constexpr float epsilon = 0.021f;
                            float posX = static_cast<float>(x) - pinPos.x;
                            float posZ = -static_cast<float>(y) - pinPos.z;

                            auto height = (readHeightMap(x, y) - pinPos.y) + epsilon;
                            SlopeVertex vert;
                            vert.position = { posX, height, posZ };

                            //this is the number of times the 'dashes' repeat if enabled in the shader
                            //and the speed/direction based on height difference
                            vert.texCoord = { 0.f, 0.f };

                            if (height < lowestHeight)
                            {
                                lowestHeight = height;
                            }
                            else if (height > highestHeight)
                            {
                                highestHeight = height;
                            }



                            glm::vec3 offset(1.f, 0.f, 0.f);
                            height = (readHeightMap(x + 1, y) - pinPos.y) + epsilon;

                            SlopeVertex vert2;
                            vert2.position = vert.position + offset;
                            vert2.position.y = height;
                            vert2.texCoord = { DashCount, std::min(2.f, std::max(-2.f, (vert.position.y - height) * SlopeSpeed)) };
                            vert.texCoord.y = vert2.texCoord.y; //must be constant across segment

                            if (height < lowestHeight)
                            {
                                lowestHeight = height;
                            }
                            else if (height > highestHeight)
                            {
                                highestHeight = height;
                            }
                            

                            //we have to copy first vert as the tex coords will be different
                            //shame we can just recycle the index...
                            auto vert3 = vert;

                            offset = glm::vec3(0.f, 0.f, -1.f);
                            height = (readHeightMap(x, y + 1) - pinPos.y) + epsilon;

                            SlopeVertex vert4;
                            vert4.position = vert.position + offset;
                            vert4.position.y = height;
                            vert4.texCoord = { DashCount, std::min(2.f, std::max(-2.f, (vert3.position.y - height) * SlopeSpeed)) };
                            vert3.texCoord.y = vert4.texCoord.y;

                            if (height < lowestHeight)
                            {
                                lowestHeight = height;
                            }
                            else if (height > highestHeight)
                            {
                                highestHeight = height;
                            }


                            //do this last once we know everything was modified
                            m_slopeBuffer.push_back(vert);
                            m_slopeIndices.push_back(currIndex++);
                            m_slopeBuffer.push_back(vert2);
                            m_slopeIndices.push_back(currIndex++);
                            m_slopeBuffer.push_back(vert3);
                            m_slopeIndices.push_back(currIndex++);
                            m_slopeBuffer.push_back(vert4);
                            m_slopeIndices.push_back(currIndex++);
                        }
                    }
                }

                float maxHeight = highestHeight - lowestHeight;
                if (maxHeight != 0)
                {
                    for (auto& v : m_slopeBuffer)
                    {
                        auto vertHeight = v.position.y - lowestHeight;
                        vertHeight /= maxHeight;
                        v.colour = { 0.f, 0.4f * vertHeight, 1.f - vertHeight, 0.8f };
                    }
                }

                m_slopeProperties.meshData->vertexCount = static_cast<std::uint32_t>(m_slopeBuffer.size());
            }

            m_wantsUpdate = false;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void TerrainBuilder::renderNormalMap()
{
#ifdef CRO_DEBUG_
    if (m_normalMapImage.getSize().x)
    {
        m_normalDebugTexture.loadFromImage(m_normalMapImage);
    }
#endif

    //hmmm is there some of this we can pre-process to save doing it here?
    const auto& meshData = m_holeData[m_currentHole].modelEntity.getComponent<cro::Model>().getMeshData();
    std::size_t normalOffset = 0;
    for (auto i = 0u; i < cro::Mesh::Normal; ++i)
    {
        normalOffset += meshData.attributes[i];
    }

    const auto& attribs = m_normalShader.getAttribMap();
    auto vaoCount = static_cast<std::int32_t>(meshData.submeshCount);

    std::vector<std::uint32_t> vaos(vaoCount);
    glCheck(glGenVertexArrays(vaoCount, vaos.data()));

    for (auto i = 0u; i < vaos.size(); ++i)
    {
        glCheck(glBindVertexArray(vaos[i]));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
        glCheck(glEnableVertexAttribArray(attribs[cro::Mesh::Position]));
        glCheck(glVertexAttribPointer(attribs[cro::Mesh::Position], 3, GL_FLOAT, GL_FALSE, static_cast<std::int32_t>(meshData.vertexSize), 0));
        glCheck(glEnableVertexAttribArray(attribs[cro::Mesh::Normal]));
        glCheck(glVertexAttribPointer(attribs[cro::Mesh::Normal], 3, GL_FLOAT, GL_FALSE, static_cast<std::int32_t>(meshData.vertexSize), (void*)(normalOffset * sizeof(float))));
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].ibo));
    }
    
    glCheck(glUseProgram(m_normalShader.getGLHandle()));
    glCheck(glDisable(GL_CULL_FACE));

    m_holeHeight.bottom = std::min(meshData.boundingBox[0].y, meshData.boundingBox[1].y);
    m_holeHeight.height = std::max(meshData.boundingBox[0].y, meshData.boundingBox[1].y) - m_holeHeight.bottom;
    glCheck(glUniform1f(m_normalShader.getUniformID("u_lowestPoint"), m_holeHeight.bottom));
    glCheck(glUniform1f(m_normalShader.getUniformID("u_maxHeight"), m_holeHeight.height));

    //clear the alpha to 0 so unrendered areas have zero height
    //then the heightmap image can be compared and highest value used
    static const cro::Colour ClearColour(0x7f7fff00);
    m_normalMap.clear(ClearColour);
    for (auto i = 0u; i < vaos.size(); ++i)
    {
        glCheck(glBindVertexArray(vaos[i]));
        glCheck(glDrawElements(GL_TRIANGLES, meshData.indexData[i].indexCount, GL_UNSIGNED_INT, 0));
    }
    m_normalMap.display();

    glCheck(glBindVertexArray(0));
    glCheck(glDeleteVertexArrays(vaoCount, vaos.data()));


    //TODO this might be faster with glReadPixels directly from the above framebuffer?
    //probably doesn't matter if it's fast enough.
    m_normalMap.getTexture().saveToImage(m_normalMapImage);
}