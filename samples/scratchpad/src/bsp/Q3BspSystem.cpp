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

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_DEFAULT_FILTER_UPSAMPLE    STBIR_FILTER_CUBICBSPLINE
#include "stb_image_resize.h"

#include "Q3BspSystem.hpp"

#include <crogine/core/FileSystem.hpp>

#include <crogine/detail/Types.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/MeshBuilder.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/util/String.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/ecs/Scene.hpp>

#include "../ErrorCheck.hpp" //include last to prevent redef of APIENTRY on windows

namespace
{
    const std::string Vertex = 
        R"(
        
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE vec3 a_normal;
        ATTRIBUTE vec4 a_colour;
        ATTRIBUTE vec2 a_texCoord0;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;
        uniform mat3 u_normalMatrix;

        uniform vec4 u_clipPlane;

        VARYING_OUT vec3 v_normal;
        VARYING_OUT vec4 v_colour;
        VARYING_OUT vec2 v_texCoord0;

        void main()
        {
            gl_Position = u_viewProjectionMatrix * a_position;

            v_normal = a_normal; //TODO this ought to be multiplied by the normal matrix, but the map geometry shouldn't be transformed anyway
            v_colour = a_colour;
            v_texCoord0 = a_texCoord0;

            gl_ClipDistance[0] = dot(u_worldMatrix * a_position, u_clipPlane);
        })";

    const std::string Fragment =
        R"(

        uniform sampler2D u_texture;

        VARYING_IN vec3 v_normal;
        VARYING_IN vec4 v_colour;
        VARYING_IN vec2 v_texCoord0;

        OUTPUT

        const vec3 LightDir = vec3(-0.2, 0.8, 0.5);

        void main()
        {
            float amount = dot(v_normal, normalize(LightDir));
            amount = 0.5 + (amount / 2.0);

            vec4 colour = TEXTURE(u_texture, v_texCoord0);// + v_colour;
            colour.rgb *= amount;
            FRAG_OUT = colour;
        })";

    std::int32_t currentLeaf = 0;
    std::int32_t visibleFaceCount = 0;
    std::int32_t visiblePatchCount = 0;
    std::int32_t clustersSkipped = 0;
    std::int32_t leavesCulled = 0; //this only counts the leaves skipped in the active cluster
    glm::vec3 camPos;
    cro::Box camAABB;
}

