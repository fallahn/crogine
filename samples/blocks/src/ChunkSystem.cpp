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
            ATTRIBUTE vec4 a_colour;
            ATTRIBUTE vec3 a_normal;
            ATTRIBUTE MED vec2 a_texCoord0;

            uniform mat4 u_worldMatrix;
            uniform mat4 u_worldViewMatrix;
            uniform mat4 u_projectionMatrix;

            VARYING_OUT LOW vec3 v_colour;
            VARYING_OUT MED vec2 v_texCoord;
            VARYING_OUT MED vec4 v_viewPosition;
            VARYING_OUT float v_worldHeight;
            VARYING_OUT float v_ao;
            VARYING_OUT vec3 v_normal;

            float directionalAmbience(vec3 normal)
            {
	            vec3 v = normal * normal;
	            if (normal.y < 0)
                {
		            return dot(v, vec3(0.67082, 0.447213f, 0.83666));
                }
	            return dot(v, vec3(0.67082, 1.0, 0.83666));
            }

            void main()
            {
                gl_Position = u_projectionMatrix * u_worldViewMatrix * a_position;

                float ambience = directionalAmbience(a_normal);
                v_colour = a_colour.rgb * ambience;
                v_texCoord = a_texCoord0;
                v_viewPosition = u_worldViewMatrix * a_position;
                v_worldHeight = (u_worldMatrix * a_position).y;
                v_ao = a_colour.a;
                v_normal = a_normal;
            })";

    const std::string Fragment = 
        R"(
            uniform float u_alpha;

            VARYING_IN LOW vec3 v_colour;
            VARYING_IN MED vec2 v_texCoord;
            VARYING_IN MED vec4 v_viewPosition;
            VARYING_IN float v_worldHeight;
            VARYING_IN float v_ao;
            VARYING_IN vec3 v_normal;

            OUTPUT

            const LOW vec4 FogColour = vec4(0.07, 0.17, 0.67, 1.0);
            const float FogDensity = 0.03;

            void main()
            {
                vec4 colour = vec4(v_colour, u_alpha);
                float fogFactor = 1.0;

                if(v_worldHeight < 23.899) //water level
                {
                    float fogDistance = length(v_viewPosition);
                    fogFactor = 1.0 / exp(fogDistance * FogDensity);
                    fogFactor = clamp(fogFactor, 0.2, 0.8);

                    colour *= 0.9;
                }

                float light = pow(v_ao, 0.9);// + max(0.15 * dot(v_normal, vec3(1.0)), 0.0);
                FRAG_OUT = mix(FogColour, colour * light, fogFactor);
            })";

    const std::string VertexDebug = 
        R"(
            ATTRIBUTE vec4 a_position;

            uniform mat4 u_worldViewMatrix;
            uniform mat4 u_projectionMatrix;

            void main()
            {
                gl_Position = u_projectionMatrix * u_worldViewMatrix * a_position;
            })";

    const std::string FragmentDebug = 
        R"(
            uniform vec4 u_colour;
            OUTPUT

            void main()
            {
                FRAG_OUT = u_colour;
            })";
}

ChunkSystem::ChunkSystem(cro::MessageBus& mb, cro::ResourceCollection& rc)
    : cro::System   (mb, typeid(ChunkSystem)),
    m_resources     (rc),
    m_threadRunning (false)
{
    requireComponent<ChunkComponent>();
    requireComponent<cro::Transform>();

    //create shaders for chunk meshes
    if (rc.shaders.preloadFromString(Vertex, Fragment, ShaderID::Chunk))
    {
        //and then material
        m_materialIDs[MaterialID::ChunkSolid] = rc.materials.add(rc.shaders.get(ShaderID::Chunk));
        rc.materials.get(m_materialIDs[MaterialID::ChunkSolid]).setProperty("u_alpha", 1.f);

        m_materialIDs[MaterialID::ChunkWater] = rc.materials.add(rc.shaders.get(ShaderID::Chunk));
        rc.materials.get(m_materialIDs[MaterialID::ChunkWater]).blendMode = cro::Material::BlendMode::Alpha;
        rc.materials.get(m_materialIDs[MaterialID::ChunkWater]).setProperty("u_alpha", 0.5f);
    }

    if (rc.shaders.preloadFromString(VertexDebug, FragmentDebug, ShaderID::ChunkDebug))
    {
        m_materialIDs[MaterialID::ChunkDebug] = rc.materials.add(rc.shaders.get(ShaderID::ChunkDebug));
        rc.materials.get(m_materialIDs[MaterialID::ChunkDebug]).setProperty("u_colour", cro::Colour::Red());
        //TODO add this to a console tab or something so we can toggle it
        glCheck(glLineWidth(5.f));
    }

    //thread for meshing
    m_mutex = std::make_unique<std::mutex>();

    for (auto& thread : m_meshThreads)
    {
        thread = std::make_unique<std::thread>(&ChunkSystem::threadFunc, this);
    }

    m_threadRunning = true;
}

