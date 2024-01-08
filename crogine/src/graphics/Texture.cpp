/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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
#include <crogine/graphics/ImageArray.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/detail/Assert.hpp>

#include "../detail/GLCheck.hpp"
#include "../detail/stb_image.h"
#include "../detail/stb_image_write.h"
#include "../detail/SDLImageRead.hpp"
#include <SDL_rwops.h>

#include <algorithm>
#include <filesystem>

using namespace cro;

namespace
{
    //std::uint32_t ensurePOW2(std::uint32_t size)
    //{
    //    /*std::uint32_t pow2 = 1;
    //    while (pow2 < size)
    //    {
    //        pow2 *= 2;
    //    }
    //    return pow2;*/
    //    return size; //TODO this needs to not exlude combination resolutions such as 768
    //}
}

Texture::Texture()
    : m_size        (0),
    m_format        (ImageFormat::None),
    m_handle        (0),
    m_type          (GL_UNSIGNED_BYTE),
    m_smooth        (false),
    m_repeated      (false),
    m_hasMipMaps    (false)
{

}

Texture::Texture(Texture&& other) noexcept
    : m_size    (other.m_size),
    m_format    (other.m_format),
    m_handle    (other.m_handle),
    m_type      (other.m_type),
    m_smooth    (other.m_smooth),
    m_repeated  (other.m_repeated),
    m_hasMipMaps(other.m_hasMipMaps)
{
    other.m_size = glm::uvec2(0);
    other.m_format = ImageFormat::None;
    other.m_handle = 0;
    other.m_type = GL_UNSIGNED_BYTE;
    other.m_smooth = false;
    other.m_repeated = false;
    other.m_hasMipMaps = false;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        Texture temp;
        swap(temp); //make sure this deals with any existing texture we had.

        m_size = other.m_size;
        m_format = other.m_format;
        m_type = other.m_type;
        m_handle = other.m_handle;
        m_smooth = other.m_smooth;
        m_repeated = other.m_repeated;
        m_hasMipMaps = other.m_hasMipMaps;

        other.m_size = glm::uvec2(0);
        other.m_format = ImageFormat::None;
        other.m_type = GL_UNSIGNED_BYTE;
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
void Texture::create(std::uint32_t width, std::uint32_t height, ImageFormat::Type format)
{
    CRO_ASSERT(width > 0 && height > 0, "Invalid texture size");
    CRO_ASSERT(format != ImageFormat::None, "Invalid image format");

    //width = ensurePOW2(width);
    //height = ensurePOW2(height);

    width = std::min(width, getMaxTextureSize());
    height = std::min(height, getMaxTextureSize());

    if (!m_handle)
    {
        GLuint handle;
        glCheck(glGenTextures(1, &handle));
        m_handle = handle;
    }

    m_size = { width, height };
    m_format = format;

    auto wrap = m_repeated ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    auto smooth = m_smooth ? GL_LINEAR : GL_NEAREST;
    //GLint texFormat = GL_RGB8;
    GLint uploadFormat = GL_RGB;
    std::int32_t pixelSize = 3;
    if (format == ImageFormat::RGBA)
    {
        //texFormat = GL_RGBA8;
        uploadFormat = GL_RGBA;
        pixelSize = 4;
    }
    else if(format == ImageFormat::A)
    {
        //texFormat = GL_R8;
        uploadFormat = GL_RED;
        pixelSize = 1;
    }


    //let's fill the texture with known empty values
    std::vector<std::uint8_t> buffer(width * height * pixelSize);
    std::fill(buffer.begin(), buffer.end(), 0);

    glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
//#ifdef GL41
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, uploadFormat, width, height, 0, uploadFormat, m_type, buffer.data()));
//#else
//    glCheck(glTexStorage2D(GL_TEXTURE_2D, 1, texFormat, width, height));
//    glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
//    glCheck(glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0,width,height, uploadFormat, m_type, buffer.data()));
//#endif
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth));
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));
}

bool Texture::loadFromFile(const std::string& filePath, bool createMipMaps)
{
    std::filesystem::path p(filePath);
    auto path = FileSystem::getResourcePath();
    //only add resource path if not done so already
    if (!p.is_absolute() &&
        filePath.find(path) == std::string::npos)
    {
        path += filePath;
    }
    else
    {
        path = filePath;
    }

    ImageArray<std::uint8_t> arr;
    if (arr.loadFromFile(path, true))
    {
        m_type = GL_UNSIGNED_BYTE;

        auto size = arr.getDimensions();
        CRO_ASSERT(size.x * size.y * arr.getChannels() == arr.size(), "");
        create(size.x, size.y, arr.getFormat());
        return update(arr.data(), createMipMaps);
    }

    return false;
}

bool Texture::loadFromImage(const Image& image, bool createMipMaps)
{
    if (image.getPixelData() == nullptr)
    {
        LogE << "Failed creating texture from image: Image is empty." << std::endl;
        return false;
    }

    auto size = image.getSize();

    create(size.x, size.y, image.getFormat());
    return update(image.getPixelData(), createMipMaps);
}

