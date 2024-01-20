/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "TerrainBuilder.hpp"
#include "PoissonDisk.hpp"
#include "Terrain.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "CommandIDs.hpp"
#include "VatFile.hpp"
#include "VatAnimationSystem.hpp"
#include "SharedStateData.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>

#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/ImageArray.hpp>

#include <crogine/gui/Gui.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>

#include "../ErrorCheck.hpp"

#include <chrono>

using namespace cl;

namespace
{
#include "shaders/TerrainShader.inl"
#include "shaders/CelShader.inl"

    constexpr glm::vec2 ChunkSize(static_cast<float>(MapSize.x) / ChunkVisSystem::ColCount, static_cast<float>(MapSize.y) / ChunkVisSystem::RowCount);

    //params for poisson disk samples
    static constexpr float GrassDensity = 1.7f; //radius for PD sampler
    static constexpr float TreeDensity = 4.f;

    static constexpr std::array MinBounds = { 0.f, 0.f };
    static constexpr std::array MaxBounds = { static_cast<float>(MapSize.x), static_cast<float>(MapSize.y) };

    static constexpr std::uint32_t QuadsPerMetre = 1;

    static constexpr float MaxShrubOffset = MaxTerrainHeight + 13.f;
    static constexpr std::int32_t SlopeGridSize = 20;
    static constexpr std::int32_t HalfGridSize = SlopeGridSize / 2;
    
    //number of times the resolution of the map to increase normal map resolution by
    //MUST be even and should be 2,4 or 8 as 1 will cause a div0!
    static constexpr std::int32_t NormalMapMultiplier = 8; 

    //callback data
    struct SwapData final
    {
        static constexpr float TransitionTime = 2.f;
        float currentTime = 0.f;
        float start = -MaxShrubOffset;
        float destination = 0.f;
        cro::Entity otherEnt;
        cro::Entity instancedEnt; //entity containing instanced waterside geometry
        std::array<cro::Entity, 2u> billboardEnts = {};
        std::array<cro::Entity, 2u> billboardTreeEnts = {}; //low quality trees
        std::array<cro::Entity, 4u> shrubberyEnts; //instanced shrubbery/trees (if HQ)
        std::vector<cro::Entity>* crowdEnts = nullptr;
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
            pos.y = std::min(-TerrainLevel, std::max(-MaxShrubOffset, pos.y));

            e.getComponent<cro::Transform>().setPosition(pos);

            if (swapData.currentTime == SwapData::TransitionTime)
            {
                pos.y = swapData.destination;
                e.getComponent<cro::Transform>().setPosition(pos);
                e.getComponent<cro::Callback>().active = false;

                if (swapData.otherEnt.isValid())
                {
                    swapData.billboardEnts[0].getComponent<cro::Model>().setHidden(true);
                    swapData.billboardTreeEnts[0].getComponent<cro::Model>().setHidden(true);

                    swapData.otherEnt.getComponent<cro::Callback>().active = true;
                    swapData.billboardEnts[1].getComponent<cro::Model>().setHidden(false);
                    swapData.billboardTreeEnts[1].getComponent<cro::Model>().setHidden(false);
                    terrainEntity.getComponent<cro::Callback>().active = true; //starts terrain morph
                }

                if (swapData.instancedEnt.isValid())
                {
                    swapData.instancedEnt.getComponent<cro::Model>().setHidden(true);
                }

                for (auto ent : swapData.shrubberyEnts)
                {
                    if (ent.isValid())
                    {
                        ent.getComponent<cro::Model>().setHidden(true);
                    }
                }

                if (swapData.crowdEnts)
                {
                    auto& ents = *swapData.crowdEnts;
                    for (auto e : ents)
                    {
                        if (e.isValid())
                        {
                            e.getComponent<cro::Model>().setHidden(true);
                        }
                    }
                }
            }
        }

        cro::Entity terrainEntity;
    };
}

