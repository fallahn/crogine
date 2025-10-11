/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2025
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
#include "BorderMeshBuilder.hpp"
#include "ClientCommandIDs.hpp"
#include "Coordinate.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

#include <optional>
#include <set>

namespace
{
    const std::string Vertex = 
        R"(
            ATTRIBUTE vec4 a_position;
            ATTRIBUTE MED vec4 a_colour;
            ATTRIBUTE vec3 a_normal;
            ATTRIBUTE MED vec2 a_texCoord0;

            uniform mat4 u_worldMatrix;
            uniform mat4 u_worldViewMatrix;
            uniform mat4 u_projectionMatrix;

            VARYING_OUT MED vec3 v_colour;
            VARYING_OUT MED vec2 v_texCoord;
            VARYING_OUT MED vec4 v_viewPosition;
            VARYING_OUT float v_worldHeight;
            VARYING_OUT float v_ao;
            VARYING_OUT vec3 v_normal;

            void main()
            {
                gl_Position = u_projectionMatrix * u_worldViewMatrix * a_position;

                v_colour = a_colour.rgb;
                v_texCoord = a_texCoord0;
                v_viewPosition = u_worldViewMatrix * a_position;
                v_worldHeight = (u_worldMatrix * a_position).y;
                v_ao = a_colour.a;
                v_normal = a_normal;
            })";

    const std::string Fragment = 
        R"(
            uniform float u_alpha;
            uniform float u_time;
            uniform vec2 u_tileSize;
            uniform sampler2D u_texture;

            VARYING_IN MED vec3 v_colour;
            VARYING_IN MED vec2 v_texCoord;
            VARYING_IN MED vec4 v_viewPosition;
            VARYING_IN float v_worldHeight;
            VARYING_IN float v_ao;
            VARYING_IN vec3 v_normal;

            OUTPUT

            const LOW vec4 FogColour = vec4(0.07, 0.17, 0.67, 1.0);
            const float FogDensity = 0.03;

            float directionalAmbience(vec3 normal)
            {
                vec3 v = normal * normal;
                if (normal.y < 0)
                {
                    return dot(v, vec3(0.67082, 0.447213, 0.83666));
                }
                return dot(v, vec3(0.67082, 1.0, 0.83666));
            }

            vec3 brightnessContrast(vec3 colour)
            {
                float amount = sin(u_time) + 1.0 / 2.0;
                colour = colour - vec3(0.5) + max(amount, 0.0) * 0.5;
                return colour;
            }

            void main()
            {
                vec2 uv = v_colour.rg;
                uv.x += (u_tileSize.x * fract(v_texCoord.x));
                uv.y -= (u_tileSize.y * fract(v_texCoord.y + u_time));

                vec4 colour = TEXTURE(u_texture, uv);

                if(colour.a < 0.1)
                {
                    discard;
                }

                //colour.rgb = brightnessContrast(colour.rgb);
                colour.rgb *= directionalAmbience(v_normal);
                colour.a *= u_alpha;

                float fogFactor = 1.0;

                if(v_worldHeight < 23.899) //water level
                {
                    float fogDistance = length(v_viewPosition);
                    fogFactor = 1.0 / exp(fogDistance * FogDensity);
                    fogFactor = clamp(fogFactor, 0.2, 0.8);

                    colour *= 0.9;
                }

                float light = v_ao + max(0.15 * dot(v_normal, vec3(1.0)), 0.0);
                FRAG_OUT = mix(FogColour, colour * light, fogFactor);

                //FRAG_OUT = vec4((v_normal + 1.0 / 2.0), 1.0);
            })";

    const std::string VertexDebug = 
        R"(
            ATTRIBUTE vec4 a_position;
            ATTRIBUTE vec3 a_colour;

            uniform mat4 u_worldViewMatrix;
            uniform mat4 u_projectionMatrix;

            VARYING_OUT MED vec3 v_colour;

            void main()
            {
                gl_Position = u_projectionMatrix * u_worldViewMatrix * a_position;
                v_colour = a_colour;
            })";

    const std::string FragmentDebug = 
        R"(
            OUTPUT

            uniform vec4 u_colour;
            VARYING_IN MED vec3 v_colour;

            void main()
            {
                FRAG_OUT = vec4(u_colour.rgb, 1.0);
            })";

    const std::string FragmentRed =
        R"(
            OUTPUT

            void main()
            {
                FRAG_OUT = vec4(1.0, 0.0, 0.0, 1.0);
            })";


    //number of tiles in the tile texture X direction.
    const std::int32_t TextureTileCount = 8;
}

