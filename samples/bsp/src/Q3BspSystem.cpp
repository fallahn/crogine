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

#include "Q3BspSystem.hpp"
#include "ErrorCheck.hpp"

#include <crogine/core/FileSystem.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/ecs/components/Camera.hpp>

namespace
{
    const std::string Vertex = 
        R"(
        
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE vec3 a_normal;
        ATTRIBUTE vec2 a_texCoord0;

        uniform mat4 u_viewProjectionMatrix;
        uniform mat3 u_normalMatrix;

        VARYING_OUT vec3 v_normal;
        VARYING_OUT vec2 v_texCoord0;

        void main()
        {
            gl_Position = u_viewProjectionMatrix * a_position;

            v_normal = a_normal;

            v_texCoord0 = a_texCoord0;
        })";

    const std::string Fragment =
        R"(

        uniform sampler2D u_texture;

        VARYING_IN vec3 v_normal;
        VARYING_IN vec2 v_texCoord0;

        OUTPUT

        const vec3 LightDir = vec3(-0.2, 0.8, 0.5);

        void main()
        {
            float amount = dot(v_normal, normalize(LightDir));

            FRAG_OUT = vec4(vec3(amount), 1.0);// TEXTURE(u_texture, v_texCoord0);
        })";
}

Q3BspSystem::Q3BspSystem(cro::MessageBus& mb)
    : cro::System       (mb, typeid(Q3BspSystem)),
    m_activeSubmeshCount(0)
{
    std::fill(m_uniforms.begin(), m_uniforms.end(), -1);


    //create material shaders
    if (m_shader.loadFromString(Vertex, Fragment))
    {
        const auto& uniforms = m_shader.getUniformMap();
        for (const auto& [uniform, location] : uniforms)
        {
            if (uniform == "u_viewProjectionMatrix")
            {
                m_uniforms[UniformLocation::ViewProjectionMatrix] = location;
            }
            else if (uniform == "u_normalMatrix")
            {
                m_uniforms[UniformLocation::NormalMatrix] = location;
            }
            else if (uniform == "u_texture")
            {
                m_uniforms[UniformLocation::Texture0] = location;
            }
        }
    }
}

Q3BspSystem::~Q3BspSystem()
{
    //delete opengl stuffs
    if (m_mesh.vbo)
    {
        for (auto& ibo : m_submeshes)
        {
#ifdef PLATFORM_DESKTOP
            glCheck(glDeleteVertexArrays(1, &ibo.vao));
#endif
            glCheck(glDeleteBuffers(1, &ibo.ibo));
        }

        glCheck(glDeleteBuffers(1, &m_mesh.vbo));
    }
}

//public
void Q3BspSystem::process(float)
{

}

void Q3BspSystem::updateDrawList(cro::Entity)
{
    //get faces visible to this camera

    //sort faces by UID so we get one IBO for each texture/lightmap combo

    //update the IBOs with the face data and count how many we used
    //for each IBO keep the texture ID/lightmap ID - probably all in a array of structs
}

