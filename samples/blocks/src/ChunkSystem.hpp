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

#include "ChunkManager.hpp"
#include "Voxel.hpp"
#include "ResourceIDs.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/network/NetData.hpp>

#include <mutex>
#include <memory>
#include <atomic>
#include <queue>
#include <thread>

namespace cro
{
    struct ResourceCollection;
}

struct ChunkComponent final
{
    bool needsUpdate = true;
    glm::ivec3 chunkPos = glm::ivec3(0);
};

class ChunkSystem final : public cro::System
{
public:
    ChunkSystem(cro::MessageBus&, cro::ResourceCollection&);
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


    ChunkManager m_chunkManager;
    vx::DataManager m_voxelData;

    struct VoxelUpdate final
    {
        glm::ivec3 position;
        std::uint8_t id = 0;
    };
    std::vector<VoxelUpdate> m_voxelUpdates; //TODO move this into the chunk component


    PositionMap<cro::Entity> m_chunkEntities;
    void updateMesh();

    std::unique_ptr<std::mutex> m_mutex;
    std::array<std::unique_ptr<std::thread>, 1u> m_meshThreads;
    std::atomic_bool m_threadRunning;
    void threadFunc();

    std::queue<glm::ivec3> m_inputQueue;
    struct VertexOutput final
    {
        std::vector<float> vertexData;
        std::vector<std::uint32_t> solidIndices;
        std::vector<std::uint32_t> waterIndices;
        std::vector<std::uint32_t> debugIndices;
        glm::ivec3 position = glm::ivec3(0);
    };
    std::queue<VertexOutput> m_outputQueue;

    struct VoxelFace final
    {
        //these are the same BL, BR, TL, TR as the positions
        //when creating a quad
        std::array<std::uint8_t, 4u> ao = { 3,3,3,3 };
        float offset = 0.f; //eg if a water surface should be slightly lower
        
        glm::ivec3 position = glm::ivec3(0); //voxel position of this face so we can track which direction the meshing is travelling

        bool visible = true;
        enum Side
        {
            South, North, East, West, Top, Bottom
        }direction = Top;
        std::uint8_t id;

        bool operator == (const VoxelFace& other)
        {
            return (other.id == id && other.visible == visible);
        }

        bool operator != (const VoxelFace& other)
        {
            return (other.id != id || other.visible != visible);
        }

        bool canAppend(const VoxelFace& other, std::int32_t direction);
    };
    VoxelFace getFace(const Chunk&, glm::ivec3, VoxelFace::Side);
    void calcAO(const Chunk&, VoxelFace&, glm::ivec3);
    void generateChunkMesh(const Chunk&, VertexOutput&);
    void generateNaiveMesh(const Chunk&, VertexOutput&);
    void generateDebugMesh(const Chunk&, VertexOutput&);

    void addQuad(VertexOutput&, std::vector<glm::vec3> positions, const std::array<std::uint8_t, 4u>& ao, float width, float height, VoxelFace face);

    void onEntityRemoved(cro::Entity);
    void onEntityAdded(cro::Entity);
};