ChunkSystem::ChunkSystem(cro::MessageBus& mb, cro::ResourceCollection& rc, ChunkManager& cm, vx::DataManager& dm)
    : cro::System       (mb, typeid(ChunkSystem)),
    m_resources         (rc),
    m_sharedChunkManager(cm),
    m_voxelData         (dm),
    m_threadRunning     (false)
{
    requireComponent<ChunkComponent>();
    requireComponent<cro::Transform>();

    //create shaders for chunk meshes
    if (rc.shaders.loadFromString(ShaderID::Chunk, Vertex, Fragment))
    {
        //and then material
        m_materialIDs[MaterialID::ChunkSolid] = rc.materials.add(rc.shaders.get(ShaderID::Chunk));
        rc.materials.get(m_materialIDs[MaterialID::ChunkSolid]).setProperty("u_alpha", 1.f);
        rc.materials.get(m_materialIDs[MaterialID::ChunkSolid]).setProperty("u_time", 0.f);

        m_materialIDs[MaterialID::ChunkWater] = rc.materials.add(rc.shaders.get(ShaderID::Chunk));
        rc.materials.get(m_materialIDs[MaterialID::ChunkWater]).blendMode = cro::Material::BlendMode::Alpha;
        rc.materials.get(m_materialIDs[MaterialID::ChunkWater]).setProperty("u_alpha", 0.4f);

        m_materialIDs[MaterialID::ChunkDetail] = rc.materials.add(rc.shaders.get(ShaderID::Chunk));
        rc.materials.get(m_materialIDs[MaterialID::ChunkDetail]).blendMode = cro::Material::BlendMode::Alpha;
        rc.materials.get(m_materialIDs[MaterialID::ChunkDetail]).setProperty("u_alpha", 1.f);
        rc.materials.get(m_materialIDs[MaterialID::ChunkDetail]).setProperty("u_time", 0.f);
    }

    if (rc.shaders.loadFromString(ShaderID::ChunkDebug, VertexDebug, FragmentDebug))
    {
        auto& shader = rc.shaders.get(ShaderID::ChunkDebug);
        m_materialIDs[MaterialID::ChunkDebug] = rc.materials.add(shader);
        rc.materials.get(m_materialIDs[MaterialID::ChunkDebug]).setProperty("u_colour", cro::Colour::Magenta);

        m_meshIDs[MeshID::Border] = rc.meshes.loadMesh(BorderMeshBuilder());
        

        /*rc.shaders.preloadFromString(VertexDebug, FragmentRed, 500);
        temp = rc.materials.add(rc.shaders.get(500));*/
    }

    //pre-parse the texture info
    /*
    Something has to be const here - so we'll go ahead and state that the
    block texture will always be 8 tiles wide. This allows for varying width
    tiles (although enforces they should be square) so blocks can optionally
    have higher resolution textures. The texture file can also be expanded
    vertically in size should more tiles be required in the future. Here
    we load the texture and parse some information about it to pass to the
    shader so that it can pick the correct tile for the UV info handed to it.
    */
    auto& texture = rc.textures.get("assets/images/blocks.png");
    texture.setRepeated(true);

    glm::vec2 textureSize(texture.getSize());
    float tileWidth = textureSize.x / TextureTileCount;

    std::int32_t tileCountY = static_cast<std::int32_t>(textureSize.y / tileWidth);

    glm::vec2 tileUVSize = { 1.f / TextureTileCount, 1.f / tileCountY };

    //store the offset for each tile so the voxel face can index into the vector
    //we reserve some size in case a face tries to index somewhere out of range
    //(although this really needs to be prevented altogether, perhaps by limiting
    //the index value of the face indices)
    m_tileOffsets.resize(256);
    for (auto y = 0; y < tileCountY; ++y)
    {
        for (auto x = 0; x < TextureTileCount; ++x)
        {
            auto index = y * TextureTileCount + x;
            m_tileOffsets[index] = glm::vec2(static_cast<float>(x) * tileUVSize.x, static_cast<float>(y + 1) * tileUVSize.y);
        }
    }
    //and tell the shader how big one tile is
    auto& shader = rc.shaders.get(ShaderID::Chunk);
    glCheck(glUseProgram(shader.getGLHandle()));
    glCheck(glUniform2f(shader.getUniformID("u_tileSize"), tileUVSize.x, tileUVSize.y));
    glCheck(glUseProgram(0));
    
    rc.materials.get(m_materialIDs[MaterialID::ChunkSolid]).setProperty("u_texture", texture);
    rc.materials.get(m_materialIDs[MaterialID::ChunkWater]).setProperty("u_texture", texture);

    //thread for meshing
    m_chunkMutex = std::make_unique<std::mutex>();

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

void ChunkSystem::process(float dt)
{
    static float elapsed = 0.f;
    elapsed += dt;

    std::vector<cro::Entity> dirtyChunks;

    auto forwardVector = getScene()->getActiveCamera().getComponent<cro::Transform>().getForwardVector();
    //auto camPos = getScene()->getActiveCamera().getComponent<cro::Transform>().getWorldPosition();
    const auto& cam = getScene()->getActiveCamera().getComponent<cro::Camera>();
    

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& chunkComponent = entity.getComponent<ChunkComponent>();
        if (chunkComponent.needsUpdate)
        {
            dirtyChunks.push_back(entity);
            chunkComponent.needsUpdate = false;
        }

        auto& model = entity.getComponent<cro::Model>();
        model.setMaterialProperty(SubMeshID::Water, "u_time", elapsed * 0.6f);

        //if the model is visible update the semi-transparent
        //triangles draw order
        if (!chunkComponent.transparentIndices.empty()
            && !model.isHidden())
        {
            for (auto& t : chunkComponent.transparentIndices)
            {
                /*auto dir = t.avgPosition - camPos;
                t.sortValue = glm::dot(forwardVector, dir);*/
                /*auto camSpacePos = glm::vec3(cam.viewMatrix * glm::vec4(t.avgPosition, 1.f));
                t.sortValue = glm::dot(camSpacePos, glm::vec3(0.f,0.f,-1.f));*/
                t.sortValue = glm::dot(t.normal, forwardVector);
            }

            std::sort(chunkComponent.transparentIndices.begin(), chunkComponent.transparentIndices.end(),
                [](const Triangle& a, const Triangle& b) { return a.sortValue > b.sortValue; });

            //shame we have to copy all these again...
            std::vector<std::uint32_t> indices;
            for (const auto& t : chunkComponent.transparentIndices)
            {
                indices.insert(indices.end(), t.indices.begin(), t.indices.end());
            }

            auto& meshData = model.getMeshData();
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[SubMeshID::Water].ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_DYNAMIC_DRAW));
        }
    }

    //push all chunks in one go with a single lock
    if (!dirtyChunks.empty())
    {
        std::lock_guard<std::mutex> lock(*m_chunkMutex);
        for (auto entity : dirtyChunks)
        {
            m_inputQueue.push(entity);
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
            
            if (!m_sharedChunkManager.hasChunk(position))
            {
                auto chunkVoxels = decompressVoxels(voxels);

                auto& chunk = m_sharedChunkManager.addChunk(position);
                chunk.setVoxels(chunkVoxels);
                chunk.setHighestPoint(cd.highestPoint);

                //create new entity for chunk
                auto entity = getScene()->createEntity();
                entity.addComponent<cro::Transform>().setPosition(glm::vec3(position * WorldConst::ChunkSize));
                entity.addComponent<ChunkComponent>().chunkPos = position;
                entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::ChunkMesh;

                //create a model with an empty mesh, this should be build on next
                //update automagically!
                auto meshID = m_resources.meshes.loadMesh(ChunkMeshBuilder());
                CRO_ASSERT(meshID > 0, "Mesh generation failed");
                auto& mesh = m_resources.meshes.getMesh(meshID);

                auto material = m_resources.materials.get(m_materialIDs[MaterialID::ChunkSolid]);
                entity.addComponent<cro::Model>(mesh, material);

                auto waterMaterial = m_resources.materials.get(m_materialIDs[MaterialID::ChunkWater]);
                entity.getComponent<cro::Model>().setMaterial(SubMeshID::Water, waterMaterial);

                auto detailMaterial = m_resources.materials.get(m_materialIDs[MaterialID::ChunkDetail]);
                entity.getComponent<cro::Model>().setMaterial(SubMeshID::Foliage, detailMaterial);

                //remember to update this if the chunk is updated
                //this can nearly double the frame rate when skipping a lot of empty chunks!
                entity.getComponent<cro::Model>().setHidden(cd.highestPoint == -1);

                
                //create a second entity for debug bounds
                entity = getScene()->createEntity();
                entity.addComponent<cro::Transform>().setPosition(glm::vec3(position * WorldConst::ChunkSize));
                entity.getComponent<cro::Transform>().setScale(glm::vec3(WorldConst::ChunkSize));
                auto debugMaterial = m_resources.materials.get(m_materialIDs[MaterialID::ChunkDebug]);
                entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_meshIDs[MeshID::Border]), debugMaterial);
                entity.getComponent<cro::Model>().setHidden(true);
                entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::DebugMesh;

                //create a copy of the chunk data for our threads to work on
                std::lock_guard<std::mutex> lock(*m_chunkMutex);
                auto& copyChunk = m_chunkManager.addChunk(position);
                copyChunk.setVoxels(chunkVoxels);
                copyChunk.setHighestPoint(cd.highestPoint);
            }
        }
    }
}

