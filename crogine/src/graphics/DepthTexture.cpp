/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

DepthTexture::DepthTexture(bool d)
    : m_depthOnly   (d),
    m_fboID         (0),
    m_depthID       (0),
    m_colourID      (0),
    m_returnTexture (0),
    m_size          (0,0),
    m_layerCount    (0)
{

}

DepthTexture::~DepthTexture()
{
#ifndef __APPLE__
    if (!m_layerHandles.empty())
    {
        glCheck(glDeleteTextures(static_cast<GLsizei>(m_layerHandles.size()), m_layerHandles.data()));
    }
#endif

    if (m_fboID)
    {
        glCheck(glDeleteFramebuffers(1, &m_fboID));
    }

    if (m_depthID)
    {
        glCheck(glDeleteTextures(1, &m_depthID));
    }

    if (m_colourID)
    {
        glCheck(glDeleteTextures(1, &m_colourID));
    }
}

DepthTexture::DepthTexture(DepthTexture&& other) noexcept
    : DepthTexture()
{
    m_depthOnly = other.m_depthOnly;
    m_fboID = other.m_fboID;
    m_depthID = other.m_depthID;
    m_colourID = other.m_colourID;
    m_returnTexture = other.m_returnTexture;
    setViewport(other.getViewport());
    setView(other.getView());
    m_layerCount = other.m_layerCount;

    other.m_fboID = 0;
    other.m_depthID = 0;
    other.m_colourID = 0;
    other.m_returnTexture = 0;
    other.setViewport({ 0, 0, 0, 0 });
    other.setView({ 0.f, 0.f });
    other.m_layerCount = 0;

#ifndef __APPLE__
    m_layerHandles.swap(other.m_layerHandles);
#endif
}

DepthTexture& DepthTexture::operator=(DepthTexture&& other) noexcept
{
    if (&other != this)
    {
#ifndef __APPLE__
        if (!m_layerHandles.empty())
        {
            glCheck(glDeleteTextures(static_cast<GLsizei>(m_layerHandles.size()), m_layerHandles.data()));
            m_layerHandles.clear();
        }

        m_layerHandles.swap(other.m_layerHandles);
#endif 

        //tidy up anything we own first!
        if (m_fboID)
        {
            glCheck(glDeleteFramebuffers(1, &m_fboID));
        }

        if (m_depthID)
        {
            glCheck(glDeleteTextures(1, &m_depthID));
        }

        if (m_colourID)
        {
            glCheck(glDeleteTextures(1, &m_colourID));
        }

        m_depthOnly = other.m_depthOnly;
        m_fboID = other.m_fboID;
        m_depthID = other.m_depthID;
        m_colourID = other.m_colourID;
        m_returnTexture = other.m_returnTexture;
        setViewport(other.getViewport());
        setView(other.getView());
        m_layerCount = other.m_layerCount;

        other.m_fboID = 0;
        other.m_depthID = 0;
        other.m_colourID = 0;
        other.m_returnTexture = 0;
        other.setViewport({ 0, 0, 0, 0 });
        other.setView({ 0.f, 0.f });
        other.m_layerCount = 0;
    }
    return *this;
}

//public
bool DepthTexture::create(std::uint32_t width, std::uint32_t height, std::uint32_t layers)
{
#ifdef PLATFORM_MOBILE
    LogE << "Depth Textures are not available on mobile platforms" << std::endl;
    return false;
#else
    CRO_ASSERT(layers > 0, "");

    if (width == m_size.x && height == m_size.y && layers == m_layerCount)
    {
        //don't do anything
        return true;
    }

    if (m_depthID)
    {
#ifdef GL41
        //resize the buffer
        glCheck(glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthID));
        glCheck(glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, width, height, layers, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));

        if (!m_depthOnly)
        {
            glCheck(glBindTexture(GL_TEXTURE_2D_ARRAY, m_colourID));
            glCheck(glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RG32F, width, height, layers, 0, GL_RG, /*GL_FLOAT*/GL_HALF_FLOAT, NULL));
        }

        setViewport({ 0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height) });
        setView(FloatRect(getViewport()));
        m_size = { width, height };
        m_layerCount = layers;

        return true;
#else
        //else we have to regenerate it as it's immutable
        glCheck(glDeleteTextures(1, &m_depthID));

        if (m_colourID)
        {
            glCheck(glDeleteTextures(1, &m_colourID));
        }
#endif
    }

    //else create it
    m_size = { 0, 0 };
    setViewport({ 0, 0, 0, 0 });

    const float borderColor[] = { 1.f, 1.f, 1.f, 1.f };

    //create the texture
    glCheck(glGenTextures(1, &m_depthID));
    glCheck(glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthID));
