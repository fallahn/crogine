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

#include <crogine/graphics/DepthTexture.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

DepthTexture::DepthTexture()
    : m_fboID   (0),
    m_textureID (0),
    m_size      (0,0),
    m_viewport  (0,0,1,1),
    m_lastBuffer(0)
{

}

DepthTexture::~DepthTexture()
{
    if (m_fboID)
    {
        glCheck(glDeleteFramebuffers(1, &m_fboID));
    }

    if (m_textureID)
    {
        glCheck(glDeleteTextures(1, &m_textureID));
    }
}

DepthTexture::DepthTexture(DepthTexture&& other) noexcept
    : m_fboID   (0),
    m_textureID (0),
    m_size      (0, 0),
    m_viewport  (0, 0, 1, 1),
    m_lastBuffer(0)
{
    m_fboID = other.m_fboID;
    m_textureID = other.m_textureID;
    m_viewport = other.m_viewport;
    m_lastViewport = other.m_lastViewport;
    m_lastBuffer = other.m_lastBuffer;

    other.m_fboID = 0;
    other.m_textureID = 0;
}

DepthTexture& DepthTexture::operator=(DepthTexture&& other) noexcept
{
    if (&other != this)
    {
        m_fboID = other.m_fboID;
        m_textureID = other.m_textureID;
        m_viewport = other.m_viewport;
        m_lastViewport = other.m_lastViewport;
        m_lastBuffer = other.m_lastBuffer;

        other.m_fboID = 0;
        other.m_textureID = 0;
    }
    return *this;
}

//public
bool DepthTexture::create(uint32 width, uint32 height)
{
#ifdef PLATFORM_MOBILE
    LogE << "Depth Textures are not available on mobile platforms" << std::endl;
    return false;
#else
    if (m_fboID)
    {
        glCheck(glDeleteFramebuffers(1, &m_fboID));
    }
    if (m_textureID)
    {
        glCheck(glDeleteTextures(1, &m_textureID));
    }

    //create the texture
    glCheck(glGenTextures(1, &m_textureID));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_textureID));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    m_viewport.width = width;
    m_viewport.height = height;

    //create the frame buffer
    glCheck(glGenFramebuffers(1, &m_fboID));
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_fboID));
    glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_textureID, 0));
    glCheck(glDrawBuffer(GL_NONE));
    glCheck(glReadBuffer(GL_NONE));

    bool result = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return result;
#endif
}

glm::uvec2 DepthTexture::getSize() const
{
    return m_size;
}

void DepthTexture::clear()
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(m_fboID, "No FBO created!");

    //store existing viewport - and apply ours
    glCheck(glGetIntegerv(GL_VIEWPORT, m_lastViewport.data()));
    glCheck(glViewport(m_viewport.left, m_viewport.bottom, m_viewport.width, m_viewport.height));

    //store active buffer
    //glCheck(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_lastBuffer));
    m_lastBuffer = RenderTarget::ActiveTarget;

    //set buffer active
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_fboID));
    RenderTarget::ActiveTarget = m_fboID;

    //clear buffer - UH OH this will clear the main buffer if FBO is null
    glCheck(glClear(GL_DEPTH_BUFFER_BIT));
#endif
}

void DepthTexture::display()
{
#ifdef PLATFORM_DESKTOP
    //restore viewport
    glCheck(glViewport(m_lastViewport[0], m_lastViewport[1], m_lastViewport[2], m_lastViewport[3]));

    //unbind buffer
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_lastBuffer));
    RenderTarget::ActiveTarget = m_lastBuffer;
#endif
}

void DepthTexture::setViewport(URect viewport)
{
    m_viewport = viewport;
}

URect DepthTexture::getViewport() const
{
    return m_viewport;
}

URect DepthTexture::getDefaultViewport() const
{
    return { 0, 0, m_size.x, m_size.y };
}

TextureID DepthTexture::getTexture() const
{
    return TextureID(m_textureID);
}