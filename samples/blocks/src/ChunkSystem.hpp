/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#pragma once

#include "ResourceIDs.hpp"
#include "Coordinate.hpp"
#include "Voxel.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/network/NetData.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <mutex>
#include <memory>
#include <atomic>
#include <queue>
#include <thread>
#include <array>

class ChunkManager;
class Chunk;
namespace cro
{
    struct ResourceCollection;
}

namespace vx
{
    class DataManager;
}

//used for sorting polygons which are semi-transparent
struct Triangle final
{
    std::array<std::uint32_t, 3> indices = {};
    glm::vec3 normal = glm::vec3(0.f);
    float sortValue = 0.f;
};

struct ChunkComponent final
{
    bool needsUpdate = true;
    glm::ivec3 chunkPos = glm::ivec3(0);

    enum MeshType
    {
        Greedy, Naive
    }meshType = Greedy;

    std::vector<Triangle> transparentIndices;
};

class ChunkSystem final : public cro::System, public cro::GuiClient
{
public:
    ChunkSystem(cro::MessageBus&, cro::ResourceCollection&, ChunkManager&, vx::DataManager&);
    ~ChunkSystem();

    ChunkSystem(const ChunkSystem&) = delete;
    ChunkSystem(ChunkSystem&&) = default;
    const ChunkSystem& operator = (const ChunkSystem&) = delete;
    ChunkSystem& operator = (ChunkSystem&&) = default;

    void handleMessage(const cro::Message&) override;
    void process(float) override;

    void parseChunkData(const cro::NetEvent::Packet&);

private:

    cro::ResourceCollection& m_resources;
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};
    std::array<std::size_t, MeshID::Count> m_meshIDs = {};
    std::vector<glm::vec2> m_tileOffsets;

    ChunkManager& m_chunkManager;
    vx::DataManager& m_voxelData;

    struct VoxelUpdate final
    {
        glm::ivec3 position;
        std::uint8_t id = 0;
    };
    std::vector<VoxelUpdate> m_voxelUpdates; //TODO move this into the chunk component


    PositionMap<cro::Entity> m_chunkEntities;
    void updateMesh();


    std::unique_ptr<std::mutex> m_chunkMutex;
    std::array<std::unique_ptr<std::thread>, 4u> m_meshThreads;
    std::atomic_bool m_threadRunning;
    void threadFunc();

    std::queue<cro::Entity> m_inputQueue;
    struct VertexOutput final
    {
        std::vector<float> vertexData;
        std::vector<std::uint32_t> solidIndices;
        std::vector<std::uint32_t> waterIndices;
        std::vector<std::uint32_t> detailIndices;
        std::vector<Triangle> triangles; //only semi-transparent
        glm::ivec3 position = glm::ivec3(0);
    };
    std::queue<VertexOutput> m_outputQueue;

    vx::Face getFace(const Chunk&, glm::ivec3, vx::Side);
    void calcAO(const Chunk&, vx::Face&);
    void generateChunkMesh(const Chunk&, VertexOutput&);
    void generateNaiveMesh(const Chunk&, VertexOutput&);
    void generateDebugMesh(const Chunk&, VertexOutput&);

    void addQuad(VertexOutput&, const std::vector<glm::vec3>& positions, const std::vector<glm::vec2>& UVs, const std::array<std::uint8_t, 4u>& ao, vx::Face face);
    void addDetail(VertexOutput&, glm::vec3, std::uint16_t);

    void onEntityRemoved(cro::Entity) override;
    void onEntityAdded(cro::Entity) override;
};
