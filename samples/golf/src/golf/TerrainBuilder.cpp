/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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
#include "VatFile.hpp"
#include "VatAnimationSystem.hpp"
#include "SharedStateData.hpp"

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
#include "ShadowMapping.inl"
    //params for poisson disk samples
    static constexpr float GrassDensity = 1.7f; //radius for PD sampler
    static constexpr float TreeDensity = 4.f;

    //TODO for grass billboard we could shrink the area slightly as we prefer trees further away
    static constexpr std::array MinBounds = { 0.f, 0.f };
    static constexpr std::array MaxBounds = { static_cast<float>(MapSize.x), static_cast<float>(MapSize.y) };

    static constexpr std::uint32_t QuadsPerMetre = 1;

    static constexpr float MaxShrubOffset = MaxTerrainHeight + 13.f;
    static constexpr std::int32_t SlopeGridSize = 20;
    static constexpr std::int32_t HalfGridSize = SlopeGridSize / 2;
    static constexpr std::int32_t NormalMapMultiplier = 4; //number of times the resolution of the map to increase normal map resolution by

    //callback data
    struct SwapData final
    {
        static constexpr float TransitionTime = 2.f;
        float currentTime = 0.f;
        float start = -MaxShrubOffset;
        float destination = 0.f;
        cro::Entity otherEnt;
        cro::Entity instancedEnt; //entity containing instanced geometry
        std::array<cro::Entity, 4> shrubberyEnts; //instanced shrubbery
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

                if (swapData.instancedEnt.isValid())
                {
                    swapData.instancedEnt.getComponent<cro::Model>().setHidden(true);
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
    //            ImGui::Image(m_normalDebugTexture, { 640.f, 400.f }, { 0.f, 1.f }, { 1.f, 0.f });
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
    resources.shaders.loadFromString(ShaderID::Terrain, TerrainVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define RX_SHADOWS\n#define ADD_NOISE\n#define TERRAIN\n");
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

    //modified billboard shader
    const auto& billboardShader = resources.shaders.get(ShaderID::Billboard);
    auto billboardMatID = resources.materials.add(billboardShader);

    //custom shader for instanced plants
    resources.shaders.loadFromString(ShaderID::CelTexturedInstanced, CelVertexShader, CelFragmentShader, "#define WIND_WARP\n#define TEXTURED\n#define DITHERED\n#define NOCHEX\n#define INSTANCING\n");
    auto reedMaterialID = resources.materials.add(resources.shaders.get(ShaderID::CelTexturedInstanced));

    //and VATs shader for crowd
    resources.shaders.loadFromString(ShaderID::Crowd, CelVertexShader, CelFragmentShader, "#define DITHERED\n#define INSTANCING\n#define VATS\n#define NOCHEX\n#define TEXTURED\n");
    auto crowdMaterialID = resources.materials.add(resources.shaders.get(ShaderID::Crowd));

    resources.shaders.loadFromString(ShaderID::CrowdShadow, ShadowVertex, ShadowFragment, "#define INSTANCING\n#define VATS\n");
    auto shadowMaterialID = resources.materials.add(resources.shaders.get(ShaderID::CrowdShadow));


    //create billboard/instanced entities
    cro::ModelDefinition billboardDef(resources);
    cro::ModelDefinition reedsDef(resources);
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
                entity.getComponent<cro::Model>().getMeshData().boundingBox = { glm::vec3(0.f), glm::vec3(MapSize.x, 30.f, -static_cast<float>(MapSize.y)) };
                entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap));
                entity.addComponent<cro::Callback>().function = transition;

                auto material = resources.materials.get(billboardMatID);
                applyMaterialData(billboardDef, material);
                entity.getComponent<cro::Model>().setMaterial(0, material);

                m_terrainEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            }

            //create a child entity for instanced geometry
            std::string instancePath = theme.instancePath.empty() ? "assets/golf/models/reeds_large.cmt" : theme.instancePath;

            if (reedsDef.loadFromFile(instancePath, true))
            {
                auto material = resources.materials.get(reedMaterialID);

                auto childEnt = scene.createEntity();
                childEnt.addComponent<cro::Transform>();
                reedsDef.createModel(childEnt);

                for (auto j = 0u; j < reedsDef.getMaterialCount(); ++j)
                {
                    applyMaterialData(reedsDef, material, j);
                    childEnt.getComponent<cro::Model>().setMaterial(j, material);
                }
                childEnt.getComponent<cro::Model>().setHidden(true);
                childEnt.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
                entity.getComponent<cro::Transform>().addChild(childEnt.getComponent<cro::Transform>());
                m_instancedEntities[i] = childEnt;
            }

            //instanced shrubs - TODO replace this with proper shrubbery
            //for (auto j = 0; j < /*MaxShrubInstances*/1; ++j)
            //{
            //    if (reedsDef.loadFromFile("assets/golf/models/palm01.cmt", true))
            //    {
            //        auto material = resources.materials.get(reedMaterialID);

            //        auto childEnt = scene.createEntity();
            //        childEnt.addComponent<cro::Transform>();
            //        reedsDef.createModel(childEnt);

            //        for (auto k = 0u; k < reedsDef.getMaterialCount(); ++k)
            //        {
            //            applyMaterialData(reedsDef, material, k);
            //            childEnt.getComponent<cro::Model>().setMaterial(k, material);
            //        }
            //        childEnt.getComponent<cro::Model>().setHidden(true);
            //        childEnt.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
            //        entity.getComponent<cro::Transform>().addChild(childEnt.getComponent<cro::Transform>());
            //        m_instancedShrubs[i][j] = childEnt;
            //    }
            //}


            //create entities to render instanced crowd models
            //TODO will breaking this up for better culling opportunity benefit perf?
            for (const auto& p : spectatorPaths)
            {
                VatFile vatFile;
                if (vatFile.loadFromFile(p) &&
                    crowdDef.loadFromFile(vatFile.getModelPath(), true))
                {
                    auto childEnt = scene.createEntity();
                    childEnt.addComponent<cro::Transform>().setPosition({ MapSize.x / 2.f, 0.f, -static_cast<float>(MapSize.y) / 2.f });
                    crowdDef.createModel(childEnt);

                    //setup material
                    auto material = resources.materials.get(crowdMaterialID);
                    applyMaterialData(crowdDef, material);

                    material.setProperty("u_vatsPosition", resources.textures.get(vatFile.getPositionPath()));
                    material.setProperty("u_vatsNormal", resources.textures.get(vatFile.getNormalPath()));

                    auto shadowMaterial = resources.materials.get(shadowMaterialID);
                    shadowMaterial.setProperty("u_vatsPosition", resources.textures.get(vatFile.getPositionPath()));

                    childEnt.getComponent<cro::Model>().setMaterial(0, material);
                    childEnt.getComponent<cro::Model>().setShadowMaterial(0, shadowMaterial);
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
        else
        {
            //hmmm we need access to shared state data to set an
            //error message and the state stack to push said error
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

    m_normalMap.create(MapSize.x * NormalMapMultiplier, MapSize.y * NormalMapMultiplier, false);
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
                swapData.instancedEnt = m_instancedEntities[second];
                for (auto i = 0u; i < m_instancedShrubs[second].size(); ++i)
                {
                    //TODO only iterate over as many as there are active
                    swapData.shrubberyEnts[i] = m_instancedShrubs[second][i];
                }
                swapData.crowdEnts = &m_crowdEntities[second];
                swapData.currentTime = 0.f;
                m_billboardEntities[second].getComponent<cro::Callback>().setUserData<SwapData>(swapData);
                m_billboardEntities[second].getComponent<cro::Callback>().active = true;

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
                        m_shrubTransforms[i].clear();
                    }
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

//private
void TerrainBuilder::threadFunc()
{
    const auto readHeightMap = [&](std::uint32_t x, std::uint32_t y)
    {
        auto size = m_normalMapImage.getSize();
        x = std::min(size.x - 1, std::max(0u, x * NormalMapMultiplier));
        y = std::min(size.y - 1, std::max(0u, y * NormalMapMultiplier));

        float height = static_cast<float>(m_normalMapImage.getPixel(x, y)[3]) / 255.f;

        return m_holeHeight.bottom + (m_holeHeight.height * height);
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
            //m_shrubTransforms.clear();

            //we checked the file validity when the game starts.
            //if the map file is broken now something more drastic happened...
            cro::Image mapImage;
            if (mapImage.loadFromFile(m_holeData[m_currentHole].mapPath))
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
                for (auto [x, y] : grass)
                {
                    auto [terrain, terrainHeight] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Rough)
                    {
                        float scale = static_cast<float>(cro::Util::Random::value(14, 16)) / 10.f;
                        float height = readHeightMap(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));

                        if (height > WaterLevel)
                        {
                            auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Grass01, BillboardID::Grass02)]);
                            bb.position = { x, height, -y };
                            bb.size *= scale;
                        }
                    }
                    //reeds at water edge
                    if (terrain == TerrainID::Rough
                        || terrain == TerrainID::Scrub)
                    {
                        float height = readHeightMap(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                        height = std::max(height, terrainHeight);

                        if (height < 0.1f)
                        {
                            float scale = static_cast<float>(cro::Util::Random::value(9, 16)) / 10.f;

                            glm::mat4 tx = glm::translate(glm::mat4(1.f), { x, height, -y });
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
                        height = std::max(height, height2);

                        //check we're actually above water height
                        if (height > -(TerrainLevel - WaterLevel))
                        {
                            glm::vec3 position(x, height, -y);

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
                                    float scale = static_cast<float>(cro::Util::Random::value(8, 12)) / 10.f;

                                    auto& mat4 = m_shrubTransforms[currIndex].emplace_back(1.f);
                                    mat4 = glm::translate(mat4, position);
                                    mat4 = glm::rotate(mat4, rotation, cro::Transform::Y_AXIS);
                                    mat4 = glm::scale(mat4, glm::vec3(scale));
                                }
                                else
                                {
                                    //no model loaded for this theme, so fall back to billboard
                                    float scale = static_cast<float>(cro::Util::Random::value(12, 22)) / 10.f;
                                    auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[BillboardID::Tree01 + currIndex]);
                                    bb.position = { x, height - 0.05f, -y }; //small vertical offset to stop floating billboards
                                    bb.size *= scale;
                                    
                                    if (cro::Util::Random::value(0, 1) == 0)
                                    {
                                        //flip billboard
                                        auto rect = bb.textureRect;
                                        bb.textureRect.left = rect.left + rect.width;
                                        bb.textureRect.width = -rect.width;
                                    }
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
                        height = std::max(height, height2);

                        if (height > -(TerrainLevel - WaterLevel))
                        {
                            glm::vec3 position(x, height, -y);

                            if (!nearProp(position))
                            {
                                float scale = static_cast<float>(cro::Util::Random::value(13, 17)) / 10.f;
                                auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Flowers01, BillboardID::Bush02)]);
                                bb.position = { x, height + 0.05f, -y };
                                bb.size *= scale;
                            }
                            else
                            {
                                float scale = static_cast<float>(cro::Util::Random::value(14, 16)) / 10.f;
                                auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Grass01, BillboardID::Grass02)]);
                                bb.position = { x, height, -y };
                                bb.size *= scale;
                            }
                        }
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

                    glm::vec3 normal = { l - r, 2.f, -(d - u) };
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
                static constexpr float DashCount = 40.f; //actual div by TAU cos its sin but eh.
                const float SlopeSpeed = -20.f * (m_holeData[m_currentHole].puttFromTee ? 0.15f : 1.f);
                const std::int32_t AvgDistance = m_holeData[m_currentHole].puttFromTee ? 1 : 5; //taking a long average on a small lumpy green will give wrong direction

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

                            //because of the low precision of the height map
                            //we average out the slope over a greater distance
                            glm::vec3 avgPosition = vert.position + glm::vec3(AvgDistance, 0.f, 0.f);
                            avgPosition.y = (readHeightMap(x + AvgDistance, y) - pinPos.y) + epsilon;

                            SlopeVertex vert2;
                            vert2.position = vert.position + offset;
                            vert2.position.y = height;
                            vert2.texCoord = { DashCount, std::min(glm::dot(glm::vec3(0.f, 1.f, 0.f), glm::normalize(avgPosition - vert.position)) * SlopeSpeed, 1.f) };
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
                            //shame we can't just recycle the index...
                            auto vert3 = vert;

                            offset = glm::vec3(0.f, 0.f, -1.f);
                            height = (readHeightMap(x, y + 1) - pinPos.y) + epsilon;

                            avgPosition = vert.position + glm::vec3(0.f, 0.f, -AvgDistance);
                            avgPosition.y = (readHeightMap(x, y + AvgDistance) - pinPos.y) + epsilon;

                            SlopeVertex vert4;
                            vert4.position = vert.position + offset;
                            vert4.position.y = height;
                            vert4.texCoord = { DashCount, std::min(glm::dot(glm::vec3(0.f, 1.f, 0.f), glm::normalize(avgPosition - vert3.position)) * SlopeSpeed, 1.f) };
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
                        //v.colour = { 0.f, 0.4f * vertHeight, 1.f - vertHeight, 0.8f };
                        v.colour = 
                        { 
                            cro::Util::Easing::easeInQuint(std::max(0.f, (vertHeight - 0.5f) * 2.f)),
                            0.f,
                            cro::Util::Easing::easeInQuint(std::min(1.f, vertHeight * 2.f)),
                            0.8f
                        };
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