//private
void ChunkSystem::updateMesh()
{
    VertexOutput vertexOutput;

    m_chunkMutex->lock();
    if (!m_outputQueue.empty())
    {
        auto& front = m_outputQueue.front();
        vertexOutput.position = front.position;
        vertexOutput.vertexData.swap(front.vertexData);
        vertexOutput.solidIndices.swap(front.solidIndices);
        vertexOutput.waterIndices.swap(front.waterIndices);
        vertexOutput.detailIndices.swap(front.detailIndices);
        vertexOutput.triangles.swap(front.triangles);
        m_outputQueue.pop();
    }
    m_chunkMutex->unlock();

    if (!vertexOutput.vertexData.empty() && (!vertexOutput.solidIndices.empty() || !vertexOutput.waterIndices.empty()))
    {
        auto entity = m_chunkEntities[vertexOutput.position];
        auto& meshData = entity.getComponent<cro::Model>().getMeshData();

        meshData.vertexCount = vertexOutput.vertexData.size() / (meshData.vertexSize / sizeof(float));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.vboID));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexOutput.vertexData.size() * sizeof(float), vertexOutput.vertexData.data(), GL_DYNAMIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        meshData.indexData[SubMeshID::Solid].indexCount = static_cast<std::uint32_t>(vertexOutput.solidIndices.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[SubMeshID::Solid].ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexOutput.solidIndices.size() * sizeof(std::uint32_t), vertexOutput.solidIndices.data(), GL_DYNAMIC_DRAW));

        meshData.indexData[SubMeshID::Water].indexCount = static_cast<std::uint32_t>(vertexOutput.waterIndices.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[SubMeshID::Water].ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexOutput.waterIndices.size() * sizeof(std::uint32_t), vertexOutput.waterIndices.data(), GL_DYNAMIC_DRAW));

        meshData.indexData[SubMeshID::Foliage].indexCount = static_cast<std::uint32_t>(vertexOutput.detailIndices.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[SubMeshID::Foliage].ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexOutput.detailIndices.size() * sizeof(std::uint32_t), vertexOutput.detailIndices.data(), GL_DYNAMIC_DRAW));

        
        //update position relative to chunk (ie place in world space)
        /*auto chunkPos = entity.getComponent<cro::Transform>().getPosition();
        for (auto& t : vertexOutput.triangles)
        {
            t.avgPosition += chunkPos;
        }*/

        entity.getComponent<ChunkComponent>().transparentIndices.swap(vertexOutput.triangles);
    }
}

