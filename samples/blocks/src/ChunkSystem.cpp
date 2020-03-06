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

#include "ChunkSystem.hpp"
#include "ServerPacketData.hpp"
#include "WorldConsts.hpp"
#include "ChunkMeshBuilder.hpp"
#include "ResourceIDs.hpp"
#include "ErrorCheck.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/graphics/ResourceAutomation.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <optional>

namespace
{
    const std::string Vertex = 
        R"(
            ATTRIBUTE vec4 a_position;
            ATTRIBUTE LOW vec3 a_colour;
            ATTRIBUTE MED vec2 a_texCoord0;

            uniform mat4 u_worldViewMatrix;
            uniform mat4 u_projectionMatrix;

            VARYING_OUT LOW vec3 v_colour;
            VARYING_OUT MED vec2 v_texCoord;

            void main()
            {
                gl_Position = u_projectionMatrix * u_worldViewMatrix * a_position;
                v_colour = a_colour;
                v_texCoord = a_texCoord0;
            })";

    const std::string Fragment = 
        R"(
            VARYING_IN LOW vec3 v_colour;
            VARYING_IN MED vec2 v_texCoord;

            OUTPUT

            void main()
            {
                FRAG_OUT = vec4(v_colour, 1.0);
            })";
}

ChunkSystem::ChunkSystem(cro::MessageBus& mb, cro::ResourceCollection& rc)
    : cro::System   (mb, typeid(ChunkSystem)),
    m_resources     (rc),
    m_materialID    (0),
    m_threadRunning (false)
{
    requireComponent<ChunkComponent>();
    requireComponent<cro::Transform>();

    //create shaders for chunk meshes
    if (rc.shaders.preloadFromString(Vertex, Fragment, ShaderID::Chunk))
    {
        //and then material
        m_materialID = rc.materials.add(rc.shaders.get(ShaderID::Chunk));
    }

    //thread for greedy meshing
    m_mutex = std::make_unique<std::mutex>();
    m_greedyThread = std::make_unique<std::thread>(&ChunkSystem::threadFunc, this);
    m_threadRunning = true;
}

ChunkSystem::~ChunkSystem()
{
    m_threadRunning = false;
    m_greedyThread->join();
}

//public
void ChunkSystem::handleMessage(const cro::Message&)
{

}

void ChunkSystem::process(float)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& chunkComponent = entity.getComponent<ChunkComponent>();
        if (chunkComponent.needsUpdate)
        {
            //TODO push chunk into thread queue
            m_mutex->lock();
            m_inputQueue.push(chunkComponent.chunkPos);
            m_mutex->unlock();

            //updateMesh(m_chunkManager.getChunk(chunkComponent.chunkPos));
            chunkComponent.needsUpdate = false;
        }
    }

    //check result queue and update VBO data if needed
    updateMesh();
}

void ChunkSystem::parseChunkData(const cro::NetEvent::Packet& packet)
{
    //TODO properly validate the size of this packet
    if (packet.getSize() > sizeof(ChunkData))
    {
        ChunkData cd;
        std::memcpy(&cd, packet.getData(), sizeof(cd));

        if (packet.getSize() - sizeof(cd) == cd.dataSize * sizeof(RLEPair))
        {
            CompressedVoxels voxels(cd.dataSize);
            std::memcpy(voxels.data(), (char*)packet.getData() + static_cast<std::intptr_t>(sizeof(cd)), sizeof(RLEPair)* cd.dataSize);

            glm::ivec3 position(cd.x, cd.y, cd.z);
            std::lock_guard<std::mutex>(*m_mutex);
            if (!m_chunkManager.hasChunk(position))
            {
                auto& chunk = m_chunkManager.addChunk(position);
                chunk.getVoxels() = decompressVoxels(voxels);

                //create new entity for chunk
                auto entity = getScene()->createEntity();
                entity.addComponent<cro::Transform>().setPosition(glm::vec3(position * WorldConst::ChunkSize));
                entity.addComponent<ChunkComponent>().chunkPos = position;

                //create a model with an empty mesh, this should be build on next
                //update automagically!
                auto meshID = m_resources.meshes.loadMesh(ChunkMeshBuilder());
                CRO_ASSERT(meshID > 0, "Mesh generation failed");
                auto& mesh = m_resources.meshes.getMesh(meshID);

                auto& material = m_resources.materials.get(m_materialID);
                entity.addComponent<cro::Model>(mesh, material);
            }
        }
    }
}