Q3BspSystem::Q3BspSystem(cro::MessageBus& mb)
    : cro::System       (mb, typeid(Q3BspSystem)),
    m_loaded            (false)
{
    std::fill(m_uniforms.begin(), m_uniforms.end(), -1);

    registerWindow([&]() 
        {
            ImGui::SetNextWindowSize({ 380.f, 400.f }, ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Window of Joy"))
            {
                ImGui::Text("Hold TAB to unlock mouse");
                ImGui::NewLine();

                auto frameRate = ImGui::GetIO().Framerate;
                ImGui::Text("Average Frame Time %3.3fms (%4.3f FPS)", (1.f / frameRate) * 1000.f, frameRate);
                ImGui::NewLine();

                ImGui::Text("Draw Calls: %lu", m_meshes[MeshData::Brush].activeSubmeshCount + m_meshes[MeshData::Patch].activeSubmeshCount);
                ImGui::Text("Of which patches: %lu", m_meshes[MeshData::Patch].activeSubmeshCount);
                ImGui::Text("Of which brushes: %lu", m_meshes[MeshData::Brush].activeSubmeshCount);
                ImGui::Text("Visible Faces: %d", visibleFaceCount);
                ImGui::Text("Visible Patches: %d", visiblePatchCount);
                ImGui::Text("Clusters Skipped: %d", clustersSkipped);
                ImGui::Text("Leaves Culled This Cluster: %d", leavesCulled);
                ImGui::Text("Current Leaf %d", currentLeaf);

                ImGui::NewLine();
                cro::Box rel = { camAABB[0] - camPos, camAABB[1] - camPos };
                ImGui::Text("Position: %3.3f, %3.3f, %3.3f", camPos.x, camPos.y, camPos.z);
                ImGui::Text("Frustum AABB: %3.1f, %3.1f, %3.1f, - %3.1f, %3.1f, %3.1f", camAABB[0].x, camAABB[0].y, camAABB[0].z, camAABB[1].x, camAABB[1].y, camAABB[1].z);
                ImGui::Text("Frustum Relative: %3.1f, %3.1f, %3.1f, - %3.1f, %3.1f, %3.1f", rel[0].x, rel[0].y, rel[0].z, rel[1].x, rel[1].y, rel[1].z);
            }
            ImGui::End();
        
        });

    //create material shaders
    if (m_shader.loadFromString(Vertex, Fragment))
    {
        const auto& uniforms = m_shader.getUniformMap();
        for (const auto& [uniform, location] : uniforms)
        {
            if (uniform == "u_viewProjectionMatrix")
            {
                m_uniforms[UniformLocation::ViewProjectionMatrix] = location.first;
            }
            else if (uniform == "u_normalMatrix")
            {
                m_uniforms[UniformLocation::NormalMatrix] = location.first;
            }
            else if (uniform == "u_texture")
            {
                m_uniforms[UniformLocation::Texture0] = location.first;
            }
            else if (uniform == "u_worldMatrix")
            {
                m_uniforms[UniformLocation::WorldMatrix] = location.first;
            }
            else if (uniform == "u_clipPlane")
            {
                m_uniforms[UniformLocation::ClipPlane] = location.first;
            }
        }
    }
}

Q3BspSystem::~Q3BspSystem()
{
    //delete opengl stuffs
    if (m_meshes[MeshData::Brush].mesh.vboAllocation.vboID)
    {
        for (auto& [ibo, mat] : m_meshes[MeshData::Brush].submeshes)
        {
#ifdef PLATFORM_DESKTOP
            glCheck(glDeleteVertexArrays(1, &ibo.vao[0]));
#endif
            glCheck(glDeleteBuffers(1, &ibo.ibo));
        }

        glCheck(glDeleteBuffers(1, &m_meshes[MeshData::Brush].mesh.vboAllocation.vboID));
    }
}

//public
void Q3BspSystem::process(float)
{

}

void Q3BspSystem::updateDrawList(cro::Entity camera)
{
    if (!m_loaded) return;

    //get faces visible to this camera
    const auto position = camera.getComponent<cro::Transform>().getWorldPosition();
    auto leafIndex = findLeaf(position);
    auto clusterIndex = m_leaves[leafIndex].cluster;

    currentLeaf = leafIndex;
    visibleFaceCount = 0;
    visiblePatchCount = 0;
    clustersSkipped = 0;
    leavesCulled = 0;
    camPos = position;
    camAABB = camera.getComponent<cro::Camera>().getPass(cro::Camera::Pass::Final).getAABB();

    static std::vector<bool> usedFaces;
    usedFaces.resize(m_faces.size());
    std::fill(usedFaces.begin(), usedFaces.end(), false);

    const auto frustum = camera.getComponent<cro::Camera>().getPass(cro::Camera::Pass::Final).getFrustum();
    static std::vector<std::int32_t> visibleFaces;
    visibleFaces.clear();

    static std::vector<std::int32_t> visiblePatches;
    visiblePatches.clear();

    auto i = m_leaves.size();
    while (i--)
    //for(auto i = 0; i < m_leaves.size(); ++i)
    {
        const auto& leaf = m_leaves[i];

        if (!clusterVisible(clusterIndex, leaf.cluster))
        {
            clustersSkipped++;
            continue;
        }

        //check leaf bb is in camera frustum
        const cro::Box& bb = m_leafBoundingBoxes[i];

        bool visible = true;
        std::size_t j = 0;
        while (visible && j < frustum.size())
        {
            visible = (cro::Spatial::intersects(frustum[j++], bb) != cro::Planar::Back);
        }

        if (!visible)
        {
            leavesCulled++;
            continue;
        }


        auto faceCount = leaf.faceCount;

        //sort the visible  faces by material ID to reduce state switching
        while (faceCount--)
        {
            auto faceIndex = m_leafFaces[leaf.firstFace + faceCount];

            auto type = m_faces[faceIndex].type;
            if (type == Q3::Billboard)
            {
                continue; 
            }

            if (!usedFaces[faceIndex])
            {
                usedFaces[faceIndex] = true;
                if (type == Q3::Patch)
                {
                    visiblePatches.push_back(faceIndex);
                    visiblePatchCount++;
                }
                else
                {
                    visibleFaces.push_back(faceIndex);
                    visibleFaceCount++;
                }
            }
        }

    }

    //sort faces by UID so we get one IBO for each texture/lightmap combo
    //TODO how are we going to split the alpha blended materials here?
    //TODO how to depth sort the faces once sorted by materials?
    std::sort(visibleFaces.begin(), visibleFaces.end(), 
        [&](std::int32_t faceA, std::int32_t faceB)
        {
            return m_faceMatIDs[faceA].combinedID < m_faceMatIDs[faceB].combinedID;
        });

    //update the IBOs with the face data and count how many we used
    //for each IBO keep the texture ID/lightmap ID so it can be bound when drawing
    m_meshes[MeshData::Brush].activeSubmeshCount = 0;
    std::vector<std::uint32_t> indexData;

    for (auto i = 0u; i < visibleFaces.size();)
    {
        auto& [submesh, matData] = m_meshes[MeshData::Brush].submeshes[m_meshes[MeshData::Brush].activeSubmeshCount];
        matData = m_faceMatIDs[visibleFaces[i]];

        indexData.clear();
        while (i < visibleFaces.size() &&
            matData.combinedID == m_faceMatIDs[visibleFaces[i]].combinedID)
        {
            const auto& face = m_faces[visibleFaces[i]];

            for(auto k = 0; k < face.meshIndexCount; ++k)
            {
                indexData.push_back(face.firstVertIndex + m_indices[face.firstMeshIndex + k]);
            }

            i++;
        }

        submesh.indexCount = static_cast<std::uint32_t>(indexData.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indexCount * sizeof(std::uint32_t), indexData.data(), GL_DYNAMIC_DRAW));
        m_meshes[MeshData::Brush].activeSubmeshCount++;
    }

    //the same thing for the patches mesh
    m_meshes[MeshData::Patch].activeSubmeshCount = 0;
    for (auto i = 0u; i < visiblePatches.size();)
    {
        auto& [submesh, matData] = m_meshes[MeshData::Patch].submeshes[m_meshes[MeshData::Patch].activeSubmeshCount];
        matData = m_faceMatIDs[visiblePatches[i]];

        indexData.clear();
        while (i < visiblePatches.size()
            && matData.combinedID == m_faceMatIDs[visiblePatches[i]].combinedID)
        {
            const auto& patch = m_patches[m_patchIndices[visiblePatches[i]]];

            auto patchIndices = patch.getIndices();

            //create a degen tri if not the first vert
            if (!indexData.empty())
            {
                indexData.push_back(patchIndices[0]);
            }

            indexData.insert(indexData.end(), patchIndices.begin(), patchIndices.end());
            i++;
            //final degen
            indexData.push_back(indexData.back());
        }
        //remove the final degen from this strip collection
        indexData.pop_back();

        submesh.indexCount = static_cast<std::uint32_t>(indexData.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indexCount * sizeof(std::uint32_t), indexData.data(), GL_DYNAMIC_DRAW));
        m_meshes[MeshData::Patch].activeSubmeshCount++;
    }

    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

