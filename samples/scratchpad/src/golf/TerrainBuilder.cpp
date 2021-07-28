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

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

#include <chrono>

namespace
{
    //params for poisson disk samples
    constexpr float GrassDensity = 1.7f; //radius for PD sampler
    constexpr float WillowDensity = 3.8f;
    constexpr float PineDensity = 2.5f;

    //TODO for grass board we could shrink the area sllightly as we prefer trees further away
    constexpr std::array MinBounds = { 0.f, 0.f };
    constexpr std::array MaxBounds = { 320.f, 200.f };
}

TerrainBuilder::TerrainBuilder(const std::vector<HoleData>& hd)
    : m_holeData    (hd),
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
    //create billboard entities
    cro::ModelDefinition modelDef(resources);
    modelDef.loadFromFile("assets/golf/models/grass.cmt");

    for (auto& entity : m_billboardEntities)
    {
        entity = scene.createEntity();
        entity.addComponent<cro::Transform>();
        modelDef.createModel(entity);
        //if the model def failed to load for some reason this will be
        //missing, so we'll add it here just to stop the thread exploding
        //if it can't find the component
        if (!entity.hasComponent<cro::BillboardCollection>())
        {
            entity.addComponent<cro::BillboardCollection>();
        }
    }



    //TODO create a mesh for the height map


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
        m_billboardEntities[holeIndex % 2].getComponent<cro::Transform>().setPosition(glm::vec3(0.f));

        //TODO set the other ent to hide
        m_billboardEntities[(holeIndex + 1) % 2].getComponent<cro::Transform>().setPosition(glm::vec3(0.f, -100.f, 0.f));

        //TODO swap the height data buffers and upload to scene



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
                auto willow = pd::PoissonDiskSampling(WillowDensity, MinBounds, MaxBounds);
                auto pine = pd::PoissonDiskSampling(PineDensity, MinBounds, MaxBounds);

                //filter distribution by map area
                m_billboardBuffer.clear();
                for (auto [x, y] : grass)
                {
                    //TODO also apply height
                    auto [terrain, height] = readMap(mapImage, x, y);
                    if (terrain == TerrainID::Rough
                        || terrain == TerrainID::Scrub)
                    {
                        auto& bb = m_billboardBuffer.emplace_back();
                        bb.position = { x, 0.f, -y };
                        bb.size = { 1.f, 0.375f };
                        bb.origin = { 0.5f, 0.f };
                    }
                }
                //TODO combine billboard textures and load subrects
                


                //TODO update vertex data for scrub terrain mesh
            }

            m_wantsUpdate = false;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

std::pair<std::uint8_t, std::uint8_t> TerrainBuilder::readMap(const cro::Image& img, float px, float py) const
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

    auto height = img.getPixelData()[index + 1];

    return { terrain, height };
}