bool Texture::update(const std::uint8_t* pixels, bool createMipMaps, URect area)
{
    m_type = GL_UNSIGNED_BYTE;
    return update(static_cast<const void*>(pixels), createMipMaps, area);
}

bool Texture::update(const std::uint16_t* pixels, bool createMipMaps, URect area)
{
    m_type = GL_UNSIGNED_SHORT;
    return update(static_cast<const void*>(pixels), createMipMaps, area);
}

bool Texture::update(const Texture& other, std::uint32_t xPos, std::uint32_t yPos)
{
    CRO_ASSERT(xPos + other.m_size.x <= m_size.x, "Won't fit!");
    CRO_ASSERT(yPos + other.m_size.y <= m_size.y, "Won't fit!");
    CRO_ASSERT(m_format == other.m_format, "Texture formats don't match");
    CRO_ASSERT(m_type == other.m_type, "Texture types don't match");

    if (!m_handle || !other.m_handle || (other.m_handle == m_handle))
    {
        return false;
    }

    auto bpp = 4;
    GLenum format = GL_RGBA;

    if (other.m_format == ImageFormat::RGB)
    {
        bpp = 3;
        format = GL_RGB;
    }
    else if (other.m_format == ImageFormat::A)
    {
        bpp = 1;
        format = GL_RED;
    }

    std::vector<std::uint8_t> buffer(other.m_size.x * other.m_size.y * bpp);

    //TODO assert this works on GLES too
#ifdef PLATFORM_DESKTOP
    glCheck(glBindTexture(GL_TEXTURE_2D, other.m_handle));
    glCheck(glGetTexImage(GL_TEXTURE_2D, 0, format, other.m_type, buffer.data()));
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));
#else
    //we don't have glGetTexImage on GLES
    GLuint frameBuffer = 0;
    glCheck(glGenFramebuffers(1, &frameBuffer));
    if (frameBuffer)
    {
        GLint previousFrameBuffer;
        glCheck(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFrameBuffer));

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, other.m_handle, 0));
        glCheck(glReadPixels(0, 0, other.m_size.x, other.m_size.y, format, other.m_type, buffer.data()));
        glCheck(glDeleteFramebuffers(1, &frameBuffer));

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, previousFrameBuffer));
    }

#endif //PLATFORM_DESKTOP

    //TODO set buffer type based on other texture data type
    update(buffer.data(), false, { xPos, yPos, other.m_size.x, other.m_size.y });

    return true;
}

glm::uvec2 Texture::getSize() const
{
    return m_size;
}

ImageFormat::Type Texture::getFormat() const
{
    return m_format;
}

std::uint32_t Texture::getGLHandle() const
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

void Texture::setBorderColour(Colour colour)
{
    if (m_handle)
    {
        m_repeated = false;

        glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
        glCheck(glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colour.asArray()));
        glCheck(glBindTexture(GL_TEXTURE_2D, 0));
    }
}

std::uint32_t Texture::getMaxTextureSize()
{
    if (Detail::SDLResource::valid())
    {
        GLint size;
        glCheck(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size));
        return size;
    }

    Logger::log("No valid gl context when querying max texture size", Logger::Type::Error);
    return 0;
}

void Texture::swap(Texture& other)
{
    std::swap(m_size, other.m_size);
    std::swap(m_format, other.m_format);
    std::swap(m_type, other.m_type);
    std::swap(m_handle, other.m_handle);
    std::swap(m_smooth, other.m_smooth);
    std::swap(m_repeated, other.m_repeated);
    std::swap(m_hasMipMaps, other.m_hasMipMaps);
}

FloatRect Texture::getNormalisedSubrect(FloatRect rect) const
{
    if (m_handle == 0)
    {
        return {};
    }

    return { rect.left / m_size.x, rect.bottom / m_size.y, rect.width / m_size.x, rect.height / m_size.y };
}

bool Texture::saveToFile(const std::string& path) const
{
    if (m_handle == 0)
    {
        LogE << "Failed to save " << path << "Texture not created." << std::endl;
        return false;
    }

    auto filePath = path;
    if (cro::FileSystem::getFileExtension(filePath) != ".png")
    {
        filePath += ".png";
    }

    CRO_ASSERT(m_type == GL_UNSIGNED_BYTE, "Need to implement writing unsigned short!");

    std::vector<GLubyte> buffer(m_size.x * m_size.y * 4);

    glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
    glCheck(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data()));
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));

    //flip row order
    stbi_flip_vertically_on_write(1);

    RaiiRWops out;
    out.file = SDL_RWFromFile(filePath.c_str(), "w");
    auto result = stbi_write_png_to_func(image_write_func, out.file, m_size.x, m_size.y, 4, buffer.data(), m_size.x * 4);

    if (result == 0)
    {
        LogE << "Failed writing " << path << std::endl;

        return false;
    }
    return true;
}

