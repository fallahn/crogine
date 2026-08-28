/*-----------------------------------------------------------------------

Matt Marchant 2025 - 2026
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

#include "GrassProcessing.hpp"
#include "../golf/PoissonDisk.hpp"
#include "../golf/Terrain.hpp"

#include <crogine/detail/Types.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/detail/ModelBinary.hpp>

namespace
{

}

GrassProcessor::GrassProcessor()
    : m_processTotal(0),
    m_currentPath   (0)
{

}

//public
void GrassProcessor::begin(const std::string& path)
{
    auto files = cro::FileSystem::listFiles(path);
    files.erase(std::remove_if(files.begin(), files.end(), 
        [](const std::string& f)
        {
            return cro::FileSystem::getFileExtension(f) != ".cmb";
        }), files.end());

    for (const auto& f : files)
    {
        m_modelPaths.push_back(path + "/" + f.string());
    }
    queueJob();
}

bool GrassProcessor::process()
{
    for (auto i = 0u; i < m_processResults.size(); ++i)
    {
        if (m_processResults[i].valid())
        {
            m_processResults[i].get().swap(m_transformData[i]);
            m_processTotal++;

            LogI << "Finished chunk " << i << " with " << m_transformData[i].size() << " positions" << std::endl;
        }
    }


    //check if we're done then write results to a file
    if (m_processTotal == ChunkVisSystem::ChunkCount)
    {
        //file writing
        Header fileHeader = {};
        std::fill(fileHeader.begin(), fileHeader.end(), 0);

        fileHeader[0] = sizeof(Header);

        //hmm we could probably do this all in one loop, but meh
        for(auto i = 0; i < ChunkVisSystem::ChunkCount; ++i)
        {
            fileHeader[i] = static_cast<std::int32_t>(sizeof(glm::mat4) * m_transformData[i].size());
        }

        auto outPath = m_modelPaths[m_currentPath];
        cro::Util::String::replace(outPath, ".cmb", ".gss");

        cro::RaiiRWops file;
        file.file = SDL_IOFromFile(outPath.c_str(), "wb");
        if (file.file)
        {
            SDL_WriteIO(file.file, fileHeader.data(), sizeof(Header));
            for (const auto& t : m_transformData)
            {
                if (!t.empty())
                {
                    SDL_WriteIO(file.file, t.data(), sizeof(glm::mat4) * t.size());
                }
            }
        }
        else
        {
            LogE << "Failed opening " << outPath << " for writing" << std::endl;
        }

        //move to next model if available
        m_currentPath++;
        if (m_currentPath == m_modelPaths.size())
        {
            //we're done
            return true;
        }

        //else queue the next job
        m_processTotal = 0;
        queueJob();
    }

    return false;
}

//private
void GrassProcessor::queueJob()
{
    LogI << "Queing job for " << m_modelPaths[m_currentPath] << std::endl;
    static constexpr glm::vec2 ChunkSize(MapSize.x / ChunkVisSystem::ColCount, MapSize.y / ChunkVisSystem::RowCount);

    std::vector<float> verts;
    std::vector<std::vector<std::uint32_t>> indices;
    const auto meshData = cro::Detail::ModelBinary::read(m_modelPaths[m_currentPath], verts, indices);

    m_collisionMesh.updateCollisionMesh(meshData, verts, indices);

    for (auto y = 0; y < ChunkVisSystem::RowCount; ++y)
    {
        for (auto x = 0; x < ChunkVisSystem::ColCount; ++x)
        {
            const glm::vec2 chunkPos = glm::vec2(ChunkSize.x * x, ChunkSize.y * y) + (ChunkSize / 2.f);
            const auto chunkIdx = y * ChunkVisSystem::ColCount + x;

            m_processResults[chunkIdx] = std::async(std::launch::async,
                [chunkPos, &collisionMesh = std::as_const(m_collisionMesh)]()
                {
                    //use world space bounds so that the positions tile correctly
                    const std::array minb = { chunkPos.x - (ChunkSize.x / 2.f), chunkPos.y - (ChunkSize.y / 2.f) };
                    const std::array maxb = { chunkPos.x + (ChunkSize.x / 2.f), chunkPos.y + (ChunkSize.y / 2.f) };

                    static constexpr float density = 0.3f; //0.02f
                    const auto points = pd::PoissonDiskSampling(density, minb, maxb);

                    std::vector<glm::mat4> result;
                    for (const auto& [x, y] : points)
                    {
                        auto pointPos = glm::vec3(x, 0.f, -y);
                        const auto res = collisionMesh.getTerrain(pointPos);
                        if (res.terrain == TerrainID::Rough
                            && res.height > 0.f)
                        {
                            pointPos.y = res.height;

                            //remember to put this point relative to ent position...
                            auto t = glm::translate(glm::mat4(1.f), pointPos - glm::vec3(chunkPos.x, 0.f, -chunkPos.y));
                            t = glm::rotate(t, cro::Util::Random::value(-cro::Util::Const::PI, cro::Util::Const::PI), cro::Transform::Y_AXIS);
                            t = glm::scale(t, glm::vec3(cro::Util::Random::value(0.8f, 1.1f)));
                            result.emplace_back(t);
                        }
                    }
                    return result;
                });
        }
    }
}