TerrainBuilder::TerrainBuilder(SharedStateData& sd, const std::vector<HoleData>& hd)
    : m_sharedData  (sd),
    m_holeData      (hd),
    m_currentHole   (0),
    m_swapIndex     (0),
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
    //            }*/

    //            /*auto pos = m_terrainEntity.getComponent<cro::Transform>().getPosition();
    //            if (ImGui::SliderFloat("Height", &pos.y, -50.f, 10.f))
    //            {
    //                m_terrainEntity.getComponent<cro::Transform>().setPosition(pos);
    //            }*/

    //            /*auto visible = m_terrainEntity.getComponent<cro::Model>().isVisible();
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
    //    }, true);
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

    static constexpr auto xCount = static_cast<std::uint32_t>(MapSize.x / QuadsPerMetre);
    static constexpr auto yCount = static_cast<std::uint32_t>(MapSize.y / QuadsPerMetre);
    std::vector<std::uint32_t> indices;// (xCount * yCount * 6);


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
    std::string wobble;
    if (m_sharedData.vertexSnap)
    {
        wobble = "#define WOBBLE\n";
    }

    resources.shaders.loadFromString(ShaderID::Terrain, TerrainVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define RX_SHADOWS\n#define ADD_NOISE\n#define TERRAIN\n#define TERRAIN_CLIP\n" + wobble);
    const auto& shader = resources.shaders.get(ShaderID::Terrain);
    m_terrainProperties.morphUniform = shader.getUniformID("u_morphTime");
    m_terrainProperties.shaderID = shader.getGLHandle();
    auto materialID = resources.materials.add(shader);
    auto flags = cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal |
        cro::VertexProperty::Tangent | cro::VertexProperty::Bitangent; //use tan/bitan slots to store target position/normal
    auto meshID = resources.meshes.loadMesh(cro::DynamicMeshBuilder(flags, 1, GL_TRIANGLE_STRIP));
    auto terrainMat = resources.materials.get(materialID);
    terrainMat.setProperty("u_noiseColour", theme.grassTint);

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
    entity.addComponent<cro::Model>(resources.meshes.getMesh(meshID), terrainMat);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::MiniGreen));

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

    //modified billboard shader - shader loading is done in GolfState::loadAssets()
    auto billboardMatID = resources.materials.add(resources.shaders.get(ShaderID::Billboard));
    auto billboardShadowID = resources.materials.add(resources.shaders.get(ShaderID::BillboardShadow));

    //custom shader for instanced plants
    auto reedMaterialID = resources.materials.add(resources.shaders.get(ShaderID::CelTexturedInstanced));
    auto reedShadowID = resources.materials.add(resources.shaders.get(ShaderID::ShadowMapInstanced));


    //HQ tree materials
    std::int32_t branchMaterialID = resources.materials.add(resources.shaders.get(ShaderID::TreesetBranch));
    std::int32_t leafMaterialID = resources.materials.add(resources.shaders.get(ShaderID::TreesetLeaf));
    std::int32_t treeShadowMaterialID = resources.materials.add(resources.shaders.get(ShaderID::TreesetShadow));
    std::int32_t leafShadowMaterialID = resources.materials.add(resources.shaders.get(ShaderID::TreesetLeafShadow));

    //and VATs shader for crowd
    auto crowdMaterialID = resources.materials.add(resources.shaders.get(ShaderID::Crowd));
    auto shadowMaterialID = resources.materials.add(resources.shaders.get(ShaderID::CrowdShadow));

    auto crowdArrayMaterialID = resources.materials.add(resources.shaders.get(ShaderID::CrowdArray));
    auto shadowArrayMaterialID = resources.materials.add(resources.shaders.get(ShaderID::CrowdShadowArray));

    //create billboard/instanced entities
    cro::ModelDefinition billboardDef(resources);
    cro::ModelDefinition shrubDef(resources);
    cro::ModelDefinition crowdDef(resources);
    std::int32_t i = 0;

    //TODO scan the directory for definition files?
    const std::array<std::string, 4u> spectatorPaths =
    {
        "assets/golf/crowd/spectator01.vat",
        "assets/golf/crowd/spectator02.vat",
        "assets/golf/crowd/spectator03.vat",
        "assets/golf/crowd/spectator04.vat"
    };

    auto& noiseTex = resources.textures.get("assets/golf/images/wind.png");
    noiseTex.setRepeated(true);
    noiseTex.setSmooth(true);
    auto b = 0;
    for (auto& entity : m_propRootEntities)
    {
        entity = scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, -MaxShrubOffset, 0.f });        
        
        auto transition = ShrubTransition();
        transition.terrainEntity = m_terrainEntity;
        entity.addComponent<cro::Callback>().function = transition;
        m_terrainEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


        //create a billboard entity for each chunk in the TerrainChunker
        //reload the the model def each time to ensure unique VBOs
        const auto createBillboardEnt = [&]()
            {
                if (billboardDef.loadFromFile(theme.billboardModel))
                {
                    auto e = scene.createEntity();
                    e.addComponent<cro::Transform>();
                    entity.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());

                    billboardDef.createModel(e);
                    //if the model def failed to load for some reason this will be
                    //missing, so we'll add it here just to stop the thread exploding
                    //if it can't find the component
                    if (!e.hasComponent<cro::BillboardCollection>())
                    {
                        e.addComponent<cro::BillboardCollection>();
                    }

                    if (e.hasComponent<cro::Model>())
                    {
                        e.getComponent<cro::Model>().setHidden(true);
                        e.getComponent<cro::Model>().getMeshData().boundingBox = { glm::vec3(0.f), glm::vec3(MapSize.x, 20.f, -static_cast<float>(MapSize.y)) };
                        e.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::MiniGreen));

                        auto material = resources.materials.get(billboardMatID);
                        applyMaterialData(billboardDef, material);
                        material.setProperty("u_noiseTexture", noiseTex);
                        e.getComponent<cro::Model>().setMaterial(0, material);

                        material = resources.materials.get(billboardShadowID);
                        applyMaterialData(billboardDef, material);
                        material.setProperty("u_noiseTexture", noiseTex);
                        material.doubleSided = true; //do this second because applyMaterial() overwrites it
                        e.getComponent<cro::Model>().setShadowMaterial(0, material);
                    }
                    return e;
                }
                return cro::Entity();
            };

        //this contains the regular billboards (always visible)
        auto bbe = createBillboardEnt();
        if (bbe.hasComponent<cro::Model>())
        {
            m_billboardEntities[b] = bbe;
        }
        //these have billboarded trees used for flight cam and when
        //tree quality is set to low.
        bbe = createBillboardEnt();
        if (bbe.hasComponent<cro::Model>())
        {
            bbe.getComponent<cro::Model>().setRenderFlags(RenderFlags::FlightCam);
            m_billboardTreeEntities[b] = bbe;
        }
        b++;

        //create a child entity for instanced geometry
        std::string instancePath = theme.instancePath.empty() ? "assets/golf/models/reeds_large.cmt" : theme.instancePath;
        //these are the plants etc that appear newar the water
        if (shrubDef.loadFromFile(instancePath, true))
        {
            auto material = resources.materials.get(reedMaterialID);
            auto shadowMat = resources.materials.get(reedShadowID);

            auto childEnt = scene.createEntity();
            childEnt.addComponent<cro::Transform>();
            shrubDef.createModel(childEnt);

            for (auto j = 0u; j < shrubDef.getMaterialCount(); ++j)
            {
                applyMaterialData(shrubDef, material, j);
                material.setProperty("u_noiseTexture", noiseTex);
                childEnt.getComponent<cro::Model>().setMaterial(j, material);

                applyMaterialData(shrubDef, shadowMat, j);
                shadowMat.setProperty("u_noiseTexture", noiseTex);
                childEnt.getComponent<cro::Model>().setShadowMaterial(j, shadowMat);
            }
            childEnt.getComponent<cro::Model>().setHidden(true);
            childEnt.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::MiniGreen));
            entity.getComponent<cro::Transform>().addChild(childEnt.getComponent<cro::Transform>());
            m_instancedEntities[i] = childEnt;
        }

        //instanced trees/shrubs
        for (auto j = 0u; j < std::min(ThemeSettings::MaxTreeSets, theme.treesets.size()); ++j)
        {
            if (shrubDef.loadFromFile(theme.treesets[j].modelPath, true))
            {
                auto childEnt = scene.createEntity();
                childEnt.addComponent<cro::Transform>();
                shrubDef.createModel(childEnt);

                for (auto idx : theme.treesets[j].branchIndices)
                {
                    auto material = resources.materials.get(branchMaterialID);
                    applyMaterialData(shrubDef, material, idx);
                    material.setProperty("u_noiseTexture", noiseTex);
                    childEnt.getComponent<cro::Model>().setMaterial(idx, material);

                    material = resources.materials.get(treeShadowMaterialID);
                    material.setProperty("u_noiseTexture", noiseTex);
                    applyMaterialData(shrubDef, material, idx);
                    childEnt.getComponent<cro::Model>().setShadowMaterial(idx, material);
                }

                auto& meshData = childEnt.getComponent<cro::Model>().getMeshData();
                for (auto idx : theme.treesets[j].leafIndices)
                {
                    auto material = resources.materials.get(leafMaterialID);
                    meshData.indexData[idx].primitiveType = GL_POINTS;
                    material.setProperty("u_diffuseMap", resources.textures.get(theme.treesets[j].texturePath));
                    material.setProperty("u_leafSize", theme.treesets[j].leafSize);
                    material.setProperty("u_randAmount", theme.treesets[j].randomness);
                    material.setProperty("u_colour", theme.treesets[j].colour);
                    material.setProperty("u_noiseTexture", noiseTex);
                    childEnt.getComponent<cro::Model>().setMaterial(idx, material);

                    material = resources.materials.get(leafShadowMaterialID);
                    if (m_sharedData.hqShadows)
                    {
                        material.setProperty("u_diffuseMap", resources.textures.get(theme.treesets[j].texturePath));
                        material.setProperty("u_noiseTexture", noiseTex);
                    }
                    material.setProperty("u_leafSize", theme.treesets[j].leafSize);
                    childEnt.getComponent<cro::Model>().setShadowMaterial(idx, material);
                }

                childEnt.getComponent<cro::Model>().setHidden(true);
                childEnt.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::MiniGreen | RenderFlags::FlightCam));
                entity.getComponent<cro::Transform>().addChild(childEnt.getComponent<cro::Transform>());
                m_instancedShrubs[i][j] = childEnt;
            }
        }

        //create entities to render instanced crowd models
        for (const auto& p : spectatorPaths)
        {
            VatFile vatFile;
            if (vatFile.loadFromFile(p) &&
                crowdDef.loadFromFile(vatFile.getModelPath(), true))
            {
                auto childEnt = scene.createEntity();
                childEnt.addComponent<cro::Transform>().setPosition({ MapSize.x / 2.f, /*-TerrainLevel*/0.f, -static_cast<float>(MapSize.y) / 2.f });
                crowdDef.createModel(childEnt);

                //setup material
                std::unique_ptr<cro::ArrayTexture<float, 4u>> tex = std::make_unique<cro::ArrayTexture<float, 4u>>();
                if (!m_sharedData.vertexSnap && vatFile.fillArrayTexture(*tex))
                {
                    auto material = resources.materials.get(crowdArrayMaterialID);
                    applyMaterialData(crowdDef, material);

                    material.setProperty("u_arrayMap", *tex);

                    auto shadowMaterial = resources.materials.get(shadowArrayMaterialID);
                    shadowMaterial.setProperty("u_arrayMap", *tex);

                    childEnt.getComponent<cro::Model>().setMaterial(0, material);
                    childEnt.getComponent<cro::Model>().setShadowMaterial(0, shadowMaterial);
                    
                    m_arrayTextures.push_back(std::move(tex));
                }
                else
                {
                    auto material = resources.materials.get(crowdMaterialID);
                    applyMaterialData(crowdDef, material);

                    material.setProperty("u_vatsPosition", resources.textures.get(vatFile.getPositionPath()));
                    material.setProperty("u_vatsNormal", resources.textures.get(vatFile.getNormalPath()));

                    auto shadowMaterial = resources.materials.get(shadowMaterialID);
                    shadowMaterial.setProperty("u_vatsPosition", resources.textures.get(vatFile.getPositionPath()));

                    childEnt.getComponent<cro::Model>().setMaterial(0, material);
                    childEnt.getComponent<cro::Model>().setShadowMaterial(0, shadowMaterial);
                }


                childEnt.getComponent<cro::Model>().setHidden(true);
                childEnt.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
                childEnt.addComponent<VatAnimation>().setVatData(vatFile);
                childEnt.addComponent<cro::CommandTarget>().ID = CommandID::Crowd;
                entity.getComponent<cro::Transform>().addChild(childEnt.getComponent<cro::Transform>());

                m_crowdEntities[i].push_back(childEnt);
            }
        }

        i++;
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

    applyTreeQuality();

    //create a mesh to display the slope data
    flags = cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal | cro::VertexProperty::UV0;
    meshID = resources.meshes.loadMesh(cro::DynamicMeshBuilder(flags, 1, GL_LINES));
    resources.shaders.loadFromString(ShaderID::Slope, SlopeVertexShader, SlopeFragmentShader);
    auto& slopeShader = resources.shaders.get(ShaderID::Slope);
    m_slopeProperties.positionUniform = slopeShader.getUniformID("u_centrePosition");
    m_slopeProperties.alphaUniform = slopeShader.getUniformID("u_alpha");
    m_slopeProperties.shader = slopeShader.getGLHandle();
    materialID = resources.materials.add(slopeShader);
    resources.materials.get(materialID).blendMode = cro::Material::BlendMode::Alpha;
    //resources.materials.get(materialID).enableDepthTest = false;

    //auto vid = slopeShader.getUniformID("u_value");
    //registerWindow([&,vid]()
    //    {
    //        if (ImGui::Begin("Grid"))
    //        {
    //            static float v = 1.f;
    //            if (ImGui::SliderFloat("V", &v, 0.1f, 6.f))
    //            {
    //                glUseProgram(m_slopeProperties.shader);
    //                glUniform1f(vid, v);
    //            }
    //        }
    //        ImGui::End();
    //    });



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
            alpha = std::min(m_sharedData.gridTransparency, alpha + dt);
            if (alpha == m_sharedData.gridTransparency)
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
        m_slopeProperties.currentAlpha = m_sharedData.gridTransparency == 0 ? 0.f : alpha / m_sharedData.gridTransparency;
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
    glm::mat4 projMat = glm::ortho(0.f, mapSize.x, 0.f, mapSize.y, -40.f, 40.f);
    auto normalViewProj = projMat * viewMat;

    //we can set this once so we don't need to store the matrix
    glCheck(glUseProgram(m_normalShader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_normalShader.getUniformID("u_projectionMatrix"), 1, GL_FALSE, &normalViewProj[0][0]));
    glCheck(glUseProgram(0));

    m_normalMap.create(MapSize.x * NormalMapMultiplier, MapSize.y * NormalMapMultiplier, 2);
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
        bool doAnim = holeIndex == 0 || (m_holeData[holeIndex - 1].modelPath != m_holeData[holeIndex].modelPath);

        if (doAnim)
        {
            auto first = (m_swapIndex + 1) % 2;
            auto second = m_swapIndex % 2;
            m_swapIndex++;

            if (m_propRootEntities[first].isValid()
                && m_propRootEntities[second].isValid())
            {
                //update the billboard data
                SwapData swapData;
                swapData.start = m_propRootEntities[first].getComponent<cro::Transform>().getPosition().y;
                swapData.destination = -TerrainLevel;
                swapData.currentTime = 0.f;

                m_billboardEntities[first].getComponent<cro::BillboardCollection>().setBillboards(m_billboardBuffer);
                m_billboardTreeEntities[first].getComponent<cro::BillboardCollection>().setBillboards(m_billboardTreeBuffer);
                m_propRootEntities[first].getComponent<cro::Callback>().setUserData<SwapData>(swapData);

                swapData.start = m_propRootEntities[second].getComponent<cro::Transform>().getPosition().y;
                swapData.destination = -MaxShrubOffset;
                swapData.otherEnt = m_propRootEntities[first];
                swapData.instancedEnt = m_instancedEntities[second];
                swapData.shrubberyEnts = m_instancedShrubs[second];
                swapData.crowdEnts = &m_crowdEntities[second];
                swapData.billboardEnts[0] = m_billboardEntities[second];
                swapData.billboardEnts[1] = m_billboardEntities[first];
                swapData.billboardTreeEnts[0] = m_billboardTreeEntities[second];
                swapData.billboardTreeEnts[1] = m_billboardTreeEntities[first];
                swapData.currentTime = 0.f;
                m_propRootEntities[second].getComponent<cro::Callback>().setUserData<SwapData>(swapData);
                m_propRootEntities[second].getComponent<cro::Callback>().active = true;

                //update any instanced geom
                if (!m_instanceTransforms.empty()
                    && m_instancedEntities[first].isValid())
                {
                    m_instancedEntities[first].getComponent<cro::Model>().setHidden(false);
                    m_instancedEntities[first].getComponent<cro::Model>().setInstanceTransforms(m_instanceTransforms);
                    m_instanceTransforms.clear();
                }

                for (auto i = 0u; i < MaxShrubInstances; ++i)
                {
                    if (!m_shrubTransforms[i].empty()
                        && m_instancedShrubs[first][i].isValid())
                    {
                        m_instancedShrubs[first][i].getComponent<cro::Model>().setHidden(false);
                        m_instancedShrubs[first][i].getComponent<cro::Model>().setInstanceTransforms(m_shrubTransforms[i]);
                    }
                    m_shrubTransforms[i].clear();
                }

                //crowd instances
                //TODO can we move some of this to the thread func (can't set transforms in it though)
                std::vector<std::vector<glm::mat4>> positions(m_crowdEntities[first].size());
                for (auto i = 0u; i < m_holeData[m_currentHole].crowdPositions.size(); ++i)
                {
                    positions[i % positions.size()].push_back(m_holeData[m_currentHole].crowdPositions[i]);
                }

                for (auto i = 0u; i < m_crowdEntities[first].size(); ++i)
                {
                    if (m_crowdEntities[first][i].isValid()
                        && !positions[i].empty())
                    {
                        m_crowdEntities[first][i].getComponent<cro::Model>().setInstanceTransforms(positions[i]);
                        m_crowdEntities[first][i].getComponent<cro::Model>().setHidden(false);
                    }
                }
            }

            //upload terrain data
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_terrainProperties.vbo));
            glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(TerrainVertex) * m_terrainBuffer.size(), m_terrainBuffer.data(), GL_STATIC_DRAW));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            m_terrainProperties.morphTime = 0.f;
            glCheck(glUseProgram(m_terrainProperties.shaderID));
            glCheck(glUniform1f(m_terrainProperties.morphUniform, m_terrainProperties.morphTime));
            //terrain callback is set active when shrubbery callback switches
        }
        //upload the slope buffer data - this might be different even if the hole model is the same
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