void Q3BspSystem::render(cro::Entity camera, const cro::RenderTarget&)
{
    const auto& camComponent = camera.getComponent<cro::Camera>();
    glm::mat4 normalMatrix = glm::mat3(1.f);

#ifndef PLATFORM_DESKTOP
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_mesh.vbo));
#endif //PLATFORM

    //glCheck(glEnable(GL_CULL_FACE));
    //glCheck(glCullFace(GL_BACK)); //TODO enable this when we know faces are wound correctly
    glCheck(glEnable(GL_DEPTH_TEST));

    //bind shader
    glCheck(glUseProgram(m_shader.getGLHandle()));

    glCheck(glUniformMatrix4fv(m_uniforms[UniformLocation::ViewProjectionMatrix], 1, GL_FALSE, glm::value_ptr(camComponent.viewProjectionMatrix)));
    glCheck(glUniformMatrix3fv(m_uniforms[UniformLocation::NormalMatrix], 1, GL_FALSE, &normalMatrix[0][0]));

    for (auto i = 0u; i < m_activeSubmeshCount; ++i) //TODO only draw up to active IBO count
    {
        //TODO look up the correct textures and set the uniform for this submesh...
        //TODO apply the correct blend mode for the submesh


#ifdef PLATFORM_DESKTOP
        glCheck(glBindVertexArray(m_submeshes[i].vao));
        glCheck(glDrawElements(static_cast<GLenum>(m_submeshes[i].primitiveType), m_submeshes[i].indexCount, static_cast<GLenum>(m_submeshes[i].format), 0));

#else //GLES 2 doesn't have VAO support without extensions

            //bind attribs
        const auto& attribs = m_material.attribs;
        for (auto j = 0u; j < m_material.attribCount; ++j)
        {
            glCheck(glEnableVertexAttribArray(attribs[j][cro::Material::Data::Index]));
            glCheck(glVertexAttribPointer(attribs[j][cro::Material::Data::Index], attribs[j][cro::Material::Data::Size],
                GL_FLOAT, GL_FALSE, static_cast<GLsizei>(m_mesh.vertexSize),
                reinterpret_cast<void*>(static_cast<intptr_t>(attribs[j][cro::Material::Data::Offset]))));
        }

        //bind element/index buffer
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_submeshes[i].ibo));

        //draw elements
        glCheck(glDrawElements(static_cast<GLenum>(m_submeshes[i].primitiveType), m_submeshes[i].indexCount, static_cast<GLenum>(m_submeshes[i].format), 0));

        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

        //unbind attribs
        for (auto j = 0u; j < m_material.attribCount; ++j)
        {
            glCheck(glDisableVertexAttribArray(attribs[j][cro::Material::Data::Index]));
        }
#endif //PLATFORM 
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
    LOG("TODO clear existing data before loading new map", cro::Logger::Type::Info);
    m_indices.clear();
    m_faces.clear();
    m_faceMatIDs.clear();

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

        //this is stored as member data as it's used by the PVS during rendering
        parseLump(m_faces, file.file, lumpInfo[Q3::Faces]);

        //create a unique lightmap/material ID pair so we can assign each group
        //of faces to a single IBO when rendering
        std::vector<std::uint16_t> uniqueFaces;
        for (const auto& face : m_faces)
        {
            std::uint16_t uid = (face.lightmapID << 8) | face.materialID;
            uniqueFaces.push_back(uid);

            m_faceMatIDs.push_back(uid);
        }

        //sort and count unique instances so we know how many IBOs we need in wosrt case
        std::sort(uniqueFaces.begin(), uniqueFaces.end());
        auto submeshCount = std::unique(uniqueFaces.begin(), uniqueFaces.end()) - uniqueFaces.begin();

        createMesh(vertices, submeshCount);


        //TODO load texture/material info
        //TODO load entity lump for props etc?

        //load the lightmap data
        std::uint32_t lightmapCount = lumpInfo[Q3::Lightmaps].length / sizeof(Q3::Lightmap);
        SDL_RWseek(file.file, lumpInfo[Q3::Lightmaps].offset, RW_SEEK_SET);
        buildLightmaps(file.file, lightmapCount);

        return true;
    }

    return false;
}

//private
void Q3BspSystem::buildLightmaps(SDL_RWops* file, std::uint32_t count)
{
    std::vector<std::uint8_t> buffer(128 * 128 * 3);

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
        //TODO flip vertically?

        auto& texture = m_lightmaps.emplace_back();
        texture.create(128, 128, cro::ImageFormat::RGB);
        texture.update(buffer.data());
        texture.setSmooth(true);

        /*cro::Image img;
        img.loadFromMemory(buffer.data(), 128, 128, cro::ImageFormat::RGB);
        img.write("img_" + std::to_string(i) + ".png");*/
    }

    //add a blank lightmap at the end of the array so faces with no
    //lightmap can be assigned this in the shader, as the shader still
    //requires the lightmap uniform be set.
    std::fill(buffer.begin(), buffer.end(), 255);
    auto& texture = m_lightmaps.emplace_back();
    texture.create(128, 128, cro::ImageFormat::RGB);
    texture.update(buffer.data());
}

