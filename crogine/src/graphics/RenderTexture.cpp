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

RenderTexture::RenderTexture(std::uint32_t samples)
    : m_samples         (samples),
    m_fboID             (0),
    m_rboID             (0),
    m_clearBits         (0),
    m_msfboID           (0),
    m_msTextureID       (0),
    m_hasDepthBuffer    (false),
    m_hasStencilBuffer  (false)
{

}

RenderTexture::~RenderTexture()
{
    if (m_msfboID)
    {
        glCheck(glDeleteFramebuffers(1, &m_msfboID));
    }
    if (m_msTextureID)
    {
        glCheck(glDeleteTextures(1, &m_msTextureID));
    }
    
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
    m_samples = other.m_samples;
    m_fboID = other.m_fboID;
    m_rboID = other.m_rboID;
    m_clearBits = other.m_clearBits;

    m_msfboID = other.m_msfboID;
    m_msTextureID = other.m_msTextureID;

    m_texture = std::move(other.m_texture);
    setViewport(other.getViewport());
    setView(other.getView());
    m_hasDepthBuffer = other.m_hasDepthBuffer;
    m_hasStencilBuffer = other.m_hasStencilBuffer;

    other.m_samples = 0;
    other.m_fboID = 0;
    other.m_rboID = 0;

    other.m_msfboID = 0;
    other.m_msTextureID = 0;

    other.setViewport({ 0, 0, 0, 0 });
    other.setView({ 0.f, 0.f });
}

RenderTexture& RenderTexture::operator=(RenderTexture&& other) noexcept
{
    if (&other != this)
    {
        //tidy up anything we currently own first
        if (m_msfboID)
        {
            glCheck(glDeleteFramebuffers(1, &m_msfboID));
        }
        if (m_msTextureID)
        {
            glCheck(glDeleteTextures(1, &m_msTextureID));
        }

        if (m_fboID)
        {
            glCheck(glDeleteFramebuffers(1, &m_fboID));
        }
        if (m_rboID)
        {
            glCheck(glDeleteRenderbuffers(1, &m_rboID));
        }

        m_samples = other.m_samples;
        m_fboID = other.m_fboID;
        m_rboID = other.m_rboID;
        m_clearBits = other.m_clearBits;

        m_msfboID = other.m_msfboID;
        m_msTextureID = other.m_msTextureID;

        m_texture = std::move(other.m_texture);
        setViewport(other.getViewport());
        setView(other.getView());
        m_hasDepthBuffer = other.m_hasDepthBuffer;
        m_hasStencilBuffer = other.m_hasStencilBuffer;

        other.m_samples = 0;
        other.m_fboID = 0;
        other.m_rboID = 0;

        other.m_msfboID = 0;
        other.m_msTextureID = 0;

        other.setViewport({ 0, 0, 0, 0 });
        other.setView({ 0.f, 0.f });
    }
    return *this;
}

//public
bool RenderTexture::create(std::uint32_t width, std::uint32_t height, bool depthBuffer, bool stencilBuffer)
{
#ifdef PLATFORM_MOBILE
    return createDefault(width, height, depthBuffer, stencilBuffer);
#else
    if (m_samples)
    {
        return createMultiSampled(width, height, depthBuffer, stencilBuffer);
    }
    else
    {
        return createDefault(width, height, depthBuffer, stencilBuffer);
    }
#endif
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

    glCheck(glEnable(GL_MULTISAMPLE)); //TODO will this cause side-effects on non-ms textures?

    //clear buffer - UH OH this will clear the main buffer if FBO is null
    glCheck(glGetFloatv(GL_COLOR_CLEAR_VALUE, m_lastClearColour.data()));
    glCheck(glClearColor(colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha()));
    glCheck(glClear(m_clearBits));
}

void RenderTexture::display()
{
    //restore clear colour
    glCheck(glClearColor(m_lastClearColour[0], m_lastClearColour[1], m_lastClearColour[2], m_lastClearColour[3]));

    
    if (m_samples)
    {
        CRO_ASSERT(m_fboID, "Texture not created");
        
        auto size = m_texture.getSize();

        //blit the multisampled buffer to the output:
        //the read frame buffer should already be current
        //then we rely on setActive(false) - below - to restore
        //the correct GL_DRAW_FRAMEBUFFER
        glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_msfboID)); //maybe misleading name - contains our regular downsampled texture
        glCheck(glBlitFramebuffer(0, 0, size.x, size.y, 0, 0, size.x, size.y, GL_COLOR_BUFFER_BIT, GL_NEAREST));

        glCheck(glDisable(GL_MULTISAMPLE));
    }


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