void TerrainBuilder::setSlopePosition(glm::vec3 position)
{
    //why do this here when we could set it in update() when we set the entity position??
    glCheck(glUseProgram(m_slopeProperties.shader));
    glCheck(glUniform3f(m_slopeProperties.positionUniform, position.x, position.y, position.z));
    glCheck(glUseProgram(0));
}

float TerrainBuilder::getSlopeAlpha() const
{
    return m_slopeProperties.currentAlpha;
}

void TerrainBuilder::applyTreeQuality()
{
    std::uint64_t hqFlags = 0;
    std::uint64_t bbFlags = RenderFlags::FlightCam | RenderFlags::Main | RenderFlags::Reflection;

    if (m_sharedData.treeQuality == SharedStateData::High)
    {
        hqFlags = RenderFlags::Main | RenderFlags::Reflection;
        bbFlags = RenderFlags::FlightCam;
    }
    
    for (auto& ents : m_instancedShrubs)
    {
        for (auto e : ents)
        {
            if (e.isValid())
            {
                e.getComponent<cro::Model>().setRenderFlags(hqFlags);
            }
        }
    }

    for (auto e : m_billboardTreeEntities)
    {
        if (e.isValid())
        {
            e.getComponent<cro::Model>().setRenderFlags(bbFlags);
        }
    }
}