ChunkSystem::~ChunkSystem()
{
    m_threadRunning = false;

    for (auto& thread : m_meshThreads)
    {
        thread->join();
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
            std::lock_guard<std::mutex> lock(*m_mutex);
            if (!m_chunkManager.hasChunk(position))
            {
                auto& chunk = m_chunkManager.addChunk(position);
                chunk.getVoxels() = decompressVoxels(voxels);
                chunk.setHighestPoint(cd.highestPoint);

                //create new entity for chunk
                auto entity = getScene()->createEntity();
                entity.addComponent<cro::Transform>().setPosition(glm::vec3(position * WorldConst::ChunkSize));
                entity.addComponent<ChunkComponent>().chunkPos = position;

                //create a model with an empty mesh, this should be build on next
                //update automagically!
                auto meshID = m_resources.meshes.loadMesh(ChunkMeshBuilder());
                CRO_ASSERT(meshID > 0, "Mesh generation failed");
                auto& mesh = m_resources.meshes.getMesh(meshID);

                auto material = m_resources.materials.get(m_materialIDs[MaterialID::ChunkSolid]);
                entity.addComponent<cro::Model>(mesh, material);

                auto waterMaterial = m_resources.materials.get(m_materialIDs[MaterialID::ChunkWater]);
                entity.getComponent<cro::Model>().setMaterial(1, waterMaterial);

                auto debugMaterial = m_resources.materials.get(m_materialIDs[MaterialID::ChunkDebug]);
                entity.getComponent<cro::Model>().setMaterial(2, debugMaterial);
            }
        }
    }
}

//private
void ChunkSystem::updateMesh()
{
    VertexOutput vertexOutput;

    m_mutex->lock();
    if (!m_outputQueue.empty())
    {
        auto& front = m_outputQueue.front();
        vertexOutput.position = front.position;
        vertexOutput.vertexData.swap(front.vertexData);
        vertexOutput.solidIndices.swap(front.solidIndices);
        vertexOutput.waterIndices.swap(front.waterIndices);
        vertexOutput.debugIndices.swap(front.debugIndices);
        m_outputQueue.pop();
    }
    m_mutex->unlock();

    if (!vertexOutput.vertexData.empty() && (!vertexOutput.solidIndices.empty() || !vertexOutput.waterIndices.empty()))
    {
        auto entity = m_chunkEntities[vertexOutput.position];
        auto& meshData = entity.getComponent<cro::Model>().getMeshData();

        meshData.vertexCount = vertexOutput.vertexData.size() / (meshData.vertexSize / sizeof(float));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexOutput.vertexData.size() * sizeof(float), vertexOutput.vertexData.data(), GL_DYNAMIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        meshData.indexData[0].indexCount = static_cast<std::uint32_t>(vertexOutput.solidIndices.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[0].ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexOutput.solidIndices.size() * sizeof(std::uint32_t), vertexOutput.solidIndices.data(), GL_DYNAMIC_DRAW));

        meshData.indexData[1].indexCount = static_cast<std::uint32_t>(vertexOutput.waterIndices.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[1].ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexOutput.waterIndices.size() * sizeof(std::uint32_t), vertexOutput.waterIndices.data(), GL_DYNAMIC_DRAW));

        meshData.indexData[2].indexCount = static_cast<std::uint32_t>(vertexOutput.debugIndices.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[2].ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexOutput.debugIndices.size() * sizeof(std::uint32_t), vertexOutput.debugIndices.data(), GL_DYNAMIC_DRAW));
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

            //skip empty/airblock chunks
            if (chunk->getHighestPoint() == -1)
            {
                chunk = nullptr;
            }
        }

        //unlock queue
        m_mutex->unlock();

        //if input do work
        if (chunk)
        {
            VertexOutput vertexOutput;
            generateChunkMesh(*chunk, vertexOutput);
            //generateNaiveMesh(*chunk, vertexOutput);
            //generateDebugMesh(*chunk, vertexOutput);


            //lock output
            m_mutex->lock();

            //swap results
            auto& result = m_outputQueue.emplace();
            result.position = position;
            result.vertexData.swap(vertexOutput.vertexData);
            result.solidIndices.swap(vertexOutput.solidIndices);
            result.waterIndices.swap(vertexOutput.waterIndices);
            result.debugIndices.swap(vertexOutput.debugIndices);

            //unlock output
            m_mutex->unlock();
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(50));
        }
    }
}

