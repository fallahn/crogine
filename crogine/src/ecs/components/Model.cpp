/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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
#include "../../detail/GLCheck.hpp"

#include <crogine/detail/glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cstring>

using namespace cro;

namespace
{
    //this asserts the min point is always 0 and max is always 1
    //to prevent spheres being created with incorrect centre points
    void assertAABB(Box& aabb)
    {
        float x = std::min(aabb[0].x, aabb[1].x);
        float y = std::min(aabb[0].y, aabb[1].y);
        float z = std::min(aabb[0].z, aabb[1].z);

        glm::vec3 minimum(x,y,z);

        x = std::max(aabb[0].x, aabb[1].x);
        y = std::max(aabb[0].y, aabb[1].y);
        z = std::max(aabb[0].z, aabb[1].z);
        glm::vec3 maximum(x,y,z);

        aabb[0] = minimum;
        aabb[1] = maximum;
    }
}

Model::Model()
    : m_hidden      (false),
    m_renderFlags   (std::numeric_limits<std::uint64_t>::max()),
    m_facing        (GL_CCW),
    m_skeleton      (nullptr),
    m_jointCount    (0)
{
    for (auto& pair : m_vaos)
    {
        std::fill(pair.begin(), pair.end(), 0);
    }
}

Model::Model(Mesh::Data data, Material::Data material)
    : m_hidden      (false),
    m_renderFlags   (std::numeric_limits<std::uint64_t>::max()),
    m_facing        (GL_CCW),
    m_boundingSphere(data.boundingSphere),
    m_boundingBox   (data.boundingBox),
    m_meshData      (data),
    m_skeleton      (nullptr),
    m_jointCount    (0)
{
    assertAABB(m_meshData.boundingBox);
    assertAABB(m_boundingBox);
    m_boundingSphere = m_boundingBox;

    for (auto& pair : m_vaos)
    {
        std::fill(pair.begin(), pair.end(), 0);
    }

    //sets all materials to given default
    bindMaterial(material);
    updateBounds();

    for (auto i = 0u; i < m_materials[Mesh::IndexData::Final].size(); ++i)
    {
        m_materials[Mesh::IndexData::Final][i] = material;
        initMaterialAnimation(i);
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
    std::swap(m_hidden, other.m_hidden);
    std::swap(m_renderFlags, other.m_renderFlags);
    std::swap(m_facing, other.m_facing);
    std::swap(m_boundingSphere, other.m_boundingSphere);

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
        m_hidden = other.m_hidden;
        m_renderFlags = other.m_renderFlags;
        m_facing = other.m_facing;
        other.m_facing = GL_CCW;
        m_boundingSphere = other.m_boundingSphere;
        other.m_boundingSphere = Sphere();

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
    
    if (m_meshData.vbo)
    {
        //remove any existing animations
        m_animations.erase(std::remove_if(m_animations.begin(), m_animations.end(),
            [idx](const std::pair<std::size_t, Material::Property*>& a)
            {
                return a.first == idx;
            }), m_animations.end());

        //the order in which this happens is important!
        bindMaterial(data);
        m_materials[Mesh::IndexData::Final][idx] = data;
        initMaterialAnimation(idx);
#ifdef PLATFORM_DESKTOP
        updateVAO(idx, Mesh::IndexData::Final);
#endif //DESKTOP
    }
    else
    {
        //this is fine if we're planning on using Sprite3D with this component
        //else the material will be ignored when the mesh is actually added.
        LOG("Material applied to model with no mesh. Is this intentional?", Logger::Type::Warning);
        m_materials[Mesh::IndexData::Final][idx] = data;
    }
}

void Model::setDepthTestEnabled(std::size_t idx, bool enabled)
{
    CRO_ASSERT(idx < m_materials.size(), "Out of Range");
    m_materials[Mesh::IndexData::Pass::Final][idx].enableDepthTest = enabled;
}

void Model::setDoubleSided(std::size_t idx, bool doubleSided)
{
    CRO_ASSERT(idx < m_materials.size(), "Out of Range");
    m_materials[Mesh::IndexData::Pass::Final][idx].doubleSided = doubleSided;
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

void Model::setFacing(Facing facing)
{
    m_facing = facing == Facing::Front ? GL_CCW : GL_CW;
}

Model::Facing Model::getFacing() const
{
    return m_facing == GL_CCW ? Facing::Front : Facing::Back;
}

const Material::Data& Model::getMaterialData(Mesh::IndexData::Pass pass, std::size_t submesh) const
{
    CRO_ASSERT(submesh < m_materials[pass].size(), "submesh index out of range");
    return m_materials[pass][submesh];
}

void Model::setInstanceTransforms(const std::vector<glm::mat4>& transforms)
{
#ifdef PLATFORM_DESKTOP
    if (transforms.empty())
    {
        LogW << "Attempt to set empty instance transform array on model" << std::endl;
        return;
    }

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

    auto bb = m_meshData.boundingBox;
    cro::Box newBB;

    //calc normal matrices and upload
    std::vector<glm::mat3> normalMatrices(transforms.size());
    for (auto i = 0u; i < normalMatrices.size(); ++i)
    {
        //while we're here we can update the AABB/sphere
        newBB = cro::Box::merge(newBB, transforms[i] * bb);

        normalMatrices[i] = glm::inverseTranspose(transforms[i]);
    }
    m_boundingBox = newBB;
    
    assertAABB(m_boundingBox);
    m_boundingSphere = m_boundingBox;

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffers.normalBuffer));
    glCheck(glBufferData(GL_ARRAY_BUFFER, m_instanceBuffers.instanceCount * sizeof(glm::mat3), normalMatrices.data(), GL_STATIC_DRAW));

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    //update VAOs if material is set
    for (auto i = 0u; i < m_meshData.submeshCount; ++i)
    {
        auto instanceAttrib = m_materials[Mesh::IndexData::Final][i].attribs[Shader::AttributeID::InstanceTransform][Material::Data::Index];
        
        if (instanceAttrib != -1)
        {
            updateVAO(i, Mesh::IndexData::Final);
        }

        instanceAttrib = m_materials[Mesh::IndexData::Shadow][i].attribs[Shader::AttributeID::InstanceTransform][Material::Data::Index];
        if (instanceAttrib != -1)
        {
            updateVAO(i, Mesh::IndexData::Shadow);
        }
    }
#endif
}