//private
void ChunkSystem::updateMesh()
{
    std::vector<float> vertexData;
    std::vector<std::uint32_t> indices;
    glm::ivec3 position = glm::ivec3(0);
    //generateChunkMesh(chunk, vertexData, indices);

    m_mutex->lock();
    if (!m_outputQueue.empty())
    {
        auto& front = m_outputQueue.front();
        position = front.position;
        vertexData.swap(front.vertexData);
        indices.swap(front.indices);
        m_outputQueue.pop();
    }
    m_mutex->unlock();

    if (!vertexData.empty() && !indices.empty())
    {
        auto entity = m_chunkEntities[position];
        auto& meshData = entity.getComponent<cro::Model>().getMeshData();

        meshData.vertexCount = vertexData.size() / (meshData.vertexSize / sizeof(float));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        meshData.indexData[0].indexCount = static_cast<std::uint32_t>(indices.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[0].ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_DYNAMIC_DRAW));
    }
}

void ChunkSystem::threadFunc()
{
    while (m_threadRunning)
    {
        const Chunk* chunk = nullptr;
        glm::ivec3 position = glm::ivec3(0);

        //lock the input queue
        m_mutex->lock();

        //check queue
        if (!m_inputQueue.empty())
        {
            position = m_inputQueue.front();
            m_inputQueue.pop();

            chunk = &m_chunkManager.getChunk(position);
        }

        //unlock queue
        m_mutex->unlock();

        //if input do work
        if (chunk)
        {
            std::vector<float> vertexData;
            std::vector<std::uint32_t> indices;
            generateChunkMesh(*chunk, vertexData, indices);


            //lock output
            m_mutex->lock();

            //swap results
            auto& result = m_outputQueue.emplace();
            result.position = position;
            result.vertexData.swap(vertexData);
            result.indices.swap(indices);

            //unlock output
            m_mutex->unlock();
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(50));
        }
    }
}

ChunkSystem::VoxelFace ChunkSystem::getFace(const Chunk& chunk, glm::ivec3 position, VoxelFace::Side side)
{
    std::lock_guard<std::mutex>(*m_mutex);

    VoxelFace face;
    face.direction = side;
    face.id = chunk.getVoxelQ(position);
    
    std::uint8_t neighbour = m_voxelData.getID(vx::CommonType::Air);

    //skip if this is an air block
    if (face.id == neighbour)
    {
        face.visible = false;
        return face;
    }

    switch (side)
    {
    case VoxelFace::Top:
        neighbour = chunk.getVoxel({ position.x, position.y + 1, position.z });
        break;
    case VoxelFace::Bottom:
        neighbour = chunk.getVoxel({ position.x, position.y - 1, position.z });
        break;
    case VoxelFace::North:
        neighbour = chunk.getVoxel({ position.x, position.y, position.z + 1 });
        break;
    case VoxelFace::South:
        neighbour = chunk.getVoxel({ position.x, position.y, position.z - 1 });
        break;
    case VoxelFace::East:
        neighbour = chunk.getVoxel({ position.x + 1, position.y, position.z });
        break;
    case VoxelFace::West:
        neighbour = chunk.getVoxel({ position.x - 1, position.y + 1, position.z });
        break;
    }

    face.visible = (neighbour == m_voxelData.getID(vx::CommonType::Air)
                    || (neighbour == m_voxelData.getID(vx::CommonType::Water) && face.id != m_voxelData.getID(vx::CommonType::Water)));
    return face;
}