void Q3BspSystem::render(cro::Entity camera, const cro::RenderTarget&)
{
    if (!m_loaded) return;

    const auto& camComponent = camera.getComponent<cro::Camera>();
    const auto& pass = camComponent.getActivePass();
    
    //we're assuming the level geometry is untransformed
    //so the world and normal matrices are identity
    glm::mat4 worldMatrix = glm::mat3(1.f);
    glm::mat4 normalMatrix = glm::mat3(1.f);

    glm::vec4 clipPlane = glm::vec4(0.f, 1.f, 0.f, -getScene()->getWaterLevel() + (0.05f * pass.getClipPlaneMultiplier())) * pass.getClipPlaneMultiplier();

    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glCullFace(/*GL_FRONT*/pass.getCullFace() == GL_FRONT ? GL_BACK : GL_FRONT)); //TODO enable this when we know faces are wound correctly
    glCheck(glEnable(GL_DEPTH_TEST));

    //bind shader
    glCheck(glUseProgram(m_shader.getGLHandle()));

    glCheck(glUniformMatrix4fv(m_uniforms[UniformLocation::ViewProjectionMatrix], 1, GL_FALSE, glm::value_ptr(pass.viewProjectionMatrix)));
    glCheck(glUniformMatrix4fv(m_uniforms[UniformLocation::WorldMatrix], 1, GL_FALSE, glm::value_ptr(worldMatrix)));
    glCheck(glUniformMatrix3fv(m_uniforms[UniformLocation::NormalMatrix], 1, GL_FALSE, &normalMatrix[0][0]));
    glCheck(glUniform4f(m_uniforms[UniformLocation::ClipPlane], clipPlane.r, clipPlane.g, clipPlane.b, clipPlane.a));

    //assume lightmap is always in unit 0 
    glCheck(glUniform1i(m_uniforms[UniformLocation::Texture0], 0));

    //and material textures are 1,2,3 etc


    for (const auto& currentMesh : m_meshes)
    {
#ifndef PLATFORM_DESKTOP
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, currentMesh.mesh.vbo));
#endif //PLATFORM

        m_activeMatData.lightmapID = -1;
        m_activeMatData.materialID = -1;
        for (auto i = 0u; i < currentMesh.activeSubmeshCount; ++i)
        {
            const auto& [submesh, matData] = currentMesh.submeshes[i];

            if (m_activeMatData.lightmapID != matData.lightmapID)
            {
                //switch lightmap
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_lightmaps[matData.lightmapID].getGLHandle());
            }
            if (m_activeMatData.materialID != matData.materialID)
            {
                //switch material textures
                //apply blend/face culling
            }
            m_activeMatData = matData;

