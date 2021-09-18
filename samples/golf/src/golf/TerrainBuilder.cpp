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
#include <crogine/detail/glm/gtx/norm.hpp>

#include "../ErrorCheck.hpp"

#include <chrono>

namespace
{
#include "TerrainShader.inl"

    cro::Entity slopeEntity;

    //params for poisson disk samples
    constexpr float GrassDensity = 1.7f; //radius for PD sampler
    constexpr float TreeDensity = 4.8f;

    //TODO for grass billboard we could shrink the area slightly as we prefer trees further away
    constexpr std::array MinBounds = { 0.f, 0.f };
    constexpr std::array MaxBounds = { static_cast<float>(MapSize.x), static_cast<float>(MapSize.y) };

    constexpr std::uint32_t QuadsPerMetre = 1;

    constexpr float MaxShrubOffset = MaxTerrainHeight + 10.5f;

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

            constexpr float Speed = 5.f;
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
    m_terrainBuffer ((MapSize.x * MapSize.y) / QuadsPerMetre),
    m_threadRunning (false),
    m_wantsUpdate   (false),
    m_currentHole   (0)
{
#ifdef CRO_DEBUG_
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("Terrain"))
    //        {
    //            auto slopePos = slopeEntity.getComponent<cro::Transform>().getPosition();
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
    //            }
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
void TerrainBuilder::create(cro::ResourceCollection& resources, cro::Scene& scene)
{
    //create a mesh for the height map - this is actually one quad short
    //top and left - but hey you didn't notice until you read this did you? :)
    for (auto i = 0u; i < m_terrainBuffer.size(); ++i)
    {
        std::size_t x = i % (MapSize.x / QuadsPerMetre);
        std::size_t y = i / (MapSize.x / QuadsPerMetre);

        m_terrainBuffer[i].position = { static_cast<float>(x * QuadsPerMetre), 0.f, -static_cast<float>(y * QuadsPerMetre) };
        m_terrainBuffer[i].targetPosition = m_terrainBuffer[i].position;
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

            //raise a message to say the transition is complete
            auto* msg = cro::App::getInstance().getMessageBus().post<SceneEvent>(MessageID::SceneMessage);
            msg->type = SceneEvent::TransitionComplete;
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
        billboardDef.loadFromFile("assets/golf/models/shrubbery.cmt");

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

    //load the billboard rects from a sprite sheet and convert to templates
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/shrubbery.spt", resources.textures);
    m_billboardTemplates[BillboardID::Grass01] = spriteToBillboard(spriteSheet.getSprite("grass01"));
    m_billboardTemplates[BillboardID::Grass02] = spriteToBillboard(spriteSheet.getSprite("grass02"));
    m_billboardTemplates[BillboardID::Flowers01] = spriteToBillboard(spriteSheet.getSprite("flowers01"));
    m_billboardTemplates[BillboardID::Flowers02] = spriteToBillboard(spriteSheet.getSprite("flowers02"));
    m_billboardTemplates[BillboardID::Flowers03] = spriteToBillboard(spriteSheet.getSprite("flowers03"));
    m_billboardTemplates[BillboardID::Pine] = spriteToBillboard(spriteSheet.getSprite("pine"));
    //m_billboardTemplates[BillboardID::Willow] = spriteToBillboard(spriteSheet.getSprite("willow"));
    m_billboardTemplates[BillboardID::Birch] = spriteToBillboard(spriteSheet.getSprite("birch"));


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

    entity = scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.01f, 0.f });
    entity.addComponent<cro::CommandTarget>().ID = CommandID::SlopeIndicator;
    entity.addComponent<cro::Model>(resources.meshes.getMesh(meshID), resources.materials.get(materialID));
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
    entity.getComponent<cro::Model>().setHidden(true);
    entity.addComponent<cro::Callback>().setUserData < std::pair<float, std::int32_t>>(0.f, 0);
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
    m_slopeProperties.meshData->boundingBox[0] = glm::vec3(0.f, -10.f, 0.f);
    m_slopeProperties.meshData->boundingBox[1] = glm::vec3(MaxBounds[0], 10.f, -MaxBounds[1]);
    m_slopeProperties.meshData->boundingSphere.centre = { MaxBounds[0] / 2.f, 0.f, -MaxBounds[1] / 2.f };
    m_slopeProperties.meshData->boundingSphere.radius = glm::length(m_slopeProperties.meshData->boundingSphere.centre);
    slopeEntity = entity;

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

