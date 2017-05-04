/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "PostRadial.hpp"

#include <crogine/graphics/shaders/PostVertex.hpp>

namespace
{
    const std::string fragment = R"(
        uniform sampler2D u_texture;

        varying vec2 v_texCoord;
        
        void main()
        {
            gl_FragColor = texture2D(u_texture, v_texCoord);
        }       
    )";

#include "PostRadial.inl"
}

PostRadial::PostRadial()
{
    m_inputShader.loadFromString(cro::PostVertex, extractionFrag);
    m_outputShader.loadFromString(cro::PostVertex, blueDream);
}

//public
void PostRadial::apply(const cro::RenderTexture& source)
{
    glm::vec2 size(getCurrentBufferSize());
    setUniform("u_texture", source.getTexture(), m_inputShader);
    m_blurBuffer.clear();
    drawQuad(m_inputShader, { 0.f, 0.f, size.x / 2.f, size.y / 2.f });
    m_blurBuffer.display();
    
    setUniform("u_baseTexture", source.getTexture(), m_outputShader);
    drawQuad(m_outputShader, { 0.f, 0.f, size.x, size.y });
}

//private
void PostRadial::bufferResized()
{
    auto size = getCurrentBufferSize() / 2u;
    
    m_blurBuffer.create(size.x, size.y, false);
    setUniform("u_texture", m_blurBuffer.getTexture(), m_outputShader);
}