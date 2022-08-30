/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/core/SysTime.hpp>
#include <crogine/detail/OpenGL.hpp>

#ifdef PATH_TRACING
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include "../ErrorCheck.hpp"

namespace
{
    cro::Mesh::Data* meshData = nullptr;
}

void GolfState::initBallDebug()
{
    auto materialID = m_materialIDs[MaterialID::WireFrame];
    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    
    auto material = m_resources.materials.get(materialID);
    material.enableDepthTest = false;
    meshData = &m_resources.meshes.getMesh(meshID);
    meshData->boundingBox = { glm::vec3(0.f), glm::vec3(320.f, 100.f, -200.f) };
    meshData->boundingSphere = meshData->boundingBox;

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(*meshData, material);

    //this has been copied into the Model, so we need to point
    //to the correct copy else we update the wrong indexData
    meshData = &entity.getComponent<cro::Model>().getMeshData();
}

void GolfState::beginBallDebug()
{
    //set VBO index count to 0
    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = 0;

    m_ballDebugActive = true;
}

void GolfState::updateBallDebug(glm::vec3 position)
{
    static const std::array Colours =
    {
        cro::Colour::Red, cro::Colour::Blue
    };
    static std::size_t idx = 0;

    if (m_ballDebugActive)
    {
        m_ballDebugIndices.push_back(static_cast<std::uint32_t>(m_ballDebugPoints.size()));
        m_ballDebugPoints.emplace_back(position, Colours[idx].getVec4());
        idx = (idx + 1) % 2;
    }
}

void GolfState::endBallDebug()
{
    m_ballDebugActive = false;

    //update VBO
    meshData->vertexCount = m_ballDebugPoints.size();
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, m_ballDebugPoints.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = m_ballDebugIndices.size();
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), m_ballDebugIndices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    m_ballDebugPoints.clear();
    m_ballDebugIndices.clear();
}

#endif

void GolfState::dumpBenchmark()
{
    std::string outFile = cro::App::getPreferencePath() + "benchmark/";

    if (!cro::FileSystem::directoryExists(outFile))
    {
        cro::FileSystem::createDirectory(outFile);
    }
    outFile += m_sharedData.mapDirectory + "/";

    if (!cro::FileSystem::directoryExists(outFile))
    {
        cro::FileSystem::createDirectory(outFile);
    }
    outFile += std::to_string(m_currentHole) + ".bmk";

    const std::array<std::string, 3u> TreeTypes
    {
        "Classic", "Low", "High"
    };

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(outFile.c_str(), "a");
    if (file.file)
    {
        std::string dateTime = cro::SysTime::dateString();
        dateTime += " - " + cro::SysTime::timeString() 
#ifdef CRO_DEBUG_
            + " (DEBUG BUILD)"
#endif
            + ":\n";

        std::string stat = "Min fps: " + std::to_string(m_benchmark.minRate) 
            + " | Max fps: " + std::to_string (m_benchmark.maxRate) 
            + " | Average fps: " + std::to_string(m_benchmark.getAverage());

        std::string shadowQ = m_sharedData.hqShadows ? "High" : "Low";
        std::string vsync = cro::App::getWindow().getVsyncEnabled() ? " | vsync: ON" : " | vsync: OFF";
        std::string vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        std::string renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        std::string settings = "\nTree Quality: " + TreeTypes[m_sharedData.treeQuality] + " | Shadow Quality: " + shadowQ
            + vsync
            + "\nVendor: " + vendor + " | Renderer: " + renderer + "\n\n";

        SDL_RWwrite(file.file, dateTime.c_str(), dateTime.length(), 1);
        SDL_RWwrite(file.file, stat.c_str(), stat.length(), 1);
        SDL_RWwrite(file.file, settings.c_str(), settings.length(), 1);
    }
    else
    {
        LOG("Failed opening benchmark file " + outFile, cro::Logger::Type::Warning);
        LogE << SDL_GetError() << std::endl;
    }

    m_benchmark.reset();
}