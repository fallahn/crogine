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

#include "../detail/GLCheck.hpp"

#include <crogine/core/App.hpp>

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/Texture.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <memory>

using namespace cro;

namespace
{
    std::int32_t activeCount = 0;
    std::unique_ptr<Shader> shader;

    glm::uvec2 screenSize = glm::uvec2(0);
    glm::mat4 projectionMatrix = glm::mat4(1.f);

    const std::string ShaderVertex = 
        R"(
        ATTRIBUTE vec2 a_position;
        ATTRIBUTE vec2 a_texCoord0;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_projectionMatrix;

        VARYING_OUT vec2 v_texCoord;

        void main()
        {
            gl_Position = u_projectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
            v_texCoord = a_texCoord0;
        })";

    const std::string ShaderFragment = 
        R"(
        uniform sampler2D u_texture;

        VARYING_IN vec2 v_texCoord;

        OUTPUT

        void main()
        {
            FRAG_OUT = TEXTURE(u_texture, v_texCoord);
        })";

    std::int32_t projectionUniform = -1;
    std::int32_t worldUniform = -1;
    std::int32_t textureUniform = -1;
}

SimpleQuad::SimpleQuad()
    : m_position    (0.f),
    m_rotation      (0.f),
    m_scale         (1.f),
    m_modelMatrix   (1.f),
    m_vbo           (0),
#ifdef PLATFORM_DESKTOP
    m_vao           (0),
#endif
    m_textureID     (-1),
    m_blendMode     (Material::BlendMode::Alpha)
{
    if (activeCount == 0)
    {
        //load shader
        shader = std::make_unique<Shader>();
        shader->loadFromString(ShaderVertex, ShaderFragment);

        auto uniforms = shader->getUniformMap();
        projectionUniform = uniforms.at("u_projectionMatrix");
        worldUniform = uniforms.at("u_worldMatrix");
        textureUniform = uniforms.at("u_texture");
    }
    activeCount++;

    //create buffer
    glCheck(glGenBuffers(1, &m_vbo));
#ifdef PLATFORM_DESKTOP
    glCheck(glGenVertexArrays(1, &m_vao));

    glCheck(glBindVertexArray(m_vao));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

    //pos
    glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0));
    glCheck(glEnableVertexAttribArray(0));

    //uv
    glCheck(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))));
    glCheck(glEnableVertexAttribArray(1));

    glCheck(glBindVertexArray(0));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif

    updateTransform();
}

SimpleQuad::SimpleQuad(const cro::Texture& texture)
    : SimpleQuad()
{
    setTexture(texture);
}

SimpleQuad::~SimpleQuad()
{
    if (m_vbo)
    {
        glCheck(glDeleteBuffers(1, &m_vbo));
    }
#ifdef PLATFORM_DESKTOP
    if (m_vao)
    {
        glCheck(glDeleteVertexArrays(1, &m_vao));
    }
#endif

    activeCount--;

    if (activeCount == 0)
    {
        shader.reset();
        projectionUniform = -1;
        worldUniform = -1;
        textureUniform = -1;
    }
}

//public
void SimpleQuad::setPosition(glm::vec2 position)
{
    m_position = position;
    updateTransform();
}

void SimpleQuad::setRotation(float rotation)
{
    m_rotation = cro::Util::Const::degToRad * rotation;
    updateTransform();
}

float SimpleQuad::getRotation() const
{
    return m_rotation * cro::Util::Const::radToDeg;
}

void SimpleQuad::setScale(glm::vec2 scale)
{
    m_scale = scale;
    updateTransform();
}

void SimpleQuad::setTexture(const cro::Texture& texture)
{
    if (texture.getGLHandle() > 0)
    {
        m_textureID = texture.getGLHandle();

        updateVertexData(texture.getSize());
    }
    else
    {
        LogE << "Invalid texture supplied to SimpleQuad" << std::endl;
    }
}

void SimpleQuad::draw() const
{
    if (m_textureID)
    {
        //check if screen size changed
        auto size = App::getWindow().getSize();
        if (size != screenSize)
        {
            projectionMatrix = glm::ortho(0.f, static_cast<float>(size.x), 0.f, static_cast<float>(size.y), -1.f, 1.f);
            screenSize = size;
        }

        //store and set viewport
        int oldVp[4];
        glCheck(glGetIntegerv(GL_VIEWPORT, oldVp));
        glCheck(glViewport(0, 0, size.x, size.y));

        //set culling/blend mode
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glEnable(GL_CULL_FACE));
        applyBlendMode();

        //bind texture
        glCheck(glActiveTexture(GL_TEXTURE0));
        glCheck(glBindTexture(GL_TEXTURE_2D, m_textureID));

        //bind shader
        glCheck(glUseProgram(shader->getGLHandle()));
        glCheck(glUniform1i(textureUniform, 0));
        glCheck(glUniformMatrix4fv(worldUniform, 1, GL_FALSE, &m_modelMatrix[0][0]));
        glCheck(glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, &projectionMatrix[0][0]));

        //draw
#ifdef PLATFORM_DESKTOP
        glCheck(glBindVertexArray(m_vao));
        glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
        glCheck(glBindVertexArray(0));
#else
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

        //pos
        glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0));
        glCheck(glEnableVertexAttribArray(0));

        //uv
        glCheck(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))));
        glCheck(glEnableVertexAttribArray(1));

        glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

        glCheck(glDisableVertexAttribArray(1));
        glCheck(glDisableVertexAttribArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif

        //restore viewport/blendmode etc
        glCheck(glDepthMask(GL_TRUE));
        glCheck(glDisable(GL_BLEND));
        glCheck(glViewport(oldVp[0], oldVp[1], oldVp[2], oldVp[3]));

        glCheck(glUseProgram(0));
    }
}

//private
void SimpleQuad::updateVertexData(glm::vec2 size)
{
    std::vector<float> verts =
    {
        0.f, size.y,
        0.f, 1.f,

        0.f, 0.f,
        0.f, 0.f,

        size.x, size.y,
        1.f, 1.f,

        size.x, 0.f,
        1.f, 0.f
    };

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void SimpleQuad::applyBlendMode() const
{
    switch (m_blendMode)
    {
    default: break;
    case Material::BlendMode::Additive:
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_ONE, GL_ONE));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Alpha:
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Multiply:
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::None:
        glCheck(glDisable(GL_BLEND));
        break;
    }
}

void SimpleQuad::updateTransform()
{
    m_modelMatrix = glm::translate(glm::mat4(1.f), glm::vec3(m_position, 0.f));
    m_modelMatrix = glm::rotate(m_modelMatrix, m_rotation, cro::Transform::Z_AXIS);
    m_modelMatrix = glm::scale(m_modelMatrix, glm::vec3(m_scale, 1.f));
}