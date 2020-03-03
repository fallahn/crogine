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

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/graphics/ResourceAutomation.hpp>

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
                md.createModel(entity, m_resources);*/

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
void ChunkSystem::updateMesh(const Chunk& chunk)
{
    //TODO create the vertex data in own thread
    //and signal to this thread when done/ready for upload

    //TODO test uploading simple vertex data first
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