void ChunkSystem::calcAO(const Chunk& chunk, VoxelFace& face, glm::ivec3 position)
{
    //this has confused itself somewhere. currently north
    //faces point +z, south faces are - z
    //east is + x and west is - x. The bottom edge of
    //a top face faces - z (south in these weird coords)

    //surrounding voxel values starting at the top left
    //of the face and moving clockwise.
    std::array<std::uint8_t, 8u> surroundingVoxels = {};
    std::uint8_t airblock = 0;
    std::uint8_t waterblock = 0;

    m_mutex->lock();
    airblock = m_voxelData.getID(vx::CommonType::Air);
    waterblock = m_voxelData.getID(vx::CommonType::Water);
    m_mutex->unlock();

    auto getSurrounding = [&](const std::vector<glm::ivec3>& positions)
    {
        //we want to lock this as briefly as possible
        std::lock_guard<std::mutex> lock(*m_mutex);
        for (auto i = 0u; i < surroundingVoxels.size(); ++i)
        {
            surroundingVoxels[i] = chunk.getVoxel(positions[i]);
        }
    };

    if (face.visible && face.id != waterblock)
    {
        if (face.direction == VoxelFace::Top)
        {
            position.y += 1;
            std::vector<glm::ivec3> positions =
            {
                position, position, position, position,
                position, position, position, position
            };
            positions[0].x += 1;
            positions[0].z += 1;

            positions[1].z += 1;

            positions[2].x -= 1;
            positions[2].z += 1;

            positions[3].x -= 1;

            positions[4].x -= 1;
            positions[4].z -= 1;

            positions[5].z -= 1;

            positions[6].x += 1;
            positions[6].z -= 1;

            positions[7].x += 1;

            getSurrounding(positions);
        }
        else if(face.direction == VoxelFace::Bottom)
        {
            position.y -= 1;
            std::vector<glm::ivec3> positions =
            {
                position, position, position, position,
                position, position, position, position
            };
            positions[0].x += 1;
            positions[0].z -= 1;

            positions[1].z -= 1;

            positions[2].x -= 1;
            positions[2].z -= 1;

            positions[3].x -= 1;

            positions[4].x -= 1;
            positions[4].z += 1;

            positions[5].z += 1;

            positions[6].x += 1;
            positions[6].z += 1;

            positions[7].x += 1;

            getSurrounding(positions);
        }
        else if (face.direction == VoxelFace::North)
        {
            position.z += 1;
            std::vector<glm::ivec3> positions =
            {
                position, position, position, position,
                position, position, position, position
            };
            positions[0].x -= 1;
            positions[0].y += 1;

            positions[1].y += 1;

            positions[2].x += 1;
            positions[2].y += 1;

            positions[3].x += 1;

            positions[4].x += 1;
            positions[4].y -= 1;

            positions[5].y -= 1;

            positions[6].x -= 1;
            positions[6].y -= 1;

            positions[7].x -= 1;

            getSurrounding(positions);
        }
        else if (face.direction == VoxelFace::East)
        {
            position.x += 1;
            std::vector<glm::ivec3> positions =
            {
                position, position, position, position,
                position, position, position, position
            };
            positions[0].z += 1;
            positions[0].y += 1;

            positions[1].y += 1;

            positions[2].z -= 1;
            positions[2].y += 1;

            positions[3].z -= 1;

            positions[4].z -= 1;
            positions[4].y -= 1;

            positions[5].y -= 1;

            positions[6].z += 1;
            positions[6].y -= 1;

            positions[7].z += 1;

            getSurrounding(positions);
        }
        else if (face.direction == VoxelFace::South)
        {
            position.z -= 1;
            std::vector<glm::ivec3> positions =
            {
                position, position, position, position,
                position, position, position, position
            };
            positions[0].x += 1;
            positions[0].y += 1;

            positions[1].y += 1;

            positions[2].x -= 1;
            positions[2].y += 1;

            positions[3].x -= 1;

            positions[4].x -= 1;
            positions[4].y -= 1;

            positions[5].y -= 1;

            positions[6].x += 1;
            positions[6].y -= 1;

            positions[7].x += 1;

            getSurrounding(positions);
        }
        else if (face.direction == VoxelFace::West)
        {
            position.x -= 1;
            std::vector<glm::ivec3> positions =
            {
                position, position, position, position,
                position, position, position, position
            };
            positions[0].z -= 1;
            positions[0].y += 1;

            positions[1].y += 1;

            positions[2].z += 1;
            positions[2].y += 1;

            positions[3].z += 1;

            positions[4].z += 1;
            positions[4].y -= 1;

            positions[5].y -= 1;

            positions[6].z -= 1;
            positions[6].y -= 1;

            positions[7].z -= 1;

            getSurrounding(positions);
        }


        auto vertexAO = [](std::int32_t side1, std::int32_t side2, std::int32_t corner)->std::uint8_t
        {
            if (side1 && side2)
            {
                return 0;
            }
            return 3 - (side1 + side2 + corner);
        };
        
        std::int32_t s1 = 0;
        std::int32_t s2 = 0;
        std::int32_t c = 0;

        //BL, BR, TL, TR
        s1 = (surroundingVoxels[5] == vx::CommonType::OutOfBounds || surroundingVoxels[5] == airblock) ? 0 : 1;
        c = (surroundingVoxels[6] == vx::CommonType::OutOfBounds || surroundingVoxels[6] == airblock) ? 0 : 1;
        s2 = (surroundingVoxels[7] == vx::CommonType::OutOfBounds || surroundingVoxels[7] == airblock) ? 0 : 1;
        face.ao[0] = vertexAO(s1, s2, c);

        s1 = (surroundingVoxels[3] == vx::CommonType::OutOfBounds || surroundingVoxels[3] == airblock) ? 0 : 1;
        c = (surroundingVoxels[4] == vx::CommonType::OutOfBounds || surroundingVoxels[4] == airblock) ? 0 : 1;
        s2 = (surroundingVoxels[5] == vx::CommonType::OutOfBounds || surroundingVoxels[5] == airblock) ? 0 : 1;
        face.ao[1] = vertexAO(s1, s2, c);

        s1 = (surroundingVoxels[7] == vx::CommonType::OutOfBounds || surroundingVoxels[7] == airblock) ? 0 : 1;
        c = (surroundingVoxels[0] == vx::CommonType::OutOfBounds || surroundingVoxels[0] == airblock) ? 0 : 1;
        s2 = (surroundingVoxels[1] == vx::CommonType::OutOfBounds || surroundingVoxels[1] == airblock) ? 0 : 1;
        face.ao[2] = vertexAO(s1, s2, c);

        s1 = (surroundingVoxels[1] == vx::CommonType::OutOfBounds || surroundingVoxels[1] == airblock) ? 0 : 1;
        c = (surroundingVoxels[2] == vx::CommonType::OutOfBounds || surroundingVoxels[2] == airblock) ? 0 : 1;
        s2 = (surroundingVoxels[3] == vx::CommonType::OutOfBounds || surroundingVoxels[3] == airblock) ? 0 : 1;
        face.ao[3] = vertexAO(s1, s2, c);
    }
}