#ifdef PLATFORM_DESKTOP
            glCheck(glBindVertexArray(submesh.vao[0]));
            glCheck(glDrawElements(static_cast<GLenum>(submesh.primitiveType), submesh.indexCount, static_cast<GLenum>(submesh.format), 0));

#else //GLES 2 doesn't have VAO support without extensions

            //bind attribs
            const auto& attribs = m_material.attribs;
            for (auto j = 0u; j < m_material.attribCount; ++j)
            {
                glCheck(glEnableVertexAttribArray(attribs[j][cro::Material::Data::Index]));
                glCheck(glVertexAttribPointer(attribs[j][cro::Material::Data::Index], attribs[j][cro::Material::Data::Size],
                    GL_FLOAT, GL_FALSE, static_cast<GLsizei>(m_meshes[MeshData::Brush].mesh.vertexSize),
                    reinterpret_cast<void*>(static_cast<intptr_t>(attribs[j][cro::Material::Data::Offset]))));
            }

            //bind element/index buffer
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));

            //draw elements
            glCheck(glDrawElements(static_cast<GLenum>(submesh.primitiveType), submesh.indexCount, static_cast<GLenum>(submesh.format), 0));

            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

            //unbind attribs
            for (auto j = 0u; j < m_material.attribCount; ++j)
            {
                glCheck(glDisableVertexAttribArray(attribs[j][cro::Material::Data::Index]));
            }
#endif //PLATFORM 
        }
    }
#ifdef PLATFORM_DESKTOP
    glCheck(glBindVertexArray(0));
#else
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif //PLATFORM

    glCheck(glUseProgram(0));

    glCheck(glDisable(GL_BLEND));
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDepthMask(GL_TRUE));
}

bool Q3BspSystem::loadMap(const std::string& mapPath)
{
    m_loaded = false;
    if (m_shader.getGLHandle() == 0)
    {
        //shader failed to load for some reason
        return false;
    }

    LOG("TODO clear existing data before loading new map", cro::Logger::Type::Info);
    m_indices.clear();
    m_faces.clear();
    m_faceMatIDs.clear();
    m_planes.clear();
    m_nodes.clear();
    m_leaves.clear();
    m_clusterBitsets.clear();
    m_leafBoundingBoxes.clear();
    m_leafFaces.clear();
    m_patches.clear();
    m_entityData.clear();
    m_spawnPoints.clear();
    m_meshes[MeshData::Brush].activeSubmeshCount = 0;
    m_meshes[MeshData::Patch].activeSubmeshCount = 0;

    auto path = cro::FileSystem::getResourcePath() + mapPath;

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "rb");
    if (file.file)
    {
        //validate size
        SDL_RWseek(file.file, 0, RW_SEEK_END);
        auto fileSize = SDL_RWtell(file.file);

        if (fileSize < sizeof(Q3::Header))
        {
            LogE << mapPath << ": error reading file, size too small" << std::endl;
            return false;
        }

        SDL_RWseek(file.file, 0, RW_SEEK_SET);

        //validate header
        Q3::Header header;

        SDL_RWread(file.file, &header, sizeof(header), 1);
        std::string id(header.id, 4);
        if (id != "IBSP")
        {
            LogE << mapPath << ": Incorrect header id. Should be IBSP, got " << id << std::endl;
            return false;
        }
        if (header.version != 46)
        {
            LogE << mapPath << ": Incorrect version id. Should be 46, got " << header.version << std::endl;
            return false;
        }

        //read lump info
        std::vector<Q3::Lump> lumpInfo(Q3::Lumps::MaxLumps);
        SDL_RWread(file.file, lumpInfo.data(), sizeof(Q3::Lump) * Q3::Lumps::MaxLumps, 1);

        //parse the vertex list
        std::vector<Q3::Vertex> vertices;
        parseLump(vertices, file.file, lumpInfo[Q3::Vertices]);

        //parse the index list - used to build the IBO at runtime from the PVS
        parseLump(m_indices, file.file, lumpInfo[Q3::Indices]);

        //used by the PVS during rendering with index list, above
        parseLump(m_faces, file.file, lumpInfo[Q3::Faces]);


        //create a unique lightmap/material ID pair so we can assign each group
        //of faces to a single IBO when rendering
        std::vector<std::uint32_t> uniqueFaces;
        for (const auto& face : m_faces)
        {
            std::uint32_t uid = (face.lightmapID << 8) | face.materialID;
            uniqueFaces.push_back(uid);

            auto& matData = m_faceMatIDs.emplace_back();
            matData.combinedID = uid;
            matData.lightmapID = face.lightmapID;
            matData.materialID = face.materialID;
        }

        //sort and count unique instances so we know how many IBOs we need in wosrt case
        std::sort(uniqueFaces.begin(), uniqueFaces.end());
        auto submeshCount = std::unique(uniqueFaces.begin(), uniqueFaces.end()) - uniqueFaces.begin();

        createMesh(vertices, submeshCount);

        //go over faces and create patch meshes
        m_patchIndices.resize(m_faces.size());
        std::fill(m_patchIndices.begin(), m_patchIndices.end(), -1);

        std::vector<float> patchVerts;
        for (auto i = 0u; i < m_faces.size(); ++i)
        {
            if (m_faces[i].type == Q3::Patch)
            {
                m_patchIndices[i] = static_cast<std::int32_t>(m_patches.size());
                m_patches.emplace_back(m_faces[i], vertices, patchVerts);
            }
        }
        createPatchMesh(patchVerts);


        //TODO load texture/material info - needed to split alpha blend pass, and set face culling
        //TODO load entity lump for props etc?

        //load the plane data
        std::vector<Q3::Plane> planes;
        parseLump(planes, file.file, lumpInfo[Q3::Planes]);
        for (const auto& plane : planes)
        {
            m_planes.emplace_back(plane.normal.x, plane.normal.z, -plane.normal.y, plane.distance);
        }

        //load the bsp node data
        parseLump(m_nodes, file.file, lumpInfo[Q3::Nodes]);

        //load the leaf data
        parseLump(m_leaves, file.file, lumpInfo[Q3::Leaves]);
        //cache the bounding boxes (remember to swap coords!)
        for (const auto& l : m_leaves)
        {
            cro::Box bb(glm::vec3(l.bbMin.x, l.bbMin.z, -l.bbMin.y), glm::vec3(l.bbMax.x, l.bbMax.z, -l.bbMax.y));
            m_leafBoundingBoxes.emplace_back(bb);
        }

        parseLump(m_leafFaces, file.file, lumpInfo[Q3::LeafFaces]);

        std::vector<char> entityData;
        parseLump(entityData, file.file, lumpInfo[Q3::Entities]);
        parseEntities(entityData);

        //read the cluster data
        SDL_RWseek(file.file, lumpInfo[Q3::VisData].offset, RW_SEEK_SET);
        SDL_RWread(file.file, &m_clusters.clusterCount, sizeof(std::int32_t), 1);
        SDL_RWread(file.file, &m_clusters.bytesPerCluster, sizeof(std::int32_t), 1);
        //as this expects a dynamic array we'll fill a vector then point the struct member to that
        std::int32_t visSize = m_clusters.clusterCount * m_clusters.bytesPerCluster;
        m_clusterBitsets.resize(visSize);
        SDL_RWread(file.file, m_clusterBitsets.data(), sizeof(std::int8_t), visSize);
        m_clusters.bitsetArray = m_clusterBitsets.data();


        //load the lightmap data
        std::uint32_t lightmapCount = lumpInfo[Q3::Lightmaps].length / sizeof(Q3::Lightmap);
        SDL_RWseek(file.file, lumpInfo[Q3::Lightmaps].offset, RW_SEEK_SET);
        buildLightmaps(file.file, lightmapCount);

        m_loaded = true;
        return true;
    }

    return false;
}

