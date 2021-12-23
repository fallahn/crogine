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

#include <crogine/graphics/RenderTexture.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

RenderTexture::RenderTexture()
    : m_fboID           (0),
    m_rboID             (0),
    m_clearBits         (0),
    m_hasDepthBuffer    (false),
    m_hasStencilBuffer  (false)
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

RenderTexture::RenderTexture(RenderTexture&& other) noexcept
    : RenderTexture()
{
    m_fboID = other.m_fboID;
    m_rboID = other.m_rboID;
    m_clearBits = other.m_clearBits;
    m_texture = std::move(other.m_texture);
    setViewport(other.getViewport());
    setView(other.getView());
    m_hasDepthBuffer = other.m_hasDepthBuffer;
    m_hasStencilBuffer = other.m_hasStencilBuffer;

    other.m_fboID = 0;
    other.m_rboID = 0;
    other.setViewport({ 0, 0, 0, 0 });
    other.setView({ 0.f, 0.f });
}

RenderTexture& RenderTexture::operator=(RenderTexture&& other) noexcept
{
    if (&other != this)
    {
        //tidy up anything we currently own first
        if (m_fboID)
        {
            glCheck(glDeleteFramebuffers(1, &m_fboID));
        }
        if (m_rboID)
        {
            glCheck(glDeleteRenderbuffers(1, &m_rboID));
        }

        m_fboID = other.m_fboID;
        m_rboID = other.m_rboID;
        m_clearBits = other.m_clearBits;
        m_texture = std::move(other.m_texture);
        setViewport(other.getViewport());
        setView(other.getView());
        m_hasDepthBuffer = other.m_hasDepthBuffer;
        m_hasStencilBuffer = other.m_hasStencilBuffer;

        other.m_fboID = 0;
        other.m_rboID = 0;
        other.setViewport({ 0, 0, 0, 0 });
        other.setView({ 0.f, 0.f });
    }
    return *this;
}

//public
bool RenderTexture::create(std::uint32_t width, std::uint32_t height, bool depthBuffer, bool stencilBuffer)
{
    if (m_fboID)
    {
        //if the settings are the same and we're just
        //resizing, skip re-creation and return with a
        //resized texture
        if (depthBuffer == m_hasDepthBuffer
            && stencilBuffer == m_hasStencilBuffer)
        {
            m_texture.create(width, height);
            if (m_rboID)
            {
                std::int32_t format = stencilBuffer? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;

                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_rboID));
                glCheck(glRenderbufferStorage(GL_RENDERBUFFER, format, width, height));
                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_rboID));
            }
            return true;
        }

        //else delete the buffer and create a fresh one
        glCheck(glDeleteFramebuffers(1, &m_fboID));
        m_clearBits = 0;
    }

    if (m_rboID)
    {
        glCheck(glDeleteRenderbuffers(1, &m_rboID));
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
            
            std::int32_t format = GL_DEPTH_COMPONENT24;
            std::int32_t attachment = GL_DEPTH_ATTACHMENT;
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

                //tidy up the texture as we won't need it
                Texture temp;
                temp.swap(m_texture);

                return false;
            }
            m_rboID = rbo;

            glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_rboID));
            glCheck(glRenderbufferStorage(GL_RENDERBUFFER, format, width, height));
            glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_rboID));
            glCheck(glBindRenderbuffer(GL_RENDERBUFFER, 0));
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            m_fboID = fbo;
            setViewport(getDefaultViewport());
            setView(FloatRect(getViewport()));

            m_hasDepthBuffer = depthBuffer;
            m_hasStencilBuffer = stencilBuffer;

            return true;
        }
    }
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    Texture temp;
    temp.swap(m_texture);

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

    //store active buffer and bind this one
    setActive(true);

    //clear buffer - UH OH this will clear the main buffer if FBO is null
    glCheck(glGetFloatv(GL_COLOR_CLEAR_VALUE, m_lastClearColour.data()));
    glCheck(glClearColor(colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha()));
    glCheck(glClear(m_clearBits));
}

void RenderTexture::display()
{
    //restore clear colour
    glCheck(glClearColor(m_lastClearColour[0], m_lastClearColour[1], m_lastClearColour[2], m_lastClearColour[3]));

    //unbind buffer
    setActive(false);
}

bool RenderTexture::saveToFile(const std::string& path) const
{
    if (m_fboID)
    {
        return m_texture.saveToFile(path);
    }
    return false;
}