void Q3BspSystem::createMesh(const std::vector<Q3::Vertex>& vertices, std::size_t submeshCount)
{
    //check we don't already have a buffer before creating a new one
    if (!m_mesh.vbo)
    {
        //create vertex data from vertices - Q3 is z-up so we're swapping coords here
        //glm::vec3 dim = glm::vec3(2.0f);
        //std::vector<float> vertexData =
        //{
        //    //front
        //    -dim.x, dim.y, dim.z,   0.f,0.f,1.f,        0.f, 1.f,
        //    -dim.x, -dim.y, dim.z,  0.f,0.f,1.f,        0.f, 0.f,
        //    dim.x, dim.y, dim.z,    0.f,0.f,1.f,        1.f, 1.f,
        //    dim.x, -dim.y, dim.z,   0.f,0.f,1.f,        1.f, 0.f,
        //    //back
        //    dim.x, dim.y, -dim.z,   0.f,0.f,-1.f,        0.f, 1.f,
        //    dim.x, -dim.y, -dim.z,  0.f,0.f,-1.f,        0.f, 0.f,
        //    -dim.x, dim.y, -dim.z,    0.f,0.f,-1.f,        1.f, 1.f,
        //    -dim.x, -dim.y, -dim.z,   0.f,0.f,-1.f,        1.f, 0.f,
        //    //left
        //    -dim.x, dim.y, -dim.z,   -1.f,0.f,0.f,        0.f, 1.f,
        //    -dim.x, -dim.y, -dim.z,  -1.f,0.f,0.f,        0.f, 0.f,
        //    -dim.x, dim.y, dim.z,    -1.f,0.f,0.f,        1.f, 1.f,
        //    -dim.x, -dim.y, dim.z,   -1.f,0.f,0.f,        1.f, 0.f,
        //    //right
        //    dim.x, dim.y, dim.z,   1.f,0.f,0.f,        0.f, 1.f,
        //    dim.x, -dim.y, dim.z,  1.f,0.f,0.f,        0.f, 0.f,
        //    dim.x, dim.y, -dim.z,    1.f,0.f,0.f,        1.f, 1.f,
        //    dim.x, -dim.y, -dim.z,   1.f,0.f,0.f,        1.f, 0.f,
        //    //top
        //    -dim.x, dim.y, -dim.z,   0.f,1.f,0.f,        0.f, 1.f,
        //    -dim.x, dim.y, dim.z,  0.f,1.f,0.f,        0.f, 0.f,
        //    dim.x, dim.y, -dim.z,    0.f,1.f,0.f,        1.f, 1.f,
        //    dim.x, dim.y, dim.z,   0.f,1.f,0.f,        1.f, 0.f,
        //    //bottom
        //    -dim.x, -dim.y, dim.z,   0.f,-1.f,0.f,        0.f, 1.f,
        //    -dim.x, -dim.y, -dim.z,  0.f,-1.f,0.f,        0.f, 0.f,
        //    dim.x, -dim.y, dim.z,    0.f,-1.f,0.f,        1.f, 1.f,
        //    dim.x, -dim.y, -dim.z,   0.f,-1.f,0.f,        1.f, 0.f
        //};
        std::vector<float> vertexData;
        for (const auto& vertex : vertices)
        {
            vertexData.push_back(vertex.position.x);
            vertexData.push_back(vertex.position.z);
            vertexData.push_back(-vertex.position.y);

            vertexData.push_back(vertex.normal.x);
            vertexData.push_back(vertex.normal.z);
            vertexData.push_back(-vertex.normal.y);

            vertexData.push_back(vertex.uv1.x);
            vertexData.push_back(vertex.uv1.y);
        }

        //create new buffers
        CRO_ASSERT(m_submeshes.empty(), "ibos not empty!");

        m_mesh.attributes[cro::Mesh::Position] = 3;
        m_mesh.attributes[cro::Mesh::Normal] = 3;
        m_mesh.attributes[cro::Mesh::UV0] = 2;

        m_mesh.primitiveType = GL_TRIANGLES;
        for (auto a : m_mesh.attributes)
        {
            m_mesh.vertexSize += a;
        }
        m_mesh.vertexSize *= sizeof(float);
        m_mesh.vertexCount = vertexData.size() / (m_mesh.vertexSize / sizeof(float));

        //upload vert data
        glCheck(glGenBuffers(1, &m_mesh.vbo));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_mesh.vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, m_mesh.vertexSize * m_mesh.vertexCount, vertexData.data(), GL_DYNAMIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

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
                m_material.attribs[i][cro::Material::Data::Size] = static_cast<std::int32_t>(m_mesh.attributes[i]);

                //calc the pointer offset for each attrib
                m_material.attribs[i][cro::Material::Data::Offset] = static_cast<std::int32_t>(pointerOffset * sizeof(float));
            }
            pointerOffset += m_mesh.attributes[i]; //count the offset regardless as the mesh may have more attributes than material
        }

        //sort by size
        std::sort(std::begin(m_material.attribs), std::end(m_material.attribs),
            [](const std::array<std::int32_t, 3>& ip,
                const std::array<std::int32_t, 3>& op)
            {
                return ip[cro::Material::Data::Size] > op[cro::Material::Data::Size];
            });

        //count attribs with size > 0
        int i = 0;
        while (m_material.attribs[i++][cro::Material::Data::Size] != 0)
        {
            m_material.attribCount++;
        }


        //create empty IBOs - these will be updated at runtime
        m_submeshes.resize(submeshCount);

        for (auto& submesh : m_submeshes)
        {
            submesh.format = GL_UNSIGNED_INT;
            submesh.primitiveType = GL_TRIANGLES;

            glCheck(glGenBuffers(1, &submesh.ibo));

#ifdef PLATFORM_DESKTOP
            glCheck(glGenVertexArrays(1, &submesh.vao));

            glCheck(glBindVertexArray(submesh.vao));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_mesh.vbo));
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));

            const auto& attribs = m_material.attribs;
            for (auto j = 0u; j < m_material.attribCount; ++j)
            {
                glCheck(glEnableVertexAttribArray(attribs[j][cro::Material::Data::Index]));
                glCheck(glVertexAttribPointer(attribs[j][cro::Material::Data::Index], attribs[j][cro::Material::Data::Size],
                    GL_FLOAT, GL_FALSE, static_cast<GLsizei>(m_mesh.vertexSize),
                    reinterpret_cast<void*>(static_cast<intptr_t>(attribs[j][cro::Material::Data::Offset]))));
            }

            glCheck(glBindVertexArray(0));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
#endif
        }

        /*std::array<std::uint32_t, 36u> indexData =
        { {
                0, 1, 2, 1, 3, 2,
                4, 5, 6, 5, 7, 6,
                8, 9, 10, 9, 11, 10,
                12, 13, 14, 13, 15, 14,
                16, 17, 18, 17, 19 ,18,
                20, 21, 22, 21, 23, 22
            } };*/

        //test the index data
        std::vector<std::uint32_t> indexData;
        for (const auto& face : m_faces)
        {
            if (face.type == 3)
            {
                //TODO mark this correction in the header file - online doc is wrong.
                for (auto k = 0; k < face.meshVertCount; ++k)
                {
                    indexData.push_back(face.firstPolyIndex + m_indices[face.firstMeshIndex + k]);
                }
            }
        }

        m_activeSubmeshCount = 1;
        m_submeshes[0].indexCount = indexData.size();
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_submeshes[0].ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_submeshes[0].indexCount * sizeof(std::uint32_t), indexData.data(), GL_DYNAMIC_DRAW));
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    }
}