//private
void Q3BspSystem::parseEntities(const std::vector<char>& entData)
{
    //TODO no error checking is currently done because it assumes we're
    //loading a valid BSP...
    bool first = true;
    for (auto i = 0u; i < entData.size(); ++i)
    {
        if (entData[i] == '{')
        {
            m_entityData.push_back(Q3::EntityData());
        }

        if (entData[i] == '\"')
        {
            i++;

            if (first)
            {
                m_entityData.back().emplace_back(std::make_pair("", ""));
            }

            do
            {
                if (first)
                {
                    m_entityData.back().back().first.push_back(entData[i]);
                }
                else
                {
                    m_entityData.back().back().second.push_back(entData[i]);
                }
            } while (entData[++i] != '\"');
            first = !first;
        }
    }

    for (const auto& data : m_entityData)
    {
        const std::pair<std::string, std::string>* result = nullptr;
        for (const auto& p : data)
        {
            if (p.first == "classname")
            {
                result = &p;
                break;
            }
        }

        if (result)
        {
            //find spawns
            if (result->second == "info_player_deathmatch")
            {
                auto& spawn = m_spawnPoints.emplace_back();
                for (const auto& prop : data)
                {
                    if (prop.first == "angle")
                    {
                        try
                        {
                            spawn.rotation = std::stof(prop.second.c_str());
                        }
                        catch (...)
                        {
                            LogE << prop.second << ": invalid spawn angle" << std::endl;
                        }
                    }
                    else if (prop.first == "origin")
                    {
                        try
                        {
                            auto values = cro::Util::String::tokenize(prop.second, ' ');
                            if (values.size() == 3)
                            {
                                spawn.position = { std::stof(values[0]), std::stof(values[2]), -std::stof(values[1]) };
                            }
                            else
                            {
                                LogE << "Too few components found in spawn position vector" << std::endl;
                            }
                        }
                        catch (...)
                        {
                            LogE << prop.second << ": invalid spawn position" << std::endl;
                        }
                    }
                }
            }

            //TODO parse other entity types
        }
    }

}