void ChunkSystem::threadFunc()
{
    while (m_threadRunning)
    {
        const Chunk* chunk = nullptr;
        glm::ivec3 position = glm::ivec3(0);
        ChunkComponent::MeshType meshType = ChunkComponent::Greedy;

        //lock the input queue
        m_chunkMutex->lock();

        //check queue
        if (!m_inputQueue.empty())
        {
            //check the chunk is even visible before updating it
            auto entity = m_inputQueue.front();
            //const auto& model = entity.getComponent<cro::Model>();
            //if (model.isVisible() /*&& !model.isHidden()*/)
            {
                m_inputQueue.pop();

                position = entity.getComponent<ChunkComponent>().chunkPos;
                meshType = entity.getComponent<ChunkComponent>().meshType;

                chunk = &m_chunkManager.getChunk(position);

                //skip empty/airblock chunks
                if (chunk->getHighestPoint() == -1)
                {
                    chunk = nullptr;
                }
            }
        }

        //unlock queue
        m_chunkMutex->unlock();

        //if input do work
        if (chunk)
        {
            VertexOutput vertexOutput;
            if (meshType == ChunkComponent::MeshType::Greedy)
            {
                generateChunkMesh(*chunk, vertexOutput);
            }
            else
            {
                generateNaiveMesh(*chunk, vertexOutput);
            }
            //generateDebugMesh(*chunk, vertexOutput);


            //lock output
            std::lock_guard<std::mutex> lock(*m_chunkMutex);

            //swap results
            auto& result = m_outputQueue.emplace();
            result.position = position;
            result.vertexData.swap(vertexOutput.vertexData);
            result.solidIndices.swap(vertexOutput.solidIndices);
            result.waterIndices.swap(vertexOutput.waterIndices);
            result.detailIndices.swap(vertexOutput.detailIndices);
            result.triangles.swap(vertexOutput.triangles);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(50));
        }
    }
}

