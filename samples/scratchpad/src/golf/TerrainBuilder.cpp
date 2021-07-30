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

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include <crogine/util/Random.hpp>

#include "../ErrorCheck.hpp"

#include <chrono>

namespace
{
    //params for poisson disk samples
    constexpr float GrassDensity = 4.7f; //radius for PD sampler
    constexpr float TreeDensity = 3.8f;

    //TODO for grass board we could shrink the area slightly as we prefer trees further away
    constexpr std::array MinBounds = { 0.f, 0.f };
    constexpr std::array MaxBounds = { 320.f, 200.f };

    constexpr glm::uvec2 Size(320, 200);
    constexpr float PixelPerMetre = 32.f; //64.f; //used for scaling billboards

    constexpr float MaxHeight = 9.f;
    constexpr std::size_t QuadsPerMetre = 2;
    constexpr float TerrainOffset = -0.05f;

    //callback for swapping shrub ents
    struct ShrubTransition final
    {
        void operator()(cro::Entity e, float dt)
        {
            auto [dest, other] = e.getComponent<cro::Callback>().getUserData<std::pair<float, cro::Entity>>();
            auto pos = e.getComponent<cro::Transform>().getPosition();
            auto diff = dest - pos.y;

            constexpr float Speed = 5.f;

            pos.y += diff * Speed * dt;
            pos.y = std::min(0.f, std::max(-MaxHeight, pos.y));

            e.getComponent<cro::Transform>().setPosition(pos);

            if (std::abs(diff) < 0.001f)
            {
                pos.y = dest;
                e.getComponent<cro::Transform>().setPosition(pos);
                e.getComponent<cro::Callback>().active = false;

                if (pos.y < 0)
                {
                    e.getComponent<cro::Model>().setHidden(true);

                    other.getComponent<cro::Callback>().active = true;
                    other.getComponent<cro::Model>().setHidden(false);
                }
            }
        }
    };
}

