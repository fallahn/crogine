/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "VatFile.hpp"

#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/ImageArray.hpp>

namespace
{
    enum
    {
        Model     = 0x1,
        FrameRate = 0x2,
        Position  = 0x4,
        Normal    = 0x8,

        FileOK = Model|FrameRate|Position|Normal
    };
}

VatFile::VatFile()
    : m_frameRate   (0.f),
    m_frameCount    (0),
    m_frameLoop     (0),
    m_binaryDims    (0u)
{

}

//public
bool VatFile::loadFromFile(const std::string& path)
{
    reset();

    std::int32_t resultFlags = 0;
    cro::ConfigFile file;
    if (!file.loadFromFile(path))
    {
        return false;
    }

    glm::uvec2 imageSize(0); //used to assert all binary data is the correct size

    std::string workingPath = cro::FileSystem::getFilePath(path);
    const auto& props = file.getProperties();
    for (const auto& prop : props)
    {
        const auto& name = prop.getName();
        if (name == "model")
        {
            auto filepath = workingPath + prop.getValue<std::string>();
            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + filepath))
            {
                resultFlags |= Model;
                m_modelPath = filepath;
            }
            else
            {
                LogE << "Couldn't find file at " << filepath << std::endl;
            }
        }
        else if (name == "frame_rate")
        {
            float frameRate = prop.getValue<float>();
            m_frameRate = frameRate;
            if (frameRate > 0)
            {
                resultFlags |= FrameRate;
            }
        }
        else if (name == "position")
        {
            auto filepath = workingPath + prop.getValue<std::string>();
            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + filepath))
            {
                resultFlags |= Position;
                m_dataPaths[DataID::Position] = filepath;

                cro::Image img;
                img.loadFromFile(m_dataPaths[DataID::Position]);
                m_frameCount = img.getSize().y;

                imageSize = img.getSize();
            }
            else
            {
                LogE << "Couldn't find file at " << filepath << std::endl;
            }
        }
        else if (name == "normal")
        {
            auto filepath = workingPath + prop.getValue<std::string>();
            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + filepath))
            {
                resultFlags |= Normal;
                m_dataPaths[DataID::Normal] = filepath;
            }
            else
            {
                LogE << "Couldn't find file at " << filepath << std::endl;
            }
        }
        else if (name == "tangent")
        {
            auto filepath = workingPath + prop.getValue<std::string>();
            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + filepath))
            {
                m_dataPaths[DataID::Tangent] = filepath;
            }
            else
            {
                LogE << "Couldn't find file at " << filepath << std::endl;
            }
        }
        else if (name == "diffuse")
        {
            auto filepath = workingPath + prop.getValue<std::string>();
            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + filepath))
            {
                m_diffusePath = filepath;
            }
        }
        else if (name == "frame_count")
        {
            m_frameLoop = std::max(0, prop.getValue<std::int32_t>());
        }
    }

    if (m_frameCount == 0)
    {
        //TODO we could assert other images are the same size as
        //the position image, but this won't break anything - we just
        //want to prevent a div0 by having a zero frame count.
        LogE << "Invalid animation frame count" << std::endl;
        return false;
    }

    m_frameLoop = std::min(m_frameLoop, m_frameCount);


    //if we loaded OK check for binary data - this is optional
    //and the vats can fall back to low-res data if needed
    if (resultFlags == FileOK)
    {
        for (auto i = 0; i < DataID::Count; ++i)
        {
            if (!m_dataPaths[i].empty())
            {
                auto ext = cro::FileSystem::getFileExtension(m_dataPaths[i]);
                auto binPath = m_dataPaths[i].substr(0, m_dataPaths[i].find(ext)) + ".bin";

                if (cro::FileSystem::fileExists(binPath))
                {
                    loadBinary(binPath, m_binaryData[i], imageSize);
                }
            }
        }

        auto expectedSize = imageSize.x * imageSize.y * 4;
        if (m_binaryData[DataID::Position].size() != expectedSize
            || m_binaryData[DataID::Normal].size() != expectedSize)
        {
            //something didn't load, so ditch it all else we
            //won't be able to create the array texture
            for (auto& bin : m_binaryData)
            {
                bin.clear();
            }
            m_binaryDims = glm::uvec2(0u);
        }
        else
        {
            m_binaryDims = imageSize;
        }
    }

    return resultFlags == FileOK;
}

const std::string& VatFile::getPositionPath() const
{
    return m_dataPaths[DataID::Position];
}

const std::string& VatFile::getNormalPath() const
{
    return m_dataPaths[DataID::Normal];
}

const std::string& VatFile::getTangentPath() const
{
    return m_dataPaths[DataID::Tangent];
}

bool VatFile::hasTangents() const
{
    return !m_dataPaths[DataID::Tangent].empty();
}

bool VatFile::fillArrayTexture(cro::ArrayTexture<float, 4u>& arrayTexture) const
{
    if (m_binaryDims.x == 0 || m_binaryDims.y == 0)
    {
        return false;
    }

    arrayTexture.create(m_binaryDims.x, m_binaryDims.y);

    cro::ImageArray<float> diffuseMap;
    if (diffuseMap.loadFromFile(cro::FileSystem::getResourcePath() + m_diffusePath, true))
    {
        if (!arrayTexture.insertLayer(diffuseMap, 0))
        {
            return false;
        }
    }
    
    if (!arrayTexture.insertLayer(m_binaryData[DataID::Position], 1))
    {
        return false;
    }

    if (!arrayTexture.insertLayer(m_binaryData[DataID::Normal], 2))
    {
        return false;
    }

    if (hasTangents())
    {
        if (!arrayTexture.insertLayer(m_binaryData[DataID::Tangent], 3))
        {
            return false;
        }
    }

    return true;
}

//private
void VatFile::loadBinary(const std::string& path, std::vector<float>& dst, glm::uvec2 dims)
{
    dst.clear();
    dst.resize(dims.x * dims.y * 4);

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "rb");
    if (file.file)
    {
        auto read = SDL_RWread(file.file, dst.data(), dst.size() * sizeof(float), 1);
        if (read == 0)
        {
            LogI << SDL_GetError() << std::endl;
            dst.clear();
        }
        //TODO check the file size isn't *greater* than the amount of data we're trying to read.
    }
}

void VatFile::reset()
{
    m_frameRate = 0.f;
    m_frameCount = 0;
    m_frameLoop = 0;
    m_binaryDims = glm::uvec2(0u);

    m_modelPath.clear();

    for (auto& path : m_dataPaths)
    {
        path.clear();
    }

    for (auto& bin : m_binaryData)
    {
        bin.clear();
    }
}