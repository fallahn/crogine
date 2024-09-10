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
    static constexpr std::int32_t MulliganID = 255;
    static constexpr std::int32_t MaxMulligans = 20;

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
        case MulliganID:
            basePath += "career/";
            assertPath();
            return basePath + "hole.dat";
        }

        return basePath + "progr.ess";
    }

    static inline void write(std::int32_t leagueID, std::size_t holeIndex, const std::vector<std::uint8_t>& holeScores, std::int32_t mulliganCount)
    {
        auto path = getFilePath(leagueID);
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "wb");
        if (file.file)
        {
            static constexpr std::size_t MaxBytes = 26; //size of holeIndex + 18 scores. TODO this will make files INCOMPATIBLE cross platform if size_t is defined as a different size...

            auto written = file.file->write(file.file, &holeIndex, sizeof(holeIndex), 1);

            const auto scoreSize = std::min(std::size_t(18), holeScores.size());
            for (auto i = 0u; i < scoreSize; ++i)
            {
                written += file.file->write(file.file, &holeScores[i], 1, 1);
            }
            //for (auto i = scoreSize; i < 18; ++i)
            while (written < MaxBytes)
            {
                std::uint8_t packing = 0;
                written += file.file->write(file.file, &packing, 1, 1);
            }
        }





        //10 here being the maxnumber of leagues (or greater than)
        //if the league count ever increases more than this, this
        //will break...
        std::array<std::int32_t, MaxMulligans> values = {};
        std::fill(values.begin(), values.end(), 0);

        path = getFilePath(MulliganID);
        cro::RaiiRWops file2;
        file2.file = SDL_RWFromFile(path.c_str(), "rb");

        if (file2.file)
        {
            file2.file->read(file2.file, values.data(), sizeof(values), 1);
            SDL_RWclose(file2.file);
        }

        values[leagueID] = std::min(1, mulliganCount);
        file2.file = SDL_RWFromFile(path.c_str(), "wb");

        if (file2.file)
        {
            file2.file->write(file2.file, values.data(), sizeof(values), 1);
        }
    }

    static inline bool read(std::int32_t leagueID, std::uint64_t& holeIndex, std::vector<std::uint8_t>& holeScores, std::int32_t& mulliganCount)
    {
        CRO_ASSERT(!holeScores.empty(), "");

        auto path = getFilePath(leagueID);
        if (cro::FileSystem::fileExists(path))
        {
            cro::RaiiRWops file;
            file.file = SDL_RWFromFile(path.c_str(), "rb");
            if (file.file)
            {
                /*auto size = file.file->seek(file.file, 0, RW_SEEK_END);
                file.file->seek(file.file, 0, RW_SEEK_SET);*/

                std::array<std::uint8_t, sizeof(holeIndex) + 18> buffer = {};
                std::size_t i = 0u;
                while (file.file->read(file.file, &buffer[i], 1, 1) 
                    && i < buffer.size() - 1) //hm some existing files have 1 byte padding too many
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


                //read whether or not we have a mulligan remaining
                std::array<std::int32_t, MaxMulligans> values = {};
                std::fill(values.begin(), values.end(), 0);

                path = getFilePath(MulliganID);
                cro::RaiiRWops file2;
                file2.file = SDL_RWFromFile(path.c_str(), "rb");

                if (file2.file)
                {
                    file2.file->read(file2.file, values.data(), sizeof(values), 1);
                    SDL_RWclose(file2.file);

                    mulliganCount = std::min(1, values[leagueID]);
                }


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