void ChunkSystem::calcAO(const Chunk& chunk, vx::Face& face)
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

    airblock = m_voxelData.getID(vx::CommonType::Air);
    waterblock = m_voxelData.getID(vx::CommonType::Water);

    auto getSurrounding = [&](const std::vector<glm::ivec3>& positions)
    {
        //we want to lock this as briefly as possible
        //TODO we want to lock this *as few times* as possible
        //std::lock_guard<std::mutex> lock(*m_chunkMutex);
        for (auto i = 0u; i < surroundingVoxels.size(); ++i)
        {
            surroundingVoxels[i] = chunk.getVoxel(positions[i]);
        }
    };

    auto position = face.position;
    if (face.visible && face.id != waterblock)
    {
        if (face.direction == vx::Top)
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
        else if(face.direction == vx::Bottom)
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
        else if (face.direction == vx::North)
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
        else if (face.direction == vx::East)
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
        else if (face.direction == vx::South)
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
        else if (face.direction == vx::West)
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
        s1 = (surroundingVoxels[5] == vx::CommonType::OutOfBounds || surroundingVoxels[5] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[5]).type == vx::Type::Detail) ? 0 : 1;
        c = (surroundingVoxels[6] == vx::CommonType::OutOfBounds || surroundingVoxels[6] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[6]).type == vx::Type::Detail) ? 0 : 1;
        s2 = (surroundingVoxels[7] == vx::CommonType::OutOfBounds || surroundingVoxels[7] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[7]).type == vx::Type::Detail) ? 0 : 1;
        face.ao[0] = vertexAO(s1, s2, c);

        s1 = (surroundingVoxels[3] == vx::CommonType::OutOfBounds || surroundingVoxels[3] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[3]).type == vx::Type::Detail) ? 0 : 1;
        c = (surroundingVoxels[4] == vx::CommonType::OutOfBounds || surroundingVoxels[4] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[4]).type == vx::Type::Detail) ? 0 : 1;
        s2 = (surroundingVoxels[5] == vx::CommonType::OutOfBounds || surroundingVoxels[5] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[5]).type == vx::Type::Detail) ? 0 : 1;
        face.ao[1] = vertexAO(s1, s2, c);

        s1 = (surroundingVoxels[7] == vx::CommonType::OutOfBounds || surroundingVoxels[7] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[7]).type == vx::Type::Detail) ? 0 : 1;
        c = (surroundingVoxels[0] == vx::CommonType::OutOfBounds || surroundingVoxels[0] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[0]).type == vx::Type::Detail) ? 0 : 1;
        s2 = (surroundingVoxels[1] == vx::CommonType::OutOfBounds || surroundingVoxels[1] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[1]).type == vx::Type::Detail) ? 0 : 1;
        face.ao[2] = vertexAO(s1, s2, c);

        s1 = (surroundingVoxels[1] == vx::CommonType::OutOfBounds || surroundingVoxels[1] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[1]).type == vx::Type::Detail) ? 0 : 1;
        c = (surroundingVoxels[2] == vx::CommonType::OutOfBounds || surroundingVoxels[2] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[2]).type == vx::Type::Detail) ? 0 : 1;
        s2 = (surroundingVoxels[3] == vx::CommonType::OutOfBounds || surroundingVoxels[3] == airblock 
            || m_voxelData.getVoxel(surroundingVoxels[3]).type == vx::Type::Detail) ? 0 : 1;
        face.ao[3] = vertexAO(s1, s2, c);
    }
}

vx::Face ChunkSystem::getFace(const Chunk& chunk, glm::ivec3 position, vx::Side side)
{
    //std::lock_guard<std::mutex> lock(*m_chunkMutex);
    const std::uint8_t airBlock = m_voxelData.getID(vx::CommonType::Air);
    const std::uint8_t waterBlock = m_voxelData.getID(vx::CommonType::Water);

    vx::Face face;
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

    face.textureIndex = m_voxelData.getVoxel(face.id).tileIDs[side];
    switch (side)
    {
    case vx::Top:
        neighbour = chunk.getVoxel({ position.x, position.y + 1, position.z });
        if (face.id == waterBlock)
        {
            face.offset = 0.1f;
        }
        break;
    case vx::Bottom:
        neighbour = chunk.getVoxel({ position.x, position.y - 1, position.z });
        break;
    case vx::North:
        neighbour = chunk.getVoxel({ position.x, position.y, position.z + 1 });
        break;
    case vx::South:
        neighbour = chunk.getVoxel({ position.x, position.y, position.z - 1 });
        break;
    case vx::East:
        neighbour = chunk.getVoxel({ position.x + 1, position.y, position.z });
        break;
    case vx::West:
        neighbour = chunk.getVoxel({ position.x - 1, position.y, position.z });
        break;
    }

    face.visible = (neighbour == airBlock || (m_voxelData.getVoxel(neighbour).type == vx::Type::Detail)
                    || (neighbour == waterBlock && face.id != waterBlock));
    return face;
}