void Model::updateInstanceTransforms(const std::vector<const std::vector<glm::mat4>*>& transforms, const std::vector<const std::vector<glm::mat3>*>& normalMatrices)
{
    //as this is intended for speed we'll assert rather than conditional
    CRO_ASSERT(!transforms.empty() && transforms.size() == normalMatrices.size(), "Invalid transform data");
    CRO_ASSERT(m_instanceBuffers.normalBuffer && m_instanceBuffers.transformBuffer, "setInstanceTransforms() must be used at least once");
    //CRO_ASSERT(transforms.size() <= initialInstanceCount, "We're using sub-data so no resizing!");

    //TODO we could double buffer and swap this

    std::uint32_t offset = 0;
    std::uint32_t instanceCount = 0;

    //upload transform data
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffers.transformBuffer));
    for (const auto* v : transforms)
    {
        auto size = v->size() * sizeof(glm::mat4);
        glCheck(glBufferSubData(GL_ARRAY_BUFFER, offset, size, v->data()));

        offset += size;
        instanceCount += v->size();
    }

    offset = 0;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffers.normalBuffer));
    for (const auto& v : normalMatrices)
    {
        auto size = v->size() * sizeof(glm::mat3);
        glCheck(glBufferSubData(GL_ARRAY_BUFFER, offset, size, v->data()));

        offset += size;
    }

    //glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    //TODO assert instance count fits within the original buffer (although we've already errored by this point if it is)
    m_instanceBuffers.instanceCount = instanceCount;
}

//private
void Model::initMaterialAnimation(std::size_t index)
{
    auto& material = m_materials[Mesh::IndexData::Final][index];
    if (material.animation.active
        && material.properties.count("u_subrect") != 0)
    {
        m_animations.emplace_back(std::make_pair(index, &material.properties.at("u_subrect").second));
    }
}

void Model::updateMaterialAnimations(float dt)
{
    for (auto [index, prop] : m_animations)
    {
        auto& material = m_materials[Mesh::IndexData::Final][index];
        material.animation.currentTime += dt;
        if (material.animation.currentTime > material.animation.frameTime)
        {
            material.animation.currentTime -= material.animation.frameTime;
            material.animation.frame.y -= material.animation.frame.w;

            if (material.animation.frame.y < 0)
            {
                material.animation.frame.y = 1.f - material.animation.frame.w;
                material.animation.frame.x = std::fmod(material.animation.frame.x + material.animation.frame.z, 1.f);
            }

            std::memcpy(prop->vecValue, &material.animation.frame, sizeof(glm::vec4));
        }
    }
}

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
        else
        {
            //reset the values in case we're re-mapping an existing material
            //with a new shader
            material.attribs[i][Material::Data::Size] = 0;
            material.attribs[i][Material::Data::Offset] = 0;
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
    std::int32_t i = 0;
    material.attribCount = 0;
    while (material.attribs[i++][Material::Data::Size] != 0)
    {
        material.attribCount++;
    }
}

void Model::updateBounds()
{
    //we can't currently do this if we've got instancing enabled
    //as the instance AABB would require keeping a copy of the transform data...
#ifdef PLATFORM_DESKTOP
    if (m_instanceBuffers.instanceCount != 0)
    {
        return;
    }
#endif

    //unfortunately we need to keep a copy of this as it's the only
    //way of comparing against the mesh data to see if it was updated
    m_meshBox = m_meshData.boundingBox;
    assertAABB(m_meshBox);

    //and yet another copy which may be modified by instancing (and is what's actually returned as the model bounds)
    m_boundingBox = m_meshData.boundingBox;
    assertAABB(m_boundingBox);
    m_boundingSphere = m_boundingBox;
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
        auto attribIndex = attribs[Shader::AttributeID::InstanceNormal][Material::Data::Index];

        //attribs are labelled as mat3/4 in shader but are actually 3*vec3 and 4*vec4
        if (attribIndex != -1)
        {
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffers.normalBuffer));
            for (auto j = 0u; j < 3u; ++j)
            {
                glCheck(glEnableVertexAttribArray(attribIndex + j));
                glCheck(glVertexAttribPointer(attribIndex + j, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(glm::vec3), reinterpret_cast<void*>(static_cast<intptr_t>(j * sizeof(glm::vec3)))));
                glCheck(glVertexAttribDivisor(attribIndex + j, 1));
            }
        }

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffers.transformBuffer));
        for (auto j = 0u; j < 4u; ++j)
        {
            glCheck(glEnableVertexAttribArray(attribs[Shader::AttributeID::InstanceTransform][Material::Data::Index] + j));
            glCheck(glVertexAttribPointer(attribs[Shader::AttributeID::InstanceTransform][Material::Data::Index] + j, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4), reinterpret_cast<void*>(static_cast<intptr_t>(j * sizeof(glm::vec4)))));
            glCheck(glVertexAttribDivisor(attribs[Shader::AttributeID::InstanceTransform][Material::Data::Index] + j, 1));
        }
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