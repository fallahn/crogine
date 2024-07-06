/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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
    : m_samples         (0),
    m_fboID             (0),
    m_rboID             (0),
    m_clearBits         (0),
    m_msfboID           (0),
    m_msTextureID       (0),
    m_depthTextureID    (0),
    m_msDepthTextureID  (0),
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
    
    if (m_depthTextureID)
    {
        glCheck(glDeleteTextures(1, &m_depthTextureID));
    }

    if (m_msDepthTextureID)
    {
        glCheck(glDeleteTextures(1, &m_depthTextureID));
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
    m_depthTextureID = other.m_depthTextureID;
    m_msDepthTextureID = other.m_msDepthTextureID;

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
    other.m_depthTextureID = 0;
    other.m_msDepthTextureID = 0;

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
        if (m_depthTextureID)
        {
            glCheck(glDeleteTextures(1, &m_depthTextureID));
        }
        if (m_msDepthTextureID)
        {
            glCheck(glDeleteTextures(1, &m_msDepthTextureID));
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
        m_depthTextureID = other.m_depthTextureID;
        m_msDepthTextureID = other.m_msDepthTextureID;

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
        other.m_depthTextureID = 0;
        other.m_msDepthTextureID = 0;

        other.setViewport({ 0, 0, 0, 0 });
        other.setView({ 0.f, 0.f });
    }
    return *this;
}

//public
bool RenderTexture::create(std::uint32_t width, std::uint32_t height, bool depthBuffer, bool stencilBuffer, std::uint32_t samples)
{
    return create({ width, height, depthBuffer, false, stencilBuffer, samples });
}

