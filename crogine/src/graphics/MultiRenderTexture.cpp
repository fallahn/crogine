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

#include <crogine/core/App.hpp>
#include <crogine/graphics/MultiRenderTexture.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

MultiRenderTexture::MultiRenderTexture()
    : m_fboID           (0),
    m_size              (0, 0),
    m_viewport          (0, 0, 1, 1),
    m_lastBuffer        (0),
    m_maxAttachments    (0),
    m_depthTextureID    (0)
{
#ifdef PLATFORM_DESKTOP
    glCheck(glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &m_maxAttachments));
#endif
}

MultiRenderTexture::~MultiRenderTexture()
{
    if (m_fboID)
    {
        glCheck(glDeleteFramebuffers(1, &m_fboID));
        glCheck(glDeleteTextures(static_cast<GLsizei>(m_textureIDs.size()), m_textureIDs.data()));
        glCheck(glDeleteTextures(1, &m_depthTextureID));
    }
}

MultiRenderTexture::MultiRenderTexture(MultiRenderTexture&& other) noexcept
    : MultiRenderTexture()
{
    m_fboID = other.m_fboID;
    m_textureIDs = other.m_textureIDs;
    m_viewport = other.m_viewport;
    m_lastViewport = other.m_lastViewport;
    m_lastBuffer = other.m_lastBuffer;
    m_maxAttachments = other.m_maxAttachments;
    m_depthTextureID = other.m_depthTextureID;
    m_textureIDs = std::move(other.m_textureIDs);

    other.m_fboID = 0;
    other.m_depthTextureID = 0;
}

MultiRenderTexture& MultiRenderTexture::operator=(MultiRenderTexture&& other) noexcept
{
    if (&other != this)
    {
        //tidy up anything we own first!
        if (m_fboID)
        {
            glCheck(glDeleteFramebuffers(1, &m_fboID));
            glCheck(glDeleteTextures(static_cast<GLsizei>(m_textureIDs.size()), m_textureIDs.data()));
            glCheck(glDeleteTextures(1, &m_depthTextureID));
        }

        m_fboID = other.m_fboID;
        m_textureIDs = other.m_textureIDs;
        m_viewport = other.m_viewport;
        m_lastViewport = other.m_lastViewport;
        m_lastBuffer = other.m_lastBuffer;
        m_maxAttachments = other.m_maxAttachments;

        other.m_fboID = 0;
    }
    return *this;
}

//public
bool MultiRenderTexture::create(std::uint32_t width, std::uint32_t height, std::size_t colourCount)
{
#ifdef PLATFORM_MOBILE
    LogE << "Depth Textures are not available on mobile platforms" << std::endl;
    return false;
#else
    CRO_ASSERT(colourCount > 0 && colourCount < m_maxAttachments, "Out of Range");

    if (m_maxAttachments == 0)
    {
        LogE << "No attachments are available" << std::endl;
        return false;
    }

    if (m_fboID == 0)
    {
        //create the frame buffer
        glCheck(glGenFramebuffers(1, &m_fboID));
        
        if (m_fboID == 0)
        {
            return false;
        }
        
        //depth
        glCheck(glGenTextures(1, &m_depthTextureID));
        glCheck(glBindTexture(GL_TEXTURE_2D, m_depthTextureID));
        glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
        const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glCheck(glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor));

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_fboID));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTextureID, 0));
        glCheck(glReadBuffer(GL_NONE));
    }


    if (colourCount == m_textureIDs.size())
    {
        //resize the existing buffers
        for (auto id : m_textureIDs)
        {
            glBindTexture(GL_TEXTURE_2D, id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        }

        glCheck(glBindTexture(GL_TEXTURE_2D, m_depthTextureID));
        glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));

        m_viewport.width = width;
        m_viewport.height = height;
        m_size = { width, height };

        return true;
    }
    else
    {
        if (colourCount > m_textureIDs.size())
        {
            //add new textures
            for (auto i = m_textureIDs.size(); i < colourCount; ++i)
            {
                std::uint32_t id = 0;
                glCheck(glGenTextures(1, &id));
                glCheck(glBindTexture(GL_TEXTURE_2D, id));
                glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
                m_textureIDs.push_back(id);
            }
        }
        else
        {
            //remove the difference
            for (auto i = colourCount; i < m_textureIDs.size(); ++i)
            {
                glCheck(glDeleteTextures(1, &m_textureIDs[i]));
            }

            std::vector<std::uint32_t> temp(m_textureIDs.begin(), m_textureIDs.begin() + (colourCount - 1));
            m_textureIDs.swap(temp);
        }

        //and rebind to FBO
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_fboID));

        std::vector<GLenum> attachments;
        for (auto i = 0u; i < m_textureIDs.size(); ++i)
        {
            glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_textureIDs[i], 0));
            attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
        }
        glCheck(glDrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data()));

        bool result = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_viewport.width = width;
        m_viewport.height = height;
        m_size = { width, height };

        return result;
    }

    return false;
#endif
}

glm::uvec2 MultiRenderTexture::getSize() const
{
    return m_size;
}

void MultiRenderTexture::clear()
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(m_fboID, "No FBO created!");

    //store existing viewport - and apply ours
    glCheck(glGetIntegerv(GL_VIEWPORT, m_lastViewport.data()));
    glCheck(glViewport(m_viewport.left, m_viewport.bottom, m_viewport.width, m_viewport.height));

    //store active buffer
    m_lastBuffer = RenderTarget::ActiveTarget;

    //set buffer active
    glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fboID));
    RenderTarget::ActiveTarget = m_fboID;

    //stoer previous clear colour
    m_lastClearColour = App::getInstance().getClearColour();
    App::getInstance().setClearColour(cro::Colour::Black);

    //clear buffer - UH OH this will clear the main buffer if FBO is null
    glCheck(glClear(GL_COLOR_BUFFER_BIT |GL_DEPTH_BUFFER_BIT));
#endif
}

void MultiRenderTexture::display()
{
#ifdef PLATFORM_DESKTOP
    //restore viewport
    glCheck(glViewport(m_lastViewport[0], m_lastViewport[1], m_lastViewport[2], m_lastViewport[3]));

    //unbind buffer
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_lastBuffer));
    RenderTarget::ActiveTarget = m_lastBuffer;

    App::getInstance().setClearColour(m_lastClearColour);
#endif
}

void MultiRenderTexture::setViewport(URect viewport)
{
    m_viewport = viewport;
}

URect MultiRenderTexture::getViewport() const
{
    return m_viewport;
}

URect MultiRenderTexture::getDefaultViewport() const
{
    return { 0, 0, m_size.x, m_size.y };
}

TextureID MultiRenderTexture::getTexture(std::size_t idx) const
{
    CRO_ASSERT(idx < m_textureIDs.size(), "");
    return TextureID(m_textureIDs[idx]);
}

TextureID MultiRenderTexture::getDepthTexture() const
{
    return TextureID(m_depthTextureID);
}