/*-----------------------------------------------------------------------

Matt Marchant - 2024
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

#pragma once

#include "Social.hpp"
#include <crogine/core/FileSystem.hpp>
#include <crogine/detail/Types.hpp>

#include <filesystem>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Progress
{

    static inline std::string getFilePath(std::int32_t id)
    {
        std::string basePath = Social::getBaseContentPath();

        const auto assertPath =
            [&]()
            {
                if (!cro::FileSystem::directoryExists(basePath))
                {
                    cro::FileSystem::createDirectory(basePath);
                }
            };

        switch (id)
        {
        default: break;
        case 1:
            basePath += "career/";
            assertPath();
            basePath += "round_01/";
            assertPath();
            break;
        case 2:
            basePath += "career/";
            assertPath();
            basePath += "round_02/";
            assertPath();
            break;
        case 3:
            basePath += "career/";
            assertPath();
            basePath += "round_03/";
            assertPath();
            break;
        case 4:
            basePath += "career/";
            assertPath();
            basePath += "round_04/";
            assertPath();
            break;
        case 5:
            basePath += "career/";
            assertPath();
            basePath += "round_05/";
            assertPath();
            break;
        case 6:
            basePath += "career/";
            assertPath();
            basePath += "round_06/";
            assertPath();
            break;
        }

        return basePath + "progr.ess";
    }

    static inline void write(std::int32_t leagueID, std::size_t holeIndex, const std::vector<std::uint8_t>& holeScores)
    {
        auto path = getFilePath(leagueID);
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "wb");
        if (file.file)
        {
            file.file->write(file.file, &holeIndex, sizeof(holeIndex), 1);

            const auto scoreSize = std::min(std::size_t(18), holeScores.size());
            for (auto i = 0u; i < scoreSize; ++i)
            {
                file.file->write(file.file, &holeScores[i], 1, 1);
            }
            for (auto i = scoreSize; i < 18; ++i)
            {
                std::uint8_t packing = 0;
                file.file->write(file.file, &packing, 1, 1);
            }
        }
    }

    static inline bool read(std::int32_t leagueID, std::size_t& holeIndex, std::vector<std::uint8_t>& holeScores)
    {
        CRO_ASSERT(!holeScores.empty(), "");

        auto path = getFilePath(leagueID);
        if (cro::FileSystem::fileExists(path))
        {
            cro::RaiiRWops file;
            file.file = SDL_RWFromFile(path.c_str(), "rb");
            if (file.file)
            {
                std::array<std::uint8_t, sizeof(holeIndex) + 18> buffer = {};
                std::size_t i = 0u;
                while (file.file->read(file.file, &buffer[i], 1, 1) 
                    && i < buffer.size() - 1) //TODO this is covering up a bug where the file being read was too large...
                {
                    i++;
                }

                if (i < sizeof(holeIndex))
                {
                    std::error_code ec;
                    std::filesystem::remove(path, ec);
                    return false;
                }

                std::memcpy(&holeIndex, buffer.data(), sizeof(holeIndex));

                i -= sizeof(holeIndex);
                auto holeSize = std::min(i, holeScores.size());

                std::memcpy(holeScores.data(), &buffer[sizeof(holeIndex)], holeSize);
                return true;
            }
        }
        return false;
    }

    static inline void clear(std::int32_t leagueID)
    {
        const auto path = getFilePath(leagueID);
        if (cro::FileSystem::fileExists(path))
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }
}