bool RenderTexture::create(RenderTarget::Context ctx)
{
#ifdef PLATFORM_MOBILE
    return createDefault(ctx.width, ctx.height, ctx.depthBuffer, ctx.stencilBuffer);
#else
    if (ctx.samples)
    {
        std::int32_t samplesAvailable = 0;
        glCheck(glGetIntegerv(GL_MAX_SAMPLES, &samplesAvailable));

        m_samples = std::min(ctx.samples, static_cast<std::uint32_t>(samplesAvailable));

        if (m_samples == 0)
        {
            LogE << "Multisampled RenderTextures not supported (Max Samples returned was 0)" << std::endl;
            return false;
        }
        else if (m_samples != ctx.samples)
        {
            LogW << "Sample count reduced to " << m_samples << " (max available)" << std::endl;
        }

        return createMultiSampled(ctx);
    }
    else
    {
        //this will make sure to reset any extra FBO/Texture used for MSAA
        //if they currently exist
        return createDefault(ctx);
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

TextureID RenderTexture::getDepthTexture() const
{
    return TextureID(m_depthTextureID);
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

void RenderTexture::setBorderColour(Colour colour)
{
    m_texture.setBorderColour(colour);
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
        glCheck(glBlitFramebuffer(0, 0, size.x, size.y, 0, 0, size.x, size.y, m_clearBits, GL_NEAREST));

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
bool RenderTexture::createDefault(RenderTarget::Context ctx)
{
    if (m_samples)
    {
        //we previously had multisampling so tidy it up
        glCheck(glDeleteFramebuffers(1, &m_fboID));
        glCheck(glDeleteFramebuffers(1, &m_msfboID));
        m_fboID = 0;
        m_msfboID = 0;

        glCheck(glDeleteTextures(1, &m_msTextureID));
        m_msTextureID = 0;

        if (m_msDepthTextureID)
        {
            glCheck(glDeleteTextures(1, &m_msDepthTextureID));
            m_msDepthTextureID = 0;
        }

        m_samples = 0;
    }


    //we were already created if the texture exists
    if (m_texture.getGLHandle())
    {
        //if the settings are the same and we're just
        //resizing, skip re-creation and return with a
        //resized texture
        if (m_fboID
            && ctx.depthBuffer == m_hasDepthBuffer
            && ctx.stencilBuffer == m_hasStencilBuffer
            && (ctx.depthTexture == (m_depthTextureID != 0)))
        {
            m_texture.create(ctx.width, ctx.height);
            if (m_rboID)
            {
                std::int32_t format = ctx.stencilBuffer ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;

                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_rboID));
                glCheck(glRenderbufferStorage(GL_RENDERBUFFER, format, ctx.width, ctx.height));
                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, 0));
            }

            if (m_depthTextureID)
            {
                glCheck(glBindTexture(GL_TEXTURE_2D, m_depthTextureID));
                glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, ctx.width, ctx.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));
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

    //only do this if the new settings
    //require removal, else we just resize...
    if ((!ctx.depthBuffer || !ctx.depthTexture)
        && m_depthTextureID)
    {
        glCheck(glDeleteTextures(1, &m_depthTextureID));
        m_depthTextureID = 0;
    }

    m_texture.create(ctx.width, ctx.height, ImageFormat::RGBA);

    GLuint fbo;
    glCheck(glGenFramebuffers(1, &fbo));
    if (fbo)
    {
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture.getGLHandle(), 0));

        m_clearBits |= GL_COLOR_BUFFER_BIT;

        if (ctx.depthBuffer)
        {
            m_clearBits |= GL_DEPTH_BUFFER_BIT;

            std::int32_t format = GL_DEPTH_COMPONENT24;
            std::int32_t attachment = GL_DEPTH_ATTACHMENT;

            //TODO why do we only allow stencilling if there's a depth buffer??
            if (ctx.stencilBuffer)
            {
                format = GL_DEPTH24_STENCIL8;
                attachment = GL_DEPTH_STENCIL_ATTACHMENT;
                m_clearBits |= GL_STENCIL_BUFFER_BIT;
            }


            
            {
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
                glCheck(glRenderbufferStorage(GL_RENDERBUFFER, format, ctx.width, ctx.height));
                glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_rboID));
                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, 0));

            }

            //create a depth texture if requested - though only do it here
            //if there's a depth buffer available
            if (ctx.depthTexture)
            {
                if (m_depthTextureID == 0)
                {
                    glCheck(glGenTextures(1, &m_depthTextureID));
                }

                glCheck(glBindTexture(GL_TEXTURE_2D, m_depthTextureID));
                glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, ctx.width, ctx.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
                const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
                glCheck(glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor));

                glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTextureID, 0));
            }
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            glCheck(glBindFramebuffer(GL_FRAMEBUFFER, RenderTarget::getActiveTargetID()));
            m_fboID = fbo;
            setViewport(getDefaultViewport());
            setView(FloatRect(getViewport()));

            m_hasDepthBuffer = ctx.depthBuffer;
            m_hasStencilBuffer = ctx.stencilBuffer;

            return true;
        }
    }
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, RenderTarget::getActiveTargetID()));

    Texture temp;
    temp.swap(m_texture);

    return false;
}

