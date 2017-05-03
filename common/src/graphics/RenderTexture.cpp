/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <crogine/graphics/RenderTexture.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

RenderTexture::RenderTexture()
    : m_fboID   (0),
    m_rboID     (0),
    m_clearBits (0)
{

}

RenderTexture::~RenderTexture()
{
    if (m_fboID)
    {
        glCheck(glDeleteFramebuffers(1, &m_fboID));
    }
    if (m_rboID)
    {
        glCheck(glDeleteRenderbuffers(1, &m_rboID));
    }
}

RenderTexture::RenderTexture(RenderTexture&& other)
{
    m_fboID = other.m_fboID;
    m_texture = std::move(other.m_texture);
    m_viewport = other.m_viewport;
    m_lastViewport = other.m_lastViewport;

    other.m_fboID = 0;
}

RenderTexture& RenderTexture::operator=(RenderTexture&& other)
{
    if (&other != this)
    {
        m_fboID = other.m_fboID;
        m_texture = std::move(other.m_texture);
        m_viewport = other.m_viewport;
        m_lastViewport = other.m_lastViewport;

        other.m_fboID = 0;
    }
    return *this;
}

//public
bool RenderTexture::create(uint32 width, uint32 height, bool depthBuffer, bool stencilBuffer)
{
    if (m_fboID)
    {
        glCheck(glDeleteFramebuffers(1, &m_fboID));
    }

    m_texture.create(width, height, ImageFormat::RGBA);

    GLuint fbo;
    glCheck(glGenFramebuffers(1, &fbo));
    if (fbo)
    {
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture.getGLHandle(), 0));

        m_clearBits |= GL_COLOR_BUFFER_BIT;

        if (depthBuffer)
        {
            m_clearBits |= GL_DEPTH_BUFFER_BIT;
            
            int32 format = GL_DEPTH_COMPONENT24;
            int32 attachment = GL_DEPTH_ATTACHMENT;
            if (stencilBuffer)
            {
                format = GL_DEPTH24_STENCIL8;
                attachment = GL_DEPTH_STENCIL_ATTACHMENT;
                m_clearBits |= GL_STENCIL_BUFFER_BIT;
            }
            
            GLuint rbo;
            glCheck(glGenRenderbuffers(1, &rbo));
            if (!rbo)
            {
                Logger::log("Failed creating depth / stencil render buffer", Logger::Type::Error);
                glCheck(glDeleteFramebuffers(1, &fbo));
                return false;
            }

            glCheck(glBindRenderbuffer(GL_RENDERBUFFER, rbo));
            glCheck(glRenderbufferStorage(GL_RENDERBUFFER, format, width, height));
            glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment , GL_RENDERBUFFER, rbo));
            m_rboID = rbo;
            glCheck(glBindRenderbuffer(GL_RENDERBUFFER, 0));
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            m_fboID = fbo;
            m_viewport = getDefaultViewport();
            return true;
        }
    }
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    return false;
}

glm::uvec2 RenderTexture::getSize() const
{
    return m_texture.getSize();
}

const Texture& RenderTexture::getTexture() const
{
    return m_texture;
}

void RenderTexture::setRepeated(bool repeated)
{
    m_texture.setRepeated(repeated);
}

bool RenderTexture::isRepeated() const
{
    return m_texture.isRepeated();
}

void RenderTexture::setSmooth(bool smooth)
{
    m_texture.setSmooth(smooth);
}

bool RenderTexture::isSmooth() const
{
    return m_texture.isSmooth();
}

void RenderTexture::clear(Colour colour)
{
    CRO_ASSERT(m_fboID, "No FBO created!");

    //store existing viewport - and apply ours
    glCheck(glGetIntegerv(GL_VIEWPORT, m_lastViewport.data()));
    glCheck(glViewport(m_viewport.left, m_viewport.bottom, m_viewport.width, m_viewport.height));

    //set buffer active
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_fboID));

    //clear buffer - UH OH this will clear the main buffer if FBO is null
    glCheck(glGetFloatv(GL_COLOR_CLEAR_VALUE, m_lastClearColour.data()));
    glCheck(glClearColor(colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha()));
    glCheck(glClear(m_clearBits));
}

void RenderTexture::display()
{
    //restore viewport
    glCheck(glViewport(m_lastViewport[0], m_lastViewport[1], m_lastViewport[2], m_lastViewport[3]));

    //restore clear colour
    glCheck(glClearColor(m_lastClearColour[0], m_lastClearColour[1], m_lastClearColour[2], m_lastClearColour[3]));

    //unbind buffer
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

void RenderTexture::setViewport(URect viewport)
{
    m_viewport = viewport;
}

URect RenderTexture::getViewport() const
{
    return m_viewport;
}

URect RenderTexture::getDefaultViewport() const
{
    return { 0, 0, m_texture.getSize().x, m_texture.getSize().y };
}