        m_normalMaps[first].loadFromImage(m_normalMapImage);
        m_normalMaps[first].setSmooth(false);
        auto modelEnt = m_holeData[holeIndex].modelEntity;
        modelEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_normalMap", cro::TextureID(m_normalMaps[first].getGLHandle()));

        //signal to the thread we want to update the buffers
        //ready for next time
        m_currentHole++;
        m_wantsUpdate = m_currentHole < m_holeData.size();
    }
}

void TerrainBuilder::updateTime(float elapsed)
{
    glCheck(glUseProgram(m_slopeProperties.shader));
    glCheck(glUniform1f(m_slopeProperties.timeUniform, elapsed));
}

void TerrainBuilder::setSlopePosition(glm::vec3 position)
{
    glCheck(glUseProgram(m_slopeProperties.shader));
    glCheck(glUniform3f(m_slopeProperties.positionUniform, position.x, position.y, position.z));
    glCheck(glUseProgram(0));
}

//private
void TerrainBuilder::threadFunc()
{
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
                auto flowers = pd::PoissonDiskSampling(TreeDensity, MinBounds, MaxBounds, 30u, seed / 2);

                //filter distribution by map area
                m_billboardBuffer.clear();
                for (auto [x, y] : grass)
                {
                    auto [terrain, height] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Rough)
                    {
                        float scale = static_cast<float>(cro::Util::Random::value(9, 11)) / 10.f;

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
                        float scale = static_cast<float>(cro::Util::Random::value(13, 18)) / 10.f;

                        auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Pine, BillboardID::Birch)]);
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
                        float scale = static_cast<float>(cro::Util::Random::value(9, 12)) / 10.f;

                        auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Flowers01, BillboardID::Flowers03)]);
                        bb.position = { x, height - 0.05f, -y }; //small vertical offset to stop floating billboards
                        bb.size *= scale;
                        //LogI << height << std::endl;
                    }
                }

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
                //TODO is this the same size as the loop above? could save on double iteration
                m_normalMapImage = loadNormalMap(m_normalMapBuffer, m_holeData[m_currentHole].mapPath);
                m_slopeBuffer.clear();
                m_slopeIndices.clear();

                std::uint32_t currIndex = 0u;
                for (auto y = 0u; y < MapSize.y; ++y)
                {
                    for (auto x = 0u; x < MapSize.x; ++x)
                    {
                        auto terrain = readMap(mapImage, x, y).first;
                        if (terrain == TerrainID::Green)
                        {
                            //TODO skip bunker too?
                            float posX = static_cast<float>(x) + 0.5f; //offset to centre of quad
                            float posZ = -(static_cast<float>(y) + 0.5f);

                            auto normal = m_normalMapBuffer[y * MapSize.x + x];

                            auto& vert = m_slopeBuffer.emplace_back();
                            vert.position = { posX, 0.f, posZ };
                            vert.colour = { 1.f, 1.f, 0.f, 0.5f };

                            m_slopeIndices.push_back(currIndex++);

                            static constexpr float MaxStrength = 0.75f;
                            auto dir = glm::vec2(normal.x, normal.z);
                            auto strength = glm::length(dir);
                            dir /= strength;
                            strength = std::min(MaxStrength, strength * 12.f);
                            dir *= strength;
                            
                            auto& vert2 = m_slopeBuffer.emplace_back();
                            vert2.position = { posX + dir.x, 0.f, posZ + dir.y };
                            vert2.colour = { 0.f, 1.f - (strength + 0.25f), 1.f, 1.f };
                            vert2.texCoord = glm::vec2(40.f  * (strength / MaxStrength));

                            m_slopeIndices.push_back(currIndex++);
                        }
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