void ChunkSystem::generateChunkMesh(const Chunk& chunk, VertexOutput& output)
{
    //list of positions to create details at
    std::vector<std::pair<glm::ivec3, std::uint16_t>> detailPositions;
    
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
                currentSide = (backface) ? vx::West : vx::East;
                break;
            case 1:
                currentSide = (backface) ? vx::Bottom : vx::Top;
                maxSlice = chunk.getHighestPoint() + 1;
                break;
            case 2:
                currentSide = (backface) ? vx::South : vx::North;
                break;
            }

            //move across the current direction/plane front to back
            for (x[direction] = -1; x[direction] < maxSlice;)
            {
                //this is the collection of faces grouped per side
                std::vector<std::optional<vx::Face>> faceMask(WorldConst::ChunkArea);
                std::fill(faceMask.begin(), faceMask.end(), std::nullopt);
                std::size_t maskIndex = 0;

                m_chunkMutex->lock();
                for (x[v] = 0; x[v] < WorldConst::ChunkSize; x[v]++)
                {
                    for (x[u] = 0; x[u] < WorldConst::ChunkSize; x[u]++)
                    {
                        auto positionA = glm::ivec3(x[0], x[1], x[2]);
                        auto positionB = glm::ivec3(x[0] + q[0], x[1] + q[1], x[2] + q[2]);


                        std::optional<vx::Face> faceA = (x[direction] >= 0) ? 
                            std::optional<vx::Face>(getFace(chunk, positionA, (vx::Side)currentSide)) : std::nullopt;

                        std::optional<vx::Face> faceB = (x[direction] < (WorldConst::ChunkSize - 1)) ?
                            std::optional<vx::Face>(getFace(chunk, positionB, (vx::Side)currentSide)) : std::nullopt;

                        //check the voxel type for detail meshes and add them to a list
                        //for later processing
                        auto voxelID = chunk.getVoxel(positionA);
                        auto data = m_voxelData.getVoxel(voxelID);
                        if (data.style == vx::MeshStyle::Cross)
                        {
                            //only add this on first pass, else we get the same position 6 times!
                            if (direction == 0 && !backface)
                            {
                                detailPositions.emplace_back(std::make_pair(positionA, data.tileIDs[0]));
                            }

                            if (faceA)
                            {
                                faceA->visible = false;
                            }
                        }
                        voxelID = chunk.getVoxel(positionB);
                        data = m_voxelData.getVoxel(voxelID);
                        if (data.style == vx::MeshStyle::Cross)
                        {
                            if (direction == 0 && !backface)
                            {
                                detailPositions.emplace_back(std::make_pair(positionB, data.tileIDs[0]));
                            }

                            if (faceB)
                            {
                                faceB->visible = false;
                            }
                        }

                        faceMask[maskIndex] = (faceA != std::nullopt && faceB != std::nullopt && (*faceA == *faceB)) ?
                            std::nullopt :
                            backface ? faceB : faceA;

                        maskIndex++;
                    }
                }
                //do this out here halves the amount of calls!
                for (auto& f : faceMask)
                {
                    if (f)
                    {
                        calcAO(chunk, *f);
                    }
                }
                m_chunkMutex->unlock();

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
                            auto prevFace = *faceMask[maskIndex];
                            
                            //calc the merged width/height
                            std::int32_t width = 1;
                            for (;
                                i + width < WorldConst::ChunkSize 
                                && faceMask[maskIndex + width] != std::nullopt 
                                && prevFace == *faceMask[maskIndex + width];
                                width++)
                            {
                                prevFace = *faceMask[maskIndex + width];
                            }


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
                                        || (prevFace != *faceMask[idx]))
                                    { 
                                        complete = true;
                                        break;
                                    }
                                    else
                                    {
                                        prevFace = *faceMask[idx];
                                    }
                                }
                                
                                //check this first as it will be trus if face
                                //mask is nullopt and we  won't try assigning
                                //mullopt to prevFace.
                                if (complete)
                                {
                                    break;
                                }

                                //move the prev face to the beginning of the row
                                //so next iter we check appending vertically
                                prevFace = *faceMask[rowStart + WorldConst::ChunkSize];
                            }

                            //create the quad geom
                            std::array<std::int32_t, 3u> du = { 0,0,0 };
                            std::array<std::int32_t, 3u> dv = { 0,0,0 };

                            x[u] = i;
                            x[v] = j;

                            du[u] = width;
                            dv[v] = height;

                            
                            std::vector<glm::vec3> positions;
                            std::vector<glm::vec2> UVs;
                            switch (faceMask[maskIndex]->direction)
                            {
                            case vx::West:
                                positions =
                                {
                                    glm::vec3(x[0], x[1], x[2]), //BL
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]), //BR
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]), //TL
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]) //TR
                                };
                                UVs =
                                {
                                    glm::vec2(0.f, width),
                                    glm::vec2(height, width),
                                    glm::vec2(0.f),
                                    glm::vec2(height, 0.f)
                                };
                                break;
                            case vx::East:
                            case vx::Top:
                                positions =
                                {
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]),
                                    glm::vec3(x[0], x[1], x[2]),
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]),
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2])
                                };
                                UVs =
                                {
                                    glm::vec2(0.f, width),
                                    glm::vec2(height, width),
                                    glm::vec2(0.f),
                                    glm::vec2(height, 0.f)
                                };
                                break;
                            case vx::North:
                                positions =
                                {
                                    glm::vec3(x[0], x[1], x[2]),
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]),
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]),
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2])
                                };

                                UVs =
                                {
                                    glm::vec2(0.f, height),
                                    glm::vec2(width, height),
                                    glm::vec2(0.f),
                                    glm::vec2(width, 0.f)
                                };
                                break;
                            case vx::South:
                                positions =
                                {
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]),
                                    glm::vec3(x[0], x[1], x[2]),
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]),
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2])
                                };
                                UVs =
                                {
                                    glm::vec2(0.f, height),
                                    glm::vec2(width, height),
                                    glm::vec2(0.f),
                                    glm::vec2(width, 0.f)
                                };
                                break;
                            case vx::Bottom:
                                positions =
                                {
                                    glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]),
                                    glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]),
                                    glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]),
                                    glm::vec3(x[0], x[1], x[2])
                                };
                                UVs =
                                {
                                    glm::vec2(0.f, width),
                                    glm::vec2(height, width),
                                    glm::vec2(0.f),
                                    glm::vec2(height, 0.f)
                                };
                                break;
                            }

                            addQuad(output, positions, UVs, faceMask[maskIndex]->ao, *faceMask[maskIndex]);


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

    //check the list of cross meshes and add the vertices
    for (const auto& [pos, tileID] : detailPositions)
    {
        addDetail(output, pos, tileID);
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
                currentSide = (backface) ? vx::West : vx::East;
                break;
            case 1:
                currentSide = (backface) ? vx::Bottom : vx::Top;
                maxSlice = chunk.getHighestPoint() + 1;
                break;
            case 2:
                currentSide = (backface) ? vx::South : vx::North;
                break;
            }

            //move across the current direction/plane front to back
            for (x[direction] = -1; x[direction] < maxSlice;)
            {
                //this is the collection of faces grouped per side
                std::vector<std::optional<vx::Face>> faceMask(WorldConst::ChunkArea);
                std::fill(faceMask.begin(), faceMask.end(), std::nullopt);
                std::size_t maskIndex = 0;

                m_chunkMutex->lock();
                for (x[v] = 0; x[v] < WorldConst::ChunkSize; x[v]++)
                {
                    for (x[u] = 0; x[u] < WorldConst::ChunkSize; x[u]++)
                    {
                        auto positionA = glm::ivec3(x[0], x[1], x[2]);
                        auto positionB = glm::ivec3(x[0] + q[0], x[1] + q[1], x[2] + q[2]);

                        std::optional<vx::Face> faceA = (x[direction] >= 0) ?
                            std::optional<vx::Face>(getFace(chunk, positionA, (vx::Side)currentSide)) : std::nullopt;

                        std::optional<vx::Face> faceB = (x[direction] < (WorldConst::ChunkSize - 1)) ?
                            std::optional<vx::Face>(getFace(chunk, positionB, (vx::Side)currentSide)) : std::nullopt;

                        std::vector<glm::vec3> positions;

                        vx::Side faceDirection = vx::North;
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
                            calcAO(chunk, *faceMask[maskIndex]);
                            switch (faceDirection)
                            {
                            case vx::Bottom:
                                positions.emplace_back(position.x + 1, position.y, position.z + 1);
                                positions.emplace_back(position.x, position.y, position.z + 1);
                                positions.emplace_back(position.x + 1, position.y, position.z);
                                positions.emplace_back(position.x, position.y, position.z);
                                break;
                            case vx::Top:
                                positions.emplace_back(position.x + 1, position.y + 1, position.z);
                                positions.emplace_back(position.x, position.y + 1, position.z);
                                positions.emplace_back(position.x + 1, position.y + 1, position.z + 1);
                                positions.emplace_back(position.x, position.y + 1, position.z + 1);
                                break;
                            case vx::North:
                                positions.emplace_back(position.x, position.y, position.z + 1);
                                positions.emplace_back(position.x + 1, position.y, position.z + 1);
                                positions.emplace_back(position.x, position.y + 1, position.z + 1);
                                positions.emplace_back(position.x + 1, position.y + 1, position.z + 1);
                                break;
                            case vx::South:
                                positions.emplace_back(position.x + 1, position.y, position.z);
                                positions.emplace_back(position.x, position.y, position.z);
                                positions.emplace_back(position.x + 1, position.y + 1, position.z);
                                positions.emplace_back(position.x, position.y + 1, position.z);
                                
                                break;
                            case vx::East:
                                positions.emplace_back(position.x + 1, position.y, position.z + 1);
                                positions.emplace_back(position.x + 1, position.y, position.z);
                                positions.emplace_back(position.x + 1, position.y + 1, position.z + 1);
                                positions.emplace_back(position.x + 1, position.y + 1, position.z);
                                break;
                            case vx::West:
                                positions.emplace_back(position.x, position.y, position.z);
                                positions.emplace_back(position.x, position.y, position.z + 1);
                                positions.emplace_back(position.x, position.y + 1, position.z);
                                positions.emplace_back(position.x, position.y + 1, position.z + 1);
                                break;
                            }

                            const std::vector<glm::vec2> UVs =
                            {
                                glm::vec2(0.f, 1.f),
                                glm::vec2(1.f),
                                glm::vec2(0.f),
                                glm::vec2(1.f, 0.f)
                            };

                            if (!positions.empty())
                            {
                                addQuad(output, positions, UVs, faceMask[maskIndex]->ao, *faceMask[maskIndex]);
                            }
                        }

                        maskIndex++;
                    }
                }
                m_chunkMutex->unlock();
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