ChunkSystem::VoxelFace ChunkSystem::getFace(const Chunk& chunk, glm::ivec3 position, VoxelFace::Side side)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    const std::uint8_t airBlock = m_voxelData.getID(vx::CommonType::Air);
    const std::uint8_t waterBlock = m_voxelData.getID(vx::CommonType::Water);

    VoxelFace face;
    face.direction = side;
    face.id = chunk.getVoxelQ(position);
    face.position = position;
    
    std::uint8_t neighbour = airBlock;

    //skip if this is an air block
    if (face.id == neighbour
        || face.id == vx::CommonType::OutOfBounds)
    {
        face.visible = false;
        return face;
    }

    switch (side)
    {
    case VoxelFace::Top:
        neighbour = chunk.getVoxel({ position.x, position.y + 1, position.z });
        if (face.id == waterBlock)
        {
            face.offset = 0.1f;
        }
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
        neighbour = chunk.getVoxel({ position.x - 1, position.y, position.z });
        break;
    }


    face.visible = (neighbour == airBlock
                    || (neighbour == waterBlock && face.id != waterBlock));
    return face;
}

void ChunkSystem::generateChunkMesh(const Chunk& chunk, VertexOutput& output)
{
    //greedy meshing from http://0fps.wordpress.com/2012/06/30/meshing-in-a-minecraft-game/
    //and https://github.com/roboleary/GreedyMesh/blob/master/src/mygame/Main.java

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
            std::int32_t maxSlice = WorldConst::ChunkSize; //on the Y plane slices don't process any air/empty layers
            switch(direction)
            {
            case 0:
                currentSide = (backface) ? VoxelFace::West : VoxelFace::East;
                break;
            case 1:
                currentSide = (backface) ? VoxelFace::Bottom : VoxelFace::Top;
                maxSlice = chunk.getHighestPoint() + 1;
                break;
            case 2:
                currentSide = (backface) ? VoxelFace::South : VoxelFace::North;
                break;
            }

            //move across the current direction/plane front to back
            for (x[direction] = -1; x[direction] < maxSlice;)
            {
                //this is the collection of faces grouped per side
                std::vector<std::optional<VoxelFace>> faceMask(WorldConst::ChunkArea);
                std::fill(faceMask.begin(), faceMask.end(), std::nullopt);
                std::size_t maskIndex = 0;

                for (x[v] = 0; x[v] < WorldConst::ChunkSize; x[v]++)
                {
                    for (x[u] = 0; x[u] < WorldConst::ChunkSize; x[u]++)
                    {
                        auto positionA = glm::ivec3(x[0], x[1], x[2]);
                        auto positionB = glm::ivec3(x[0] + q[0], x[1] + q[1], x[2] + q[2]);

                        std::optional<VoxelFace> faceA = (x[direction] >= 0) ? 
                            std::optional<VoxelFace>(getFace(chunk, positionA, (VoxelFace::Side)currentSide)) : std::nullopt;

                        std::optional<VoxelFace> faceB = (x[direction] < (WorldConst::ChunkSize - 1)) ?
                            std::optional<VoxelFace>(getFace(chunk, positionB, (VoxelFace::Side)currentSide)) : std::nullopt;

                        //calculate the AO values
                        //TODO we want to save some time here and only calc on the face
                        //which was added - but we have to do both to be able to compare
                        //them and know if we need to add the face or not...
                        if(faceA) calcAO(chunk, *faceA, positionA);
                        if(faceB) calcAO(chunk, *faceB, positionB);

                        faceMask[maskIndex++] = (faceA != std::nullopt && faceB != std::nullopt && (*faceA == *faceB)) ?
                            std::nullopt :
                            backface ? faceB : faceA;
                    }
                }

                x[direction]++;


                //create a quad from the mask and add it to the vertex output
                //if it's visible (no point processing invisible faces...)
                maskIndex = 0;
                for (auto j = 0; j < WorldConst::ChunkSize; ++j)
                {
                    for (auto i = 0; i < WorldConst::ChunkSize;)
                    {
                        if (faceMask[maskIndex] != std::nullopt
                            && faceMask[maskIndex]->visible)
                        {
                            std::array<std::uint8_t, 4u> aoValues = {3,3,3,3};
                            
                            aoValues[0] = faceMask[maskIndex]->ao[0];

                            auto prevFace = *faceMask[maskIndex];

                            //calc the merged width/height
                            std::int32_t width = 1;
                            for (;
                                i + width < WorldConst::ChunkSize 
                                && faceMask[maskIndex + width] != std::nullopt 
                                && prevFace.canAppend(*faceMask[maskIndex + width], currentSide);
                                width++)
                            {
                                prevFace = *faceMask[maskIndex + width];
                            }
                            auto endFace = *faceMask[maskIndex + (width - 1)];
                            aoValues[1] = endFace.ao[1];

                            bool complete = false;
                            std::int32_t height = 0;
                            prevFace = *faceMask[maskIndex];
                            for (height = 1; j + height < WorldConst::ChunkSize; ++height)
                            {
                                auto rowStart = maskIndex + ((height - 1) * WorldConst::ChunkSize);

                                for (auto k = 0; k < width; ++k)
                                {
                                    auto idx = maskIndex + k + height * WorldConst::ChunkSize;

                                    if (faceMask[idx] == std::nullopt
                                        || !prevFace.canAppend(*faceMask[idx], currentSide)
                                        /*|| (*faceMask[maskIndex] != *faceMask[idx])*/)
                                    { 

                                        aoValues[0] = faceMask[rowStart]->ao[0];
                                        aoValues[1] = faceMask[rowStart + (width - 1)]->ao[1];

                                        complete = true;
                                        break;
                                    }
                                    else
                                    {
                                        prevFace = *faceMask[idx];
                                    }
                                }
                                
                                //move the prev face to the beginning of the row
                                //so next iter we check canAppend() vertically
                                prevFace = *faceMask[rowStart + WorldConst::ChunkSize];


                                if (complete)
                                {
                                    break;
                                }
                            }

                            //create the quad geom
                            std::array<std::int32_t, 3u> du = { 0,0,0 };
                            std::array<std::int32_t, 3u> dv = { 0,0,0 };

                            x[u] = i;
                            x[v] = j;

                            du[u] = width;
                            dv[v] = height;

                            
                            std::vector<glm::vec3> positions;
                            switch (faceMask[maskIndex]->direction)
                            {
                            case VoxelFace::West:
                                positions =
                                {
                                    glm::vec3(x[0], x[1], x[2]), //BL
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]), //BR
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]), //TL
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]) //TR
                                };
                                break;
                            case VoxelFace::East:
                            case VoxelFace::Top:
                                positions =
                                {
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]),
                                    glm::vec3(x[0], x[1], x[2]),
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]),
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2])
                                };
                                break;
                            case VoxelFace::North:
                                positions =
                                {
                                    glm::vec3(x[0], x[1], x[2]),
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]),
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]),
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2])
                                };
                                break;
                            case VoxelFace::South:
                                positions =
                                {
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]),
                                    glm::vec3(x[0], x[1], x[2]),
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]),
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2])
                                };
                                break;
                            case VoxelFace::Bottom:
                                positions =
                                {
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]),
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]),
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]),
                                    glm::vec3(x[0], x[1], x[2])
                                };
                                break;
                            }
                            addQuad(output, positions, aoValues, static_cast<float>(width), static_cast<float>(height), *faceMask[maskIndex]);


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

