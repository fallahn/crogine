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

#include <crogine/graphics/SimpleDrawable.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <memory>

using namespace cro;

namespace
{
    std::int32_t activeTextureShader = 0;
    std::int32_t activeColourShader = 0;

    std::unique_ptr<Shader> textureShader;
    std::unique_ptr<Shader> colourShader;

    const std::string ShaderVertex =
        R"(
    ATTRIBUTE vec2 a_position;
    ATTRIBUTE vec2 a_texCoord0;
    ATTRIBUTE vec4 a_colour;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_projectionMatrix;

    VARYING_OUT vec2 v_texCoord;
    VARYING_OUT LOW vec4 v_colour;

    void main()
    {
        gl_Position = u_projectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
        v_texCoord = a_texCoord0;
        v_colour = a_colour;
    })";

    const std::string TextureShaderFragment =
        R"(
    uniform sampler2D u_texture;

    VARYING_IN vec2 v_texCoord;
    VARYING_IN LOW vec4 v_colour;

    OUTPUT

    void main()
    {
        FRAG_OUT = TEXTURE(u_texture, v_texCoord) * v_colour;
    })";

    const std::string ColourShaderFragment =
        R"(
    VARYING_IN LOW vec4 v_colour;

    OUTPUT

    void main()
    {
        FRAG_OUT = v_colour;
    })";

    void increaseTextureShader()
    {
        if (activeTextureShader == 0)
        {
            //load shader
            textureShader = std::make_unique<Shader>();
            textureShader->loadFromString(ShaderVertex, TextureShaderFragment);
        }
        activeTextureShader++;
    }

    void decreaseTextureShader()
    {
        activeTextureShader--;

        if (activeTextureShader == 0)
        {
            textureShader.reset();
        }
    }

    void increaseColourShader()
    {
        if (activeColourShader == 0)
        {
            colourShader = std::make_unique<Shader>();
            colourShader->loadFromString(ShaderVertex, ColourShaderFragment);
        }
        activeColourShader++;
    }

    void decreaseColourShader()
    {
        activeColourShader--;

        if (activeColourShader == 0)
        {
            colourShader.reset();
        }
    }
}

SimpleDrawable::SimpleDrawable()
    : m_primitiveType   (GL_TRIANGLE_STRIP),
    m_vbo               (0),
#ifdef PLATFORM_DESKTOP
    m_vao               (0),
#endif
    m_vertexCount       (0),
    m_textureID         (0),
    m_texture           (nullptr),
    m_blendMode         (Material::BlendMode::Alpha)
{
    increaseColourShader();
    setShader(*colourShader);

    //create buffer
    glCheck(glGenBuffers(1, &m_vbo));
#ifdef PLATFORM_DESKTOP
    glCheck(glGenVertexArrays(1, &m_vao));

    glCheck(glBindVertexArray(m_vao));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

    auto stride = 8 * static_cast<std::uint32_t>(sizeof(float)); //size of a vert

    //pos
    glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0));
    glCheck(glEnableVertexAttribArray(0));

    //uv
    glCheck(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float))));
    glCheck(glEnableVertexAttribArray(1));

    //colour
    glCheck(glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float))));
    glCheck(glEnableVertexAttribArray(2));

    glCheck(glBindVertexArray(0));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif
}

SimpleDrawable::~SimpleDrawable()
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

    if (textureShader &&
        m_uniforms.shaderID == textureShader->getGLHandle())
    {
        decreaseTextureShader();
    }
    else if (colourShader &&
        m_uniforms.shaderID == colourShader->getGLHandle())
    {
        decreaseColourShader();
    }
}