void ChunkSystem::addQuad(VertexOutput& output, const std::vector<glm::vec3>& positions, const std::vector<glm::vec2>& UVs,  const std::array<std::uint8_t, 4u>& ao, vx::Face face)
{
    //add indices to the index array, remembering to offset into the current VBO
    std::array<std::uint32_t, 6> localIndices = { 2,0,1,  1,3,2 };
    //auto avgPos = [&](bool first)->glm::vec3
    //{
    //    auto pos = first ? 
    //        (positions[2] + positions[0] + positions[1]) : 
    //        (positions[1] + positions[3] + positions[2]);
    //    return pos / 3.f;
    //};

    //switch tri direction to maintain ao interp artifacting symmetry
    static const std::array<float, 4u> aoLevels = { 0.25f, 0.6f, 0.8f, 1.f };
    if (aoLevels[ao[2]] + aoLevels[ao[1]] < aoLevels[ao[0]] + aoLevels[ao[3]])
    {
        localIndices = { 3,0,1, 2,0,3 };
    }

    //normal is also used for face sorting...
    glm::vec3 normal = glm::vec3(0.f);
    switch (face.direction)
    {
    case vx::North:
        normal.z = 1.f;
        break;
    case vx::South:
        normal.z = -1.f;
        break;
    case vx::East:
        normal.x = 1.f;
        break;
    case vx::West:
        normal.x = -1.f;
        break;
    case vx::Top:
        normal.y = 1.f;
        break;
    case vx::Bottom:
        normal.y = -1.f;
        break;
    }

    //update the index output
    std::int32_t indexOffset = static_cast<std::int32_t>(output.vertexData.size() / ChunkMeshBuilder::getVertexComponentCount());
    for (auto& i : localIndices)
    {
        i += indexOffset;
    }

    if (face.id == m_voxelData.getID(vx::CommonType::Water))
    {
        output.waterIndices.insert(output.waterIndices.end(), localIndices.begin(), localIndices.end());
        output.triangles.emplace_back().indices = { localIndices[0], localIndices[1], localIndices[2] };
        output.triangles.back().normal = normal;
        output.triangles.emplace_back().indices = { localIndices[3], localIndices[4], localIndices[5] };
        output.triangles.back().normal = normal;
    }
    else
    {
        output.solidIndices.insert(output.solidIndices.end(), localIndices.begin(), localIndices.end());
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

        //output.vertexData.push_back(colour.r);
        //output.vertexData.push_back(colour.g);

        //these are the offset coords into the tile texture
        output.vertexData.push_back(m_tileOffsets[face.textureIndex].x);
        output.vertexData.push_back(m_tileOffsets[face.textureIndex].y);

        output.vertexData.push_back(1.f);
        output.vertexData.push_back(aoLevels[ao[i]]);

        output.vertexData.push_back(normal.x);
        output.vertexData.push_back(normal.y);
        output.vertexData.push_back(normal.z);

        output.vertexData.push_back(UVs[i].x);
        output.vertexData.push_back(UVs[i].y);
    }
}

