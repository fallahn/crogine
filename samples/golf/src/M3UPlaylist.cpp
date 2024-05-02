/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2024
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "M3UPlaylist.hpp"

#include <crogine/audio/AudioResource.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/util/Random.hpp>

#include <array>

namespace
{
    const std::string FilePrefix("file:///");
    const std::array FileExtensions =
    {
        std::string(".m3u"),
        std::string(".m3u8"),
    };

    //hm, we pre-cache these so this needs to be less than the OpenAL limit for buffers
    constexpr std::size_t MaxTotalFiles = 64;
}

M3UPlaylist::M3UPlaylist(cro::AudioResource& ar, const std::string& searchDir, std::uint32_t maxFiles)
    : m_audioResource   (ar),
    m_currentIndex      (0)
{
    if (cro::FileSystem::directoryExists(searchDir))
    {
        const auto files = cro::FileSystem::listFiles(searchDir);
        const auto fileCount = std::min(files.size(), std::size_t(maxFiles));

        for (auto i = 0u; i < fileCount; ++i)
        {
            const auto& file = files[i];
            auto ext = cro::FileSystem::getFileExtension(file);
            if (std::find(FileExtensions.cbegin(), FileExtensions.cend(), ext) != FileExtensions.cend())
            {
                loadPlaylist(searchDir + file);
            }
        }
    }
}

//public
bool M3UPlaylist::loadPlaylist(const std::string& path)
{
    cro::RaiiRWops rFile;
    rFile.file = SDL_RWFromFile(path.c_str(), "r");

    if (!rFile.file)
    {
        LogE << "Failed opening " << path << std::endl;
        return false;
    }

    if (auto fSize = rFile.file->size(rFile.file); fSize > 0)
    {
        std::vector<std::uint8_t> buffer(fSize);
        auto read = SDL_RWread(rFile.file, buffer.data(), fSize, 1);

        if (read == 0)
        {
            LogE << "failed reading data from " << path << std::endl;
            return false;
        }


        //hmmmm this should probably need to ensure utf8 encoding
        std::string fileString(buffer.cbegin(), buffer.cend());
        std::size_t pos = 0;
        std::size_t prev = 0;

        const auto replaceSpace = [](std::string& str)
        {
            const std::string from("%20");
            const std::string to(" ");

            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) != std::string::npos)
            {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
        };
        const auto parseString = [&](const std::string str)
        {
            if (str.find(FilePrefix) == 0
                && m_resourceIDs.size() < MaxTotalFiles)
            {
                auto fp = str.substr(FilePrefix.size());
                std::replace(fp.begin(), fp.end(), '\\', '/');
                replaceSpace(fp);

                if (fp.back() == '\r')
                {
                    fp.pop_back();
                }
                if (fp.front() == '\r')
                {
                    fp = fp.substr(1);
                }

                if (cro::FileSystem::fileExists(fp))
                {
                    const auto id = m_audioResource.load(fp, true);
                    if (id != -1)
                    {
                        m_resourceIDs.push_back(id);
                    }
                }
            }
        };


        while ((pos = fileString.find('\n', prev)) != std::string::npos)
        {
            parseString(fileString.substr(prev, pos - prev));
            prev = pos + 1;
        }
        parseString(fileString.substr(prev));

        return !m_resourceIDs.empty();
    }

    LogE << path << " file is empty" << std::endl;
    return false;
}

void M3UPlaylist::addTrack(const std::string& path)
{
    static const std::array ValidExt =
    {
        std::string(".mp3"),
        std::string(".ogg"),
        std::string(".wav"),
    };

    if (m_resourceIDs.size() < MaxTotalFiles &&
        std::find(ValidExt.begin(), ValidExt.end(), cro::FileSystem::getFileExtension(path)) != ValidExt.end())
    {
        const auto id = m_audioResource.load(path, true);
        if (id != -1)
        {
            m_resourceIDs.push_back(id);
        }
    }
}

void M3UPlaylist::shuffle()
{
    std::shuffle(m_resourceIDs.begin(), m_resourceIDs.end(), cro::Util::Random::rndEngine);
}

void M3UPlaylist::nextTrack()
{
    if (!m_resourceIDs.empty())
    {
        m_currentIndex = (m_currentIndex + 1) % m_resourceIDs.size();
    }
}

void M3UPlaylist::prevTrack()
{
    if (!m_resourceIDs.empty())
    {
        m_currentIndex = (m_currentIndex + (m_resourceIDs.size() - 1)) % m_resourceIDs.size();
    }
}

std::int32_t M3UPlaylist::getCurrentTrack() const
{
    if (m_resourceIDs.empty())
    {
        return -1;
    }
    return m_resourceIDs[m_currentIndex];
}