void ChunkSystem::generateNaiveMesh(const Chunk& chunk, VertexOutput& output)
{
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
            std::int32_t maxSlice = WorldConst::ChunkSize; //on the Y plane slices don't process any air/empty layers
            switch (direction)
            {
            case 0:
                currentSide = (backface) ? VoxelFace::West : VoxelFace::East;
                break;
            case 1:
                currentSide = (backface) ? VoxelFace::Bottom : VoxelFace::Top;
                maxSlice = chunk.getHighestPoint() + 1;
                break;
            case 2:
                currentSide = (backface) ? VoxelFace::South : VoxelFace::North;
                break;
            }

            //move across the current direction/plane front to back
            for (x[direction] = -1; x[direction] < maxSlice;)
            {
                //this is the collection of faces grouped per side
                std::vector<std::optional<VoxelFace>> faceMask(WorldConst::ChunkArea);
                std::fill(faceMask.begin(), faceMask.end(), std::nullopt);
                std::size_t maskIndex = 0;

                for (x[v] = 0; x[v] < WorldConst::ChunkSize; x[v]++)
                {
                    for (x[u] = 0; x[u] < WorldConst::ChunkSize; x[u]++)
                    {
                        auto positionA = glm::ivec3(x[0], x[1], x[2]);
                        auto positionB = glm::ivec3(x[0] + q[0], x[1] + q[1], x[2] + q[2]);

                        std::optional<VoxelFace> faceA = (x[direction] >= 0) ?
                            std::optional<VoxelFace>(getFace(chunk, positionA, (VoxelFace::Side)currentSide)) : std::nullopt;

                        std::optional<VoxelFace> faceB = (x[direction] < (WorldConst::ChunkSize - 1)) ?
                            std::optional<VoxelFace>(getFace(chunk, positionB, (VoxelFace::Side)currentSide)) : std::nullopt;

                        //calculate the AO values
                        //TODO we want to save some time here and only calc on the face
                        //which was added - but we have to do both to be able to compare
                        //them and know if we need to add the face or not...
                        if (faceA) calcAO(chunk, *faceA, positionA);
                        if (faceB) calcAO(chunk, *faceB, positionB);

                        std::vector<glm::vec3> positions;

                        VoxelFace::Side faceDirection = VoxelFace::North;
                        glm::ivec3 position(0);

                        if (faceA != std::nullopt && faceB != std::nullopt && (*faceA == *faceB))
                        {
                            faceMask[maskIndex] = std::nullopt;
                        }
                        else
                        {
                            if (backface)
                            {
                                faceMask[maskIndex] = faceB;
                                faceDirection = faceB->direction;
                                position = positionB;
                            }
                            else
                            {
                                faceMask[maskIndex] = faceA;
                                faceDirection = faceA->direction;
                                position = positionA;
                            }
                        }

                        if (faceMask[maskIndex] && faceMask[maskIndex]->visible)
                        {
                            switch (faceDirection)
                            {
                            case VoxelFace::Bottom:
                                positions.emplace_back(position.x + 1, position.y, position.z + 1);
                                positions.emplace_back(position.x, position.y, position.z + 1);
                                positions.emplace_back(position.x + 1, position.y, position.z);
                                positions.emplace_back(position.x, position.y, position.z);
                                break;
                            case VoxelFace::Top:
                                positions.emplace_back(position.x + 1, position.y + 1, position.z);
                                positions.emplace_back(position.x, position.y + 1, position.z);
                                positions.emplace_back(position.x + 1, position.y + 1, position.z + 1);
                                positions.emplace_back(position.x, position.y + 1, position.z + 1);
                                break;
                            case VoxelFace::North:
                                positions.emplace_back(position.x, position.y, position.z + 1);
                                positions.emplace_back(position.x + 1, position.y, position.z + 1);
                                positions.emplace_back(position.x, position.y + 1, position.z + 1);
                                positions.emplace_back(position.x + 1, position.y + 1, position.z + 1);
                                break;
                            case VoxelFace::South:
                                positions.emplace_back(position.x + 1, position.y, position.z);
                                positions.emplace_back(position.x, position.y, position.z);
                                positions.emplace_back(position.x + 1, position.y + 1, position.z);
                                positions.emplace_back(position.x, position.y + 1, position.z);
                                
                                break;
                            case VoxelFace::East:
                                positions.emplace_back(position.x + 1, position.y, position.z + 1);
                                positions.emplace_back(position.x + 1, position.y, position.z);
                                positions.emplace_back(position.x + 1, position.y + 1, position.z + 1);
                                positions.emplace_back(position.x + 1, position.y + 1, position.z);
                                break;
                            case VoxelFace::West:
                                positions.emplace_back(position.x, position.y, position.z);
                                positions.emplace_back(position.x, position.y, position.z + 1);
                                positions.emplace_back(position.x, position.y + 1, position.z);
                                positions.emplace_back(position.x, position.y + 1, position.z + 1);
                                break;
                            }
                            if(!positions.empty())
                            addQuad(output, positions, faceMask[maskIndex]->ao, 1.f, 1.f, *faceMask[maskIndex]);
                        }

                        maskIndex++;
                    }
                }

                x[direction]++;
            }
        }
    }
}

