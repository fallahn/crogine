/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include <algorithm>

using namespace cro;

Model::Model()
    : m_visible     (true),
    m_hidden        (false),
    m_renderFlags   (std::numeric_limits<std::uint64_t>::max()),
    m_skeleton      (nullptr),
    m_jointCount    (0)
{}

Model::Model(Mesh::Data data, Material::Data material)
    : m_visible     (true),
    m_hidden        (false),
    m_renderFlags   (std::numeric_limits<std::uint64_t>::max()),
    m_meshData      (data),
    m_skeleton      (nullptr),
    m_jointCount    (0)
{
    //sets all materials to given default
    bindMaterial(material);
    for (auto& mat : m_materials)
    {
        mat = material;
    }
#ifdef PLATFORM_DESKTOP
    for (auto i = 0u; i < m_meshData.submeshCount; ++i)
    {
        updateVAO(i);
    }
#endif //DESKTOP
}

void Model::setMaterial(std::size_t idx, Material::Data data)
{
    CRO_ASSERT(idx < m_materials.size(), "Index out of range");
    CRO_ASSERT(m_meshData.vbo != 0, "Can't set a material until mesh has been added");
    bindMaterial(data);
    m_materials[idx] = data;

#ifdef PLATFORM_DESKTOP
    updateVAO(idx);
#endif //DESKTOP
}

void Model::setSkeleton(glm::mat4* frame, std::size_t jointCount)
{
    m_skeleton = frame;
    m_jointCount = jointCount;
}

void Model::setShadowMaterial(std::size_t idx, Material::Data material)
{
    CRO_ASSERT(idx < m_shadowMaterials.size(), "Index out of range");
    bindMaterial(material);
    m_shadowMaterials[idx] = material;
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
            material.attribs[i][Material::Data::Size] = static_cast<int32>(m_meshData.attributes[i]);

            //calc the pointer offset for each attrib
            material.attribs[i][Material::Data::Offset] = static_cast<int32>(pointerOffset * sizeof(float));
        }
        pointerOffset += m_meshData.attributes[i]; //count the offset regardless as the mesh may have more attributes than material
    }

    //sort by size
    std::sort(std::begin(material.attribs), std::end(material.attribs),
        [](const std::array<int32, 3>& ip,
            const std::array<int32, 3>& op)
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
void Model::updateVAO(std::size_t idx)
{
    auto& submesh = m_meshData.indexData[idx];

    //I guess we have to remove any old binding
    //if there's an existing material
    if (submesh.vao != 0)
    {
        glCheck(glDeleteVertexArrays(1, &submesh.vao));
        submesh.vao = 0;
    }

    glCheck(glGenVertexArrays(1, &submesh.vao));

    glCheck(glBindVertexArray(submesh.vao));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_meshData.vbo));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));

    const auto& attribs = m_materials[idx].attribs;
    for (auto j = 0u; j < m_materials[idx].attribCount; ++j)
    {
        glCheck(glEnableVertexAttribArray(attribs[j][Material::Data::Index]));
        glCheck(glVertexAttribPointer(attribs[j][Material::Data::Index], attribs[j][Material::Data::Size],
            GL_FLOAT, GL_FALSE, static_cast<GLsizei>(m_meshData.vertexSize),
            reinterpret_cast<void*>(static_cast<intptr_t>(attribs[j][Material::Data::Offset]))));
    }
    //glCheck(glEnableVertexAttribArray(0));

    glCheck(glBindVertexArray(0));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}
#endif //DESKTOP
