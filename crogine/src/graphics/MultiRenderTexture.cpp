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
    m_lastBuffer        (0)
{
    std::fill(m_textureIDs.begin(), m_textureIDs.end(), 0);
}

MultiRenderTexture::~MultiRenderTexture()
{
    if (m_fboID)
    {
        glCheck(glDeleteFramebuffers(1, &m_fboID));
        glCheck(glDeleteTextures(3, m_textureIDs.data()));
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

    other.m_fboID = 0;
    std::fill(other.m_textureIDs.begin(), other.m_textureIDs.end(), 0);
}

MultiRenderTexture& MultiRenderTexture::operator=(MultiRenderTexture&& other) noexcept
{
    if (&other != this)
    {
        //tidy up anything we own first!
        if (m_fboID)
        {
            glCheck(glDeleteFramebuffers(1, &m_fboID));
            glCheck(glDeleteTextures(3, m_textureIDs.data()));
        }

        m_fboID = other.m_fboID;
        m_textureIDs = other.m_textureIDs;
        m_viewport = other.m_viewport;
        m_lastViewport = other.m_lastViewport;
        m_lastBuffer = other.m_lastBuffer;

        other.m_fboID = 0;
        std::fill(other.m_textureIDs.begin(), other.m_textureIDs.end(), 0);
    }
    return *this;
}

//public
bool MultiRenderTexture::create(std::uint32_t width, std::uint32_t height)
{
#ifdef PLATFORM_MOBILE
    LogE << "Depth Textures are not available on mobile platforms" << std::endl;
    return false;
#else
    if (m_fboID)
    {
        //resize the buffers
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[TextureIndex::Position]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glBindTexture(GL_TEXTURE_2D, m_textureIDs[TextureIndex::Normal]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glCheck(glBindTexture(GL_TEXTURE_2D, m_textureIDs[TextureIndex::Depth]));
        glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));

        m_viewport.width = width;
        m_viewport.height = height;
        m_size = { width, height };

        return true;
    }

    //else create them
    m_size = { 0, 0 };
    m_viewport = { 0,0,0,0 };

    //create the texture
    glCheck(glGenTextures(3, m_textureIDs.data()));

    //position
    glCheck(glBindTexture(GL_TEXTURE_2D, m_textureIDs[TextureIndex::Position]));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    //normals
    glCheck(glBindTexture(GL_TEXTURE_2D, m_textureIDs[TextureIndex::Normal]));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    //depth
    glCheck(glBindTexture(GL_TEXTURE_2D, m_textureIDs[TextureIndex::Depth]));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
    const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glCheck(glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor));


    //create the frame buffer
    glCheck(glGenFramebuffers(1, &m_fboID));
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_fboID));
    glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textureIDs[TextureIndex::Position], 0));
    glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_textureIDs[TextureIndex::Normal], 0));
    glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_textureIDs[TextureIndex::Depth], 0));
    
    std::array<GLenum, 2u> colourAttachments =
    {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1
    };
    
    glCheck(glDrawBuffers(colourAttachments.size(), colourAttachments.data()));
    glCheck(glReadBuffer(GL_NONE));

    bool result = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (result)
    {
        m_viewport.width = width;
        m_viewport.height = height;
        m_size = { width, height };
    }
    else
    {
        //we should probably tidy up by deleting partially created
        //buffers/textures?
    }

    return result;
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

TextureID MultiRenderTexture::getPositionTexture() const
{
    return TextureID(m_textureIDs[TextureIndex::Position]);
}

TextureID MultiRenderTexture::getNormalTexture() const
{
    return TextureID(m_textureIDs[TextureIndex::Normal]);
}

TextureID MultiRenderTexture::getDepthTexture() const
{
    return TextureID(m_textureIDs[TextureIndex::Depth]);
}