void ChunkSystem::generateDebugMesh(const Chunk& chunk, VertexOutput& output)
{
    auto& indices = output.solidIndices;
    auto& vertexData = output.vertexData;

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


                    auto offset = static_cast<std::uint32_t>(output.vertexData.size() / ChunkMeshBuilder::getVertexComponentCount());
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
                    vertexData.push_back(1.f);

                    vertexData.push_back(0.f);
                    vertexData.push_back(1.f);
                    vertexData.push_back(0.f);

                    vertexData.push_back(0.f);
                    vertexData.push_back(0.f);
                    //
                    vertexData.push_back(position.x);
                    vertexData.push_back(position.y);
                    vertexData.push_back(position.z + 0.9f);

                    vertexData.push_back(colour.r);
                    vertexData.push_back(colour.g);
                    vertexData.push_back(colour.b);
                    vertexData.push_back(1.f);

                    vertexData.push_back(0.f);
                    vertexData.push_back(1.f);
                    vertexData.push_back(0.f);

                    vertexData.push_back(0.f);
                    vertexData.push_back(0.f);
                    //

                    vertexData.push_back(position.x + 0.9f);
                    vertexData.push_back(position.y);
                    vertexData.push_back(position.z);

                    vertexData.push_back(colour.r);
                    vertexData.push_back(colour.g);
                    vertexData.push_back(colour.b);
                    vertexData.push_back(1.f);

                    vertexData.push_back(0.f);
                    vertexData.push_back(1.f);
                    vertexData.push_back(0.f);

                    vertexData.push_back(0.f);
                    vertexData.push_back(0.f);
                }
            }
        }
    }
}