//private
void TerrainBuilder::onChunkUpdate(const std::vector<std::int32_t>& visibleChunks)
{
    auto shrubIndex = m_swapIndex % 2;

    //for each instanced model
    for (auto i = 0u; i < MaxShrubInstances; ++i)
    {
        //if this instance exists...
        if (m_instancedShrubs[shrubIndex][i].isValid())
        {
            //grab its cells
            const auto& treeCells = m_cellData[shrubIndex][i];
            std::vector<const std::vector<glm::mat4>*> transforms;
            std::vector<const std::vector<glm::mat3>*> normals;

            //check each visible cell and stash it if not empty
            for (auto c : visibleChunks)
            {
                if (!treeCells[c].transforms.empty())
                {
                    transforms.push_back(&treeCells[c].transforms);
                    normals.push_back(&treeCells[c].normalMats);
                }
            }

            //if transforms exist update the model
            if (!transforms.empty())
            {
                m_instancedShrubs[shrubIndex][i].getComponent<cro::Model>().setHidden(false);
                m_instancedShrubs[shrubIndex][i].getComponent<cro::Model>().updateInstanceTransforms(transforms, normals);
            }
            //else hide is as there's no point drawing it.
            else
            {
                m_instancedShrubs[shrubIndex][i].getComponent<cro::Model>().setHidden(true);
            }
        }
    }
}