TerrainBuilder::TerrainBuilder(const std::vector<HoleData>& hd)
    : m_holeData    (hd),
    m_terrainBuffer ((Size.x * Size.y) / QuadsPerMetre),
    m_terrainVBO    (0),
    m_threadRunning (false),
    m_wantsUpdate   (false),
    m_currentHole   (0)
{

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
        std::size_t x = i % (Size.x / QuadsPerMetre);
        std::size_t y = i / (Size.x / QuadsPerMetre);

        m_terrainBuffer[i].position = { static_cast<float>(x * QuadsPerMetre), 0.f, -static_cast<float>(y * QuadsPerMetre) };
        m_terrainBuffer[i].targetPosition = m_terrainBuffer[i].position;
    }

    constexpr auto xCount = static_cast<std::uint32_t>(Size.x / QuadsPerMetre);
    constexpr auto yCount = static_cast<std::uint32_t>(Size.y / QuadsPerMetre);
    std::vector<std::uint32_t> indices;

    for (auto y = 0u; y < yCount - 1; y++)
    {
        if (y > 0)
        {
            indices.push_back(y * xCount);
        }

        for (auto x = 0u; x < xCount; x++)
        {
            indices.push_back(((y + 1) * xCount) + x);
            indices.push_back((y * xCount) + x);
        }

        if (y < yCount - 2)
        {
            indices.push_back(((y + 1) * xCount) + (xCount - 1));
        }
    }


    //TODO use a custom material for morphage
    auto shaderID = resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    auto materialID = resources.materials.add(resources.shaders.get(shaderID));
    auto flags = cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal |
        cro::VertexProperty::Tangent | cro::VertexProperty::Bitangent; //use tan/bitan slots to store target position/normal
    auto meshID = resources.meshes.loadMesh(cro::DynamicMeshBuilder(flags, 1, GL_TRIANGLE_STRIP));

    auto entity = scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, TerrainOffset, 0.f });
    entity.addComponent<cro::Model>(resources.meshes.getMesh(meshID), resources.materials.get(materialID));

    auto* meshData = &entity.getComponent<cro::Model>().getMeshData();
    meshData->vertexCount = static_cast<std::uint32_t>(m_terrainBuffer.size());
    m_terrainVBO = meshData->vbo;
    //vert data is uploaded to vbo via update()

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    //parent the shrubbery so they always stay the same relative height
    auto meshEnt = entity;


    //create billboard entities
    cro::ModelDefinition modelDef(resources);
    modelDef.loadFromFile("assets/golf/models/shrubbery.cmt");

    for (auto& entity : m_billboardEntities)
    {
        entity = scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, -MaxHeight, 0.f });
        modelDef.createModel(entity);
        //if the model def failed to load for some reason this will be
        //missing, so we'll add it here just to stop the thread exploding
        //if it can't find the component
        if (!entity.hasComponent<cro::BillboardCollection>())
        {
            entity.addComponent<cro::BillboardCollection>();
        }

        if (entity.hasComponent<cro::Model>())
        {
            entity.getComponent<cro::Model>().setHidden(true);
            entity.addComponent<cro::Callback>().function = ShrubTransition();

            meshEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }

    //load the billboard rects from a sprite sheet and convert to templates
    const auto convertSprite = [](const cro::Sprite& sprite)
    {
        auto bounds = sprite.getTextureRect();
        auto texSize = glm::vec2(sprite.getTexture()->getSize());

        cro::Billboard bb;
        bb.size = { bounds.width / PixelPerMetre, bounds.height / PixelPerMetre };
        bb.textureRect = { bounds.left / texSize.x, bounds.bottom / texSize.y, bounds.width / texSize.x, bounds.height / texSize.y };
        bb.origin = { bb.size.x / 2.f, 0.f };
        return bb;
    };
    
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/shrubbery.spt", resources.textures);
    m_billboardTemplates[BillboardID::Grass01] = convertSprite(spriteSheet.getSprite("grass01"));
    m_billboardTemplates[BillboardID::Pine] = convertSprite(spriteSheet.getSprite("pine"));
    m_billboardTemplates[BillboardID::Willow] = convertSprite(spriteSheet.getSprite("willow"));
    m_billboardTemplates[BillboardID::Birch] = convertSprite(spriteSheet.getSprite("birch"));


 
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
        //update the billboard data
        m_billboardEntities[holeIndex % 2].getComponent<cro::BillboardCollection>().setBillboards(m_billboardBuffer);
        m_billboardEntities[holeIndex % 2].getComponent<cro::Callback>().setUserData<std::pair<float, cro::Entity>>(0.f, cro::Entity());

        m_billboardEntities[(holeIndex + 1) % 2].getComponent<cro::Callback>().setUserData<std::pair<float, cro::Entity>>(-MaxHeight, m_billboardEntities[holeIndex % 2]);
        m_billboardEntities[(holeIndex + 1) % 2].getComponent<cro::Callback>().active = true;

        //upload terrain data
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_terrainVBO));
        glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * m_terrainBuffer.size(), m_terrainBuffer.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
        //TODO trigger morph animation/reset morph anim


        //signal to the thread we want to update the buffers
        //ready for next time
        m_currentHole++;
        m_wantsUpdate = m_currentHole < m_holeData.size();
    }
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
                auto grass = pd::PoissonDiskSampling(GrassDensity, MinBounds, MaxBounds, 30u, static_cast<std::uint32_t>(std::time(nullptr)));
                auto trees = pd::PoissonDiskSampling(TreeDensity, MinBounds, MaxBounds);

                //filter distribution by map area
                m_billboardBuffer.clear();
                for (auto [x, y] : grass)
                {
                    auto [terrain, height] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Rough)
                    {
                        auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[BillboardID::Grass01]);
                        bb.position = { x, height, -y };
                    }
                }
                
                for (auto [x, y] : trees)
                {
                    auto [terrain, height] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Scrub)
                    {
                        float scale = static_cast<float>(cro::Util::Random::value(9, 11)) / 10.f;

                        auto& bb = m_billboardBuffer.emplace_back(m_billboardTemplates[cro::Util::Random::value(BillboardID::Pine, BillboardID::Birch)]);
                        bb.position = { x, height, -y };
                        bb.size *= scale;
                    }
                }
                


                //update vertex data for scrub terrain mesh
                for (auto i = 0u; i < m_terrainBuffer.size(); ++i)
                {
                    //for each vert copy the target to the current (as this is where we should be)
                    //then update the target with the new map height at that position
                    std::size_t x = i % (Size.x / QuadsPerMetre);
                    std::size_t y = i / (Size.x / QuadsPerMetre);

                    auto height = readMap(mapImage, x, y).second;
                    /*m_terrainBuffer[i].position = m_terrainBuffer[i].targetPosition;
                    m_terrainBuffer[i].normal = m_terrainBuffer[i].targetNormal;
                    m_terrainBuffer[i].targetPosition.y = height;*/

                    //TODO include normal calc in here

                    //m_terrainBuffer[i].position.y = height;
                } 
            }

            m_wantsUpdate = false;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

std::pair<std::uint8_t, float> TerrainBuilder::readMap(const cro::Image& img, float px, float py) const
{
    std::uint32_t x = static_cast<std::uint32_t>(std::min(MaxBounds[0], std::max(0.f, std::floor(px))));
    std::uint32_t y = static_cast<std::uint32_t>(std::min(MaxBounds[1], std::max(0.f, std::floor(py))));

    std::uint32_t stride = 4;
    //TODO we should have already asserted the format is RGBA elsewhere...
    if (img.getFormat() == cro::ImageFormat::RGB)
    {
        stride = 3;
    }

    auto index = (y * static_cast<std::uint32_t>(MaxBounds[0]) + x) * stride;

    std::uint8_t terrain = img.getPixelData()[index] / 10;
    terrain = std::min(static_cast<std::uint8_t>(TerrainID::Scrub), terrain);

    auto height = static_cast<float>(img.getPixelData()[index + 1]) / 255.f;
    height *= MaxHeight;

    return { terrain, height };
}