//public
bool SimpleDrawable::setShader(const Shader& shader)
{
    const auto& attribs = shader.getAttribMap();
    if (attribs[Mesh::Position] == -1)
    {
        LogE << "No position attribute was found in shader" << std::endl;
        LogI << "SimpleDrawable expects vec2 a_position" << std::endl;
        return false;
    }

    if (attribs[Mesh::Colour] == -1)
    {
        LogE << "No colour attribute was found in shader" << std::endl;
        LogI << "SimpleDrawable expects vec4 a_colour" << std::endl;
        return false;
    }

    //texture is optional
    /*if (attribs[Mesh::UV0] == -1)
    {
        LogE << "No texture coordinate attribute was found in shader" << std::endl;
        LogI << "SimpleDrawable expects vec2 a_texCoord0" << std::endl;
        return false;
    }*/

    //TODO the above doesn't actually check that the attribs are the correct *order*


    //decrease the existing count if we're replacing a built-in
    if (textureShader &&
        m_uniforms.shaderID == textureShader->getGLHandle())
    {
        decreaseTextureShader();
    }
    else if (colourShader &&
        m_uniforms.shaderID == colourShader->getGLHandle())
    {
        decreaseColourShader();
    }



    auto uniforms = shader.getUniformMap();
    m_uniforms = {};
    if (uniforms.count("u_projectionMatrix"))
    {
        m_uniforms.projectionMatrix = uniforms.at("u_projectionMatrix");
    }
    else
    {
        m_uniforms.projectionMatrix = -1;
        LogW << "Uniform u_projectionMatrix not found in SimpleDrawble shader" << std::endl;
    }

    if (uniforms.count("u_worldMatrix"))
    {
        m_uniforms.worldMatrix = uniforms.at("u_worldMatrix");
    }
    else
    {
        m_uniforms.worldMatrix = -1;
        LogW << "Uniform u_worldMatrix not found in SimpleDrawable shader" << std::endl;
    }

    m_uniforms.texture = shader.getUniformID("u_texture");
    m_uniforms.shaderID = shader.getGLHandle();


    return true;
}

//protected
void SimpleDrawable::setTexture(const Texture& texture)
{
    m_textureID = texture.getGLHandle();
    m_texture = &texture;

    //only replace the texture if  active shader is
    //colour shader (don't replace custom shaders)
    if (colourShader &&
        m_uniforms.shaderID == colourShader->getGLHandle())
    {
        increaseTextureShader();

        //set shader will reference count outgoing shader
        setShader(*textureShader);
    }
}

void SimpleDrawable::setPrimitiveType(std::uint32_t primitiveType)
{
    CRO_ASSERT(primitiveType >= GL_POINTS && primitiveType <= GL_TRIANGLE_FAN, "");
    m_primitiveType = primitiveType;
}

void SimpleDrawable::setVertexData(const std::vector<Vertex2D>& vertexData)
{
    CRO_ASSERT(m_vbo, "");
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(Vertex2D), vertexData.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    m_vertexCount = static_cast<std::uint32_t>(vertexData.size());
}

void SimpleDrawable::drawGeometry(const glm::mat4& worldTransform) const
{
    //set projection
    const auto& projectionMatrix = RenderTarget::getActiveTarget()->getProjectionMatrix();    
       
    //bind shader
    glCheck(glUseProgram(m_uniforms.shaderID));
    glCheck(glUniformMatrix4fv(m_uniforms.worldMatrix, 1, GL_FALSE, &worldTransform[0][0]));
    glCheck(glUniformMatrix4fv(m_uniforms.projectionMatrix, 1, GL_FALSE, &projectionMatrix[0][0]));

    if (m_textureID)
    {
        //bind texture
        glCheck(glActiveTexture(GL_TEXTURE0));
        glCheck(glBindTexture(GL_TEXTURE_2D, m_textureID));

        glCheck(glUniform1i(m_uniforms.texture, 0));
    }

    //set viewport
    auto vp = RenderTarget::getActiveTarget()->getViewport();
    glCheck(glViewport(vp.left, vp.bottom, vp.width, vp.height));

    //set culling/blend mode
    glCheck(glDepthMask(GL_FALSE));
    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glEnable(GL_CULL_FACE));
    applyBlendMode();


    //draw
#ifdef PLATFORM_DESKTOP
    glCheck(glBindVertexArray(m_vao));
    glCheck(glDrawArrays(m_primitiveType, 0, m_vertexCount));
    glCheck(glBindVertexArray(0));
#else
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

    auto stride = 8 * static_cast<std::uint32_t>(sizeof(float))

    //pos
    glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0));
    glCheck(glEnableVertexAttribArray(0));

    //uv
    glCheck(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float))));
    glCheck(glEnableVertexAttribArray(1));

    //colour
    glCheck(glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float))));
    glCheck(glEnableVertexAttribArray(2));

    glCheck(glDrawArrays(m_primitiveType, 0, m_vertexCount));

    glCheck(glDisableVertexAttribArray(1));
    glCheck(glDisableVertexAttribArray(0));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif

    //restore viewport/blendmode etc
    glCheck(glDepthMask(GL_TRUE));
    glCheck(glDisable(GL_BLEND));

    glCheck(glUseProgram(0));
}

//private
void SimpleDrawable::applyBlendMode() const
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