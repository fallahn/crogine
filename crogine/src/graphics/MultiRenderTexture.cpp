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
    m_maxAttachments    (-1),
    m_depthTextureID    (0),
    m_size              (0, 0)
{

}

MultiRenderTexture::~MultiRenderTexture()
{
    if (m_fboID)
    {
        glCheck(glDeleteFramebuffers(1, &m_fboID));

        //we've shoehorned in a regular texture in slot 0
        //so only delete buffers > than that
        if (m_textureIDs.size() > 1)
        {
            glCheck(glDeleteTextures(static_cast<GLsizei>(m_textureIDs.size() - 1), &m_textureIDs[1]));
        }
        glCheck(glDeleteTextures(1, &m_depthTextureID));
    }
}

MultiRenderTexture::MultiRenderTexture(MultiRenderTexture&& other) noexcept
    : MultiRenderTexture()
{
    m_fboID = other.m_fboID;
    m_depthTextureID = other.m_depthTextureID;
    m_textureIDs = std::move(other.m_textureIDs);
    m_defaultTexture = std::move(other.m_defaultTexture);
    m_maxAttachments = other.m_maxAttachments;
    setViewport(other.getViewport());
    setView(other.getView());

    other.m_fboID = 0;
    other.m_depthTextureID = 0;
    other.setViewport({ 0, 0, 0, 0 });
    other.setView({ 0.f, 0.f });
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
        m_depthTextureID = other.m_depthTextureID;
        m_textureIDs = std::move(other.m_textureIDs);
        m_defaultTexture = std::move(other.m_defaultTexture);
        m_maxAttachments = other.m_maxAttachments;
        setViewport(other.getViewport());
        setView(other.getView());

        other.m_fboID = 0;
        other.m_depthTextureID = 0;
        other.setViewport({ 0,0,0,0 });
        other.setView({ 0.f, 0.f });
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
    if (width == m_size.x && height == m_size.y && colourCount == m_textureIDs.size())
    {
        return true;
    }

    getMaxAttaments(); //just updates the attachment count if not init
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

    //automatically resizes if already created
    m_defaultTexture.create(width, height);
    if (m_textureIDs.empty())
    {
        //store this so the handles align to indices correctly
        m_textureIDs.push_back(m_defaultTexture.getGLHandle());
    }

    std::uint32_t removeCount = 0;
    if (colourCount == m_textureIDs.size())
    {
        //resize the existing buffers
        for(auto i = 1u; i < m_textureIDs.size(); ++i)
        {
            glBindTexture(GL_TEXTURE_2D, m_textureIDs[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        }

        glCheck(glBindTexture(GL_TEXTURE_2D, m_depthTextureID));
        glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));

        setViewport({ 0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height) });
        setView(FloatRect(getViewport()));
        m_size = { width, height };

        return true;
    }
    else if (colourCount > 1)
    {
        if (colourCount > m_textureIDs.size())
        {
            //add new textures
            for (auto i = m_textureIDs.size(); i < colourCount; ++i)
            {
                std::uint32_t id = 0;
                glCheck(glGenTextures(1, &id));
                glCheck(glBindTexture(GL_TEXTURE_2D, id));
                glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
                m_textureIDs.push_back(id);
            }
        }
        else
        {
            //remove the difference
            removeCount = m_textureIDs.size() - colourCount;
            //for (auto i = colourCount; i < m_textureIDs.size(); ++i)
            {
                glCheck(glDeleteTextures(removeCount, &m_textureIDs[colourCount]));
            }

            std::vector<std::uint32_t> temp(m_textureIDs.begin(), m_textureIDs.begin() + (colourCount - 1));
            m_textureIDs.swap(temp);
        }
    }
    //and rebind to FBO - TODO if we just removed attachments we need to bind 0 to the previous attachment points...
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_fboID));

    std::vector<GLenum> attachments;
    glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_defaultTexture.getGLHandle(), 0));
    attachments.push_back(GL_COLOR_ATTACHMENT0);

    std::uint32_t i = 1u;
    for (; i < m_textureIDs.size(); ++i)
    {
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_textureIDs[i], 0));
        attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
    }
    glCheck(glDrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data()));

    //set previously used to null
    for (; i < m_textureIDs.size() + removeCount; ++i)
    {
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0));
    }

    bool result = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    setViewport({ 0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height) });
    setView(FloatRect(getViewport()));
    m_size = { width, height };

    return result;
    

    //return false;
#endif
}

glm::uvec2 MultiRenderTexture::getSize() const
{
    return m_size;
}

void MultiRenderTexture::clear(cro::Colour clearColour)
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(m_fboID, "No FBO created!");


    //store active buffer and bind this one
    setActive(true);

    //store previous clear colour
    glCheck(glGetFloatv(GL_COLOR_CLEAR_VALUE, m_lastClearColour.data()));
    glCheck(glClearColor(clearColour.getRed(), clearColour.getGreen(), clearColour.getBlue(), clearColour.getAlpha()));

    //clear buffer - UH OH this will clear the main buffer if FBO is null
    glCheck(glClear(GL_COLOR_BUFFER_BIT |GL_DEPTH_BUFFER_BIT));
#endif
}

void MultiRenderTexture::clear(const std::vector<Colour>& colours)
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(m_fboID, "No FBO created!");


    //store active buffer and bind this one
    setActive(true);

    //store previous clear colour
    glCheck(glGetFloatv(GL_COLOR_CLEAR_VALUE, m_lastClearColour.data()));

    for (auto i = 0u; i < colours.size() && i < m_textureIDs.size(); ++i)
    {
        glCheck(glClearBufferfv(GL_COLOR, i, colours[i].asArray()));
    }
    static const auto depthColour = glm::vec4(1.f);
    glCheck(glClearBufferfv(GL_DEPTH, 0, &depthColour[0]));
#endif
}

void MultiRenderTexture::display()
{
#ifdef PLATFORM_DESKTOP

    //unbind buffer
    setActive(false);

    glCheck(glClearColor(m_lastClearColour[0], m_lastClearColour[1], m_lastClearColour[2], m_lastClearColour[3]));
#endif
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

std::int32_t MultiRenderTexture::getMaxAttaments() const
{
    if (m_maxAttachments == -1)
    {
        glCheck(glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &m_maxAttachments));
    }
    return m_maxAttachments;
}