bool RenderTexture::createMultiSampled(RenderTarget::Context ctx)
{
    if (m_msTextureID) //already created at least once
    {
        //if the settings are the same and we're just
        //resizing, skip re-creation and return with a
        //resized texture
        if (ctx.depthBuffer == m_hasDepthBuffer
            && ctx.stencilBuffer == m_hasStencilBuffer
            && (ctx.depthTexture == (m_depthTextureID != 0)))
        {
            if (m_depthTextureID)
            {
                glCheck(glBindTexture(GL_TEXTURE_2D, m_depthTextureID));
                glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, ctx.width, ctx.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));

                //existence of depth texture implies existence of msDepth
                glCheck(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_msDepthTextureID));
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_samples, GL_DEPTH_COMPONENT, ctx.width, ctx.height, GL_TRUE);
            }
            
            glCheck(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_msTextureID));
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_samples, GL_RGBA, ctx.width, ctx.height, GL_TRUE);
            glCheck(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0));

            m_texture.create(ctx.width, ctx.height);

            if (m_rboID)
            {
                std::int32_t format = ctx.stencilBuffer ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;

                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_rboID));
                glCheck(glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, format, ctx.width, ctx.height));
                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, 0));
            }

            setViewport(getDefaultViewport());
            setView(FloatRect(getViewport()));

            return true;
        }

        //else delete the buffer and create a fresh one
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

    if (!ctx.depthTexture || !ctx.depthBuffer)
    {
        //remove unused depth texture if it exists
        if (m_depthTextureID)
        {
            glCheck(glDeleteTextures(1, &m_depthTextureID));
            m_depthTextureID = 0;
        }

        if (m_msDepthTextureID)
        {
            glCheck(glDeleteTextures(1, &m_msDepthTextureID));
            m_msDepthTextureID = 0;
        }
    }


    if (m_msTextureID == 0)
    {
        glCheck(glGenTextures(1, &m_msTextureID));
    }

    if (m_msTextureID)
    {
        glCheck(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_msTextureID));
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_samples, GL_RGBA, ctx.width, ctx.height, GL_TRUE);
        glCheck(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0));
    }
    else
    {
        LogE << "Failed to create multisampled texture" << std::endl;
        return false;
    }

    m_texture.create(ctx.width, ctx.height, ImageFormat::RGBA);

    GLuint fbos[2] = {};
    glCheck(glGenFramebuffers(2, fbos));
    if (fbos[0] && fbos[1])
    {
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbos[1]));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture.getGLHandle(), 0));

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbos[0]));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_msTextureID, 0));

        m_clearBits |= GL_COLOR_BUFFER_BIT;

        if (ctx.depthBuffer)
        {
            m_clearBits |= GL_DEPTH_BUFFER_BIT;

            //only use rbo if not using depth texture
            if (!ctx.depthTexture)
            {
                std::int32_t format = GL_DEPTH_COMPONENT24;
                std::int32_t attachment = GL_DEPTH_ATTACHMENT;
            
                //TODO are stencil buffers only available if depth
                //is present? Else why are we doing this?
                if (ctx.stencilBuffer)
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
                glCheck(glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, format, ctx.width, ctx.height));
                glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_rboID));
            }
            else
            {
                //again, this could be done in an array - but it works so meh, let's not micro-optimise
                if (m_depthTextureID == 0)
                {
                    glCheck(glGenTextures(1, &m_depthTextureID));
                }

                if (m_msDepthTextureID == 0)
                {
                    glCheck(glGenTextures(1, &m_msDepthTextureID));
                }

                //output texture
                glCheck(glBindTexture(GL_TEXTURE_2D, m_depthTextureID));
                glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, ctx.width, ctx.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));

                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST)); //MSAA depth textures ONLY supports GL_NEAREST
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
                const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
                glCheck(glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor));

                glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbos[1]));
                glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTextureID, 0));


                //ms texture
                glCheck(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_msDepthTextureID));
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_samples, GL_DEPTH_COMPONENT, ctx.width, ctx.height, GL_TRUE);

                glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbos[0]));
                glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, m_msDepthTextureID, 0));
            }
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            glCheck(glBindFramebuffer(GL_FRAMEBUFFER, RenderTarget::getActiveTargetID()));
            m_fboID = fbos[0];
            m_msfboID = fbos[1];
            setViewport(getDefaultViewport());
            setView(FloatRect(getViewport()));

            m_hasDepthBuffer = ctx.depthBuffer;
            m_hasStencilBuffer = ctx.stencilBuffer && !ctx.depthTexture; //TODO we haven't implemented stencil textures

            return true;
        }
    }
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, RenderTarget::getActiveTargetID()));

    //remove unused textures cos creating FBOs failed
    glCheck(glDeleteTextures(1, &m_msTextureID));
    m_msTextureID = 0;


    Texture temp;
    temp.swap(m_texture);

    return false;
}