void ChunkSystem::addQuad(VertexOutput& output, std::vector<glm::vec3> positions, const std::array<std::uint8_t, 4u>& ao, float width, float height, VoxelFace face)
{
    //add indices to the index array, remembering to offset into the current VBO
    std::array<std::int32_t, 6> localIndices = { 2,0,1,  1,3,2 };

    //switch tri direction to maintain ao interp artifacting symmetry
    static const std::array<float, 4u> aoLevels = { 0.25f, 0.6f, 0.8f, 1.f };
    if (aoLevels[ao[2]] + aoLevels[ao[1]] < aoLevels[ao[0]] + aoLevels[ao[3]])
    {
        localIndices = { 3,0,1, 2,0,3 };
    }

    //update the index output
    std::int32_t indexOffset = static_cast<std::int32_t>(output.vertexData.size() / ChunkMeshBuilder::getVertexComponentCount());
    for (auto& i : localIndices)
    {
        i += indexOffset;
    }

    std::array<std::uint32_t, 8u> debugIndices = { 0, 1, 1, 3, 3, 2, 2, 0 };
    for (auto& i : debugIndices)
    {
        i += indexOffset;
    }

    if (face.id == m_voxelData.getID(vx::CommonType::Water))
    {
        output.waterIndices.insert(output.waterIndices.end(), localIndices.begin(), localIndices.end());
    }
    else
    {
        output.solidIndices.insert(output.solidIndices.end(), localIndices.begin(), localIndices.end());
    }
    output.debugIndices.insert(output.debugIndices.end(), debugIndices.begin(), debugIndices.end());

    //for now we're colouring types, eventually this
    //will be the offset into the atlas and we'll use the w/h as UVs
    //to repeat the correct texture in the shader.
    glm::vec3 colour(1.f, 0.f, 0.f);
    //reading this without a lock should be OK as this data
    //is only written to on construction
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
        colour = { 0.07f, 0.17f, 0.87f };
    }

    //remember our vert order...
    std::array<glm::vec2, 4u> UVs =
    {
        glm::vec2(0.f, height),
        glm::vec2(width, height),
        glm::vec2(0.f),
        glm::vec2(width, 0.f)
    };

    glm::vec3 normal = glm::vec3(0.f);
    switch (face.direction)
    {
    case VoxelFace::North:
        normal.z = -1.f;
        break;
    case VoxelFace::South:
        normal.z = 1.f;
        break;
    case VoxelFace::East:
        normal.x = 1.f;
        break;
    case VoxelFace::West:
        normal.x = -1.f;
        break;
    case VoxelFace::Top:
        normal.y = 1.f;
        break;
    case VoxelFace::Bottom:
        normal.y = -1.f;
        break;
    }

    //std::array<glm::vec3, 4u> colours =
    //{
    //    glm::vec3(1.f, 0.f, 0.f),
    //    glm::vec3(0.f, 1.f, 0.f),
    //    glm::vec3(1.f, 1.f, 1.f),
    //    glm::vec3(0.f, 0.f, 1.f)
    //};

    //NOTE: when adding more attributes
    //remember to update the component count in
    //the mesh builder class.
    for (auto i = 0u; i < positions.size(); ++i)
    {
        output.vertexData.push_back(positions[i].x);
        output.vertexData.push_back(positions[i].y - face.offset);
        output.vertexData.push_back(positions[i].z);

        output.vertexData.push_back(colour.r);
        output.vertexData.push_back(colour.g);
        output.vertexData.push_back(colour.b);
        output.vertexData.push_back(aoLevels[ao[i]]);

        output.vertexData.push_back(normal.x);
        output.vertexData.push_back(normal.y);
        output.vertexData.push_back(normal.z);

        output.vertexData.push_back(UVs[i].x);
        output.vertexData.push_back(UVs[i].y);
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

bool ChunkSystem::VoxelFace::canAppend(const VoxelFace& other, std::int32_t direction)
{
    //TODO refactor this into something more coherant
    //for instance hide the nasty reinterpret_cast by converting
    //the ao values to a bitmask for comparing several values at once

    const std::uint8_t NoLight = 3;
    bool append = false;
    auto diff = other.position - position;

    std::uint32_t aoMask = *reinterpret_cast<std::uint32_t*>(ao.data());
    std::uint32_t otherAoMask = *reinterpret_cast<const std::uint32_t*>(other.ao.data());

    //only join if the tiles both have ao
    if (aoMask != 0
        && otherAoMask != 0)
    {
        switch (direction)
        {
        default: break;
        case VoxelFace::North:
            //should be pos X, new face to the right
            /*
            2---3    2---3
            | t | -> | o |
            0---1    0---1
            t = this o = other
            */
            if (diff.x == 1)
            {
                //at least one joining vert should have ao
                if (ao[1] != NoLight || ao[3] != NoLight) //these light levels could get confusing without a name
                {
                    append = (ao[1] == other.ao[0] && ao[3] == other.ao[2]);
                }
            }

            /*
            This is moving up the face

            2---3
            | o |
            0---1
              ^
            2---3
            | t |
            0---1
            */
            else if (diff.y == 1
                && (ao[2] != NoLight || ao[3] != NoLight))
            {
                append = (ao[2] == other.ao[0] && ao[3] == other.ao[1]);
            }
            break;
        case VoxelFace::South:
            //diff is pos X, but looking from other
            //direction means new face is on the left

            /*
            3---2    3---2
            | t | -> | o |
            1---0    1---0
            t = this o = other
            */
            if (diff.x == 1
                && (ao[2] != NoLight || ao[0] != NoLight))
            {
                append = (ao[2] == other.ao[3] && ao[0] == other.ao[1]);
            }
            else if (diff.y == 1
                && (ao[2] != NoLight || ao[3] != NoLight))//moving up the face
            {
                append = (ao[2] == other.ao[0] && ao[3] == other.ao[1]);
            }
            break;
        case VoxelFace::East:
            //diff is pos Z, new face on left
            if (diff.z == 1
                && (ao[2] != NoLight || ao[0] != NoLight))
            {
                append = (ao[2] == other.ao[3] && ao[0] == other.ao[1]);
            }
            else if (diff.y == 1
                && (ao[2] != NoLight || ao[3] != NoLight))//moving up the face
            {
                append = (ao[2] == other.ao[0] && ao[3] == other.ao[1]);
            }
            break;
        case VoxelFace::West:
            //diff is pos Z, new face on right
            if (diff.z == 1
                && (ao[1] != NoLight || ao[3] != NoLight))
            {
                append = (ao[1] == other.ao[0] && ao[3] == other.ao[2]);
            }
            else if (diff.y == 1
                && (ao[2] != NoLight || ao[3] != NoLight))//moving up the face
            {
                append = (ao[2] == other.ao[0] && ao[3] == other.ao[1]);
            }
            break;
        case VoxelFace::Top:
            //moves pos x, but new face is on left
            //because the view is flipped vertically
            if (diff.x == 1
                && (ao[1] != NoLight || ao[3] != NoLight))
            {
                append = (ao[1] == other.ao[0] && ao[3] == other.ao[2]);
            }
            else if (diff.z == 1
                && (ao[2] != NoLight || ao[3] != NoLight))//moving up the face
            {
                append = (ao[2] == other.ao[0] && ao[3] == other.ao[1]);
            }
            break;
        case VoxelFace::Bottom:
            //also pos X with new face on left, but not flipped
            if (diff.x == 1
                && (ao[2] != NoLight || ao[0] != NoLight))
            {
                append = (ao[2] == other.ao[3] && ao[0] == other.ao[1]);
            }
            else if (diff.z == 1
                && (ao[2] != NoLight || ao[3] != NoLight))//moving up the face
            {
                append = (ao[2] == other.ao[0] && ao[3] == other.ao[1]);
            }
            break;
        }
    }
    //or both have no ao
    else if (aoMask + otherAoMask == 0)
    {
        append = true;
    }
    //return false;
    return (other.id == id && other.visible == visible && append);
}
