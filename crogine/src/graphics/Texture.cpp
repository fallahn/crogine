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

#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/detail/Assert.hpp>

#include "../detail/GLCheck.hpp"

#include <algorithm>

using namespace cro;

namespace
{
    uint32 ensurePOW2(uint32 size)
    {
        /*uint32 pow2 = 1;
        while (pow2 < size)
        {
            pow2 *= 2;
        }
        return pow2;*/
        return size; //TODO this needs to not exlude combination resolutions such as 768
    }
}

Texture::Texture()
    : m_format          (ImageFormat::None),
    m_handle            (0),
    m_smooth            (false),
    m_repeated          (false),
    m_hasMipMaps        (false)
{

}

Texture::Texture(Texture&& other)
    : m_format  (other.m_format),
    m_handle    (other.m_handle),
    m_smooth    (other.m_smooth),
    m_repeated  (other.m_repeated),
    m_hasMipMaps(other.m_hasMipMaps)
{
    other.m_format = ImageFormat::None;
    other.m_handle = 0;
    other.m_smooth = false;
    other.m_repeated = false;
    other.m_hasMipMaps = false;
}

Texture& Texture::operator=(Texture&& other)
{
    if (this != &other)
    {
        m_format = other.m_format;
        m_handle = other.m_handle;
        m_smooth = other.m_smooth;
        m_repeated = other.m_repeated;
        m_hasMipMaps = other.m_hasMipMaps;

        other.m_format = ImageFormat::None;
        other.m_handle = 0;
        other.m_smooth = false;
        other.m_repeated = false;
        other.m_hasMipMaps = false;
    }
    return *this;
}

Texture::~Texture()
{
    if(m_handle)
    {
        glCheck(glDeleteTextures(1, &m_handle));
    }
}

//public
void Texture::create(uint32 width, uint32 height, ImageFormat::Type format)
{
    CRO_ASSERT(width > 0 && height > 0, "Invalid texture size");
    CRO_ASSERT(format != ImageFormat::None, "Invalid image format");

    //width = ensurePOW2(width);
    //height = ensurePOW2(height);

    width = std::min(width, getMaxTextureSize());
    height = std::min(height, getMaxTextureSize());

    if (m_handle)
    {
        glCheck(glDeleteTextures(1, &m_handle));
    }
    GLuint handle;
    glCheck(glGenTextures(1, &handle));
    m_handle = handle;

    m_size = { width, height };
    m_format = format;

    auto wrap = m_repeated ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    auto smooth = m_smooth ? GL_LINEAR : GL_NEAREST;
    GLint texFormat = GL_RGB;
    if (format == ImageFormat::RGBA)
    {
        texFormat = GL_RGBA;
    }
    else if(format == ImageFormat::A)
    {
        texFormat = GL_RED;
    }

    glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, texFormat, width, height, 0, texFormat, GL_UNSIGNED_BYTE, NULL));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth));
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));
}

bool Texture::loadFromFile(const std::string& path, bool createMipMaps)
{
    Image image;
    if (image.loadFromFile(path))
    {
        auto size = image.getSize();
        /*if (!((size.x & (size.x - 1)) == 0) && ((size.y & (size.y - 1)) == 0))
        {
            LOG("Image not POW2", Logger::Type::Error);
            return false;
        }*/
        
        create(size.x, size.y, image.getFormat());
        return update(image.getPixelData(), createMipMaps);
    }
    
    return false;
}

bool Texture::update(const uint8* pixels, bool createMipMaps, URect area)
{
    if (area.left + area.width > m_size.x)
    {
        Logger::log("Failed updating image, source pixels too wide", Logger::Type::Error);
        return false;
    }
    if (area.bottom + area.height > m_size.y)
    {
        Logger::log("Failed updating image, source pixels too tall", Logger::Type::Error);
        return false;
    }

    if (pixels && m_handle)
    {
        if (area.width == 0) area.width = m_size.x;
        if (area.height == 0) area.height = m_size.y;
        
        GLint format = GL_RGBA;
        if (m_format == ImageFormat::RGB)
        {
            format = GL_RGB;
        }
        else if (m_format == ImageFormat::A)
        {
            format = GL_RED;
        }

        glCheck(glActiveTexture(GL_TEXTURE0));
        glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
        glCheck(glTexSubImage2D(GL_TEXTURE_2D, 0, area.left, area.bottom, area.width, area.height, format, GL_UNSIGNED_BYTE, pixels));
        
        //attempt to generate mip maps and set correct filter
        if (m_hasMipMaps || createMipMaps)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
            auto err = glGetError();
            if (err == GL_INVALID_OPERATION || err == GL_INVALID_ENUM)
            {
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_smooth ? GL_LINEAR : GL_NEAREST));
                m_hasMipMaps = false;
                LOG("Failed to create Mipmaps", Logger::Type::Warning);
            }
            else
            {
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST));
                m_hasMipMaps = true;
                //LOG("Created Mipmaps", Logger::Type::Warning);
            }
        }
        else
        {
            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_smooth ? GL_LINEAR : GL_NEAREST));
            m_hasMipMaps = false;
        }
        glCheck(glBindTexture(GL_TEXTURE_2D, 0));
        return true;
    }

    return false;
}

glm::uvec2 Texture::getSize() const
{
    return m_size;
}

ImageFormat::Type Texture::getFormat() const
{
    return m_format;
}

uint32 Texture::getGLHandle() const
{
    return m_handle;
}

void Texture::setSmooth(bool smooth)
{
    if (smooth != m_smooth)
    {
        m_smooth = smooth;

        if (m_handle)
        {
            glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_smooth ? GL_LINEAR : GL_NEAREST));

            if (m_hasMipMaps)
            {
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST));
            }
            else
            {
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_smooth ? GL_LINEAR : GL_NEAREST));
            }
            glCheck(glBindTexture(GL_TEXTURE_2D, 0));
        }
    }
}

bool Texture::isSmooth() const
{
    return m_smooth;
}

void Texture::setRepeated(bool repeat)
{
    if (repeat != m_repeated)
    {
        m_repeated = repeat;

        if (m_handle)
        {
            glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE));
            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE));
            glCheck(glBindTexture(GL_TEXTURE_2D, 0));
        }
    }
}

bool Texture::isRepeated() const
{
    return m_repeated;
}

uint32 Texture::getMaxTextureSize()
{
    if (Detail::SDLResource::valid())
    {
        GLint size;
        glCheck(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size));
        return size;
    }

    Logger::log("No valid gl context which querying max teure size", Logger::Type::Error);
    return 0;
}