/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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

#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/Log.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

namespace
{
    //same order as GL_TEXTURE_CUBE_MAP_XXXX_YYYY
    enum CubemapDirection
    {
        Right, Left, Up, Down, Front, Back, Count
    };

    const std::array<std::string, CubemapDirection::Count> Labels =
    {
        "right", "left", "up", "down", "front", "back"
    };
}

CubemapTexture::CubemapTexture()
    : m_handle      (0),
    m_cubemapCount  (0)
{

}

CubemapTexture::CubemapTexture(CubemapTexture&& other) noexcept
    : CubemapTexture()
{
    std::swap(m_handle, other.m_handle);
    std::swap(m_cubemapCount, other.m_cubemapCount);
}

CubemapTexture& CubemapTexture::operator= (CubemapTexture&& other) noexcept
{
    //hmm we should check the other handle is not this
    //but if it is we're doing something silly like self-assigning?

    if (m_handle == other.m_handle)
    {
        return *this;
    }

    if (m_handle)
    {
        glCheck(glDeleteTextures(1, &m_handle));
        m_handle = 0;
        m_cubemapCount = 0;
    }
    std::swap(m_handle, other.m_handle);
    std::swap(m_cubemapCount, other.m_cubemapCount);

    return *this;
}

CubemapTexture::~CubemapTexture()
{
    if (m_handle)
    {
        glCheck(glDeleteTextures(1, &m_handle));
    }
}

//public
std::uint32_t CubemapTexture::getGLHandle() const
{
    return m_handle;
}

bool CubemapTexture::loadFromFile(const std::string& path)
{
    std::array<std::string, CubemapDirection::Count> paths = {};
    if (!parseInputFile(path, paths))
    {
        return false;
    }


    if (m_handle == 0)
    {
        glCheck(glGenTextures(1, &m_handle));
    }

    if (m_handle)
    {
        //load textures, filling in fallback where needed
        Image fallback;
        fallback.create(2, 2, Colour::Magenta, ImageFormat::RGB);

        Image side(true);

        glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_handle));

        Image* currImage = &fallback;
        GLenum format = GL_RGB;
        std::uint32_t prevSize = 0;

        for (auto i = 0u; i < CubemapDirection::Count; i++)
        {
            if (side.loadFromFile(paths[i]))
            {
                currImage = &side;
                if (currImage->getFormat() == ImageFormat::RGB)
                {
                    format = GL_RGB;
                }
                else if (currImage->getFormat() == ImageFormat::RGBA)
                {
                    format = GL_RGBA;
                }
                else
                {
                    currImage = &fallback;
                    format = GL_RGB;
                }
            }
            else
            {
                currImage = &fallback;
                format = GL_RGB;
            }

            auto size = currImage->getSize();
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, size.x, size.y, 0, format, GL_UNSIGNED_BYTE, currImage->getPixelData());

            if (i != 0 
                && prevSize != size.x)
            {
                LogE << cro::FileSystem::getFileName(path) << ": Cubemap face " << i << " was not the same size as previous faces." << std::endl;
                return false;
            }
            prevSize = size.x;
        }
        glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));

        m_cubemapCount = 1;

        return true;
    }

    LogE << "Failed creating texture handle" << std::endl;
    return false;
}

bool CubemapTexture::loadFromFiles(const std::vector<std::string>& paths)
{
    if (paths.empty())
    {
        LogE << "Cubemap paths was empty" << std::endl;
        return false;
    }

    //don't create an array if this is a regular map
    if (paths.size() == 1)
    {
        return loadFromFile(paths[0]);
    }

    if (m_handle == 0)
    {
        glCheck(glGenTextures(1, &m_handle));
    }

    //this is a complete faff - we need to parse at least one image first
    //so we can allot the memory up front based on the size/depth of the
    //very first image.

    if (m_handle)
    {
        std::array<std::string, CubemapDirection::Count> imagePaths = {};
        if (!parseInputFile(paths[0], imagePaths))
        {
            return false;
        }

        Image fallback;
        Image side(true);
        if (!side.loadFromFile(imagePaths[0]))
        {
            return false;
        }

        auto size = side.getSize();
        fallback.create(size.x, size.y, Colour::Magenta, side.getFormat());

        GLenum format = side.getFormat() == cro::ImageFormat::RGB ? GL_RGB : side.getFormat() == cro::ImageFormat::RGBA ? GL_RGBA : GL_R8;
        if (format == GL_R8)
        {
            LogE << "Bad image format" << std::endl;
            return false;
        }

        GLsizei currentDepth = 0;
        glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_handle));
        glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, format, size.x, size.y, paths.size() * 6, 0, format, GL_UNSIGNED_BYTE, nullptr);

        for (const auto& cm : paths)
        {
            if (parseInputFile(cm, imagePaths))
            {
                Image* currImage = &fallback;
                std::uint32_t prevSize = 0;

                for (auto i = 0u; i < CubemapDirection::Count; i++)
                {
                    if (side.loadFromFile(imagePaths[i]))
                    {
                        if (side.getFormat() == fallback.getFormat()
                            && side.getSize() == fallback.getSize())
                        {
                            currImage = &side;
                        }
                        else
                        {
                            currImage = &fallback;
                        }                        
                    }
                    else
                    {
                        currImage = &fallback;
                    }

                    size = currImage->getSize();
                    glCheck(glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, (m_cubemapCount * 6) + i, size.x, size.y, 1, format, GL_UNSIGNED_BYTE, currImage->getPixelData()));
                    if (i != 0
                        && prevSize != size.x)
                    {
                        LogE << cro::FileSystem::getFileName(cm) << ": Cubemap face " << i << " was not the same size as previous faces." << std::endl;
                    }
                    prevSize = size.x;
                }

                m_cubemapCount++;
            }
        }
    }

    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));

    return m_cubemapCount != 0;
}

void CubemapTexture::generateMipMaps()
{
    if (m_handle)
    {
        glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_handle));
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        auto err = glGetError();
        if (err == GL_INVALID_OPERATION || err == GL_INVALID_ENUM)
        {
            LOG("Failed to create Mipmaps", Logger::Type::Warning);
        }
        else
        {
            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
        }
    }
    else
    {
        LOG("Cubemap texture: no mipmaps generated, texture not loaded.", Logger::Type::Warning);
    }
}

//private
bool CubemapTexture::parseInputFile(const std::string& path, std::array<std::string, CubemapDirection::Count>& outPaths)
{
    if (FileSystem::getFileExtension(path) != ".ccm")
    {
        LogE << path << ": not a *.ccm file" << std::endl;
        return false;
    }

    cro::ConfigFile cfg;
    if (!cfg.loadFromFile(path))
    {
        LogE << "Failed to open cubemap " << path << std::endl;
        return false;
    }

    auto currPath = cro::FileSystem::getFilePath(path);
    const auto processPath =
        [&](std::string& outPath, std::string inPath)
    {
        std::replace(inPath.begin(), inPath.end(), '\\', '/');
        if (inPath.find('/') == std::string::npos)
        {
            //assume this is in the same dir
            outPath = currPath + inPath;
        }
        else
        {
            outPath = inPath;
        }
    };

    for (auto i = 0; i < CubemapDirection::Count; ++i)
    {
        if (auto* prop = cfg.findProperty(Labels[i]); prop != nullptr)
        {
            processPath(outPaths[i], prop->getValue<std::string>());
        }
        else
        {
            LogW << "Path to " << Labels[i] << " image is missing from " << path << std::endl;
        }
    }

    return true;
}