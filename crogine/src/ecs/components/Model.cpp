/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/ecs/components/Model.hpp>
#include <crogine/detail/Assert.hpp>
#include "../../detail/glad.hpp"
#include "../../detail/GLCheck.hpp"

#include <crogine/detail/glm/gtc/matrix_inverse.hpp>

#include <algorithm>

using namespace cro;

Model::Model()
    : m_visible     (true),
    m_hidden        (false),
    m_renderFlags   (std::numeric_limits<std::uint64_t>::max()),
    m_skeleton      (nullptr),
    m_jointCount    (0)
{
    for (auto& pair : m_vaos)
    {
        std::fill(pair.begin(), pair.end(), 0);
    }
}

Model::Model(Mesh::Data data, Material::Data material)
    : m_visible     (true),
    m_hidden        (false),
    m_renderFlags   (std::numeric_limits<std::uint64_t>::max()),
    m_meshData      (data),
    m_skeleton      (nullptr),
    m_jointCount    (0)
{
    for (auto& pair : m_vaos)
    {
        std::fill(pair.begin(), pair.end(), 0);
    }

    //sets all materials to given default
    bindMaterial(material);
    for (auto& mat : m_materials[Mesh::IndexData::Final])
    {
        mat = material;
    }
#ifdef PLATFORM_DESKTOP

    for (auto i = 0u; i < m_meshData.submeshCount; ++i)
    {
        updateVAO(i, Mesh::IndexData::Final);
    }
#endif //DESKTOP
}

#ifdef PLATFORM_DESKTOP
Model::~Model()
{
    for (auto& [p1, p2] : m_vaos)
    {
        if (p1)
        {
            glCheck(glDeleteVertexArrays(1, &p1));
        }

        if (p2)
        {
            glCheck(glDeleteVertexArrays(1, &p2));
        }
    }

    if (m_instanceBuffers.instanceCount != 0)
    {
        glCheck(glDeleteBuffers(1, &m_instanceBuffers.normalBuffer));
        glCheck(glDeleteBuffers(1, &m_instanceBuffers.transformBuffer));
        m_instanceBuffers.instanceCount = 0;
    }
}

Model::Model(Model&& other) noexcept
    : Model()
{
    //we can swap because we initialised to nothing
    std::swap(m_visible, other.m_visible);
    std::swap(m_hidden, other.m_hidden);
    std::swap(m_renderFlags, other.m_renderFlags);

    std::swap(m_meshData, other.m_meshData);
    std::swap(m_materials, other.m_materials);

    std::swap(m_vaos, other.m_vaos);

    std::swap(m_skeleton, other.m_skeleton);
    std::swap(m_jointCount, other.m_jointCount);

    std::swap(m_instanceBuffers, other.m_instanceBuffers);

    //check the other property to see which draw func we need
    if (m_instanceBuffers.instanceCount != 0)
    {
        draw = DrawInstanced(*this);
    }
    else
    {
        draw = DrawSingle(*this);
    }
    other.draw = {};
}

Model& Model::operator=(Model&& other) noexcept
{
    if (&other != this)
    {
        m_visible = other.m_visible;
        m_hidden = other.m_hidden;
        m_renderFlags = other.m_renderFlags;

        m_meshData = other.m_meshData;
        other.m_meshData = {};
        m_materials = other.m_materials;
        other.m_materials = {};

        for (auto& [p1, p2] : m_vaos)
        {
            if (p1)
            {
                glCheck(glDeleteVertexArrays(1, &p1));
            }

            if (p2)
            {
                glCheck(glDeleteVertexArrays(1, &p2));
            }
        }

        m_vaos = other.m_vaos;
        for (auto& pair : other.m_vaos)
        {
            std::fill(pair.begin(), pair.end(), 0);
        }

        m_skeleton = other.m_skeleton;
        other.m_skeleton = nullptr;

        m_jointCount = other.m_jointCount;
        other.m_jointCount = 0;

        if (m_instanceBuffers.instanceCount != 0)
        {
            glCheck(glDeleteBuffers(1, &m_instanceBuffers.normalBuffer));
            glCheck(glDeleteBuffers(1, &m_instanceBuffers.transformBuffer));
            m_instanceBuffers.instanceCount = 0;
        }
        m_instanceBuffers = other.m_instanceBuffers;
        other.m_instanceBuffers = {};

        if (m_instanceBuffers.instanceCount)
        {
            draw = DrawInstanced(*this);
        }
        else
        {
            draw = DrawSingle(*this);
        }
        other.draw = {};
    }

    return* this;
}
#endif

//public
void Model::setMaterial(std::size_t idx, Material::Data data)
{
    CRO_ASSERT(idx < m_materials[Mesh::IndexData::Final].size(), "Index out of range");
    CRO_ASSERT(m_meshData.vbo != 0, "Can't set a material until mesh has been added");
    bindMaterial(data);
    m_materials[Mesh::IndexData::Final][idx] = data;

#ifdef PLATFORM_DESKTOP
    updateVAO(idx, Mesh::IndexData::Final);
#endif //DESKTOP
}

void Model::setSkeleton(glm::mat4* frame, std::size_t jointCount)
{
    m_skeleton = frame;
    m_jointCount = jointCount;
}

void Model::setShadowMaterial(std::size_t idx, Material::Data material)
{
    CRO_ASSERT(idx < m_materials[Mesh::IndexData::Shadow].size(), "Index out of range");
    bindMaterial(material);
    m_materials[Mesh::IndexData::Shadow][idx] = material;

#ifdef PLATFORM_DESKTOP
    updateVAO(idx, Mesh::IndexData::Shadow);
#endif //DESKTOP
}