bool Texture::saveToImage(Image& dst) const
{
    if (m_handle == 0)
    {
        LogE << "Failed to save texture to image, texture not created." << std::endl;
        return false;
    }

    CRO_ASSERT(m_type == GL_UNSIGNED_BYTE, "Unsigned short is not supported");

    std::vector<GLubyte> buffer(m_size.x * m_size.y * 4);

    glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
    glCheck(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data()));
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));

    dst.loadFromMemory(buffer.data(), m_size.x, m_size.y, cro::ImageFormat::RGBA);

    return true;
}

bool Texture::saveToBuffer(std::vector<float>& dst) const
{
    if (m_handle == 0)
    {
        LogE << "Failed to save texture to image, texture not created." << std::endl;
        return false;
    }

    dst.clear();
    dst.resize(m_size.x * m_size.y * 4);

    glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
    glCheck(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, dst.data()));
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));

    return true;
}

//private
bool Texture::update(const void* pixels, bool createMipMaps, URect area)
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

        GLint format = GL_RGB;
        if (m_format == ImageFormat::RGBA)
        {
            format = GL_RGBA;
        }
        else if (m_format == ImageFormat::A)
        {
            format = GL_RED;
        }


        glCheck(glBindTexture(GL_TEXTURE_2D, m_handle));
        glCheck(glTexSubImage2D(GL_TEXTURE_2D, 0, area.left, area.bottom, area.width, area.height, format, m_type, pixels));

        //attempt to generate mip maps and set correct filter
        if (m_hasMipMaps || createMipMaps)
        {
            generateMipMaps();
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

void Texture::generateMipMaps(/*const std::uint8_t* pixels, URect area*/)
{
#ifdef CRO_DEBUG_
    std::int32_t result = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &result);
    CRO_ASSERT(result == m_handle, "Texture not bound!");
#endif

    //if (m_smooth)
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
            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST));
            m_hasMipMaps = true;
        }
    }

    //else
    //{
    //    //generate mipmaps by hand, using a 'nearest' filter
    //    CRO_ASSERT(pixels != nullptr, "");
    //    CRO_ASSERT(area.width != 0, "");
    //    CRO_ASSERT(area.height != 0, "");

    //    std::int32_t stride = 4;
    //    GLint format = GL_RGBA;
    //    if (m_format == ImageFormat::RGB)
    //    {
    //        stride = 3;
    //        format = GL_RGB;
    //    }
    //    else if (m_format == ImageFormat::A)
    //    {
    //        stride = 1;
    //        format = GL_RED;
    //    }

    //    std::vector<std::uint8_t> buffA(area.width * area.height * stride); //technically we only need to be 1/4 the size first iter...
    //    std::vector<std::uint8_t> buffB(area.width * area.height * stride);

    //    auto src = pixels;
    //    auto dst = buffA.data();
    //    GLint level = 1;
    //    GLuint sizeX = m_size.x;
    //    GLuint sizeY = m_size.y;

    //    while (area.width > 1 && area.height > 1)
    //    {
    //        std::int32_t i = 0;
    //        for (auto y = 0u; y < area.height; y += 2)
    //        {
    //            for (auto x = 0u; x < area.width; x += 2)
    //            {
    //                //TODO add in area left/bottom

    //                auto p = (area.width * stride) * y + (x * stride);
    //                for (auto j = 0; j < stride; ++j)
    //                {
    //                    dst[i++] = src[p + j];
    //                }
    //            }
    //        }            
    //        
    //        //mipmaps require both dimensions go down to 1
    //        //so for non-square images 8x1, 4x1, 2x1 are all required
    //        if (area.width > 1)
    //        {
    //            area.width /= 2;
    //            sizeX /= 2;
    //        }
    //        if (area.height > 1)
    //        {
    //            area.height /= 2;
    //            sizeY /= 2;
    //        }
    //        area.left /= 2;
    //        area.bottom /= 2;

    //        //we're assuming texture is currently bound
    //        glCheck(glTexImage2D(GL_TEXTURE_2D, level++, format, sizeX, sizeY, 0, format, GL_UNSIGNED_BYTE, dst));
    //        //glCheck(glTexSubImage2D(GL_TEXTURE_2D, level++, area.left, area.bottom, area.width, area.height, format, GL_UNSIGNED_BYTE, dst));


    //        //probably a nicer way to swap these, but let's just get things
    //        //working first, eh?
    //        src = (src == buffA.data()) ? buffB.data() : buffA.data();
    //        dst = (dst == buffA.data()) ? buffB.data() : buffA.data();
    //    }

    //    //while this works (ish) texture atlases will be reduced down to the 
    //    //corner of the given area, not the corner of the subrect - somthing we'll
    //    //never be able to read here. A different tactic is requried methinks.
    //    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST));
    //    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0));
    //    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level - 1));
    //}
}