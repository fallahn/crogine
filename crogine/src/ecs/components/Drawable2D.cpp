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

#include <crogine/ecs/components/Drawable2D.hpp>
#include "../../detail/glad.hpp"
#include "../../detail/GLCheck.hpp"

#include <crogine/graphics/Shader.hpp>

#include <limits>

using namespace cro;

Drawable2D::Drawable2D()
    : m_texture         (nullptr),
    m_shader            (nullptr),
    m_customShader      (false),
    m_applyDefaultShader(true),
    m_textureUniform    (-1),
    m_worldViewUniform  (-1),
    m_projectionUniform (-1),
    m_blendMode         (Material::BlendMode::Alpha),
    m_primitiveType     (GL_TRIANGLE_STRIP),
    m_vbo               (0),
    m_vao               (0),
    m_updateBufferData  (false),
    m_lastSortValue     (0.f)
{

}

//public
void Drawable2D::setTexture(const Texture* texture)
{
    m_texture = texture;
    m_applyDefaultShader = !m_customShader;
}

void Drawable2D::setShader(Shader* shader)
{
    m_shader = shader;
    m_customShader = (shader != nullptr);
    m_applyDefaultShader = !m_customShader;

    applyShader();
}

void Drawable2D::setBlendMode(Material::BlendMode mode)
{
    m_blendMode = mode;
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
    return m_texture;
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

    m_updateBufferData = true; //tells the system we have new data to upload
}
//private
void Drawable2D::applyShader()
{
    if (m_shader)
    {
        //grab uniform locations
        if (m_texture != nullptr)
        {
            if (m_shader->getUniformMap().count("u_texture") != 0)
            {
                m_textureUniform = m_shader->getUniformMap().at("u_texture");
            }
            else
            {
                m_textureUniform = -1;
                Logger::log("Missing texture uniform in Drawable2D shader", Logger::Type::Error);

                setShader(nullptr);
                return;
            }
        }
        else
        {
            m_textureUniform = -1;
        }

        if (m_shader->getUniformMap().count("u_worldViewMatrix") != 0)
        {
            m_worldViewUniform = m_shader->getUniformMap().at("u_worldViewMatrix");
        }
        else
        {
            m_worldViewUniform = -1;
            Logger::log("Missing World View Matrix uniform in Drawable2D shader", Logger::Type::Error);

            setShader(nullptr);
            return;
        }

        if (m_shader->getUniformMap().count("u_projectionMatrix") != 0)
        {
            m_projectionUniform = m_shader->getUniformMap().at("u_projectionMatrix");
        }
        else
        {
            m_projectionUniform = -1;
            Logger::log("Missing Projection Matrix uniform in Drawable2D shader", Logger::Type::Error);

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

        if (m_texture)
        {
            if (attribs[Mesh::Attribute::UV0] == -1)
            {
                Logger::log("UV0 attribute missing from Drawable2D shader", Logger::Type::Error);
                setShader(nullptr);
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