void ChunkSystem::generateChunkMesh(const Chunk& chunk, std::vector<float>& verts, std::vector<std::uint32_t>& indices)
{
    //greedy meshing from http://0fps.wordpress.com/2012/06/30/meshing-in-a-minecraft-game/
    //and https://github.com/roboleary/GreedyMesh/blob/master/src/mygame/Main.java


    //positions are BL, BR, TL, TR
    auto addQuad = [&](const std::vector<glm::vec3>& positions, float width, float height, VoxelFace face, bool backface) mutable
    {
        //add indices to the index array, remembering to offset into the current VBO
        std::array<std::int32_t, 6> localIndices;
        if (backface)
        {
            localIndices = { 2,0,1,  1,3,2 };
        }
        else
        {
            localIndices = { 2,3,1,  1,0,2 };
        }
        //8 is the number of floats per vert. Need to hook this up to the format somewhere..
        std::int32_t indexOffset = static_cast<std::int32_t>(verts.size() / 8);
        for (auto& i : localIndices)
        {
            i += indexOffset;
        }
        indices.insert(indices.end(), localIndices.begin(), localIndices.end());


        //for now we're colouring types, eventually this
        //will be the offset into the atlas and we'll use the w/h as UVs
        //to repeat the correct texture in the shader.
        glm::vec3 colour(1.f, 0.f, 0.f);
        if (face.id == m_voxelData.getID(vx::CommonType::Dirt))
        {
            colour = { 0.4f, 0.2f, 0.05f };
        }
        else if (face.id == m_voxelData.getID(vx::CommonType::Grass))
        {
            colour = { 0.1f, 0.7f, 0.1f };
        }
        else if (face.id == m_voxelData.getID(vx::CommonType::Sand))
        {
            colour = { 0.98f, 0.99f, 0.8f };
        }
        else if (face.id == m_voxelData.getID(vx::CommonType::Stone))
        {
            colour = { 0.7f, 0.7f, 0.7f };
        }
        else if (face.id == m_voxelData.getID(vx::CommonType::Water))
        {
            colour = { 0.17f, 0.47f, 0.87f };
        }

        std::array<float, 6u> multipliers = {0.89f, 0.95f, 0.85f, 0.96f, 1.f, 0.2f};
        colour *= multipliers[face.direction];

        //remember our vert order...
        std::array<glm::vec2, 4u> UVs =
        {
            glm::vec2(0.f, height),
            glm::vec2(width, height),
            glm::vec2(0.f),
            glm::vec2(width, 0.f)
        };

        for (auto i = 0u; i < positions.size(); ++i)
        {
            verts.push_back(positions[i].x);
            verts.push_back(positions[i].y);
            verts.push_back(positions[i].z);

            verts.push_back(colour.r);
            verts.push_back(colour.g);
            verts.push_back(colour.b);

            verts.push_back(UVs[i].x);
            verts.push_back(UVs[i].y);
        }
    };

    //TODO consider non-solid types such as fauna and add to different sub-meshes of the VBO

    //flip flop loop - means we can run verts in reverse order when backfacing
    for (bool backface = true, b = false; b != backface; backface = (backface && b), b = !b)
    {
        //3 directions which are performed for both front and backfacing
        //providing a total of 6 directions
        for (auto direction = 0; direction < 3; direction++)
        {
            std::int32_t u = (direction + 1) % 3;
            std::int32_t v = (direction + 2) % 3;

            std::array<std::int32_t, 3u> x = { 0,0,0 };
            std::array<std::int32_t, 3u> q = { 0,0,0 };
            q[direction] = 1;

            std::int32_t currentSide = -1;
            switch(direction)
            {
            case 0:
                currentSide = (backface) ? VoxelFace::West : VoxelFace::East;
                break;
            case 1:
                currentSide = (backface) ? VoxelFace::Bottom : VoxelFace::Top;
                break;
            case 2:
                currentSide = (backface) ? VoxelFace::South : VoxelFace::North;
                break;
            }

            //move across the current direction/plane front to back
            for (x[direction] = -1; x[direction] < WorldConst::ChunkSize;)
            {
                //this is the collection of faces grouped per side
                std::vector<std::optional<VoxelFace>> faceMask(WorldConst::ChunkArea);
                std::fill(faceMask.begin(), faceMask.end(), std::nullopt);
                std::size_t maskIndex = 0;

                for (x[v] = 0; x[v] < WorldConst::ChunkSize; x[v]++)
                {
                    for (x[u] = 0; x[u] < WorldConst::ChunkSize; x[u]++)
                    {
                        std::optional<VoxelFace> faceA = (x[direction] >= 0) ? 
                            std::optional<VoxelFace>(getFace(chunk, glm::ivec3(x[0], x[1], x[2]), (VoxelFace::Side)currentSide)) : std::nullopt;

                        std::optional<VoxelFace> faceB = (x[direction] < (WorldConst::ChunkSize - 1)) ?
                            std::optional<VoxelFace>(getFace(chunk, glm::ivec3(x[0] + q[0], x[1] + q[1], x[2] + q[2]), (VoxelFace::Side)currentSide)) : std::nullopt;

                        faceMask[maskIndex++] = (faceA != std::nullopt && faceB != std::nullopt && (*faceA == *faceB)) ?
                            std::nullopt :
                            backface ? faceB : faceA;
                    }
                }

                x[direction]++;


                //create a quad from the mask and add it to the vertex output
                maskIndex = 0;
                for (auto j = 0; j < WorldConst::ChunkSize; ++j)
                {
                    for (auto i = 0; i < WorldConst::ChunkSize;)
                    {
                        if (faceMask[maskIndex] != std::nullopt)
                        {
                            std::int32_t width = 0;
                            for (width = 1;
                                i + width < WorldConst::ChunkSize 
                                && faceMask[maskIndex + width] != std::nullopt 
                                && *faceMask[maskIndex + width] == *faceMask[maskIndex];
                                width++) {}

                            bool complete = false;
                            std::int32_t height = 0;
                            for (height = 1; j + height < WorldConst::ChunkSize; ++height)
                            {
                                for (auto k = 0; k < width; ++k)
                                {
                                    if (faceMask[maskIndex + k + height * WorldConst::ChunkSize] == std::nullopt
                                        || (*faceMask[maskIndex + k + height * WorldConst::ChunkSize] != *faceMask[maskIndex]))
                                    { 
                                        complete = true;
                                        break;
                                    }
                                }

                                if (complete)
                                {
                                    break;
                                }
                            }

                            //discard any marked as not visible
                            if (faceMask[maskIndex]->visible)
                                //&& faceMask[maskIndex]->id != m_voxelData.getID(vx::CommonType::Water)) //TODO remove this
                                //&& faceMask[maskIndex]->id != m_voxelData.getID(vx::CommonType::Air))
                            {
                                std::array<std::int32_t, 3u> du = { 0,0,0 };
                                std::array<std::int32_t, 3u> dv = { 0,0,0 };

                                x[u] = i;
                                x[v] = j;

                                du[u] = width;
                                dv[v] = height;

                                //note this assumes block sizes of 1x1x1
                                //scale this to change block size
                                std::vector<glm::vec3> positions =
                                {
                                    glm::vec3(x[0], x[1], x[2]), //BL
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]), //BR
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]), //TL
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]) //TR
                                };
                                addQuad(positions, static_cast<float>(width), static_cast<float>(height), *faceMask[maskIndex], backface);
                            }

                            //reset any faces used
                            for (auto l = 0; l < height; ++l)
                            {
                                for (auto k = 0; k < width; ++k)
                                {
                                    faceMask[maskIndex + k + l * WorldConst::ChunkSize] = std::nullopt;
                                }
                            }

                            i += width;
                            maskIndex += width;
                        }
                        else
                        {
                            i++;
                            maskIndex++;
                        }
                    }
                }
            }
        }
    }
}