//private
bool RenderTexture::createDefault(std::uint32_t width, std::uint32_t height, bool depthBuffer, bool stencilBuffer)
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
                std::int32_t format = stencilBuffer ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;

                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_rboID));
                glCheck(glRenderbufferStorage(GL_RENDERBUFFER, format, width, height));
                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_rboID));
            }

            setViewport(getDefaultViewport());
            setView(FloatRect(getViewport()));

            return true;
        }

        //else delete the buffer and create a fresh one
        glCheck(glDeleteFramebuffers(1, &m_fboID));
        m_fboID = 0;
        m_clearBits = 0;
    }

    if (m_rboID)
    {
        glCheck(glDeleteRenderbuffers(1, &m_rboID));
        m_rboID = 0;
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

bool RenderTexture::createMultiSampled(std::uint32_t width, std::uint32_t height, bool depthBuffer, bool stencilBuffer)
{
    std::int32_t samplesAvailable = 0;
    glCheck(glGetIntegerv(GL_MAX_SAMPLES, &samplesAvailable));

    auto oldSamples = m_samples;
    m_samples = std::min(m_samples, static_cast<std::uint32_t>(samplesAvailable));

    if(m_samples == 0)
    {
        LogE << "Multisampled RenderTextures not supported (Max Samples returned was 0)" << std::endl;
        return false;
    }
    else if (m_samples != m_samples)
    {
        LogW << "Sample count reduced to " << m_samples << " (max available)" << std::endl;
    }


    if (m_fboID)
    {
        //if the settings are the same and we're just
        //resizing, skip re-creation and return with a
        //resized texture
        if (depthBuffer == m_hasDepthBuffer
            && stencilBuffer == m_hasStencilBuffer)
        {
            glCheck(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_msTextureID));
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_samples, GL_RGBA, width, height, GL_TRUE);
            glCheck(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0));

            m_texture.create(width, height);

            if (m_rboID)
            {
                std::int32_t format = stencilBuffer ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;

                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_rboID));
                glCheck(glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, format, width, height));
                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_rboID));
            }

            setViewport(getDefaultViewport());
            setView(FloatRect(getViewport()));

            return true;
        }

        //else delete the buffer and create a fresh one
        //TODO we could do this in an array but I doubt the hit is noticable
        glCheck(glDeleteFramebuffers(1, &m_fboID));
        glCheck(glDeleteFramebuffers(1, &m_msfboID));

        m_fboID = 0;
        m_msfboID = 0;

        m_clearBits = 0;
    }

    if (m_rboID)
    {
        glCheck(glDeleteRenderbuffers(1, &m_rboID));
        m_rboID = 0;
    }

    glCheck(glGenTextures(1, &m_msTextureID));
    if (m_msTextureID)
    {
        glCheck(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_msTextureID));
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_samples, GL_RGBA, width, height, GL_TRUE);
        glCheck(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0));
    }
    else
    {
        LogE << "Failed to create multisampled texture" << std::endl;
        return false;
    }

    m_texture.create(width, height, ImageFormat::RGBA);

    GLuint fbos[2] = {};
    glCheck(glGenFramebuffers(2, fbos));
    if (fbos[0] && fbos[1])
    {
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbos[1]));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture.getGLHandle(), 0));
        
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbos[0]));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_msTextureID, 0));

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

            GLuint rbo = 0;
            glCheck(glGenRenderbuffers(1, &rbo));
            if (!rbo)
            {
                Logger::log("Failed creating depth / stencil render buffer", Logger::Type::Error);
                glCheck(glDeleteFramebuffers(2, fbos));

                //tidy up the textures as we won't need it
                Texture temp;
                temp.swap(m_texture);

                glCheck(glDeleteTextures(1, &m_msTextureID));
                m_msTextureID = 0;

                return false;
            }
            m_rboID = rbo;

            glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_rboID));
            glCheck(glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, format, width, height));
            glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_rboID));
            glCheck(glBindRenderbuffer(GL_RENDERBUFFER, 0));
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            m_fboID = fbos[0];
            m_msfboID = fbos[1];
            setViewport(getDefaultViewport());
            setView(FloatRect(getViewport()));

            m_hasDepthBuffer = depthBuffer;
            m_hasStencilBuffer = stencilBuffer;

            return true;
        }
    }
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    //remove unused textures cos creating FBOs failed
    glCheck(glDeleteTextures(1, &m_msTextureID));
    m_msTextureID = 0;


    Texture temp;
    temp.swap(m_texture);

    return false;
}