void Q3BspSystem::buildLightmaps(SDL_RWops* file, std::uint32_t count)
{
    //this would be a perfect opportunity to use GL_TEXTURE_2D_ARRAY
    //because we'd only have as many submeshes as materials, and fewer
    //texture switches... but guess what - no GLES2 support :(

    const std::uint32_t smallSize = 128;
    const std::uint32_t largeSize = 512;

    std::vector<std::uint8_t> buffer(smallSize * smallSize * 3);
    std::vector<std::uint8_t> resizeBuffer(largeSize * largeSize * 3);

    auto adjustGamma = [&buffer]()
    {
        const float amount = 8.f;

        for (auto i = 0u; i < buffer.size(); ++i)
        {
            float scale = 1.f;
            float temp = 0.f;

            float currentByte = static_cast<float>(buffer[i]);
            currentByte *= amount / 255.f;

            //clamp to max value
            if (currentByte > 1.f && (temp = (1.f / currentByte)) < scale) scale = temp;

            currentByte *= scale * 255.f;
            buffer[i] = static_cast<std::uint8_t>(currentByte);
        }
    };


    for (auto i = 0u; i < count; ++i)
    {
        SDL_RWread(file, buffer.data(), buffer.size(), 1);
        adjustGamma();

        //stbir_resize_uint8(buffer.data(), smallSize, smallSize, 0,
        //                    resizeBuffer.data(), largeSize, largeSize, 0, 3);

        //stbir_resize_uint8_generic(buffer.data(), smallSize, smallSize, 0,
        //                            resizeBuffer.data(), largeSize, largeSize, 0, 3,
        //                            0, 0,
        //                            STBIR_EDGE_ZERO, STBIR_FILTER_CUBICBSPLINE, STBIR_COLORSPACE_LINEAR,
        //                            nullptr);


        auto& texture = m_lightmaps.emplace_back();
        texture.create(smallSize, smallSize, cro::ImageFormat::RGB);
        texture.update(buffer.data());
        texture.setSmooth(true);
        //texture.setRepeated(true);

        /*cro::Image img;
        img.loadFromMemory(buffer.data(), 128, 128, cro::ImageFormat::RGB);
        img.write("img_" + std::to_string(i) + ".png");*/
    }

    //add a blank lightmap at the end of the array so faces with no
    //lightmap can be assigned this in the shader, as the shader still
    //requires the lightmap uniform be set.
    std::vector<std::uint8_t> smallBuff = { 255,255,255,255, 255,255,255,255, 255,255,255,255 };
    auto& texture = m_lightmaps.emplace_back();
    texture.create(2,2, cro::ImageFormat::RGB);
    texture.update(smallBuff.data());

    //and correct the face indices to point to the correct location
    for (auto& face : m_faceMatIDs)
    {
        if (face.lightmapID == -1)
        {
            face.lightmapID += static_cast<std::int32_t>(m_lightmaps.size());
        }
    }
}

