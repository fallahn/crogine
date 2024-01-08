/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include "../../detail/GLCheck.hpp"
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/graphics/Texture.hpp>

#include <limits>

using namespace cro;

Drawable2D::Drawable2D()
    : m_shader              (nullptr),
    m_customShader          (false),
    m_applyDefaultShader    (true),
    m_autoCrop              (true),
    m_textureUniform        (-1),
    m_worldUniform          (-1),
    m_viewProjectionUniform (-1),
    m_facing                (GL_CCW),
    m_blendMode             (Material::BlendMode::Alpha),
    m_primitiveType         (GL_TRIANGLE_STRIP),
    m_vbo                   (0),
    m_vao                   (0),
    m_updateBufferData      (false),
    m_renderFlags           (DefaultRenderFlag),
    m_croppingArea          (std::numeric_limits<float>::lowest() / 2.f, std::numeric_limits<float>::lowest() / 2.f,
                                std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
    m_cropped               (false),
    m_wasCulledLastFrame    (true),
    m_sortCriteria          (0)
{

}

//public
bool Drawable2D::setTexture(const Texture* texture)
{
    if (texture != m_textureInfo.texture ||
        (texture && (texture->getGLHandle() != m_textureInfo.textureID.textureID)))
    {
        m_textureInfo.texture = texture;

        if (texture)
        {
            m_textureInfo.textureID = texture->getGLHandle();
            m_textureInfo.size = texture->getSize();
        }
        else
        {
            m_textureInfo.textureID = {};
            m_textureInfo.size = { 0u, 0u };
        }

        m_applyDefaultShader = !m_customShader;

        applyShader();
        return true;
    }
    return false;
}

void Drawable2D::setTexture(TextureID textureID, glm::uvec2 size)
{
    //it might be that we just resized the texture
    m_textureInfo.size = size;

    if (textureID.textureID != m_textureInfo.textureID.textureID)
    {
        m_textureInfo.texture = nullptr;
        m_textureInfo.textureID = textureID;

        m_applyDefaultShader = !m_customShader;

        applyShader();
    }
}

void Drawable2D::setShader(Shader* shader)
{
    if (shader == m_shader)
    {
        return;
    }

    m_shader = shader;
    m_customShader = (shader != nullptr);
    m_applyDefaultShader = !m_customShader;

    m_textureIDBindings.clear();
    m_floatBindings.clear();
    m_vec2Bindings.clear();
    m_vec3Bindings.clear();
    m_vec4Bindings.clear();
    m_boolBindings.clear();
    m_matBindings.clear();

    m_vertexAttributes.clear();

    applyShader();
}

void Drawable2D::setBlendMode(Material::BlendMode mode)
{
    m_blendMode = mode;
}

void Drawable2D::setCroppingArea(FloatRect area)
{
    m_croppingArea = area;
}

void Drawable2D::setVertexData(const std::vector<Vertex2D>& data)
{
    m_vertices = data;
    updateLocalBounds();
}

void Drawable2D::setPrimitiveType(std::uint32_t primitiveType)
{
    m_primitiveType = primitiveType;
}

const Texture* Drawable2D::getTexture() const
{
    return m_textureInfo.texture;
}

Shader* Drawable2D::getShader()
{
    return m_shader;
}

const Shader* Drawable2D::getShader() const
{
    return m_shader;
}

Material::BlendMode Drawable2D::getBlendMode() const
{
    return m_blendMode;
}

std::vector<Vertex2D>& Drawable2D::getVertexData()
{
    //this assumes as we're non-const that we'll
    //want to update some of the vertex data. It
    //means we can change the colour/texcoords
    //without having to recalc (or manually call)
    //updateLocalBounds() repeatedly
    m_updateBufferData = !m_vertices.empty();
    return m_vertices;
}

const std::vector<Vertex2D>& Drawable2D::getVertexData() const
{
    return m_vertices;
}

std::uint32_t Drawable2D::getPrimitiveType() const
{
    return m_primitiveType;
}

FloatRect Drawable2D::getLocalBounds() const
{
    return m_localBounds;
}

void Drawable2D::updateLocalBounds()
{
    if (m_vertices.empty()) return;

    auto xExtremes = std::minmax_element(m_vertices.begin(), m_vertices.end(),
        [](const Vertex2D& lhs, const Vertex2D& rhs)
        {
            return lhs.position.x < rhs.position.x;
        });

    auto yExtremes = std::minmax_element(m_vertices.begin(), m_vertices.end(),
        [](const Vertex2D& lhs, const Vertex2D& rhs)
        {
            return lhs.position.y < rhs.position.y;
        });

    m_localBounds.left = xExtremes.first->position.x;
    m_localBounds.bottom = yExtremes.first->position.y;
    m_localBounds.width = xExtremes.second->position.x - m_localBounds.left;
    m_localBounds.height = yExtremes.second->position.y - m_localBounds.bottom;

    //if one or other is empty (say we're drawing a perfectly
    //straight line) add a small amount so we don't get culled
    if (m_localBounds.width + m_localBounds.height != 0)
    {
        m_localBounds.width += 0.001f;
        m_localBounds.height += 0.001f;
    }

    m_updateBufferData = true; //tells the system we have new data to upload
}

void Drawable2D::updateLocalBounds(FloatRect localBounds)
{
    m_localBounds = localBounds;
    m_updateBufferData = true;
}

void Drawable2D::bindUniform(const std::string& name, TextureID texture)
{
    bindUniform(name, texture.textureID, m_textureIDBindings);
}

void Drawable2D::bindUniform(const std::string& name, float value)
{
    bindUniform(name, value, m_floatBindings);
}

void Drawable2D::bindUniform(const std::string& name, glm::vec2 value)
{
    bindUniform(name, value, m_vec2Bindings);
}

void Drawable2D::bindUniform(const std::string& name, glm::vec3 value)
{
    bindUniform(name, value, m_vec3Bindings);
}

void Drawable2D::bindUniform(const std::string& name, glm::vec4 value)
{
    bindUniform(name, value, m_vec4Bindings);
}

void Drawable2D::bindUniform(const std::string& name, bool value)
{
    bindUniform(name, value ? 1 : 0, m_boolBindings);
}

void Drawable2D::bindUniform(const std::string& name, Colour value)
{
    bindUniform(name, glm::vec4(value.getRed(), value.getGreen(), value.getBlue(), value.getAlpha()), m_vec4Bindings);
}

void Drawable2D::bindUniform(const std::string& name, const float* value)
{
    bindUniform(name, value, m_matBindings);
}

void Drawable2D::setFacing(Facing direction)
{
    m_facing = direction == Facing::Front ? GL_CCW : GL_CW;
}

Drawable2D::Facing Drawable2D::getFacing() const
{
    return m_facing == GL_CCW ? Facing::Front : Facing::Back;
}

//private
void Drawable2D::applyShader()
{
    if (m_shader)
    {
        //grab uniform locations
        if (m_textureInfo.textureID.textureID)
        {
            if (m_shader->getUniformMap().count("u_texture") != 0)
            {
                m_textureUniform = m_shader->getUniformMap().at("u_texture");
            }
            else
            {
                m_textureUniform = -1;
                Logger::log("Missing texture uniform in Drawable2D shader", Logger::Type::Warning);
                return;
            }
        }
        else
        {
            m_textureUniform = -1;
        }

        if (m_shader->getUniformMap().count("u_worldMatrix") != 0)
        {
            m_worldUniform = m_shader->getUniformMap().at("u_worldMatrix");
        }
        else
        {
            m_worldUniform = -1;
            Logger::log("Missing World Matrix uniform in Drawable2D shader", Logger::Type::Error);
            assert(true);
            setShader(nullptr);
            return;
        }

        if (m_shader->getUniformMap().count("u_viewProjectionMatrix") != 0)
        {
            m_viewProjectionUniform = m_shader->getUniformMap().at("u_viewProjectionMatrix");
        }
        else
        {
            m_viewProjectionUniform = -1;
            Logger::log("Missing View Projection Matrix uniform in Drawable2D shader", Logger::Type::Error);
            assert(true);
            setShader(nullptr);
            return;
        }

        //store available attribs so we can also use this on mobile
        m_vertexAttributes.clear();

        auto attribs = m_shader->getAttribMap();
        if (attribs[Mesh::Attribute::Position] == -1)
        {
            Logger::log("Position attribute missing from Drawable2D shader", Logger::Type::Error);
            setShader(nullptr);
            return;
        }
        else
        {
            //these are fixed properties of Vertex2D - if the struct is
            //changed then this is going to break...
            AttribData& data = m_vertexAttributes.emplace_back();
            data.id = attribs[Mesh::Attribute::Position];
            data.size = 2;
            data.offset = 0;
        }

        if (attribs[Mesh::Attribute::Colour] == -1)
        {
            Logger::log("Colour attribute missing from Drawable2D shader", Logger::Type::Error);
            setShader(nullptr);
            return;
        }
        else
        {
            AttribData& data = m_vertexAttributes.emplace_back();
            data.id = attribs[Mesh::Attribute::Colour];
            data.size = 4;
            data.offset = 4 * sizeof(float); //last after 2 position and 2 UV
        }

        if (m_textureInfo.textureID.textureID)
        {
            if (attribs[Mesh::Attribute::UV0] == -1)
            {
                Logger::log("UV0 attribute missing from Drawable2D shader", Logger::Type::Error);
                return;
            }
            else
            {
                AttribData& data = m_vertexAttributes.emplace_back();
                data.id = attribs[Mesh::Attribute::UV0];
                data.size = 2;
                data.offset = 2 * sizeof(float);
            }
        }


#ifdef PLATFORM_DESKTOP
        //only update the vao on desktop
        if (m_vao != 0)
        {
            glCheck(glDeleteVertexArrays(1, &m_vao));
            m_vao = 0;
        }

        glCheck(glGenVertexArrays(1, &m_vao));

        //this might be done before the system has
        //a chance to create it, ie when setting a custom shader immediately
        //upon component creation.
        if (m_vbo == 0)
        {
            glCheck(glGenBuffers(1, &m_vbo));
        }

        glCheck(glBindVertexArray(m_vao));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

        for (const auto& [id, size, offset] : m_vertexAttributes)
        {
            glCheck(glEnableVertexAttribArray(id));
            glCheck(glVertexAttribPointer(id, size,
                                            GL_FLOAT, GL_FALSE, static_cast<GLsizei>(Vertex2D::Size),
                                            reinterpret_cast<void*>(static_cast<intptr_t>(offset))));
        }

        glCheck(glBindVertexArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

#endif //PLATFORM
    }
}