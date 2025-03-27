/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include <crogine/graphics/postprocess/PostChromeAB.hpp>
#include <crogine/graphics/postprocess/PostVertex.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include "../../detail/GLCheck.hpp"

using namespace cro;

namespace
{
#include "PostChromeAB.inl"
}

PostChromeAB::PostChromeAB()
    :   m_textureIndex(0)
{
    if (!m_postShader.loadFromString(PostVertex, FragChromeAB))
    {
        Logger::log("Failed loading chromatic abberation shader", Logger::Type::Error);
    }
    else
    {
        m_textureIndex = m_postShader.getUniformID("u_input");
    }
    addPass(m_postShader);
}

//public
void PostChromeAB::apply(const RenderTexture& input)
{
    glCheck(glUseProgram(m_postShader.getGLHandle()));
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, input.getTexture().getGLHandle()));
    glCheck(glUniform1i(m_textureIndex, 0));

    auto size = glm::vec2(RenderTarget::getActiveTarget()->getSize());
    drawQuad(0, { 0.f, 0.f, size.x, size.y });
}