#ifdef GL41
    //apple drivers don't support immutable textures.
    glCheck(glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, width, height, layers, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));
#else
    glCheck(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT24, width, height, layers));
#endif
    glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER));
    glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
    glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
    glCheck(glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor));
    m_returnTexture = m_depthID;

    if (!m_depthOnly)
    {
        glCheck(glGenTextures(1, &m_colourID));
        glCheck(glBindTexture(GL_TEXTURE_2D_ARRAY, m_colourID));

#ifdef GL41
        //apple drivers don't support immutable textures.
        glCheck(glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RG32F, width, height, layers, 0, GL_RG, /*GL_FLOAT*/GL_HALF_FLOAT, NULL));
#else
        glCheck(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RG32F, width, height, layers));
#endif
        glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER));
        glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
        glCheck(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
        glCheck(glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor));
        //glGenerateMipmap(GL_TEXTURE_2D_ARRAY); //this would need to be done every time the texture is rendered to...

        m_returnTexture = m_colourID;
    }



    //create the frame buffer
    if (m_fboID == 0)
    {
        glCheck(glGenFramebuffers(1, &m_fboID));
    }
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_fboID));
    glCheck(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthID, 0, 0));

    if (!m_depthOnly)
    {
        glCheck(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_colourID, 0, 0));
        glCheck(glDrawBuffer(GL_COLOR_ATTACHMENT0));
    }
    else 
    {
        glCheck(glDrawBuffer(GL_NONE));
    }
    glCheck(glReadBuffer(GL_NONE));

    auto result = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    if (result)
    {
        setViewport({ 0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height) });
        setView(FloatRect(getViewport()));
        m_size = { width, height };
        m_layerCount = layers;
        updateHandles();
    }

    return result;
#endif
}

glm::uvec2 DepthTexture::getSize() const
{
    return m_size;
}

void DepthTexture::clear(std::uint32_t layer)
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(m_fboID, "No FBO created!");
    CRO_ASSERT(m_layerCount > layer, "");

    //store active buffer and bind this one
    setActive(true);

    //I wish we didn't have to branch here, but even using functor/std::function would be indirect
    if (!m_depthOnly)
    {
        glCheck(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthID, 0, layer));
        glCheck(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_colourID, 0, layer));

        glCheck(glColorMask(true, true, false, false));

        glCheck(glGetFloatv(GL_COLOR_CLEAR_VALUE, m_lastClearColour.data()));
        glClearColor(1.f, 1.f, 0.f, 0.f);
        glCheck(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    }
    else
    {
        //TODO does checking to see we're not already on the
        //active layer take less time than just setting it every time?
        glCheck(glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthID, 0, layer));

        glCheck(glColorMask(false, false, false, false));
        glCheck(glClear(GL_DEPTH_BUFFER_BIT));
    }

#endif
}

void DepthTexture::display()
{
#ifdef PLATFORM_DESKTOP
    glCheck(glColorMask(true, true, true, true));

    if (!m_depthOnly)
    {
        glClearColor(m_lastClearColour[0], m_lastClearColour[1], m_lastClearColour[2], m_lastClearColour[3]);
    }

    //unbind buffer
    setActive(false);
#endif
}

TextureID DepthTexture::getTexture() const
{
    return TextureID(m_returnTexture, true);
}

TextureID DepthTexture::getTexture(std::uint32_t index) const
{
#ifdef GL41
    return TextureID(0);
#else
    CRO_ASSERT(index < m_layerHandles.size(), "Layer doesn't exist");
    return TextureID(m_layerHandles[index]);
#endif
}

//private
void DepthTexture::updateHandles()
{
#ifndef GL41
    if (!m_layerHandles.empty())
    {
        //this assumes we've recreated a depth texture (we have to, it's immutable)
        //so we have to delete all the viewtextures and create new ones
        glCheck(glDeleteTextures(static_cast<std::uint32_t>(m_layerHandles.size()), m_layerHandles.data()));
    }

    m_layerHandles.resize(m_layerCount);
    glCheck(glGenTextures(m_layerCount, m_layerHandles.data()));

    for (auto i = 0u; i < m_layerHandles.size(); ++i)
    {
        if (!m_depthOnly)
        {
            glCheck(glTextureView(m_layerHandles[i], GL_TEXTURE_2D, m_colourID, GL_RG32F, 0, 1, i, 1));
        }
        else
        {
            glCheck(glTextureView(m_layerHandles[i], GL_TEXTURE_2D, m_depthID, GL_DEPTH_COMPONENT24, 0, 1, i, 1));
        }
    }
#endif
}