const Material::Data& Model::getMaterialData(Mesh::IndexData::Pass pass, std::size_t submesh) const
{
    CRO_ASSERT(submesh < m_materials[pass].size(), "submesh index out of range");
    return m_materials[pass][submesh];
}

void Model::setInstanceTransforms(const std::vector<glm::mat4>& transforms)
{
    //TODO check we have a material which supports this...

#ifdef PLATFORM_DESKTOP
    //create VBOs if needed
    if (m_instanceBuffers.instanceCount == 0)
    {
        glCheck(glGenBuffers(1, &m_instanceBuffers.normalBuffer));
        glCheck(glGenBuffers(1, &m_instanceBuffers.transformBuffer));
    }
    m_instanceBuffers.instanceCount = static_cast<std::uint32_t>(transforms.size());

    //upload transform data
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffers.transformBuffer));
    glCheck(glBufferData(GL_ARRAY_BUFFER, m_instanceBuffers.instanceCount * sizeof(glm::mat4), transforms.data(), GL_STATIC_DRAW));

    //calc normal matrices and upload
    std::vector<glm::mat3> normalMatrices(transforms.size());
    for (auto i = 0u; i < normalMatrices.size(); ++i)
    {
        normalMatrices[i] = glm::inverseTranspose(transforms[i]);
    }
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffers.normalBuffer));
    glCheck(glBufferData(GL_ARRAY_BUFFER, m_instanceBuffers.instanceCount * sizeof(glm::mat3), normalMatrices.data(), GL_STATIC_DRAW));

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    //update VAOs if material is set
    for (auto i = 0u; i < m_meshData.submeshCount; ++i)
    {
        updateVAO(i, Mesh::IndexData::Final);

        //TODO we need to update shadow materials if they exist (might not match the regular material count)
        //TODO check if we need to modify bindMaterial with the new transform attribs. This might only be relevant
        //to non-vao rendering/interleaved vertex attribs
    }
#endif
}

//private
void Model::bindMaterial(Material::Data& material)
{
    //map attributes to material
    std::size_t pointerOffset = 0;
    for (auto i = 0u; i < Mesh::Attribute::Total; ++i)
    {
        if (material.attribs[i][Material::Data::Index] > -1)
        {
            //attrib exists in shader so map its size
            material.attribs[i][Material::Data::Size] = static_cast<std::int32_t>(m_meshData.attributes[i]);

            //calc the pointer offset for each attrib
            material.attribs[i][Material::Data::Offset] = static_cast<std::int32_t>(pointerOffset * sizeof(float));
        }
        pointerOffset += m_meshData.attributes[i]; //count the offset regardless as the mesh may have more attributes than material
    }

    //sort by size
    std::sort(std::begin(material.attribs), std::end(material.attribs),
        [](const std::array<std::int32_t, 3>& ip,
            const std::array<std::int32_t, 3>& op)
        {
            return ip[Material::Data::Size] > op[Material::Data::Size];
        });

    //count attribs with size > 0
    int i = 0;
    while (material.attribs[i++][Material::Data::Size] != 0)
    {
        material.attribCount++;
    }
}
    //if we're on desktop core opengl profile requires VAOs
#ifdef PLATFORM_DESKTOP
void Model::updateVAO(std::size_t idx, std::int32_t passIndex)
{
    auto& submesh = m_meshData.indexData[idx];
    auto& vaoPair = m_vaos[idx];

    //I guess we have to remove any old binding
    //if there's an existing material
    if (vaoPair[passIndex] != 0)
    {
        glCheck(glDeleteVertexArrays(1, &vaoPair[passIndex]));
        vaoPair[passIndex] = 0;
    }

    glCheck(glGenVertexArrays(1, &vaoPair[passIndex]));

    glCheck(glBindVertexArray(vaoPair[passIndex]));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_meshData.vbo));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));

    const auto& attribs = m_materials[passIndex][idx].attribs;
    for (auto j = 0u; j < m_materials[passIndex][idx].attribCount; ++j)
    {
        glCheck(glEnableVertexAttribArray(attribs[j][Material::Data::Index]));
        glCheck(glVertexAttribPointer(attribs[j][Material::Data::Index], attribs[j][Material::Data::Size],
            GL_FLOAT, GL_FALSE, static_cast<GLsizei>(m_meshData.vertexSize),
            reinterpret_cast<void*>(static_cast<intptr_t>(attribs[j][Material::Data::Offset]))));
    }
    
    //bind instance buffers if they exist
    if (m_instanceBuffers.instanceCount != 0)
    {
        //TODO we need to query the material for attrib position
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffers.normalBuffer));
        glCheck(glEnableVertexAttribArray(?));
        glCheck(glVertexAttribPointer(?, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(glm::vec3), reinterpret_cast<void*>(static_cast<intptr_t>(0)));

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffers.transformBuffer));
        glCheck(glEnableVertexAttribArray(?));
        glCheck(glVertexAttribPointer(?, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4), reinterpret_cast<void*>(static_cast<intptr_t>(0))));

        
        draw = DrawInstanced(*this);
    }
    else
    {
        draw = DrawSingle(*this);
    }

    glCheck(glBindVertexArray(0));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

//draw functions
void Model::DrawSingle::operator()(std::int32_t matID, std::int32_t pass) const
{
    const auto& indexData = m_model.m_meshData.indexData[matID];
    glCheck(glBindVertexArray(m_model.m_vaos[matID][pass]));
    glCheck(glDrawElements(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), NULL));
}

void Model::DrawInstanced::operator()(std::int32_t matID, std::int32_t pass) const
{
    const auto& indexData = m_model.m_meshData.indexData[matID];
    glCheck(glBindVertexArray(m_model.m_vaos[matID][pass]));
    glCheck(glDrawElementsInstanced(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), NULL, m_model.m_instanceBuffers.instanceCount));
}

#endif //DESKTOP