void ChunkSystem::generateDebugMesh(const Chunk& chunk, std::vector<float>& vertexData, std::vector<std::uint32_t>& indices)
{
    using namespace WorldConst;
    for (auto y = 0; y < ChunkSize; ++y)
    {
        for (auto z = 0; z < ChunkSize; ++z)
        {
            for (auto x = 0; x < ChunkSize; ++x)
            {
                auto id = chunk.getVoxel({ x,y,z });
                glm::vec3 colour(1.f, 0.f, 0.f);
                if (id != m_voxelData.getID(vx::CommonType::Air))
                {
                    if (id == m_voxelData.getID(vx::CommonType::Dirt))
                    {
                        colour = { 0.5f, 0.4f, 0.2f };
                    }
                    else if (id == m_voxelData.getID(vx::CommonType::Grass))
                    {
                        colour = { 0.1f, 0.7f, 0.4f };
                    }
                    else if (id == m_voxelData.getID(vx::CommonType::Sand))
                    {
                        colour = { 0.9f, 0.89f, 0.8f };
                    }
                    else if (id == m_voxelData.getID(vx::CommonType::Stone))
                    {
                        colour = { 0.6f, 0.6f, 0.6f };
                    }
                    else if (id == m_voxelData.getID(vx::CommonType::Water))
                    {
                        colour = { 0.09f, 0.039f, 0.78f };
                    }


                    auto offset = static_cast<std::uint32_t>(vertexData.size() / 8);
                    indices.push_back(offset + 1);
                    indices.push_back(offset + 2);
                    indices.push_back(offset);

                    glm::vec3 position(x, y, z);
                    vertexData.push_back(position.x);
                    vertexData.push_back(position.y);
                    vertexData.push_back(position.z);

                    vertexData.push_back(colour.r);
                    vertexData.push_back(colour.g);
                    vertexData.push_back(colour.b);

                    vertexData.push_back(0.f);
                    vertexData.push_back(0.f);
                    //
                    vertexData.push_back(position.x);
                    vertexData.push_back(position.y);
                    vertexData.push_back(position.z + 0.1f);

                    vertexData.push_back(colour.r);
                    vertexData.push_back(colour.g);
                    vertexData.push_back(colour.b);

                    vertexData.push_back(0.f);
                    vertexData.push_back(0.f);
                    //

                    vertexData.push_back(position.x + 0.1f);
                    vertexData.push_back(position.y);
                    vertexData.push_back(position.z);

                    vertexData.push_back(colour.r);
                    vertexData.push_back(colour.g);
                    vertexData.push_back(colour.b);

                    vertexData.push_back(0.f);
                    vertexData.push_back(0.f);
                }
            }
        }
    }
}

void ChunkSystem::onEntityRemoved(cro::Entity entity)
{
    if (auto found = m_chunkEntities.find(entity.getComponent<ChunkComponent>().chunkPos); found != m_chunkEntities.end())
    {
        m_chunkEntities.erase(found);
    }
}

void ChunkSystem::onEntityAdded(cro::Entity entity)
{
    if (auto found = m_chunkEntities.find(entity.getComponent<ChunkComponent>().chunkPos); found != m_chunkEntities.end())
    {
        //this chunk already exists so remove the new entity
        getScene()->destroyEntity(entity);
    }
    else
    {
        m_chunkEntities.insert(std::make_pair(entity.getComponent<ChunkComponent>().chunkPos, entity));
    }
}