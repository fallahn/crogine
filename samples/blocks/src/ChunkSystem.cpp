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
    m_materialID    (0)
{
    requireComponent<ChunkComponent>();
    requireComponent<cro::Transform>();

    //create shaders for chunk meshes
    if (rc.shaders.preloadFromString(Vertex, Fragment, ShaderID::Chunk))
    {
        //and then material
        m_materialID = rc.materials.add(rc.shaders.get(ShaderID::Chunk));
    }
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
            updateMesh(m_chunkManager.getChunk(chunkComponent.chunkPos));
            chunkComponent.needsUpdate = false;
        }
    }
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
            if (!m_chunkManager.hasChunk(position))
            {
                auto& chunk = m_chunkManager.addChunk(position);
                chunk.getVoxels() = decompressVoxels(voxels);

                //create new entity for chunk
                auto entity = getScene()->createEntity();
                entity.addComponent<cro::Transform>().setPosition(glm::vec3(position * WorldConst::ChunkSize));
                entity.addComponent<ChunkComponent>().chunkPos = position;

                //temp just to see where we are
                /*cro::ModelDefinition md;
                md.loadFromFile("assets/models/ground_plane.cmt", m_resources);
                md.createModel(entity, m_resources);
                entity.getComponent<ChunkComponent>().needsUpdate = false;*/

                //create a model with an empty mesh, this should be build on next
                //update automagically!
                auto meshID = m_resources.meshes.loadMesh(ChunkMeshBuilder());
                CRO_ASSERT(meshID > 0, "Mesh generation failed");
                auto& mesh = m_resources.meshes.getMesh(meshID);

                auto& material = m_resources.materials.get(m_materialID);
                entity.addComponent<cro::Model>(mesh, material);

                /*entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().function =
                    [](cro::Entity e, float)
                {
                    if (e.getComponent<cro::Model>().isVisible())
                    {
                        static int buns = 0;
                        std::cout << buns++ << "\n";
                    }
                };*/
            }
        }
    }
}

//private
void ChunkSystem::updateMesh(const Chunk& chunk)
{
    //auto colour = glm::normalize(glm::vec3(chunk.getPosition()) + glm::vec3(1.f));
    //colour += 1.f;
    //colour /= 2.f;

    ////test uploading simple vertex data first
    //std::vector<float> vertexData =
    //{
    //    0.f, 0.f, 0.f,  colour.r, colour.g, colour.b,  0.f, 0.f,
    //    32.f, 0.f, 0.f,  colour.r, colour.g, colour.b,  0.f, 0.f,
    //    32.f, 0.f, 32.f,  colour.r, colour.g, colour.b,  0.f, 0.f,
    //    0.f, 0.f, 32.f,  colour.r, colour.g, colour.b,  0.f, 0.f,
    //    0.f, 32.f, 32.f,  colour.r, colour.g, colour.b,  0.f, 0.f,
    //    32.f, 32.f, 32.f,  colour.r, colour.g, colour.b,  0.f, 0.f,
    //};

    //std::vector<std::uint32_t> indices =
    //{
    //    2,1,0,
    //    0,3,2,
    //    2,4,3,
    //    5,4,2
    //};

    //TODO create the vertex data in own thread
    //and signal to this thread when done/ready for upload
    std::vector<float> vertexData;
    std::vector<std::uint32_t> indices;
    generateChunkMesh(chunk, vertexData, indices);

    auto entity = m_chunkEntities[chunk.getPosition()];
    auto& meshData = entity.getComponent<cro::Model>().getMeshData();

    meshData.vertexCount = vertexData.size() / (meshData.vertexSize / sizeof(float));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    meshData.indexData[0].indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[0].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_DYNAMIC_DRAW));
}

ChunkSystem::VoxelFace ChunkSystem::getFace(const Chunk& chunk, glm::ivec3 position, VoxelFace::Side side)
{
    VoxelFace face;
    face.direction = side;
    face.id = chunk.getVoxelQ(position);

    std::uint8_t neighbour = 0;

    switch (side)
    {
    case VoxelFace::Top:
        neighbour = chunk.getVoxel({ position.x, position.y + 1, position.z });
        break;
    case VoxelFace::Bottom:
        neighbour = chunk.getVoxel({ position.x, position.y - 1, position.z });
        break;
    case VoxelFace::North:
        neighbour = chunk.getVoxel({ position.x, position.y, position.z - 1 });
        break;
    case VoxelFace::South:
        neighbour = chunk.getVoxel({ position.x, position.y, position.z + 1 });
        break;
    case VoxelFace::East:
        neighbour = chunk.getVoxel({ position.x + 1, position.y, position.z });
        break;
    case VoxelFace::West:
        neighbour = chunk.getVoxel({ position.x - 1, position.y + 1, position.z });
        break;
    }
    face.visible = (neighbour == 0);
    return face;
}

void ChunkSystem::generateChunkMesh(const Chunk&, std::vector<float>& verts, std::vector<std::uint32_t>& indices)
{
    //greedy meshing from http://0fps.wordpress.com/2012/06/30/meshing-in-a-minecraft-game/
    
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
        std::int32_t indexOffset = static_cast<std::int32_t>(verts.size());
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

    //REMEMBER when calling the above function the vertices need to be arranged
    //in the CORRECT ORDER in the vector, which they are not in the sample.
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