void Q3BspSystem::createMesh(const std::vector<Q3::Vertex>& vertices, std::size_t submeshCount)
{
    auto addSubmesh = [&]()
    {
        auto& [submesh, matData] = m_meshes[MeshData::Brush].submeshes.emplace_back();
        submesh.format = GL_UNSIGNED_INT;
        submesh.primitiveType = GL_TRIANGLES;

        glCheck(glGenBuffers(1, &submesh.ibo));

#ifdef PLATFORM_DESKTOP
        glCheck(glGenVertexArrays(1, &submesh.vao[0]));

        glCheck(glBindVertexArray(submesh.vao[0]));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_meshes[MeshData::Brush].mesh.vboAllocation.vboID));
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));

        const auto& attribs = m_material.attribs;
        for (auto j = 0u; j < m_material.attribCount; ++j)
        {
            glCheck(glEnableVertexAttribArray(attribs[j][cro::Material::Data::Index]));
            glCheck(glVertexAttribPointer(attribs[j][cro::Material::Data::Index], attribs[j][cro::Material::Data::ComponentCount],
                GL_FLOAT, GL_FALSE, static_cast<GLsizei>(m_meshes[MeshData::Brush].mesh.vertexSize),
                reinterpret_cast<void*>(static_cast<intptr_t>(attribs[j][cro::Material::Data::Offset]))));
        }

        glCheck(glBindVertexArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
#endif
    };

    //check we don't already have a buffer before creating a new one
    if (!m_meshes[MeshData::Brush].mesh.vboAllocation.vboID)
    {
        //create new buffers
        CRO_ASSERT(m_meshes[MeshData::Brush].submeshes.empty(), "ibos not empty!");
        glCheck(glGenBuffers(1, &m_meshes[MeshData::Brush].mesh.vboAllocation.vboID));

        m_meshes[MeshData::Brush].mesh.attributes[cro::Mesh::Attribute::Position].componentCount = 3;
        m_meshes[MeshData::Brush].mesh.attributes[cro::Mesh::Attribute::Colour].componentCount = 4;
        m_meshes[MeshData::Brush].mesh.attributes[cro::Mesh::Attribute::Normal].componentCount = 3;
        m_meshes[MeshData::Brush].mesh.attributes[cro::Mesh::Attribute::UV0].componentCount = 2;
        m_meshes[MeshData::Brush].mesh.attributeFlags = (cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal | cro::VertexProperty::UV0);

        m_meshes[MeshData::Brush].mesh.primitiveType = GL_TRIANGLES;
        for (auto a : m_meshes[MeshData::Brush].mesh.attributes)
        {
            m_meshes[MeshData::Brush].mesh.vertexSize += a.componentCount;
        }
        m_meshes[MeshData::Brush].mesh.vertexSize *= sizeof(float);
        
        initMaterial();

        //create empty IBOs - these will be updated at runtime
        for (auto j = 0u; j < submeshCount; ++j)
        {
            addSubmesh();
        }
    }

    //check if we need to increase the IBO size when loading a new map
    if (submeshCount > m_meshes[MeshData::Brush].submeshes.size())
    {
        for (auto j = m_meshes[MeshData::Brush].submeshes.size(); j < submeshCount; ++j)
        {
            addSubmesh();
        }
    }

    //create vertex data from vertices - Q3 is z-up so we're swapping coords here
    std::vector<float> vertexData;
    for (const auto& vertex : vertices)
    {
        vertexData.push_back(vertex.position.x);
        vertexData.push_back(vertex.position.z);
        vertexData.push_back(-vertex.position.y);

        vertexData.push_back(static_cast<float>(vertex.colour[0]) / 255.f);
        vertexData.push_back(static_cast<float>(vertex.colour[1]) / 255.f);
        vertexData.push_back(static_cast<float>(vertex.colour[2]) / 255.f);
        vertexData.push_back(static_cast<float>(vertex.colour[3]) / 255.f);

        vertexData.push_back(vertex.normal.x);
        vertexData.push_back(vertex.normal.z);
        vertexData.push_back(-vertex.normal.y);

        vertexData.push_back(vertex.uv1.x);
        vertexData.push_back(vertex.uv1.y);
    }
    m_meshes[MeshData::Brush].mesh.vertexCount = vertexData.size() / (m_meshes[MeshData::Brush].mesh.vertexSize / sizeof(float));

    //upload vert data
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_meshes[MeshData::Brush].mesh.vboAllocation.vboID));
    glCheck(glBufferData(GL_ARRAY_BUFFER, m_meshes[MeshData::Brush].mesh.vertexSize * m_meshes[MeshData::Brush].mesh.vertexCount, vertexData.data(), GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void Q3BspSystem::createPatchMesh(const std::vector<float>& vertices)
{
    auto addSubmesh = [&]()
    {
        auto& [submesh, matData] = m_meshes[MeshData::Patch].submeshes.emplace_back();
        submesh.format = GL_UNSIGNED_INT;
        submesh.primitiveType = GL_TRIANGLE_STRIP;

        glCheck(glGenBuffers(1, &submesh.ibo));

#ifdef PLATFORM_DESKTOP
        glCheck(glGenVertexArrays(1, &submesh.vao[0]));

        glCheck(glBindVertexArray(submesh.vao[0]));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_meshes[MeshData::Patch].mesh.vboAllocation.vboID));
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));

        const auto& attribs = m_material.attribs;
        for (auto j = 0u; j < m_material.attribCount; ++j)
        {
            glCheck(glEnableVertexAttribArray(attribs[j][cro::Material::Data::Index]));
            glCheck(glVertexAttribPointer(attribs[j][cro::Material::Data::Index], attribs[j][cro::Material::Data::ComponentCount],
                GL_FLOAT, GL_FALSE, static_cast<GLsizei>(m_meshes[MeshData::Patch].mesh.vertexSize),
                reinterpret_cast<void*>(static_cast<intptr_t>(attribs[j][cro::Material::Data::Offset]))));
        }

        glCheck(glBindVertexArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
#endif
    };

    //check we don't already have a buffer before creating a new one
    if (!m_meshes[MeshData::Patch].mesh.vboAllocation.vboID)
    {
        //create new buffers
        CRO_ASSERT(m_meshes[MeshData::Patch].submeshes.empty(), "ibos not empty!");
        glCheck(glGenBuffers(1, &m_meshes[MeshData::Patch].mesh.vboAllocation.vboID));

        m_meshes[MeshData::Patch].mesh.attributes[cro::Mesh::Attribute::Position].componentCount = 3;
        m_meshes[MeshData::Patch].mesh.attributes[cro::Mesh::Attribute::Colour].componentCount = 4;
        m_meshes[MeshData::Patch].mesh.attributes[cro::Mesh::Attribute::Normal].componentCount = 3;
        m_meshes[MeshData::Patch].mesh.attributes[cro::Mesh::Attribute::UV0].componentCount = 2;
        m_meshes[MeshData::Patch].mesh.attributeFlags = (cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal | cro::VertexProperty::UV0);

        m_meshes[MeshData::Patch].mesh.primitiveType = GL_TRIANGLE_STRIP;
        for (auto a : m_meshes[MeshData::Patch].mesh.attributes)
        {
            m_meshes[MeshData::Patch].mesh.vertexSize += a.componentCount;
        }
        m_meshes[MeshData::Patch].mesh.vertexSize *= sizeof(float);

        //create empty IBOs - these will be updated at runtime
        //TODO this may be creating more than necessary, but its better than not creating enough
        //ideally we should be checking the max unique number of material IDs for patches
        for (auto j = 0u; j < m_patches.size(); ++j)
        {
            addSubmesh();
        }
    }

    //upload vert data
    m_meshes[MeshData::Patch].mesh.vertexCount = vertices.size() / (m_meshes[MeshData::Patch].mesh.vertexSize / sizeof(float));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_meshes[MeshData::Patch].mesh.vboAllocation.vboID));
    glCheck(glBufferData(GL_ARRAY_BUFFER, m_meshes[MeshData::Patch].mesh.vertexSize * m_meshes[MeshData::Patch].mesh.vertexCount, vertices.data(), GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void Q3BspSystem::initMaterial() 
{
    //TODO this assumes one material for everything for now - and that
    //the patch and brush meshes have the same vertex attribs

    //calc attrib map for the shader
    const auto& shaderAttribs = m_shader.getAttribMap();
    for (auto i = 0u; i < shaderAttribs.size(); ++i)
    {
        m_material.attribs[i][cro::Material::Data::Index] = shaderAttribs[i];
    }

    std::size_t pointerOffset = 0;
    for (auto i = 0u; i < cro::Mesh::Attribute::Total; ++i)
    {
        if (m_material.attribs[i][cro::Material::Data::Index] > -1)
        {
            //attrib exists in shader so map its size
            m_material.attribs[i][cro::Material::Data::ComponentCount] = static_cast<std::int32_t>(m_meshes[MeshData::Brush].mesh.attributes[i].componentCount);

            //calc the pointer offset for each attrib
            m_material.attribs[i][cro::Material::Data::Offset] = static_cast<std::int32_t>(pointerOffset * sizeof(float));
        }
        pointerOffset += m_meshes[MeshData::Brush].mesh.attributes[i].componentCount; //count the offset regardless as the mesh may have more attributes than material
    }

    //sort by size
    std::sort(std::begin(m_material.attribs), std::end(m_material.attribs),
        [](const std::array<std::int32_t, cro::Material::Data::Count>& ip,
            const std::array<std::int32_t, cro::Material::Data::Count>& op)
        {
            return ip[cro::Material::Data::ComponentCount] > op[cro::Material::Data::ComponentCount];
        });

    //count attribs with size > 0
    int i = 0;
    while (m_material.attribs[i++][cro::Material::Data::ComponentCount] != 0)
    {
        m_material.attribCount++;
    }
}

std::int32_t Q3BspSystem::findLeaf(glm::vec3 camPos) const
{
    std::int32_t i = 0;
    float distance = 0.f;

    //walk the BSP tree until we find the leaf containing our position
    while (i >= 0)
    {
        const auto& node = m_nodes[i];
        auto plane = m_planes[node.planeIndex];

        distance = glm::dot(glm::vec3(plane), camPos) - plane.w;
        i = (distance >= 0) ? node.frontIndex : node.rearIndex;

        //LogI << node.planeIndex << std::endl;
    }
    return ~i;
}

bool Q3BspSystem::clusterVisible(std::int32_t currentCluster, std::int32_t testCluster) const
{
    if (!m_clusters.bitsetArray || currentCluster < 0)
    {
        return true;
    }

    auto index = (currentCluster * m_clusters.bytesPerCluster) + (testCluster >> 3);

    /*if (index < 0)
    {
        return false;
    }*/

    std::int8_t visSet = m_clusterBitsets[index];

    std::int32_t result = visSet & (1 << (testCluster & 7));
    return (result != 0);
}