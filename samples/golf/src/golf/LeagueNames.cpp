/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "LeagueNames.hpp"
#include "RandNames.hpp"
#include "CommonConsts.hpp"

#include <crogine/detail/Types.hpp>
#include <crogine/core/Log.hpp>

namespace
{
    constexpr std::size_t MaxNames = 15;
    constexpr std::uint8_t NewLine = '\n';
}

LeagueNames::LeagueNames()
    : m_names(MaxNames)
{
    for (auto i = 0u; i < MaxNames; ++i)
    {
        m_names[i] = RandomNames[i];
    }
}

//public
void LeagueNames::read(const std::string& path)
{
    std::size_t currName = 0;

    cro::RaiiRWops rFile;
    rFile.file = SDL_RWFromFile(path.c_str(), "r");
    if (rFile.file)
    {
        std::size_t read = 0;
        std::uint8_t b = 0;
        std::vector<std::uint8_t> buffer;

        do
        {
            read = SDL_RWread(rFile.file, &b, 1, 1);
            if (b != NewLine
                && buffer.size() < ConstVal::MaxNameChars)
            {
                buffer.push_back(b);
            }
            else if (!buffer.empty())
            {
                m_names[currName++] = cro::String::fromUtf8(buffer.begin(), buffer.end());
                buffer.clear();
            }
        } while (currName < MaxNames && read != 0);
    }

    if (currName < MaxNames)
    {
        //fill out remaining with RandNames
        for (auto i = currName; i < MaxNames; ++i)
        {
            m_names[i] = RandomNames[i];
        }
    }
}

bool LeagueNames::write(const std::string& path) const
{
    cro::RaiiRWops rFile;
    rFile.file = SDL_RWFromFile(path.c_str(), "w");
    if (rFile.file)
    {
        for (const auto& n : m_names)
        {
            auto t = n.toUtf8();
            rFile.file->write(rFile.file, t.c_str(), t.size(), 1);
            rFile.file->write(rFile.file, &NewLine, 1, 1);
        }

        return true;
    }

    return false;
}

const cro::String& LeagueNames::operator[](const std::size_t idx) const
{
    return m_names[idx];
}

cro::String& LeagueNames::operator[](const std::size_t idx)
{
    return m_names[idx];
}

//private