void TerrainBuilder::threadFunc()
{
    const auto readHeightMap = [&](std::uint32_t x, std::uint32_t y, std::int32_t gridRes = 1)
    {
        auto size = m_normalMap.getSize();
        x = std::min(size.x - 1, std::max(0u, x * (NormalMapMultiplier / gridRes)));
        y = std::min(size.y - 1, std::max(0u, y * (NormalMapMultiplier / gridRes)));

        auto idx = 4 * (y * size.x + x);
        return m_normalMapValues[idx + 3];
    };

    const auto readNormal = [&](std::uint32_t x, std::uint32_t y, std::int32_t gridRes = 1)
    {
        auto size = m_normalMap.getSize();
        x = std::min(size.x - 1, std::max(0u, x * (NormalMapMultiplier / gridRes)));
        y = std::min(size.y - 1, std::max(0u, y * (NormalMapMultiplier / gridRes)));

        auto idx = 4 * (y * size.x + x);
        glm::vec3 normal(m_normalMapValues[idx], m_normalMapValues[idx + 1], m_normalMapValues[idx + 2]);

        return normal;
    };

    //we can optimise this a bit by storing each prop
    //in a grid-indexed array to save all the looping...
    const auto GridSize = 8;
    const auto GridCount = MapSize.x * MapSize.y / GridSize;
    std::vector<std::vector<cro::Entity>> propGrid(GridCount);

    const auto nearProp = [&](glm::vec3 pos)->bool
    {
        std::int32_t gridX = static_cast<std::int32_t>(pos.x / GridSize);
        std::int32_t gridY = static_cast<std::int32_t>(-pos.z / GridSize);
        auto index = std::min(GridCount - 1, std::max(0u, gridY * MapSize.x + gridX));

        const auto& props = propGrid[index];
        for (const auto prop : props)
        {
            auto propPos = pos - prop.getComponent<cro::Transform>().getPosition(); //don't use world pos because it'll be scaled by parent
            float propRadius = prop.getComponent<cro::Model>().getBoundingSphere().radius * 1.5f;

            if (glm::length2(glm::vec2(propPos.x, propPos.z)) < (propRadius * propRadius))
            {
                return true;
            }
        }

        return false;
    };

    while (m_threadRunning)
    {
        if (m_wantsUpdate)
        {
            //should be empty anyway because we clear after assigning them
            m_instanceTransforms.clear();
            for (auto& tx : m_shrubTransforms)
            {
                tx.clear();
            }

            auto cellIndex = (m_swapIndex + 1) % 2;
            for (auto& cellData : m_cellData[cellIndex])
            {
                for (auto& cell : cellData)
                {
                    cell.normalMats.clear();
                    cell.transforms.clear();
                }
            }

            //we checked the file validity when the game starts.
            //if the map file is broken now something more drastic happened...
            cro::ImageArray<std::uint8_t> mapImage;
            if (mapImage.loadFromFile(m_holeData[m_currentHole].mapPath, true))
            {
                //partition the prop entities;
                propGrid.clear();
                propGrid.resize(GridCount);

                const auto& props = m_holeData[m_currentHole].propEntities;
                for (const auto prop : props)
                {
                    auto propPos = prop.getComponent<cro::Transform>().getPosition();
                    std::int32_t gridX = static_cast<std::int32_t>(propPos.x / GridSize);
                    std::int32_t gridY = static_cast<std::int32_t>(-propPos.z / GridSize);
                    auto index = std::min(GridCount - 1, std::max(0u, gridY * MapSize.x + gridX));
                    propGrid[index].push_back(prop);
                }

                //recreate the distribution(s)
                auto seed = static_cast<std::uint32_t>(std::time(nullptr));
                auto grass = pd::PoissonDiskSampling(GrassDensity, MinBounds, MaxBounds, 30u, seed);
                auto trees = pd::PoissonDiskSampling(TreeDensity, MinBounds, MaxBounds);
                auto flowers = pd::PoissonDiskSampling(TreeDensity * 0.5f, MinBounds, MaxBounds, 30u, seed / 2);

                //filter distribution by map area
                m_billboardBuffer.clear();
                m_billboardTreeBuffer.clear();
                
                for (auto [x, y] : grass)
                {
                    auto [terrain, terrainHeight] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Rough)
                    {
                        float scale = static_cast<float>(cro::Util::Random::value(14, 16)) / 10.f;
                        float height = readHeightMap(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));

                        if (height > WaterLevel)
                        {
                            auto n = readNormal(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                            //don't place on steep slopes
                            if (glm::dot(n, cro::Transform::Y_AXIS) > 0.3f)
                            {
                                glm::vec3 bbPos({ x, height - 0.02f, -y });
                                
                                auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Grass01, BillboardID::Grass02)]);
                                bb.position = bbPos;
                                bb.size *= scale;
                                bb.origin *= scale;
                            }
                        }
                    }
                    //reeds at water edge
                    if (terrain == TerrainID::Rough
                        || terrain == TerrainID::Scrub)
                    {
                        float height = readHeightMap(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                        height = std::max(height, terrainHeight + TerrainLevel);

                        if (height < 0.1f)
                        {
                            float scale = static_cast<float>(cro::Util::Random::value(9, 16)) / 10.f;

                            glm::mat4 tx = glm::translate(glm::mat4(1.f), { x, height - 0.01f, -y });
                            tx = glm::rotate(tx, cro::Util::Random::value(-cro::Util::Const::PI, cro::Util::Const::PI), cro::Transform::Y_AXIS);
                            tx = glm::scale(tx, glm::vec3(scale));
                            m_instanceTransforms.push_back(tx);
                        }
                    }
                }
                
                for (auto [x, y] : trees)
                {
                    auto [terrain, height] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Scrub)
                    {
                        //check if model mesh is higher than terrain
                        float height2 = readHeightMap(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                        height = std::max(height + TerrainLevel, height2);

                        //check we're actually above water height
                        if (height > -(TerrainLevel - WaterLevel))
                        {
                            glm::vec3 position(x, height - 0.01f, -y);

                            bool isNearProp = false;
                            for (auto v = position.z - 1; v < position.z + 2; ++v)
                            {
                                for (auto u = position.x - 1; u < position.x + 2; ++u)
                                {
                                    isNearProp = nearProp({ u, height, v });
                                    if (isNearProp)
                                    {
                                        break;
                                    }
                                }
                                if (isNearProp)
                                {
                                    break;
                                }
                            }

                            if (!isNearProp)
                            {
                                static std::size_t shrubIdx = 0;
                                auto currIndex = shrubIdx % MaxShrubInstances;

                                if (m_instancedShrubs[0][currIndex].isValid())
                                {
                                    glm::vec3 position(x, height - 0.05f, -y);
                                    float rotation = static_cast<float>(cro::Util::Random::value(0, 36) * 10) * cro::Util::Const::degToRad;
                                    float scale = static_cast<float>(cro::Util::Random::value(16, 20)) / 10.f;

                                    auto& mat4 = m_shrubTransforms[currIndex].emplace_back(1.f);
                                    mat4 = glm::translate(mat4, position);
                                    mat4 = glm::rotate(mat4, rotation, cro::Transform::Y_AXIS);
                                    mat4 = glm::scale(mat4, glm::vec3(scale));

                                    //find which chunk this is in based on position and update the 
                                    //appropriate cell data for culling
                                    auto norm = glm::inverseTranspose(mat4);

                                    auto xCell = std::floor(x / ChunkSize.x);
                                    auto yCell = std::floor(y / ChunkSize.y);

                                    auto idx = static_cast<std::int32_t>(yCell * ChunkVisSystem::ColCount + xCell);
                                    //LogI << idx <<std::endl;
                                    m_cellData[cellIndex][currIndex][idx].transforms.push_back(mat4);
                                    m_cellData[cellIndex][currIndex][idx].normalMats.push_back(norm);
                                }

                                //low quality version - always rendered on flight cam and optionally on LQ settings
                                glm::vec3 bbPos({ x, height - 0.05f, -y });

                                float scale = static_cast<float>(cro::Util::Random::value(12, 22)) / 10.f;
                                auto& bb = m_billboardTreeBuffer.emplace_back(m_billboardTemplates[BillboardID::Tree01 + currIndex]);
                                bb.position = bbPos; //small vertical offset to stop floating billboards
                                bb.size *= scale;
                                bb.origin *= scale;
                                    
                                if (cro::Util::Random::value(0, 1) == 0)
                                {
                                    //flip billboard
                                    auto rect = bb.textureRect;
                                    bb.textureRect.left = rect.left + rect.width;
                                    bb.textureRect.width = -rect.width;
                                }

                                shrubIdx++;
                            }
                        }
                    }
                }
                
                for (auto [x, y] : flowers)
                {
                    auto [terrain, height] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Scrub
                        /*&& height > 0.6f*/)
                    {
                        float height2 = readHeightMap(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                        height = std::max(height + TerrainLevel, height2);

                        if (height > /*-(TerrainLevel - WaterLevel)*/0)
                        {
                            glm::vec3 position(x, height - 0.001f, -y);

                            if (!nearProp(position))
                            {
                                glm::vec3 bbPos({ x, height - 0.05f, -y });
                                
                                float scale = static_cast<float>(cro::Util::Random::value(13, 17)) / 10.f;
                                auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Flowers01, BillboardID::Bush02)]);
                                bb.position = bbPos;
                                bb.size *= scale;
                                bb.origin *= scale;
                            }
                            else
                            {
                                //TODO not sure how this position is different, but hey
                                glm::vec3 bbPos({ x, height - 0.05f, -y });
                                
                                float scale = static_cast<float>(cro::Util::Random::value(14, 16)) / 10.f;
                                auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Grass01, BillboardID::Grass02)]);
                                bb.position = bbPos;
                                bb.size *= scale;
                                bb.origin *= scale;
                            }
                        }
                    }
                }

                //this isn't the same as the readHeightMap above - it scales the
                //result to MaxTerrainHeight, whereas the above returns world coords
                const auto heightAt = [&](std::uint32_t x, std::uint32_t y)
                {
                    auto size = mapImage.getDimensions();

                    x = std::min(size.x - 1, std::max(0u, x));
                    y = std::min(size.y - 1, std::max(0u, y));

                    auto index = y * size.x + x;
                    index *= 4;
                    return (static_cast<float>(mapImage[index + 1]) / 255.f) * MaxTerrainHeight;
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

                    glm::vec3 normal = { l - r, 2.f, -(d - u) };
                    normal = glm::normalize(normal);

                    m_terrainBuffer[i].position = m_terrainBuffer[i].targetPosition;
                    m_terrainBuffer[i].normal = m_terrainBuffer[i].targetNormal;
                    m_terrainBuffer[i].targetPosition.y = height;
                    m_terrainBuffer[i].targetNormal = normal;
                } 

                //update the vertex data for the slope indicator
                m_slopeBuffer.clear();
                m_slopeIndices.clear();

                std::uint32_t currIndex = 0u;
                auto pinPos = m_holeData[m_currentHole].pin;

                //we can optimise this by only looping the grid around the pin pos
                const std::int32_t startX = std::max(0, static_cast<std::int32_t>(std::floor(pinPos.x)) - HalfGridSize);
                const std::int32_t startY = std::max(0, static_cast<std::int32_t>(-std::floor(pinPos.z)) - HalfGridSize);
                static constexpr float DashCount = 80.f; //actual div by TAU cos its sin but eh.
                static constexpr float SlopeSpeed = -40.f;//REMEMBER this const is also used in the slope frag shader
                static constexpr std::int32_t AvgDistance = 1;
                static constexpr std::int32_t GridDensity = NormalMapMultiplier; //verts per metre, however grid size is half this.
                static constexpr float GridSpacing = 1.f / GridDensity;

                static constexpr float SurfaceOffset = 0.02f; //verts are pushed along normal by this much

                for (auto y = 0; y < (SlopeGridSize * GridDensity); ++y)
                {
                    for (auto x = 0; x < (SlopeGridSize * GridDensity); ++x)
                    {
                        auto worldX = startX + (x / GridDensity);
                        auto worldY = startY + (y / GridDensity);

                        auto terrain = readMap(mapImage, worldX, worldY).first;
                        if (terrain == TerrainID::Green)
                        {
                            float posX = static_cast<float>(x / GridDensity) + ((x % GridDensity) * GridSpacing) + startX;
                            float posZ = -(static_cast<float>(y / GridDensity) + ((y % GridDensity) * GridSpacing) + startY);

                            posX -= pinPos.x;
                            posZ -= pinPos.z;

                            worldX = startX * GridDensity + x;
                            worldY = startY * GridDensity + y;

                            auto height = (readHeightMap(worldX, worldY, GridDensity) - pinPos.y);
                            SlopeVertex vert;
                            vert.position = { posX, height, posZ };
                            vert.normal = readNormal(worldX, worldY, GridDensity);

                            //this is the number of times the 'dashes' repeat if enabled in the shader
                            //and the speed/direction based on height difference
                            vert.texCoord = { 0.f, 0.f };

                            glm::vec3 offset(GridSpacing, 0.f, 0.f);
                            height = (readHeightMap(worldX + 1, worldY, GridDensity) - pinPos.y);

                            //because of the low precision of the height map
                            //we average out the slope over a greater distance
                            glm::vec3 avgPosition = vert.position + glm::vec3(AvgDistance, 0.f, 0.f);
                            avgPosition.y = (readHeightMap(worldX + AvgDistance, worldY, GridDensity) - pinPos.y);

                            SlopeVertex vert2;
                            vert2.position = vert.position + offset;
                            vert2.position.y = height;
                            vert2.normal = readNormal(worldX + 1, worldY, GridDensity);
                            vert2.texCoord = { DashCount / GridDensity, std::min(glm::dot(glm::vec3(0.f, 1.f, 0.f), glm::normalize(avgPosition - vert.position)) * SlopeSpeed, 1.f) };
                            vert.texCoord.y = vert2.texCoord.y; //must be constant across segment
                            

                            //we have to copy first vert as the tex coords will be different
                            //shame we can't just recycle the index...
                            auto vert3 = vert;

                            offset = glm::vec3(0.f, 0.f, -GridSpacing);
                            height = (readHeightMap(worldX, worldY + 1, GridDensity) - pinPos.y);

                            avgPosition = vert.position + glm::vec3(0.f, 0.f, -AvgDistance);
                            avgPosition.y = (readHeightMap(worldX, worldY + AvgDistance, GridDensity) - pinPos.y);

                            SlopeVertex vert4;
                            vert4.position = vert.position + offset;
                            vert4.position.y = height;
                            vert4.normal = readNormal(worldX, worldY + 1, GridDensity);
                            vert4.texCoord = { DashCount / GridDensity, std::min(glm::dot(glm::vec3(0.f, 1.f, 0.f), glm::normalize(avgPosition - vert3.position)) * SlopeSpeed, 1.f) };
                            vert3.texCoord.y = vert4.texCoord.y;

                            vert.position += vert.normal * SurfaceOffset;
                            vert2.position += vert2.normal * SurfaceOffset;
                            vert3.position += vert3.normal * SurfaceOffset;
                            vert4.position += vert4.normal * SurfaceOffset;

                            //do this last once we know everything was modified
                            //TODO this is a lazy addition where we could really skip
                            //all vert processing entirely when not needed, but it
                            //doesn't actually make processing time *worse*
                            if ((y % (NormalMapMultiplier / 2) == 0))
                            {
                                m_slopeBuffer.push_back(vert);
                                m_slopeIndices.push_back(currIndex++);
                                m_slopeBuffer.push_back(vert2);
                                m_slopeIndices.push_back(currIndex++);
                            }

                            if ((x % (NormalMapMultiplier / 2)) == 0)
                            {
                                m_slopeBuffer.push_back(vert3);
                                m_slopeIndices.push_back(currIndex++);
                                m_slopeBuffer.push_back(vert4);
                                m_slopeIndices.push_back(currIndex++);
                            }
                        }
                    }
                }
                
                //static constexpr float LowestHeight = -0.04f;
                //static constexpr float HighestHeight = 0.04f;
                //static constexpr float MaxHeight = HighestHeight - LowestHeight;
                ////if (MaxHeight != 0)
                //{
                //    for (auto& v : m_slopeBuffer)
                //    {
                //        auto vertHeight = (v.position.y - epsilon) - LowestHeight;
                //        vertHeight /= MaxHeight;
                //        //v.colour = { 0.f, 0.4f * vertHeight, 1.f - vertHeight, 0.8f };
                //        v.colour = 
                //        { 
                //            cro::Util::Easing::easeInQuint(std::max(0.f, (vertHeight - 0.5f) * 2.f)),
                //            //0.f,
                //            0.5f,
                //            cro::Util::Easing::easeInQuint(0.8f + (std::min(1.f, vertHeight * 2.f) * 0.2f)),
                //            0.8f
                //        };
                //    }
                //}

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
    //skip this if we rendered the model the previous hole
    if (m_currentHole && 
        m_holeData[m_currentHole].modelEntity == m_holeData[m_currentHole - 1].modelEntity)
    {
        return;
    }


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


    //copy the texture to an array we can query
    m_normalMapValues.resize(m_normalMap.getSize().x * m_normalMap.getSize().y * 4);
    glBindTexture(GL_TEXTURE_2D, m_normalMap.getTexture(1).textureID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, m_normalMapValues.data());
}