void ChunkSystem::addDetail(VertexOutput& output, glm::vec3 position, std::uint16_t textureIndex)
{
    std::int32_t indexOffset = static_cast<std::int32_t>(output.vertexData.size() / ChunkMeshBuilder::getVertexComponentCount());

    std::array<std::uint32_t, 12u> detailIndices = { 2,0,1,  1,3,2, 6,4,5,  5,7,6 };
    for (auto& i : detailIndices)
    {
        i += indexOffset;
    }

    output.detailIndices.insert(output.detailIndices.end(), detailIndices.begin(), detailIndices.end());

    std::array<glm::vec2, 4u> UVs =
    {
        glm::vec2(0.f, 1.f),
        glm::vec2(1.f),
        glm::vec2(0.f),
        glm::vec2(1.f, 0.f)
    };

    std::array<glm::vec3, 8u> positions =
    {
        glm::vec3(position),
        glm::vec3(position.x + 0.7f, position.y, position.z + 0.7f),
        glm::vec3(position.x, position.y + 1, position.z),
        glm::vec3(position.x + 0.7f, position.y + 1.f, position.z + 0.7f),

        glm::vec3(position.x, position.y, position.z + 0.7f),
        glm::vec3(position.x + 0.7f, position.y, position.z),
        glm::vec3(position.x, position.y + 1.f, position.z + 0.7f),
        glm::vec3(position.x + 0.7f, position.y + 1.f, position.z),
    };

    for (auto i = 0u; i < positions.size(); ++i)
    {
        output.vertexData.push_back(positions[i].x + 0.15f);
        output.vertexData.push_back(positions[i].y);
        output.vertexData.push_back(positions[i].z - 0.15f);

        //these are the offset coords into the tile texture
        //stored in the colour attribute
        output.vertexData.push_back(m_tileOffsets[textureIndex].x);
        output.vertexData.push_back(m_tileOffsets[textureIndex].y);
        output.vertexData.push_back(1.f);

        output.vertexData.push_back(1.f); //ao value

        output.vertexData.push_back(0.f); //normal
        output.vertexData.push_back(1.f);
        output.vertexData.push_back(0.f);

        output.vertexData.push_back(UVs[i % 4].x);
        output.vertexData.push_back(UVs[i % 4].y);
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