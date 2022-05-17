/*-----------------------------------------------------------------------

Matt Marchant 2022
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
    m_frameLoop     (0)
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
                m_positionPath = filepath;

                cro::Image img;
                img.loadFromFile(m_positionPath);
                m_frameCount = img.getSize().y;
            }
            else
            {
                LogE << "Couldn't find file at " << filepath << std::endl;
            }
        }
        else if (name == "normal")
        {
            auto filepath = workingPath + prop.getValue<std::string>();
            if (cro::FileSystem::fileExists(filepath))
            {
                resultFlags |= Normal;
                m_normalPath = filepath;
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
                m_tangentPath = filepath;
            }
            else
            {
                LogE << "Couldn't find file at " << filepath << std::endl;
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

    return resultFlags == FileOK;
}

//private
void VatFile::reset()
{
    m_frameRate = 0.f;
    m_frameCount = 0;
    m_frameLoop = 0;

    m_modelPath.clear();
    m_positionPath.clear();
    m_normalPath.clear();
    m_tangentPath.clear();
}